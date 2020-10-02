//
// SATRACE: Fetch spectrum analyzer trace(s) 
//
// Author: John Miles, KE5FX (john@miles.io)
//

#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <time.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include "typedefs.h"
#include "specan.h"

#define VERSION "1.09"

SA_STATE *state;

//*************************************************************
//
// Return numeric value of string, with optional
// location of first non-numeric character
//
/*************************************************************/

S64 ascnum(C8 *string, U32 base, C8 **end = NULL)
{
   U32 i,j;
   U64 total = 0L;
   S64 sgn = 1;

   for (i=0; i < strlen(string); i++)
      {
      if ((i == 0) && (string[i] == '-'))
         {
         sgn = -1;
         continue;
         }

      for (j=0; j < base; j++)
         {
         if (toupper(string[i]) == "0123456789ABCDEF"[j])
            {
            total = (total * (U64) base) + (U64) j;
            break;
            }
         }

      if (j == base) 
         {
         if (end != NULL)
            {
            *end = &string[i];
            }

         return sgn * total;
         }
      }

   if (end != NULL)
      {
      *end = &string[i];
      }

   return sgn * total;
}

void shutdown(void)
{
   SA_shutdown();
}

void main(S32 argc, C8 **argv)
{
   setbuf(stdout,NULL);

   if (argc < 2)
      {
      printf("\nSATRACE version "VERSION" of "__DATE__" by John Miles, KE5FX\n\nThis program acquires one or more spectrum analyzer traces, writing\n"
             "frequency/amplitude pairs to stdout with optional resampling\n\n");

      printf("Usage: satrace <address> [<commands>...]\n\n");
      printf("Examples using GPIB address 18:\n\n");
      printf("satrace 18               Auto-identify analyzer at GPIB address 18 and acquire\n"
             "                         a single trace from it\n\n");
      printf("satrace -sa44            Special option required for use with USB-SA44/SA124 \n"
             "                         Signal Hound  (No GPIB address is needed)\n\n");
      printf("satrace 18 -856xa        Special option required for use with HP8566A-HP8568A\n\n");
      printf("satrace 18 -8569b        Special option required for use with HP8569B/8570A\n\n"); 
      printf("satrace 18 -358xa        Special option required for use with HP3588A/3589A\n\n");           
      printf("satrace 18 -3585         Special option required for use with HP3585A/B\n\n"); 
      printf("satrace 18 -advantest    Special option required for use with supported \n"
             "                         Advantest R3200/R3400-series analyzers\n\n");
      printf("satrace 18 -r3261        Special option required for use with R3261/R3361\n\n");
      printf("satrace 18 -scpi         Special option required for use with Agilent E4400-\n"
             "                         series and other SCPI-compatible analyzers\n\n");
      printf("satrace 18 -f            Favor speed over resolution, CRT updates, or other\n"
             "                         factors (if supported)\n\n"); 
      printf("satrace 18 -t            Disable GPIB timeout checking during long sweeps\n\n"); 
      printf("satrace 18 -ao:-7        Add -7 dBm to all reported amplitude values\n\n");
      printf("satrace 18 -fo:150000000 Add 150 MHz to all reported frequency values\n\n");
      printf("satrace 18 -reps:15      Acquire 15 successive traces (0 = run until keypress)\n\n");
      printf("satrace 18 -header       Display time/datestamp and available analyzer control\n"
             "                         settings\n\n");
      printf("satrace 18 -lf           Separate frequency/amplitude pairs with linefeeds\n"
             "                         rather than commas\n\n"); 
      printf("satrace 18 -spline:800   Resample trace using cubic spline reconstruction to\n"
             "                         generate (e.g.) 800 points, regardless of the\n"
             "                         analyzer's trace array width\n\n");
      printf("satrace 18 -point:128    Resample trace using point-sampled values\n\n");
      printf("satrace 18 -min:128      Resample trace using minimum bucket values when more\n"
             "                         source points than requested are available\n"
             "                         (otherwise use spline)\n\n");
      printf("satrace 18 -max:128      Resample trace using maximum bucket values when more\n"
             "                         source points than requested are available\n"
             "                         (otherwise use spline)\n\n");
      printf("satrace 18 -avg:128      Resample trace using averaged bucket values when more\n"
             "                         source points than requested are available\n"
             "                         (otherwise use spline)\n\n");
      printf("satrace 18 -connect:\"xxx\" \n"
             "                         Specify GPIB command string to be issued when \n"
             "                         initially connecting to instrument\n\n");
      printf("satrace 18 -disconnect:\"xxx\" \n"
             "                         Specify GPIB command string to be issued when\n"
             "                         disconnecting prior to program termination\n\n");
      printf("satrace 18 -856xa -f     Example using multiple options\n\n"); 

      printf("Additional control options supported by Signal Hound only:\n\n");

      printf("     -RL:-40             Specify reference level in dBm (default = -30)\n");
      printf("     -CF:90.3E6          Specify center frequency in Hz (default = 900 MHz)\n"); 
      printf("     -span:10E6          Specify span in Hz (default = 1 MHz)\n"); 
      printf("     -start:88E6         Specify sweep start frequency (default = 899 MHz)\n"); 
      printf("     -stop:108E6         Specify sweep stop frequency (default = 901 MHz)\n"); 
      printf("     -bins:64            Specify FFT kernel size (power of 2 from 16 to 256,\n                         default = 128)\n"); 
      printf("     -sens:2             Specify sensitivity factor (default = 2, range 0-2)\n"); 
      printf("     -RFATT:10           Specify RF attenuation in dB (0-15 dB in 5-dB steps,\n                         default = 0)\n"); 

      exit(1);
      }

   //
   // Check for CLI options
   //

   C8 _lpCmdLine[MAX_PATH] = "";

   for (S32 i=1; i < argc; i++)
      {
      strcat(_lpCmdLine, argv[i]);
      strcat(_lpCmdLine, " ");
      }

   C8 lpCmdLine[MAX_PATH];
   C8 lpUCmdLine[MAX_PATH];

   memset(lpCmdLine, 0, sizeof(lpCmdLine));
   strcpy(lpCmdLine, _lpCmdLine);
   strcpy(lpUCmdLine, _lpCmdLine);
   _strupr(lpUCmdLine);

   S32 reps              = 1;
   S32 n_dest_pts        = -1;   // -1 = no resampling
   RESAMPLE_OP resamp_op = RT_POINT;
   S32 LF_separator      = FALSE;
   S32 unity             = FALSE;
   S32 show_header       = FALSE;

   C8 *option = NULL;
   C8 *end    = NULL;
   C8 *beg    = NULL;
   C8 *ptr    = NULL;

   while (1)
      {
      //
      // Remove leading and trailing spaces from command line
      //
      // Caution: Argument checks below must not accept strings from lpUCmdLine that would
      // also be caught by lpCmdLine, since text excised from the lpUCmdLine string
      // remains in lpCmdLine!
      //

      for (U32 i=0; i < strlen(lpCmdLine); i++)
         {
         if (!isspace(lpCmdLine[i]))
            {
            memmove(lpCmdLine, &lpCmdLine[i], strlen(&lpCmdLine[i])+1);
            break;
            }
         }

      C8 *p = &lpCmdLine[strlen(lpCmdLine)-1];

      while (p >= lpCmdLine)
         {
         if (!isspace(*p))
            {
            break;
            }

         *p-- = 0;
         }

      //
      // -reps: Set # of traces to acquire
      //

      option = strstr(lpCmdLine,"-reps:");

      if (option != NULL)
         {
         reps = (S32) ascnum(&option[6], 10, &end);
         memmove(option, end, strlen(end)+1);
         continue;
         }

      //
      // -lf: Linefeed separators
      //

      option = strstr(lpCmdLine,"-lf");

      if (option != NULL)
         {
         LF_separator = TRUE;
         memmove(option, &option[3], strlen(&option[3])+1);
         continue;
         }

      //
      // -header: Show analyzer/timestamp parameters
      //

      option = strstr(lpCmdLine,"-header");

      if (option != NULL)
         {
         show_header = TRUE;
         memmove(option, &option[7], strlen(&option[7])+1);
         continue;
         }

      //
      // -min,-max,-avg,-point,-spline: Request trace resampling 
      //

      option = strstr(lpCmdLine,"-min:");

      if (option != NULL)
         {
         n_dest_pts = (S32) ascnum(&option[5], 10, &end);
         resamp_op = RT_MIN;
         memmove(option, end, strlen(end)+1);
         continue;
         }

      option = strstr(lpCmdLine,"-max:");

      if (option != NULL)
         {
         n_dest_pts = (S32) ascnum(&option[5], 10, &end);
         resamp_op = RT_MAX;
         memmove(option, end, strlen(end)+1);
         continue;
         }

      option = strstr(lpCmdLine,"-avg:");

      if (option != NULL)
         {
         n_dest_pts = (S32) ascnum(&option[5], 10, &end);
         resamp_op = RT_AVG;
         memmove(option, end, strlen(end)+1);
         continue;
         }

      option = strstr(lpCmdLine,"-point:");

      if (option != NULL)
         {
         n_dest_pts = (S32) ascnum(&option[7], 10, &end);
         resamp_op = RT_POINT;
         memmove(option, end, strlen(end)+1);
         continue;
         }

      option = strstr(lpCmdLine,"-spline:");

      if (option != NULL)
         {
         n_dest_pts = (S32) ascnum(&option[8], 10, &end);
         resamp_op = RT_SPLINE;
         memmove(option, end, strlen(end)+1);
         continue;
         }

      //
      // Exit loop when no more CLI options are recognizable
      // (This should leave the GPIB-related options on the command line)
      //

      break;
      }

   //
   // Pass remaining command-line args to spectrum-analyzer access library
   // Whatever is left will be treated as the GPIB address
   //

   state = SA_startup();
   atexit(shutdown);

   SA_parse_command_line(lpCmdLine);

   S32 addr = (S32) ascnum(lpCmdLine, 10);

   if (((!lpCmdLine[0]) || (!addr)) && (!state->SA44_mode))
      {
      printf("Error -- must specify GPIB address\n");
      exit(1);
      }
     
   //
   // Connect to analyzer and show header if requested
   //

   if (!SA_connect(addr))
      {
      exit(1);
      }

   if (show_header)
      {
      printf("instrument_model %s\n",state->ID_string);
      printf("start_freq_Hz %lf\n", state->min_Hz);
      printf("stop_freq_Hz %lf\n", state->max_Hz);
      printf("source_trace_points %d\n", state->n_trace_points);
      printf("reference_level_dBm %lf\n", state->max_dBm);
      printf("n_vertical_divisions %d\n",state->amplitude_levels);
      printf("dB_per_division %d\n", state->dB_division);
      if (state->RBW_Hz >= 0.0F) printf("RBW_Hz %lf\n",state->RBW_Hz);
      if (state->VBW_Hz >= 0.0F) printf("VBW_Hz %lf\n",state->VBW_Hz);
      if (state->vid_avgs >= 0) printf("vid_avgs %d\n",state->vid_avgs);
      if (state->sweep_secs >= 0.0F) printf("sweep_secs %lf\n",state->sweep_secs);
      if (state->RFATT_dB > -10000.0F) printf("RF_atten_dB %lf\n",state->RFATT_dB);
      }

   //
   // Fetch and display requested trace(s) 
   //

   for (S32 n=0; (reps == 0) ? TRUE : (n < reps); n++)
      {
      if (_kbhit())
         {
         _getch();
         break;
         }

      if (show_header)
         {
         time_t acq_time = 0;
         time(&acq_time);
                                  
         printf("\ntrace_num %d\n", n);
         printf("acquisition_time %s",ctime(&acq_time));
         }

      SA_fetch_trace();

      printf("\n");

      DOUBLE *src       = state->dBm_values;
      S32     n_src_pts = state->n_trace_points;  

      if (n_dest_pts != -1)      // was resampling requested?
         {
         static DOUBLE dest[65536];

         if ((n_dest_pts < 1) || (n_dest_pts > 65536))
            {
            printf("Error -- point count must be from 1 to 65536\n");
            exit(1);
            }

         SA_resample_data(src,  n_src_pts,
                          dest, n_dest_pts,
                          resamp_op);

         src       = dest;       // use resampled trace data
         n_src_pts = n_dest_pts;
         }

      if (show_header) printf(LF_separator ? "trace_data\n":"trace_data ");

      DOUBLE df = (state->max_Hz - state->min_Hz) / (DOUBLE) n_src_pts;
      DOUBLE f  = state->min_Hz + (df / 2.0);    // samples correspond to bucket centers, 
                                                 // even when point/min/max-sampling 
      for (S32 i=0; i < n_src_pts; i++)
         {
         printf("%lf,%lf",f, src[i]);            // freq in Hz, amplitude in dBm

         if (i != n_src_pts-1)
            {
            printf(LF_separator ? "\n" : ", ");
            }

         f += df;
         }

      if (n != reps-1)
         {
         printf(LF_separator ? "\n" : "\n");
         }
      }

   SA_disconnect(TRUE);
}
