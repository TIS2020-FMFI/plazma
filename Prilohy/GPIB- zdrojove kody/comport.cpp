// *****************************************************************************
//
// COMPORT: Lightweight API for serial communications     
//
// john@miles.io 7-May-10
//
// *****************************************************************************

//
// Individual COMPORT objects and methods are multithread safe unless
// COMPORT_THREAD_UNSAFE is defined
//
// (You must define COMPORT_THREAD_UNSAFE if compiling a DLL that may be
// dynamically loaded, as the static TLS buffers won't be allocated in XP)
//

#if COMPORT_THREAD_UNSAFE
   #define COMPORT_THREAD_SAFE 0
   #define COMPORT_STATIC static
#else
   #define COMPORT_THREAD_SAFE 1
   #define COMPORT_STATIC static __declspec(thread)
#endif

void ___________________________________________________________COMPORT____________________________________________________(void) 
{}

class COMPORT
{
   HANDLE hSerial;
   bool   use_DTR;
   S32    last_error_code;
   DCB    dcb;

public:

   COMPORT()
      {
      hSerial         = INVALID_HANDLE_VALUE;
      use_DTR         = FALSE;
      last_error_code = 0;

      memset(&dcb, 0, sizeof(dcb));
      }

   virtual ~COMPORT()
      {
      disconnect();
      }

   virtual bool connected(void)
      {
      return (hSerial != INVALID_HANDLE_VALUE);
      }

   //
   // Copy OS-specific error text to buffer and return it
   //

   virtual C8 *error_text(S32 last_error = 0)
      {
      LPVOID lpMsgBuf = "Hosed"; 

      last_error_code = (last_error == 0) ? GetLastError() : last_error;

      FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 
                    NULL,
                    last_error_code,
                    0,
          (LPTSTR) &lpMsgBuf, 
                    0, 
                    NULL);

      COMPORT_STATIC C8 msg_buf[256];
      C8 *text            = msg_buf;
      S32 text_array_size = sizeof(msg_buf);

      memset(text, 0, text_array_size);
      strncpy(text, (C8 *) lpMsgBuf, text_array_size-1);

      if (strcmp((C8 *) lpMsgBuf,"Hosed"))
         {
         LocalFree(lpMsgBuf);
         }

