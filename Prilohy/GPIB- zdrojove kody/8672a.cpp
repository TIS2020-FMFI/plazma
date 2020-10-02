#include <stdio.h>
#include <conio.h>
#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include "gpiblib.h"

void WINAPI GPIB_error(C8 *msg, S32 ibsta, S32 iberr, S32 ibcntl)
{
   printf("%s",msg);

   exit(1);
}

void shutdown(void)
{
   GPIB_disconnect();
}

void main(S32 argc, C8 **argv)
{
   setbuf(stdout,NULL);

   if (argc < 2)
      {
      printf("Usage: 8672a <address> <freq_MHz> [<ampl_dBm>]\n\n");
      printf("If amplitude is specified, the program will pause for a keypress\n");
      printf("before terminating.  This is necessary because the HP 8672A reverts to \n");
      printf("itse front-panel amplitude setting when local control is restored.\n");
      exit(1);
      }

   atexit(shutdown);

   GPIB_connect(atoi(argv[1]),
                GPIB_error,
                0,
                3000);

   //
   // Set frequency
   //

   DOUBLE freq_MHz = 0.0;
   sscanf(argv[2], "%lf", &freq_MHz);

   S64 freq_Hz = (S64) ((freq_MHz * 1E6) + 0.5);

   S64 freq = (freq_Hz / 1000LL) * 1000LL;            // quantize to whole-number kHz
      
   if (freq > 6199999000LL && freq < 12400000000LL)   // band 2 sets in 2 kHz increments
      {
      freq = (S64) (freq / 2000LL) * 2000LL;
      }
      
   if (freq > 12399999999LL)                          // band 3 sets in 3 kHz increments
      {
      freq = (S64) (freq / 3000LL) * 3000LL;
      }

   GPIB_printf("P%08I64dZ1M0N6",                                   // "P" loads freq, Z1 executes freq, M0=AM off, N6=FM off
          (S64) max(2000000LL, min((freq/1000LL), 18599997LL)));   // freq in 8-digits of kHz, min 2 GHz, max 18.6 GHz

   //
   // Set amplitude
   //

   if (argc > 3)
      {
      DOUBLE ampl_dBm = 0.0;
      sscanf(argv[3], "%lf", &ampl_dBm);

      S32 ampl = (ampl_dBm > 0.0) ? (S32) (ampl_dBm + 0.5) : (S32) (ampl_dBm - 0.5);

      if (ampl > 0.0)
         {
         GPIB_printf("K0L%cO3", '=' - ampl);
         }
      else  
         {
         GPIB_printf("K%cL%cO1",                            // "P" loads freq, Z1 executes freq, K: step atten, L: vernier, M: AM, N: FM, O: ALC
               C8( '0' + (abs(ampl) / 10) % 12 ),           // K-command: 0 to -110 dB step attenuation ('0','1',...';')
               C8( '3' + (abs(ampl) % 10 )));               // L-command: 0 to -9 dB vernier, offset by 3 ('3','4',...'<')
         }

      //
      // Amplitude will revert to front-panel setting upon GPIB disconnection, so
      // we need to wait for a keypress here
      //

      printf("Press any key to exit and restore front-panel amplitude control...\n");
      _getch();
      }
}           
