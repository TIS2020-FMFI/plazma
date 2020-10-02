// *****************************************************************************
//
// Read data from DataQ DI-154RS/DI-194RS
// john@miles.io 18-Mar-10
//
// (Large) portions copied from work of Paul Hubbard et al. 
// (http://users.sdsc.edu/~hubbard/neesgrid/dataq/)
//
// *****************************************************************************

//
// Configuration
//

const S32 DI154_MAX_CHANS = 4;

struct DI154_READING
{
    DOUBLE analog[DI154_MAX_CHANS];
    U16    raw   [DI154_MAX_CHANS];
    bool   digital[3];
    bool   is_valid;
};

enum DI154MSGLVL
{
   DI154_DEBUG = 0,  // Debugging traffic
   DI154_VERBOSE,    // Low-level notice
   DI154_NOTICE,     // Standard notice
   DI154_WARNING,    // Warning (does not imply loss of connection or failure of operation)
   DI154_ERROR       // Error (implies failure of operation, and/or connection loss)
};

class DI154
{
public:
   COMPORT serial;
   S32     precision_bits;
   S32     rate_BPS;
   S32     n_chans;
   S32     sample_bytes;

   DI154()
      {
      precision_bits = 0;
      rate_BPS = 0;
      n_chans = 0;
      sample_bytes = 0;
      }

   virtual ~DI154()
      {
      close();
      }

   virtual void message_sink(DI154MSGLVL level,   
                             C8         *text)
      {
      fprintf(stderr, "%s\n",text);
      }

   virtual void message_printf(DI154MSGLVL level,
                               C8         *fmt,
                               ...)
      {
      C8 buffer[4096];

      va_list ap;

      va_start(ap, 
               fmt);

      memset(buffer, 0, sizeof(buffer));

      _vsnprintf(buffer,
                 sizeof(buffer)-1,
                 fmt,
                 ap);

      va_end(ap);

      //
      // Remove trailing whitespace
      //

      C8 *end = &buffer[strlen(buffer)-1];

      while (end > buffer)
         {
         if (!isspace((U8) *end)) 
            {
            break;
            }

         *end = 0;
         end--;
         }

      message_sink(level,
                   buffer);
      }

   virtual S32 buf_validate(const U8 *buf)
      {
      S32 idx;

      // Initial byte has trailing zero
      if ((buf[0] & 0x01) != 0x00)
         return(1);

      // rest are constant 1
      for (idx = 1; idx < sample_bytes; idx++)
         if ((buf[idx] & 0x01) != 0x01)
            return(idx + 1);

      // success
      return 0;
      }

   virtual S32 daq_sync()
      {
      U8   sync_buf[5 * DI154_MAX_CHANS * 2] = "";
      S32  idx, rc, clear_count;
      bool found_match = false;

      // Get three samples' worth of serial data, plus slop
      S32 len = 4 * sample_bytes;
      rc = serial.read(sync_buf, &len);

      if (rc != 0)
         {
         message_printf(DI154_ERROR, "daq_sync() error: %s", serial.error_text(rc));
         return FALSE;
         }

      // Scan for pattern match
      for (idx = 0; idx < 3 * sample_bytes; idx++)
         {
         rc = buf_validate(&sync_buf[idx]);

         if (rc != 0)
            {
            message_printf(DI154_VERBOSE, "No sync at index %d", idx);
            }
         else
            {
            found_match = true;
            break;
            }
         }

      if (found_match != true)
         {
         message_printf(DI154_ERROR,  "Sync failure");
         return FALSE;
         }

      // Figure how many to read to sync up
      clear_count = idx;

      rc = serial.read(sync_buf, &clear_count);

      if (rc != 0)
         {
         message_printf(DI154_ERROR, "daq_sync() error: %s", serial.error_text(rc));
         return FALSE;
         }
      
      return TRUE;
      }

