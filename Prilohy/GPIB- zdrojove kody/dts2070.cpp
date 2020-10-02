#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <float.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include "gpiblib.h"

void WINAPI GPIB_error(C8 *msg, S32 ibsta, S32 iberr, S32 ibcntl)
{
   printf("%s",msg);

   exit(1);
}

C8 *read_response(void)
{  
   for (;;)
      {
      if (_kbhit())
         {
         _getch();
         break;
         }

      Sleep(10);     

      S32 spoll = GPIB_serial_poll();

      if (spoll & 0x10)
         {
         return GPIB_read_ASC();
         }
      }

   return NULL;
}

void kill_trailing_whitespace(C8 *dest)
{
   S32 l = strlen(dest);

   while (--l >= 0)
      {
      if (!isspace(((U8 *) dest)[l]))
         {
         dest[l+1] = 0;
         break;
         }
      }
}

void shutdown(void)
{
   timeEndPeriod(1);
   GPIB_disconnect();         
}

void main(S32 argc, C8 **argv)
{
   setbuf(stdout,NULL);
   
   if (argc != 2)
      {
      printf("Usage: dts2070 <address>\n");
      exit(1);
      }

   timeBeginPeriod(1);
   atexit(shutdown);

   GPIB_connect(atoi(argv[1]),
                GPIB_error,
                0,
                500);

   GPIB_auto_read_mode(FALSE);
   
   GPIB_set_EOS_mode(10);
   GPIB_set_serial_read_dropout(500);        // (short timeout since serial-polling is used)

   //
   // Make sure CONT mode is not active -- if it is, the queries below will probably
   // time out
   //
   // Apparently the only way to disable CONT mode is to issue a trigger...
   //

   GPIB_write("*CLS;:SYST:HEADOFF;:SYST:LONGOFF;*ESE125;*SRE49;*TER?");     // From AN125

   S32 start_time = timeGetTime();

   while ((GPIB_serial_poll() & 0x01) != 0)
      {
      Sleep(10);
      if ((timeGetTime() - start_time) > 2000) break;
      }

   GPIB_write("*TRG");

   while ((GPIB_serial_poll() & 0x01) != 1)
      {
      Sleep(10);
      if ((timeGetTime() - start_time) > 2000) break;
      }

   GPIB_write(":TER?");
   read_response();
          
   //
   // Show basic setup info
   //
                   
   GPIB_write("*IDN?");
   fprintf(stderr,"\nConnected to %s",read_response());

   GPIB_write(":ACQ:FUNC?");
   C8 *response = read_response();
   fprintf(stderr,"Measurement function: %s",response);

   C8 acq_cmd[256];
   sprintf(acq_cmd,":ACQ:RUN%s",response);
   kill_trailing_whitespace(acq_cmd);

   GPIB_write(":TRIG:SEQ?");
   fprintf(stderr,"Trigger sequence: %s", read_response());

   GPIB_write(":TRIG:LEVARM1?");
   fprintf(stderr,"EXT ARM 1 level: %s", read_response());

   GPIB_write(":TRIG:SLOPARM1?");
   fprintf(stderr,"EXT ARM 1 slope: %s", read_response());

   GPIB_write(":TRIG:LEVARM2?");
   fprintf(stderr,"EXT ARM 2 level: %s", read_response());

   GPIB_write(":TRIG:SLOPARM2?");
   fprintf(stderr,"EXT ARM 2 slope: %s", read_response());

   GPIB_write(":TRIG:SOUR?");
   fprintf(stderr,"Trigger source: %s", read_response());

   GPIB_write(":CHANSTAR:EXT?");
   fprintf(stderr,"External arm START: %s", read_response());

   GPIB_write(":CHANSTOP:EXT?");
   fprintf(stderr,"External arm STOP: %s", read_response());

   GPIB_write(":CHANSTAR:LEV?");
   fprintf(stderr,"START trigger level: %s", read_response());

   GPIB_write(":CHANSTAR:COUN?");
   fprintf(stderr,"START arming count: %s", read_response());

   GPIB_write(":CHANSTOP:LEV?");
   fprintf(stderr,"STOP trigger level: %s", read_response());

   GPIB_write(":CHANSTOP:COUN?");
   fprintf(stderr,"STOP arming count: %s", read_response());

   GPIB_write(":ACQ:COUN?");
   fprintf(stderr,"Sample size: %s",read_response());

   GPIB_write(":ACQ:SETCOUN?");
   fprintf(stderr,"Set size: %s",read_response());

   GPIB_write(":SYST:CHAN?");
   fprintf(stderr,"Channel: %s",read_response());

   //
   // Enter measurement loop
   // 

   fprintf(stderr,"\nPress any key to stop ...\n");

   for (;;)
      {
      GPIB_write(acq_cmd);
      C8 *meas = read_response();

      if (meas == NULL)
         {
         fprintf(stderr,"Done\n");
         break;
         }

      printf("%s",meas);
      }
}
