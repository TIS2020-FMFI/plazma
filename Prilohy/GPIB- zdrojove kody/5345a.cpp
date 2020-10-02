#include <stdio.h>
#include <conio.h>
#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "gpiblib.h"

void WINAPI GPIB_error(C8 *msg, S32 ibsta, S32 iberr, S32 ibcntl)
{
   printf("%s",msg);

   exit(1);
}

void shutdown(void)
{
   //
   // Don't do an explicit reset-to-local at shutdown time -- ibloc() causes HP 5345A option 12 to 
   // blank its display for some reason...
   //

   GPIB_disconnect(FALSE);
}

void main(S32 argc, C8 **argv)
{
   setbuf(stdout,NULL);

   if (argc < 2)
      {
      printf("Usage: 5345A <address>\n");
      exit(1);
      }

   GPIB_CTYPE type = GPIB_connection_type();

   if (type != GC_NI488)
      {
      printf("Error: 5345A requires a genuine National Instruments GPIB interface -- it will\n");
      printf("not work with Prologix or other single-address adapters\n");
      exit(1);
      }

   atexit(shutdown);

   S32 addr = atoi(argv[1]);

   //
   // Choose between two possible measurement examples, compilable by 
   // changing the #if conditional
   //

#if 0

   //
   // The counter's current display may be read simply by reading from the counter 
   // at its base address...
   // 

   GPIB_connect(addr,
                GPIB_error,
                0,
                10000);

   GPIB_set_EOS_mode(13);

   printf("%s\n",GPIB_read_ASC());

#else

   //
   // More-sophisticated application using "computer dump" mode to move the
   // reciprocal calculation to the host PC for faster processing of successive 
   // measurements
   //
   // This is a translation of the BASIC example from page 3-38 of the
   // HP 05345-90060 service manual.  Note that computer-dump mode requires us to 
   // listen on the counter's base GPIB address+1, after configuring the desired 
   // measurement parameters on the base address itself.  Because GPIBLIB.DLL won't 
   // let us work with multiple addresses in the same program, we can't send the 
   // BASIC example's measurement initialization string on the counter's normal address.  
   //
   // Consequently, the counter should be manually configured for frequency 
   // measurement at the front panel prior to running this example!  Use a fast gate time
   // to avoid timing out, or increase the 10000-millisecond timeout value below.
   //
   // Also note that this method will not work properly when the measurement is made
   // using a frequency-converter plugin!
   //

   GPIB_connect(addr + 1,
                GPIB_error,
                0,
                10000);

   GPIB_set_EOS_mode(13);

   //
   // We take 5 successive measurements in this example.  Each measurement will
   // add 32 ASCII digits to the returned block
   //

   S32 n = 5;

   C8 *block = GPIB_read_ASC(32 * n);

   //
   // Parse block and print the results of the measurements we just took
   //

   printf("\nMeasurement #              Time            Events      Frequency Hz\n");
   printf("----------------------------------------------------------------------\n");

   S32 i,j;

   for (i=0; i < n; i++)
      {
      //
      // Make a reversed copy of the nth 32-byte measurement string, and
      // advance block pointer to the beginning of the next string
      // 

      C8 rev[32];
      
      for (j=0; j < 32; j++)
         {
         rev[31-j] = block[j];
         }

      block += 32;

      //
      // Get numeric value of numerator and denominator
      // (TIME and EVENT values, respectively)
      //

      DOUBLE tim = 0.0;
      DOUBLE evt = 0.0;

      for (j=0; j < 16; j++)
         {
         tim = (tim * 10.0) + (rev[j]    - '0');
         evt = (evt * 10.0) + (rev[j+16] - '0');
         }

      //
      // Frequency = event / (time * 2ns)
      //
      // This calculation is normally performed by the 5345A, but the host PC
      // can do it faster, and without imposing an added delay between measurements
      //

      DOUBLE freq = evt / (tim * 2E-9);

      printf("       %6d   %15I64d   %15I64d   %15I64d\n",
         i+1,
         S64(tim+0.5),
         S64(evt+0.5),
         S64(freq+0.5));
      }

   //
   // Send reset and initialize commands to return the counter to front-panel
   // control.  Note that further reads after a write to address+1 will not work!
   //

   GPIB_write("I2I1");

#endif
}