   virtual S32 buf_decode(const U8      *buf, 
                          DI154_READING *result, 
                          const S32      num_bits)
      {
      U16 atmp[4];
      S32 idx;

      if ((buf == NULL) || (result == NULL))
         {
         return 0;
         }

      // Check for signature
      if (buf_validate(buf) == 0)
         {
         result->is_valid = true;
         }
      else
         {
         result->is_valid = false;

         message_printf(DI154_VERBOSE, "Signature not found; invalid buffer");

         for (idx = 0; idx < sample_bytes; idx++)
            message_printf(DI154_VERBOSE, "  %d: %.02X", idx, buf[idx]);

         return 0;
         }

      //
      // This bit-mangling won't make sense until you stare at the
      // vendor-supplied decode table. Obfuscated data, basically.
      //
      // The digital-to-voltage conversion is simple: 0x00 == -10VDC,
      // and 0xFF means +10VDC. This is just an mx+b conversion.
      //

      if (num_bits == 12) // DI-154RS, added 2/3/04 pfh
         {
         // Corrected decode courtesy of Christian Heon 6/11/03
      
         for (S32 i=0; i < n_chans; i++)
            {
            atmp[i] = ((buf[(2*i)+1] & 0xFE) << 4) | ((buf[(2*i)] & 0xF8) >> 3);
            }
      
         // In 240 Hz mode, we get 4 different reads on the digital (JM: untested, not sure what this means)
      
         result->digital[0] = (buf[2] & 0x08) ? 1 : 0;
         result->digital[1] = (buf[2] & 0x04) ? 1 : 0;
         result->digital[2] = (buf[2] & 0x02) ? 1 : 0;
         }
      else if (num_bits == 10) // DI-194RS
         {
         // Corrected decode courtesy of Christian Heon 6/11/03
      
         for (S32 i=0; i < n_chans; i++)
            {
            atmp[i] = ((buf[(2*i)+1] & 0xFE) << 2) | ((buf[(2*i)] & 0xF0) >> 5);
            }
      
         // In 240 Hz mode, we get 4 different reads on the digital!
         result->digital[0] = (buf[2] & 0x08) ? 1 : 0;
         result->digital[1] = (buf[2] & 0x04) ? 1 : 0;
         result->digital[2] = (buf[2] & 0x02) ? 1 : 0;
         }
      else if (num_bits == 8)
         {
         // Corrected decode courtesy of Christian Heon 6/11/03
      
         for (S32 i=0; i < n_chans; i++)
            {
            atmp[i] = (buf[(2*i)+1] & 0xFE) | ((buf[(2*i)] & 0xF0) >> 7);
            }
      
         // In 240 Hz mode, we get 4 different reads on the digital!
         result->digital[0] = (buf[0] & 0x08) ? 1 : 0;
         result->digital[1] = (buf[0] & 0x04) ? 1 : 0;
         result->digital[2] = (buf[0] & 0x02) ? 1 : 0;
         }

      // Float conversion is the same for both, just the slope changes

      DOUBLE slope = 20.0 / ((DOUBLE) (1 << num_bits));

      for (idx = 0; idx < n_chans; idx++)
         {
         result->analog[idx] = (DOUBLE) (-10.0 + (slope * (atmp[idx])));
         result->raw   [idx] = atmp[idx];
         }
         
      return 1;
      }

   virtual BOOL cmd_send(const U8 *data, S32 len, BOOL report_echo_errors = TRUE)
      {
      for (S32 i=0; i < len; i++)
         {
         S32 rc = serial.write(&data[i], 1);

         if (rc != 0)
            {
            message_printf(DI154_ERROR, "cmd_send() error: %s",serial.error_text(rc));
            return FALSE;
            }

         if (i==0)      // leading zero is never echoed
            {
            continue;
            }

         S32 cnt = 1;
         U8 *echo = serial.read(&cnt);

         if (report_echo_errors)
            {
            if (cnt != 1)
               {
               message_printf(DI154_ERROR, "Error: no serial traffic");
               return FALSE;
               }

            if (echo[0] != data[i])
               {
               message_printf(DI154_ERROR, "Error: mismatched echo character (sent %c, received %c)",data[i], echo[0]);
               return FALSE;  
               }
            }
         }

      return TRUE;
      }

