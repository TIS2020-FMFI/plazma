//
// Read data from DataQ DI-154RS/DI-194RS
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
#include "di154.cpp"

#define VERSION "1.10"
#define MAX_CHANS 16

struct DATAQ : public DI154
{
   virtual void message_sink(DI154MSGLVL level,   
                             C8         *text)
      {
      fprintf(stderr,"%s\n", text);
      }
}
DAQ;

void main(int argc, char **argv)
{
   setbuf(stdout, NULL);

   //
   // Parse args
   //

   fprintf(stderr,"\nDATAQ version "VERSION" of "__DATE__" by John Miles, KE5FX\n"
                  "_________________________________________________________________________\n\n");

   C8 outfile[MAX_PATH] = { 0 };

   if (((argc-3) % 5) == 1)
      {
      strcpy(outfile, argv[argc-1]);
      _strlwr(outfile);
      --argc;
      }

   if ((argc-3) % 5)
      { 
      fprintf(stderr,"This program acquires data from a DataQ DI-154RS or DI-194-series DAQ adapter,\n" 
                     "writing it to stdout after scaling.\n\n"
                     "Usage: dataq COM_port rate_Hz\n"
                   "             { chan_# pos_rail_value neg_rail_value multiplier offset }\n"
                   "             { outfile.txt | outfile.csv | outfile.xxx }\n\n"
                     "All numeric args except COM_port and chan_# accept reals in scientific notation.\n"
                     "Channel #s are zero-based.  (For multichannel acquisition, repeat the chan_# \n"
                     "through offset arguments.)  Specify offset of 'DC' to remove DC offset \n"
                     "from the scaled data automatically.\n\n"
                     "If outfile parameter is not provided, data will be written to stdout.\n"
                     "If outfile suffix is .txt or .csv, ASCII data will be written.  Otherwise \n"
                     "double-precision binary data will be written.\n\n"
                     "Example:\n\n"
                     "     dataq COM1 100 3 -12.17E-11 509.83E-11 1.5 10E6 my_file.txt\n\n"
                     "accepts data from COM1 at 100 samples/second via DAQ channel 3.  The +10V rail\n"
                     "is scaled to -12.17E-11 while the -10V rail is scaled to 509.83E-11.\n"
                     "After scaling, the values are multiplied by 1.5.  Finally, 10E6 is added to\n"
                     "the result.  Output values are written to my_file.txt in the current \n"
                     "working directory.\n");
      exit(1);
      }

   C8    *port = argv[1];
   DOUBLE rate = 240.0; 
   sscanf(argv[2],"%lf",&rate);

   S32    chan        [MAX_CHANS] = { 0 };
   DOUBLE pos_val     [MAX_CHANS] = { 0 }; 
   DOUBLE neg_val     [MAX_CHANS] = { 0 }; 
   DOUBLE multiplier  [MAX_CHANS] = { 0 }; 
   DOUBLE offset      [MAX_CHANS] = { 0 }; 

   bool   DC_remove   [MAX_CHANS] = { 0 };
   bool   DC_prestart [MAX_CHANS] = { 0 };
   DOUBLE DC_pos      [MAX_CHANS] = { 0 };
   DOUBLE DC_neg      [MAX_CHANS] = { 0 };

   S32 n_chans = 0;
   S32 analog_chan_mask = 0;

   for (S32 i=3; i < argc; i+= 5)
      {
      sscanf(argv[i+0],"%d", &chan       [n_chans]);
      sscanf(argv[i+1],"%lf",&pos_val    [n_chans]); 
      sscanf(argv[i+2],"%lf",&neg_val    [n_chans]); 
      sscanf(argv[i+3],"%lf",&multiplier [n_chans]); 

      if (_stricmp(argv[i+4],"dc"))
         {
         sscanf(argv[i+4],"%lf",&offset[n_chans]); 
         }
      else
         {
         offset      [n_chans] =  0.0;
         DC_remove   [n_chans] =  TRUE;
         DC_prestart [n_chans] =  TRUE;
         DC_pos      [n_chans] = -DBL_MAX;
         DC_neg      [n_chans] =  DBL_MAX;
         }

      analog_chan_mask |= (1 << chan[n_chans]);
      n_chans++;
      }

   //
   // Use serial # to identify the device (DI-194, DI-194RS, or DI-154RS)
   //
   // DI-154RS has four 12-bit analog inputs which we enable on request and 
   // two digital inputs which we leave enabled normally
   //

   if (!DAQ.open(port, (S32) (rate+0.5), analog_chan_mask, 0x03))
      {
      exit(1);
      }

   fprintf(stderr,"%d channel(s) in use\n", n_chans);

   fprintf(stderr,"Available precision = %d bits, %d bps\n", DAQ.precision_bits, DAQ.rate_BPS);

   if (!DAQ.start())
      {
      exit(1);
      }

   FILE *out = NULL;
   bool write_binary = FALSE;

   if (outfile[0])
      {
      write_binary = !((strstr(outfile,".csv") != NULL) || ((strstr(outfile,".txt") != NULL)));
      out = fopen(outfile, write_binary ? "wb" : "wt");

      if (out == NULL)
         {
         fprintf(stderr,"Error: couldn't open %s for writing\n", outfile);
         exit(1);
         }

      fprintf(stderr,"\nWriting to %s file %s, press ESC to stop . . .\n\n", 
         write_binary ? "binary" : "ASCII",
         outfile);
      }
   else
      {
      fprintf(stderr,"\nAcquiring data, press ESC to stop . . .\n\n");
      }

   USTIMER timer;

   S64 cnt = 0;
   S64 start = timer.us();

   for (;;)
      {
      S64 cur = timer.us();

      if (_kbhit())
         {
         if (_getch() == 27)
            {
            break;
            }
         }

      DI154_READING data;

      if (!DAQ.read(&data))  
         {
         break;
         }

      if (!data.is_valid)
         {
         break;
         }

      DOUBLE val[2] = { 0 };

      for (S32 ch=0; ch < n_chans; ch++)
         {
         val[ch] = (DAQ.scale(data.analog[ch],                              
                              10.0,                                         
                             -10.0,                                         
                              pos_val[ch],                                  
                              neg_val[ch]) * multiplier[ch]) + offset[ch]; 

         if (DC_prestart[ch])
            {
            if (val[ch] > DC_pos[ch]) DC_pos[ch] = val[ch];
            if (val[ch] < DC_neg[ch]) DC_neg[ch] = val[ch];

            if ((cur - start) >= 10000000)
               {
               if (ch == n_chans-1)
                  {
                  start = cur;
                  }

               DC_prestart[ch] = FALSE;
               }
            }
         }

      S32 written = 0;

      for (S32 ch=0; ch < n_chans; ch++)
         {
         if (DC_prestart[ch])
            {
            continue;
            }

         if (DC_remove[ch])
            {
            val[ch] -= ((DC_pos[ch] + DC_neg[ch]) / 2.0);
            }

         if (out == NULL)  
            {
            printf("%+0.16lf", val[ch]);

            if (ch == n_chans-1)
               printf("\n");
            else
               printf(",");
            }
         else
            {
            if (write_binary)
               {
               fwrite(&val[ch], 8, 1, out);
               }
            else
               {
               fprintf(out, "%+0.16lf", val[ch]);

               if (ch == n_chans-1)
                  fprintf(out, "\n");
               else
                  fprintf(out, ",");
               }
            }

         ++written;
         }

      if (written)
         {
         ++cnt;
         }
      }

   if (cnt != 0)
      {
      DOUBLE Hz = ((DOUBLE) cnt * 1E6) / ((DOUBLE) (timer.us() - start));
      fprintf(stderr,"\nDone, %I64d value(s) written\nAverage rate = %lf Hz\n",cnt, Hz);
      }

   DAQ.stop();
}