      return text;
      }

   virtual void set_DTR(bool on)
      {   
      if ((!use_DTR) ||
          (hSerial == INVALID_HANDLE_VALUE))
         {  
         return;
         }

      EscapeCommFunction(hSerial, on ? SETDTR : CLRDTR);
      }

   virtual void set_RTS(bool on)
      {   
      if (hSerial == INVALID_HANDLE_VALUE) 
         {
         return;
         }

      EscapeCommFunction(hSerial, on ? SETRTS : CLRRTS);
      }

   virtual void disconnect(void)
      {
      if (hSerial == INVALID_HANDLE_VALUE)
         {
         return;
         }

      set_DTR(FALSE);

      CloseHandle(hSerial);
      hSerial = INVALID_HANDLE_VALUE;
      }

   virtual S32 connect(C8  *port,                   // Returns zero on success, else an error code that can be passed to error_text() 
                       S32  rate,
                       bool control_DTR = FALSE,    // Resets certain devices such as Arduinos if enabled
                       bool DTR_state   = TRUE)    
      {
      if (hSerial != INVALID_HANDLE_VALUE)
         {
         disconnect();
         }

      use_DTR = control_DTR;

      C8 com_name[64];
      sprintf(com_name, "\\\\.\\%s", port);
   
      hSerial = CreateFile(com_name,
                           GENERIC_READ | GENERIC_WRITE, 
                           0,
                           0,
                           OPEN_EXISTING, 
                           FILE_ATTRIBUTE_NORMAL, 
                           0);
   
      if (hSerial == INVALID_HANDLE_VALUE) 
         {
         return GetLastError();
         }
   
      dcb.DCBlength = sizeof(dcb);
   
      if (!GetCommState(hSerial, &dcb)) 
         {
         S32 result = GetLastError();
         disconnect();
         return result;
         }
   
      dcb.BaudRate        = rate;
      dcb.ByteSize        = 8;
      dcb.Parity          = NOPARITY;
      dcb.StopBits        = ONESTOPBIT;
      dcb.fBinary         = TRUE;
      dcb.fOutxCtsFlow    = FALSE;
      dcb.fOutxDsrFlow    = FALSE;
      dcb.fDtrControl     = use_DTR ? DTR_CONTROL_ENABLE : 0;    
      dcb.fDsrSensitivity = FALSE;
      dcb.fOutX           = FALSE;
      dcb.fInX            = FALSE;
      dcb.fErrorChar      = FALSE;
      dcb.fNull           = FALSE;
      dcb.fRtsControl     = RTS_CONTROL_ENABLE;    
      dcb.fAbortOnError   = FALSE;
   
      if (!SetCommState(hSerial, &dcb)) 
         {
         S32 result = GetLastError();
         disconnect();
         return result;
         }
   
      //
      // Set com port timeouts so we return immediately if no serial port
      // character is available
      //
   
      COMMTIMEOUTS cto = { 0, 0, 0, 0, 0 };
      cto.ReadIntervalTimeout = ULONG_MAX;
      cto.ReadTotalTimeoutConstant = 0;
      cto.ReadTotalTimeoutMultiplier = 0;
   
      if (!SetCommTimeouts(hSerial, &cto)) 
         {
         S32 result = GetLastError();
         disconnect();
         return result;
         }
   
      set_DTR(DTR_state);

      return 0;
      }

   // ---------------------------------------------
   // Higher-level COM routines
   // ---------------------------------------------

   virtual S32 change_rate(S32 rate_BPS)
      {
      if (hSerial == INVALID_HANDLE_VALUE)
         {
         return 0;
         }

      dcb.BaudRate = rate_BPS;

      if (!SetCommState(hSerial, &dcb)) 
         {
         S32 result = GetLastError();
         disconnect();
         return result;
         }

      return 0;
      }

   virtual S32 printf(C8 *fmt,
                      ...)
      {
      if (hSerial == INVALID_HANDLE_VALUE)
         {
         return 0;
         }

      C8 buffer[4096] = { 0 };

      va_list ap;

      va_start(ap, 
               fmt);

      _vsnprintf(buffer,
                 sizeof(buffer)-1,
                 fmt,
                 ap);

      va_end(ap);

      return write((U8 *) buffer, strlen(buffer));
      }

   //
   // Write data
   //
   // Returns zero on success, else an error code that can be passed to error_text()   
   //

   virtual S32 write(const C8 *data, 
                     S32      *written = NULL) 
      {
      return write((const U8 *) data,
                   strlen(data),
                   written);
      }

   virtual S32 write(const U8 *data, 
                     S32       len, 
                     S32      *written = NULL) 
      {
      if (hSerial == INVALID_HANDLE_VALUE)
         {
         return 0;
         }

      S32 w = 0;

      S32 flag = WriteFile(hSerial, 
                    (C8 *) data,
                           len,
                (LPDWORD) &w, 
                           NULL);

      if (written != NULL)
         {
         *written = w;
         }

      if (!flag)
         {
         return GetLastError();
         }

      return 0;
      }

   //
   // Read arbitrary amount of data into user buffer
   //
   // Returns zero on success, else an error code that can be passed to error_text()   
   //

   virtual S32 read(U8  *buff,  
                    S32 *cnt, 
                    S32  timeout_ms = 1000, 
                    S32  dropout_ms = 100,
                    S32  EOS_char   = -1)
      {
      if (hSerial == INVALID_HANDLE_VALUE)
         {
         *cnt = 0;
         return 0;
         }

      S32 i = 0;
      U8 *d = buff;

      S32 start = timeGetTime();
      S32 limit = timeout_ms;
   
      while (i < *cnt)
         {
         S32 bc = 0;
         U8 ch = 0;

         if (!ReadFile(hSerial, &ch, 1, (LPDWORD) &bc, NULL))
            {
            return GetLastError();
            }

         if (bc == 1)
            {
            start = timeGetTime();
            limit = dropout_ms;
            
            if (d != NULL) *d++ = ch;
            i++;

            if (ch == EOS_char)
               {
               break;
               }
            }
         else
            {
            S32 cur = timeGetTime();
   
            if ((cur-start) > limit)
               {
               break;
               }
   
            Sleep(10);
            }
         }
   
      *cnt = i;
      return 0;
      }

   //
   // Read up to 16K bytes of arbitrary data
   //
   // Returns NULL on failure, or pointer to short buffer 
   //

   virtual U8 *read(S32 *cnt, 
                    S32  timeout_ms = 1000, 
                    S32  dropout_ms = 100)     
      {
      if (hSerial == INVALID_HANDLE_VALUE)
         {
         return NULL;
         }

      COMPORT_STATIC U8 data[16384];
      memset(data, 0, sizeof(data));
   
      if (*cnt > sizeof(data))
         {
         *cnt = sizeof(data);
         }
   
      if (read(data, cnt, timeout_ms, dropout_ms) != 0)
         {
         return NULL;
         }
   
      return data;
      }

   //
   // Read ASCII data
   //
   // Always returns valid ASCII string of length < 16K
   //

   virtual C8 *gets(S32 EOS_char   = 10, 
                    S32 timeout_ms = 1000, 
                    S32 dropout_ms = 100)     
      {
      if (hSerial == INVALID_HANDLE_VALUE)
         {
         return NULL;
         }

      COMPORT_STATIC C8 data[16384];
      memset(data, 0, sizeof(data));

      S32 cnt = sizeof(data)-1;
      read((U8 *) data, &cnt, timeout_ms, dropout_ms, EOS_char);

      return data;
      }

   //
   // Flush any pending data by issuing repetitive read operations with
   // short timeouts until one of the reads times out or fails, or
   // a total of flush_timeout_ms is spent in the loop
   //

   virtual S32 flush(S32 flush_timeout_ms = 200)
      {
      if (hSerial == INVALID_HANDLE_VALUE)
         {
         return 0;
         }

      S32 cnt = LONG_MAX;

      read(NULL, 
          &cnt, 
           flush_timeout_ms, 
           flush_timeout_ms);

      return cnt;
      }

};