   virtual S32 stop(void)
      {
      //
      // Send stop command, but don't verify echoed characters since data
      // will still be coming in at first
      //

      U8 stop_cmd[] = {0x00, 'S', '0'}; 

      if (!cmd_send(stop_cmd, sizeof(stop_cmd), FALSE))
         {
         return FALSE;
         }

      //
      // Flush any remaining characters
      //

      S32 max_cnt = 4096;
      serial.read(NULL, &max_cnt, 100, 100);

      return TRUE;
      }

   virtual S32 start(void)
      {
      U8 start_cmd[] = {0x00, 'S', '1'}; 

      if (!cmd_send(start_cmd, sizeof(start_cmd)))
         {
         return FALSE;
         }

      return daq_sync();
      }

   virtual S32 reset(void)
      {
      U8 rst_cmd[] =  {0x00, 'R', 'Z'};

      return cmd_send(rst_cmd, sizeof(rst_cmd));
      }

   virtual void close(void)
      {
      serial.disconnect();
      }

   virtual BOOL open(C8 *port, S32 Hz, U32 analog_chan_mask, U32 digital_chan_mask)
      {
      n_chans = 0;

      for (S32 ch=0; ch < 32; ch++)
         {
         if (analog_chan_mask & (1 << ch))
            {
            if (ch >= DI154_MAX_CHANS)
               {
               message_printf(DI154_ERROR, "Channel numbers must range from 0-%d", DI154_MAX_CHANS-1);
               return FALSE;
               }

            n_chans++;
            }
         }

      sample_bytes = n_chans * 2;

      //
      // Power adapter on
      //

      S32 rc = serial.connect(port, 4800);

      if (rc != 0)
         {
         message_printf(DI154_ERROR, "Could not open %s", port);
         return FALSE;
         }

      serial.set_DTR(TRUE);
      serial.set_RTS(TRUE);

      Sleep(100);

      //
      // Swallow any initial data (FF byte observed, 7F also seen)
      //

      S32 cnt = 32;
      U8 *data = serial.read(&cnt);

      if ((cnt > 0) 
           &&
         ((data[0] != 0xff) && (data[0] != 0xfe) && (data[0] != 0x7f)))
         {
         message_printf(DI154_NOTICE, "Notice: %d initial byte(s) received, first byte was %X",cnt, data[0]);
         }

      //
      // Ensure acquisition is stopped
      //

      if (!stop())
         {
         close();
         return FALSE;
         }

      //
      // Optionally switch from default 4800 bps to 9600 bps
      // For >1 channels, or rates > 240 Hz, 9600 is needed
      //

      rate_BPS = 4800;

      if ((n_chans > 1) || (Hz > 240))
         {
         rate_BPS = 9600;
         }

      U8 bps_cmd[] = {0x00, 'B', (rate_BPS == 4800) ? 0x01 : 0x04}; 

      if (!cmd_send(bps_cmd, 3))
         {
         close();
         return FALSE;
         }

      rc = serial.change_rate(rate_BPS);

      if (rc != 0)
         {
         message_printf(DI154_ERROR, "Couldn't set baud rate: %s",serial.error_text(rc));
         }

      Sleep(100);

      //
      // Interrogate serial # (really a product ID)
      //

      U8 sn_cmd[] = {0x00, 'N', 'Z'};

      if (!cmd_send(sn_cmd, 3))
         {
         close();
         return FALSE;
         }

      cnt = 32;
      data = serial.read(&cnt);

      const S32 SERIAL_NUM_LEN = 10;
      const C8 rs194_signature[SERIAL_NUM_LEN + 1] = "0000000000";
      const C8 rs154_signature[SERIAL_NUM_LEN + 1] = "0000000001";

      if (cnt < SERIAL_NUM_LEN)
         {
         message_printf(DI154_ERROR, "Couldn't read DAQ serial number");
         close();
         return FALSE;
         }

      precision_bits = 0;

      if (strncmp((C8 *) data, rs194_signature, SERIAL_NUM_LEN) == 0)
         {
         message_printf(DI154_NOTICE, "DI-194RS found at %s", port);
         precision_bits = 10;
         }
      else if (strncmp((C8 *) data, rs154_signature, SERIAL_NUM_LEN) == 0)
         {
         message_printf(DI154_NOTICE, "DI-154RS found at %s", port);
         precision_bits = 12;
         }
      else
         {
         precision_bits = 8;
         message_printf(DI154_NOTICE, "Old DI-194 found");

         for (S32 idx = 0; idx < SERIAL_NUM_LEN; idx++)
            {
            message_printf(DI154_VERBOSE, "%c", data[idx]);
            }

         message_printf(DI154_VERBOSE, "Sending enable key to device");

         // Key to enable old DI-194 units - copied from original source code
         const U8 key_cmd[] = {0x00, 'E'};

         const U8 enable_key[] = { 0x33, 0x35, 0x46, 0x46, 0x44, 0x30, 0x42, 0x44 };
         // This version was from original source; above is from portlog 
         //    {0x41, 0x38, 0x34, 0x34, 0x38, 0x45, 0x35, 0x41};

         // Send the key (enabler) out
         // Each key byte is prefixed with 0x00 0x45

         for (S32 idx = 0; idx < sizeof(enable_key); idx++)
            {
            if (!cmd_send(key_cmd, sizeof(key_cmd)))
               {
               close();
               return FALSE;
               }

            // That was key prefix, now the key byte itself

            if (!cmd_send(&enable_key[idx], sizeof(U8)))
               {
               close();
               return FALSE;
               }
            }
         }

      //
      // By default, all digital channels are inputs
      //

      U8 dig_chan_cmd[] = {0x00, 'D', 0x00};
      dig_chan_cmd[2] = (U8) (digital_chan_mask & 0xff);

      if (!cmd_send(dig_chan_cmd, sizeof(dig_chan_cmd)))
         {
         close();
         return FALSE;
         }

      //
      // Select specified analog channel(s)
      // 

      U8 analog_chan_cmd[] = {0x00, 'C', 0x00};
      analog_chan_cmd[2] = (U8) (analog_chan_mask & 0xff);

      if (!cmd_send(analog_chan_cmd, sizeof(analog_chan_cmd)))
         {
         close();
         return FALSE;
         }

      //
      // Set rate
      //

      U32 d = 0;     // word is exactly 0 for 240 Hz or 480 Hz (only the baud rate changes)

      if (Hz < 240)
         {
         d = (U32) (((1.0 / Hz) - (1.0 / 244.0)) * rate_BPS);    // tweaked empirically from 240 to 244 for better accuracy
         }
      else if ((Hz > 240) && (Hz < 480))
         {
         d = (U32) (((1.0 / Hz) - (1.0 / 488.0)) * rate_BPS);    
         }

      U8 M_cmd[] = {0x00, 'M', (U8) ((d & 0xff00) >> 8)};
      U8 L_cmd[] = {0x00, 'L', (U8) (d & 0xff)};

      if ((!cmd_send(L_cmd, sizeof(L_cmd))) ||
          (!cmd_send(M_cmd, sizeof(M_cmd))))
         {
         close();
         return FALSE;
         }

      return TRUE;
      }

   virtual S32 read(DI154_READING *data, S32 num_bits = 0)
      {
      if (num_bits == 0)
         {
         num_bits = precision_bits;
         }

      U8 raw_buf[DI154_MAX_CHANS * 2];

      S32 cnt = sample_bytes;
      DWORD rc = serial.read(raw_buf, &cnt);
      
      if (rc != 0)
         {
         message_printf(DI154_ERROR, "Read error: %s",serial.error_text(rc));
         return FALSE;
         }

      if (cnt != sample_bytes)
         {
         message_printf(DI154_ERROR, "Couldn't read sample data");
         return FALSE;
         }

      if (!buf_decode(raw_buf, data, num_bits))
         {
         if (!daq_sync())
            {
            return FALSE;
            }
         }

      return TRUE;   // we got the data, but the app still needs to check data->is_valid in case a resync was needed
      }

   virtual DOUBLE scale(DOUBLE val, DOUBLE pos_val, DOUBLE neg_val, DOUBLE pos_fs, DOUBLE neg_fs)
      {
      DOUBLE percent = (val - neg_val) / (pos_val - neg_val);
      return neg_fs + (percent * (pos_fs - neg_fs));
      }
};

