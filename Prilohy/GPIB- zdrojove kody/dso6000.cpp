#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <assert.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "gpiblib.h"

const S32 BLOCK_READ_BYTES = 1000;

S8  digital_data[8000000][16];
S32 digital_n_samples = 0;

void WINAPI GPIB_error(C8 *msg, S32 ibsta, S32 iberr, S32 ibcntl)
{
   printf("%s",msg);

   exit(1);
}

void shutdown(void)
{
   _fcloseall();
   GPIB_disconnect();            // Use GPIB_disconnect(0) if 5370 jams bus at shutdown under Prologix
}

void main(S32 argc, C8 **argv)
{
   setbuf(stdout,NULL);

   if (argc < 4)
      {
      printf("Usage: dso6000 <address> <chan #> <# points> [outfile.bin]\n\n"
             "       (0 points = :WAV:POIN MAX)\n\n"
             "       If chan # is P1 or P2, data will be retrieved from digital\n"
             "       channels on the corresponding pod\n\n"
             "       If chan # is DIG, data will be retrieved from both pods\n"
             "       and saved as a 16-channel 8-bit signed PCM data file\n"
             "       (values > 127 = '1', values < 128 = '0')\n");

      exit(1);
      }

   atexit(shutdown);

   GPIB_connect(atoi(argv[1]),
                GPIB_error,
                0,
                10000);

   GPIB_set_EOS_mode(10);
   GPIB_set_serial_read_dropout(10000);

   C8 text[1024];
   GPIB_printf("*IDN?");
   strcpy(text,GPIB_read_ASC());

   fprintf(stderr,"\n");
   fprintf(stderr,"       Instrument: %s",text);

#if 0
   //
   // Annoyingly, :OPER:COND? times out if the scope is waiting for a trigger.  
   // There appears to be no other way to query the run/stop status...
   //

   U32 status = 0;
   GPIB_printf(":OPER:COND?");
   strcpy(text,GPIB_read_ASC());

   fprintf(stderr,"           Status:");

   if (status & (1 << 3)) printf("RUN ");
   if (status & (1 << 5)) printf("WAIT ");
   if (status & (1 << 9)) printf("MTE ");
   if (status & (1 << 11)) printf("OVLR ");
   if (status & (1 << 12)) printf("HWE ");
   printf("\n");

   if (status & (1 << 3))
      {
      printf("Error: Acquisition must be stopped before running this program\n");
      exit(1);
      }
#endif

   FILE *out = NULL;

   if (argc > 4)
      {
      out = fopen(argv[4],"wb");
      assert(out);
      }

   bool is_digital = (argv[2][0] == 'P') || (argv[2][0] == 'p') || (argv[2][0] == 'D') || (argv[2][0] == 'd');
   bool both_pods  = is_digital && ((argv[2][0] == 'D') || (argv[2][0] == 'd'));

   S32 n_points = 0;

   for (S32 pod=1 + (both_pods != 0) + (!is_digital); pod >= 1 + (!is_digital); pod--)
      {
      S32 chan = both_pods ? pod : (is_digital ? atoi(&argv[2][1]) : atoi(argv[2]));

      if (is_digital)
         GPIB_printf(":WAV:SOUR POD%d",chan);   
      else
         GPIB_printf(":WAV:SOUR CHAN%d",chan);   
      
      GPIB_printf(":WAV:POIN:MODE RAW");

      S32 desired_points = atoi(argv[3]);

      if (desired_points==0)
         GPIB_printf(":WAV:POIN MAX");
      else
         GPIB_printf(":WAV:POIN %d",desired_points);

      GPIB_printf(":WAV:UNS %s", is_digital ? "1" : "0");
      GPIB_printf(":WAV:BYT LSBF");
      GPIB_printf(":WAV:FORM BYTE");

      GPIB_printf(":WAV:PRE?");
      C8 preamble[1024];
      strcpy(preamble,GPIB_read_ASC());

      S32 format = 0;
      S32 type = 0;
      S32 count = 0;
      DOUBLE xinc = 0.0;
      DOUBLE xorg = 0.0;
      DOUBLE xref = 0.0;
      DOUBLE yinc = 0.0;
      DOUBLE yorg = 0.0;
      DOUBLE yref = 0.0;

      sscanf(preamble,"%d,%d,%d,%d,%lf,%lf,%lf,%lf,%lf,%lf",
         &format,
         &type,
         &n_points,
         &count,
         &xinc,
         &xorg,
         &xref,
         &yinc,
         &yorg,
         &yref);

      const C8 *types[] = {"Normal","Peak","Average","HiRes"};
      const C8 *formats[] = {"Byte","Word","2","3","ASCII"};

      fprintf(stderr,"         Acq type: %s\n",(type >= ARY_CNT(types)) ? "Unknown" : types[type]);
      fprintf(stderr,"        Acq count: %d\n", count);
      fprintf(stderr,"    Acq precision: %s\n",(format >= ARY_CNT(formats)) ? "Unknown" : formats[format]);
      fprintf(stderr,"      Data source: %s %d\n", is_digital ? "Pod" : "Ch", chan);
      fprintf(stderr," Requested points: %d\n",desired_points);
      fprintf(stderr,"    Actual points: %d\n",n_points);
      fprintf(stderr,"      File format: %s\n", is_digital ? "8-bit signed PCM values" : "Double-precision values in volts");
      fprintf(stderr,"       Time/point: %.lG us (%.lf MS/s)\n", xinc*1E6, (1.0 / xinc) / 1E6);
      fprintf(stderr,"   Total duration: %.3lG s\n", xinc * n_points);

      GPIB_printf(":WAV:DATA?");

      C8 header[16];
      strcpy(header,GPIB_read_ASC(10));
      S32 n_bytes = atoi(&header[2]);

      fprintf(stderr,"    Bytes to read: %d\n",n_bytes);
      fprintf(stderr,"      Destination: %s\n",(out == NULL) ? "stdout" : argv[4]);
      fprintf(stderr,"\n");

      S32 points_read = 0;

      while (n_bytes > 0)
         {
         if (_kbhit())
            {
            _getch();
            break;
            }

         S32 bytes_read = 0;
         S8 *buffer = (S8 *) GPIB_read_BIN(min(n_bytes,BLOCK_READ_BYTES),
                                           TRUE, 
                                           FALSE, 
                                          &bytes_read);

         S32 point_cnt = bytes_read;

         if (is_digital)
            {
            for (S32 i=0; i < point_cnt; i++)
               {
               for (S32 b=7; b >= 0; b--)
                  {
                  S32 ch = b + (8 * (chan-1));

                  //
                  // Represent binary on/off values as 8-bit signed PCM values that will
                  // look good in a wave editor...
                  //

                  if (out == NULL)
                     {
                     printf("%d", (buffer[i] & (1 << b)) ? 1 : 0);
                     }
                  else
                     {
                     digital_data[points_read][ch] = (buffer[i] & (1 << b)) ? 64 : -64;
                     }
                  }

               if (out == NULL)
                  {
                  printf("\n");
                  }

               points_read++;
               }
            }
         else
            {
            static double volts[BLOCK_READ_BYTES];

            for (S32 i=0; i < point_cnt; i++)
               {
               DOUBLE v = (DOUBLE) buffer[i];
               volts[i] = ((v - yref) * yinc) + yorg;
               points_read++;
               }

            if (out != NULL)
               {
               if (fwrite(volts,sizeof(volts[0]),point_cnt,out) != point_cnt)
                  {
                  printf("Couldn't write to %s: disk full?\n",argv[4]);
                  exit(1);
                  }
               }
            else
               {
               for (S32 i=0; i < point_cnt; i++)   
                  {
                  printf("%.3lf\n",volts[i]);
                  }
               }
            }

         n_bytes -= bytes_read;

         if (out != NULL)
            {
            fprintf(stderr,"%d of %d points read     \r",points_read,n_points);
            }
         }

      GPIB_read_BIN(1);    // trailing LF
      }

   printf("\n");

   if ((out != NULL) && both_pods)
      {
      fprintf(stderr, "Writing %d points\n", n_points);

      if (fwrite(digital_data, 16, n_points, out) != n_points)
         {
         printf("Couldn't write to %s: disk full?\n",argv[4]);
         exit(1);
         }
      }
}
