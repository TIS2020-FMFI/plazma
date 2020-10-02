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

#include "gpiblib.h"

#define MAX_STAT_CNT 10000000    // Record up to 10 million readings for statistical purposes

void WINAPI GPIB_error(C8 *msg, S32 ibsta, S32 iberr, S32 ibcntl)
{
   printf("%s",msg);

   exit(1);
}

void shutdown(void)
{
   GPIB_disconnect();            // Use GPIB_disconnect(0) if 5370 jams bus at shutdown under Prologix
}

void main(S32 argc, C8 **argv)
{
   setbuf(stdout,NULL);

   //
   // Choose between four possible measurement examples, compilable by 
   // changing the #if conditionals
   //

#if 1 // Enable for continuous readings in current mode (freq/period/TI) at full rate

   if (argc < 2)
      {
      printf("Usage: 5370B <address>\n");
      exit(1);
      }

   atexit(shutdown);

   GPIB_connect(atoi(argv[1]),
                GPIB_error,
                0,
                20000); // Set 20-second timeout/dropout values to tolerate poor IP connections

   GPIB_set_EOS_mode(10);
   GPIB_set_serial_read_dropout(20000);

//   GPIB_write("AR1");   // Optionally select +TI arming
//   GPIB_write("SS2");   // Optionally select sample size = 100

   GPIB_write("MD2");   // Lock out display rate control, hold display until MR command
   Sleep(2000);
   GPIB_write("MR");    // Manual read (discard first reading)
   Sleep(1000);
   GPIB_read_ASC();

   for (;;)
      {
      Sleep(10);
      GPIB_write("MR");

      printf("%s",GPIB_read_ASC());

      if (_kbhit())
         {
         _getch();
         exit(1);
         }
      }

#endif

#if 0 // Enable for single frequency reading

   if (argc < 2)
      {
      printf("Usage: 5370B <address>\n");
      exit(1);
      }

   atexit(shutdown);

   GPIB_connect(atoi(argv[1]),
                GPIB_error,
                0,
                3000);

   GPIB_set_EOS_mode(10);

   GPIB_write("FN3GT3MD2");  // Frequency function, 0.1 second gate time, hold until MR command issued
   GPIB_write("MR");         // Issue Manual Rate command to enable reading

   printf("%s",GPIB_read_ASC());

#endif

#if 0 // Enable for continuous readings in current mode (freq/period/TI) at 1 reading/sec

   if (argc < 2)
      {
      printf("Usage: 5370B <address>\n");
      exit(1);
      }

   atexit(shutdown);

   GPIB_connect(atoi(argv[1]),
                GPIB_error,
                0,
                20000); // Set 20-second timeout/dropout values to tolerate poor IP connections

   GPIB_set_EOS_mode(10);
   GPIB_set_serial_read_dropout(20000);

//   GPIB_write("AR1");   // Optionally select +TI arming
//   GPIB_write("SS2");   // Optionally select sample size = 100

   GPIB_write("MD2");   // Lock out display rate control, hold display until MR command
   Sleep(2000);

   GPIB_write("MR");    // Manual read (discard first reading)
   Sleep(1000);

   for (S32 h=0; h < 24; h++)
      {
      for (S32 m=0; m < 60; m++)
         {
         for (S32 s=0; s < 60; s++)
            {
            GPIB_write("MR");
            Sleep(1000);

            printf("%d:%d:%d  %s",h,m,s,GPIB_read_ASC());

            if (_kbhit())
               {
               _getch();
               exit(1);
               }
            }
         }
      }

#endif

#if 0 // Enable for continuous time-interval readings

   if (argc < 3)
      {
      printf("Usage: 5370B <address> <n_samples, from 1-16777215>\n\n(Assumes 5370B is already in TI mode)\n");
      exit(1);
      }

   atexit(shutdown);

   //
   // The 5370B can process about 3,000 samples per second, so our GPIB timeout needs to be
   // at least n_samples/3
   //

   U32 n_samples     = (U32) atoi(argv[2]);
   S32 timeout_msecs = (n_samples / 3) + 2000;     

   GPIB_connect(atoi(argv[1]),
                GPIB_error,
                0,
                timeout_msecs);

   GPIB_set_EOS_mode(10);

   //
   // Tell counter how many samples to take for each returned reading, after
   // forcing it to wrap up any measurement in progress by taking a single sample
   //
   // If sample size is not a supported power of 10, we must compose an SB 
   // (Sample Binary) command string to specify the number of
   // samples we want the counter to take for each reading
   //
   // Prologix users will not be able to use this feature, since arbitrary binary
   // strings cannot be transmitted!
   //

   GPIB_write("SS1");
   Sleep(100);

   C8 sample_string[16] = "";                      

        if (n_samples == 1)      sprintf(sample_string,"SS1");
   else if (n_samples == 100)    sprintf(sample_string,"SS2");
   else if (n_samples == 1000)   sprintf(sample_string,"SS3");
   else if (n_samples == 10000)  sprintf(sample_string,"SS4");
   else if (n_samples == 100000) sprintf(sample_string,"SS5");

   if (sample_string[0])
      {
      GPIB_write(sample_string);
      }
   else
      {
      GPIB_CTYPE type = GPIB_connection_type();

      if (type != GC_NI488)
         {
         printf("WARNING: Binary command transmission requires National Instruments interface\n");
         printf("Measurement may not work -- use a power of 10 from 1-100000 inclusive\n\n");
         }

      U8 bin_string[16];

      bin_string[0] = 'S';
      bin_string[1] = 'B';
      bin_string[2] = (U8) (n_samples / 65536);
      bin_string[3] = (U8) ((n_samples - ((n_samples / 65536) * 65536)) / 256);
      bin_string[4] = (U8) (n_samples % 256);

      GPIB_write_BIN(bin_string, 5);
      }

   //
   // Set up the time-interval measurement
   //

   GPIB_write("SL");       // Slope and trigger level controls = local
   GPIB_write("TL");
   GPIB_write("FN1");      // Measure time interval
   GPIB_write("ST1");      // Take the mean value of all acquired samples
   GPIB_write("MD3");      // Display rate fast

   fprintf(stderr,"Measuring with %d samples (expected time <= %d sec(s)/reading)\nPress any key to stop...\n\n",
      n_samples,
      (timeout_msecs / 1000) + 1);

   //
   // Read results as they come back from the counter
   //
   // We discard the first measurement, since it reflects whatever was on the counter 
   // display at the time the program was launched
   //

   S32    cnt   = 0;
   DOUBLE accum = 0.0;
   DOUBLE maxval = -FLT_MAX;
   DOUBLE minval = FLT_MAX;

   static DOUBLE results[MAX_STAT_CNT];

   S32 reading = 0;

   while (!_kbhit())
      {
      reading++;

      C8 *result = GPIB_read_ASC();

      if (reading > 1)
         {
         //
         // Maintain statistics
         //

         DOUBLE val = 0.0;
         sscanf(result,"TI = %lf",&val);

         if (val > maxval) maxval = val;
         if (val < minval) minval = val;

         if (cnt < MAX_STAT_CNT)
            {
            results[cnt] = val;
            }

         accum += val;
         cnt++;

         //
         // Display reading with or without timestamp
         //

         C8 t[512];

#if 0
         time_t clock;
         time(&clock);

         strcpy(t,asctime(localtime(&clock)));

         C8 *terminate = strchr(t,'\n');
         if (terminate != NULL) *terminate = 0;

         printf("Interval %d\t (%s) \t%s",
            cnt,
            t,
            result);
#else
         t[0] = 0;
         sscanf(result,"TI = %s",t);

         printf("%s\n",t);
#endif
         }
      }

   _getch();

   //
   // Display statistics for sample set prior to exiting
   //

   DOUBLE mean = accum / cnt;

   fprintf(stderr,"\n%d measurements taken\n",cnt);
   fprintf(stderr,"Mean    = %lE\n",mean);
   fprintf(stderr,"Max     = %lE\n",maxval);
   fprintf(stderr,"Min     = %lE\n",minval);

   if (cnt < MAX_STAT_CNT)
      {
      DOUBLE v = 0.0;
      S32    n = cnt-1;

      for (S32 i=0; i < n; i++)
         {
         DOUBLE r = results[i] - mean;

         v += (r*r);
         }

      fprintf(stderr,"Std dev = %lE\n",sqrt(v/n));
      }

#endif
}
