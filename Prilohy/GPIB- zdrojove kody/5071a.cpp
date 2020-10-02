//
// Set HP 5071A front panel time from an NTP server, optionally synchronizing it to
// a local 1 PPS source
//

#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <float.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include "typedefs.h"
#include "timeutil.cpp"
#include "comport.cpp"
#include "ipconn.cpp"

#define VERSION "3.50"
#define NTP_PORT_NUM 123

struct NTPXCVR : public UDPXCVR
{
   virtual void message_sink(IPMSGLVL level,   
                             C8      *text)
      {
      if (level >= IP_WARNING)
         {
         fprintf(stderr,"%s\n",text);
         }
      }

   virtual void shutdown(void)
      {
      printf("NTP shutdown\n");
      }
};

NTPXCVR NTP;
COMPORT serial;

void shutdown(void)
{
   if (serial.connected())
      {
      serial.printf("SYST:REM OFF\n");  serial.gets(); serial.gets();
      }
}

void main(int argc, char **argv)
{
   setbuf(stdout, NULL);

   //
   // Parse args
   //

   fprintf(stderr,"\n5071A version "VERSION" of "__DATE__" by John Miles, KE5FX\n"
                  "_________________________________________________________________________\n\n");

   if (argc < 3)
      { 
      fprintf(stderr,"This program retrieves a status report from an HP 5071A primary frequency\n"
                     "standard, optionally setting its clock from a specified NTP server and\n"
                     "synchronizing the unit to an external 1 PPS source.  The front panel LED\n"
                     "clock is also turned on.\n\n"
                     "Usage:\n\n     5071a COM_port baud_rate [cson | csoff] [NTP_address] [sync_spec]\n\n"
                     "where COM_port is the serial port name, baud_rate is the baud rate for which\n"
                     "the 5071A is configured, NTP_address is the NTP server address, and sync_spec\n"
                     "is either FRONT or REAR, depending on which port should be armed for 1-pps\n"
                     "synchronization.\n\n"
                     "Examples: \n\n     5071A com5 2400 csoff pool.ntp.org rear\n\n"
                     "sets the clock using pool.ntp.org, then arms the rear sync input.\nThe unit is placed in standby mode (cesium beam off).\n"
                     "\n     5071A com5 2400 cson\n\n"
                     "exits from standby mode and enables the cesium beam tube\n"
                     "\n     5071A com5 2400\n\n"
                     "retrieves and displays the current status report but does not change the\n"
                     "operating mode\n");
      exit(1);
      }

   C8   *port           = argv[1];
   S32   baud           = atoi(argv[2]);

   bool  do_standby      = (argc > 3);
   bool  do_NTP          = (argc > 4);
   bool  do_sync         = (argc > 5);

   bool  standby        = do_standby ? (_stricmp(argv[3],"cson") != 0) : TRUE;
   C8   *server_address = do_NTP     ? argv[4] : NULL;
   C8   *sync           = do_sync    ? argv[5] : NULL;

   S32 rc = serial.connect(port, baud);

   if (rc != 0)
      {
      fprintf(stderr,"Could not open %s", port);
      exit(1);
      }

   atexit(shutdown);

   //
   // Identify unit and set to remote mode
   //
   // Must do a serial.gets() to retrieve each echoed command, then another to retrieve the result of any
   // query commands, then a final one to swallow the 'scpi >' prompt
   //

   serial.printf("*CLS\n");   serial.gets(); serial.gets();
   serial.printf("*IDN?\n");  serial.gets();

   fprintf(stderr,"Connected to %s", serial.gets()); serial.gets();

   serial.printf("SYST:REM ON\n");  serial.gets(); serial.gets();

   //
   // Turn LED clock on
   // 

   serial.printf("DISP:ENAB ON\n"); serial.gets(); serial.gets();

   //
   // Dump status report and (optional) error log to stdout, if no other
   // operations requested
   //

   if (!(do_NTP || do_standby || do_sync))
      {
     serial.printf("SYST:PRINT?\n"); serial.gets(); 

     for (;;)
        {
        C8 *ptr = (C8 *) serial.gets();

        if ((!ptr[0]) || (!_strnicmp(ptr, "scpi >", 6)))
           {
           break;
           }

        printf("%s", ptr);
        }

#if 1
      serial.printf("DIAG:LOG:PRINT?\n"); serial.gets(); 

      for (;;)
         {
         C8 *ptr = (C8 *) serial.gets();

         if ((!ptr[0]) || (!_strnicmp(ptr, "scpi >", 6)))
            {
            break;
            }

         printf("%s", ptr);
         }
#endif
   }

   //
   // Optionally sync to 1-pps input and wait 3 seconds to ensure completion
   //

   if (do_sync)
      {
      serial.printf("SOUR:PTIM:SYNC %s\n", sync); serial.gets(); serial.gets();

      if (_stricmp(sync,"off"))
         {
         fprintf(stderr,"Synchronizing with %s 1-pps input... ", sync);
         Sleep(3000);
         fprintf(stderr,"Done\n\n");
         }
      }

   //
   // Request time from NTP
   //

   if (do_NTP)
      {
      if (!NTP.set_remote_address(server_address, NTP_PORT_NUM))
         {
         exit(1);
         }

      NTP.set_timeout_ms(500);

      //
      // Repeat NTP request until transition from one second 
      // to the next is observed
      //
      // (NTP servers hate this one weird trick)
      //

      fprintf(stderr, "Querying %s ..", server_address);

      S64 NTP_init_secs   = 0;
      S64 NTP_usecs       = 0;
      S32 NTP_stratum     = 0;
      C8  NTP_response[5] = { 0 };

      for (S32 tries=0; tries < 25; tries++)
         {
         printf(".");
         Sleep(50);

         U8 msg[48] = { 0 };

         msg[0]  = 0xE3;    // Leap, Version, Mode
         msg[1]  = 0;       // Stratum
         msg[2]  = 6;       // Polling Interval
         msg[3]  = 0xEC;    // Peer Clock Precision
         msg[12] = 'I';     // Reference Identifier
         msg[13] = 'N';    
         msg[14] = 'I';
         msg[15] = 'T';

         if (!NTP.send_block(msg, sizeof(msg)))
            {
            exit(1);
            }

         S32 result = NTP.read_block(msg, sizeof(msg));

         if (result == -1)
            {
            continue;      // (Server didn't respond in time to be useful for 1-pps synchronization)
            }

         if (result < 48)
            {
            fprintf(stderr,"\n\nError: NTP.read_block() returned %d, expected at least 48 bytes\n", result);
            exit(1);
            }

         NTP_response[0] = msg[12];
         NTP_response[1] = msg[13];
         NTP_response[2] = msg[14];
         NTP_response[3] = msg[15];
         NTP_response[4] = 0;

         NTP_stratum = msg[1];

         if (NTP_stratum == 0)
            {
            fprintf(stderr,"\n\nError: Server sent KoD: %s\n", NTP_response);
            exit(1);
            }

         S64 usecs = USTIMER::NTP_to_us(&msg[40]);
         S64 secs = usecs / 1000000LL;

         if ((NTP_init_secs != 0) && (NTP_init_secs != secs))
            {
            NTP_usecs = usecs;
            break;
            }

         NTP_init_secs = secs;
         }

      if (NTP_usecs == 0)
         {
         fprintf(stderr, "\n\nError: NTP time transition not detected\n");
         exit(1);
         }

      C8 text[512] = { 0 };
      USTIMER::timestamp(text, sizeof(text), NTP_usecs);

      printf("\n");

      if (NTP_stratum < 2)
         {
         printf("Received NTP time: %s, stratum %d, source %s\n\n", text, NTP_stratum, NTP_response);
         }
      else
         {
         C8 IP_address[MAX_PATH] = { 0 };
         sockaddr_in server_addr;

         _snprintf(IP_address, sizeof(IP_address)-1, "%d.%d.%d.%d\n", (U8) NTP_response[0], (U8) NTP_response[1], (U8) NTP_response[2], (U8) NTP_response[3]);

         if (!IPCONN::parse_address(IP_address, 0, &server_addr))
            {
            printf("Received NTP time: %s, stratum %d, server %s\n\n", text, NTP_stratum, IP_address);
            }
         else
            {
            C8 hostname[MAX_PATH] = { 0 };

            if (IPCONN::hostname_string(&server_addr, hostname, sizeof(hostname), 5000))
               printf("Received NTP time: %s, stratum %d, source %s\n\n", text, NTP_stratum, hostname);
            else
               printf("Received NTP time: %s, stratum %d, server %s\n\n", text, NTP_stratum, IP_address);
            }
         }

      //
      // Set 5071A clock
      //

      FILETIME   Ft;
      FILETIME   Lft;
      SYSTEMTIME St;

      S64 file_time = NTP_usecs * 10;

      Ft.dwLowDateTime = S32(file_time & 0xffffffff);
      Ft.dwHighDateTime = S32(U64(file_time) >> 32);

      FileTimeToLocalFileTime(&Ft,  &Lft);
      FileTimeToSystemTime   (&Lft, &St);

      serial.printf("SOUR:PTIM:TIME %d,%d,%d\n", St.wHour, St.wMinute, St.wSecond); serial.gets(); serial.gets(); 

      fprintf(stderr,"5071A clock time set to %.2d:%.2d:%.2d\n", St.wHour, St.wMinute, St.wSecond);

      //
      // Set MJD (rounded down)
      // 

      S32 MJD = (S32) USTIMER::us_to_MJD(NTP_usecs);

      serial.printf("SOUR:PTIM:MJD %d\n", MJD); serial.gets(); serial.gets(); 

      fprintf(stderr,"5071A MJD set to %d\n", MJD);
      }

   //
   // Turn cesium beam tube on/off
   //

   if (do_standby)
      {
      if (standby)
         {
         serial.printf("SOUR:PTIM:STAN ON\n"); serial.gets(); serial.gets(); 
         fprintf(stderr,"5071A standby mode on (Cs tube OFF)\n");
         }
      else
         {
         serial.printf("SOUR:PTIM:STAN OFF\n"); serial.gets(); serial.gets(); 
         fprintf(stderr,"5071A standby mode off (Cs tube ON)\n");
         }
      }
}

