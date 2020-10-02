//
// Read temperatures and other operating data from T962 oven
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

#define VERSION "1.00"

COMPORT serial;

C8 *states[64] = 
{ 
   "Idle",           // 0
   "Ramp to soak",   // 1 
   "Soak",           // 2 
   "Ramp to peak",   // 3 
   "Peak",           // 4 
   "Ramp down",      // 5 
   "Cool down"       // 6 
};

void shutdown(void)
{
   if (serial.connected())
      {
      serial.disconnect();
      }
}

int main(int argc, char **argv)
{
   setbuf(stdout, NULL);

   fprintf(stderr,"\nT962 version "VERSION" of "__DATE__" by John Miles, KE5FX\n"
                  "_________________________________________________________________________\n\n");

   if (argc < 2)
      { 
      fprintf(stderr,"This program reads data from a Puhui T962-series oven with ESTechnical firmware\n\n"
                     "Usage:\n\n     t962 COM_port\n");
      exit(1);
      }

   C8   *port = argv[1];
   S32   baud = 57600;

   S32 rc = serial.connect(port, baud);

   if (rc != 0)
      {
      fprintf(stderr,"Could not open %s: %s", port, serial.error_text());
      exit(1);
      }

   atexit(shutdown);

   //
   // Data format:
   //
   // %%%cycleTime_msecs,cycleState,oven_temp,setpoint_temp,cal_temp,
   //
   // Cycle states:
   //
   //   0=idle
   //   1=ramp to soak
   //   2=soak
   //   3=ramp to peak
   //   4=peak
   //   5=ramp down
   //   6=cool down
   //

   for (;;)
      {
      if (_kbhit())
         {
         if (_getch() == 27)
            {
            exit(1);
            }
         else
            {
            exit(0);
            }
         }

      C8 data[512] = { 0 };

      S32 cnt = sizeof(data)-1;
      S32 timeout_ms = 1000;
      S32 dropout_ms = 1000;
      S32 EOS_char   = 10;

      if (serial.read((U8 *) data, 
                     &cnt, 
                     timeout_ms, 
                     dropout_ms, 
                     EOS_char))
         {
         printf("%s\n", serial.error_text());
         break;
         }

      //
      // Get timestamp and reject malformed strings
      //

      if (!data[0])
         {
         Sleep(10);
         continue;
         }

      if ((data[0] != '%') || (data[1] != '%') || (data[2] != '%'))
         {
         continue;
         }

      C8 *vals = strchr(data,',');
      if (vals == NULL) continue;

      *vals++ = 0;

      S32 l = strlen(data);
      if (l < 6) continue;

      U32 ms = atoi(&data[l-7]);

      //
      // Read remaining data values
      //

      S32      cycle      = 0;
      DOUBLE   setpoint   = 0.0;
      S32      heater_val = 0;
      S32      fan_val    = 0;
      DOUBLE   oven_degs  = 999.0;
      DOUBLE   cal_degs   = 999.0;      // "--" if thermocouple not plugged in

      sscanf(vals,"%d,%lf,%d,%d,%lf,%lf", 
         &cycle,
         &setpoint,
         &heater_val,
         &fan_val,
         &oven_degs,
         &cal_degs);

      if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
         {
         printf("%s", vals);
         continue;
         }

      printf("%8.2lfs: %13s ", (DOUBLE) ms / 1000.0, states[cycle % 7]);

      printf("Set:%.2lfC Oven:%6.1lfC PCB:%6.1lfC Htr:%3d%% Fan:%3d%%", 
         setpoint,oven_degs,cal_degs,heater_val,fan_val);

      printf("\n");
      }

   return 0;
}

