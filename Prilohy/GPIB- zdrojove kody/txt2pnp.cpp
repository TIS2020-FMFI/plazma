//
// txt2pnp.cpp
// john@miles.io 1-2008
//

#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <assert.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "typedefs.h"

#define VERSION         "1.02"
const S32 MAX_LINE_CHARS   =  1024;
const S32 N_DEST_POINTS    =  1000;
const S32 MAX_INPUT_POINTS =  100000;
const S32 MAX_DECADE       =  7;          // 10 MHz-100 MHz
const S32 MIN_DECADE       = -6;          // 1 uHz-10 uHz
const S32 NUM_DECADES      = (MAX_DECADE - MIN_DECADE + 1);

FILE *TSC;
FILE *PNP;

DOUBLE F[MAX_INPUT_POINTS];
DOUBLE A[MAX_INPUT_POINTS];
S32 n_input_points = 0;

S32 min_decade_exp;  // exponents at _beginning_ of each decade (e.g., 5 = 100 kHz-1 MHz decade)
S32 max_decade_exp;

S32 first_point_in_decade[NUM_DECADES];
S32 last_point_in_decade [NUM_DECADES];

DOUBLE dest_F[NUM_DECADES][N_DEST_POINTS];
DOUBLE dest_A[NUM_DECADES][N_DEST_POINTS];

void shutdown(void)
{
   _fcloseall();
}

void abend(C8 *fmt, ...)
{
   C8 error_text[256];

   va_list args;
   va_start(args, fmt);
   _vsnprintf(error_text, sizeof(error_text)-1, fmt, args);
   va_end(args);
   
   fprintf(stderr,error_text);

   exit(1);
}

void rewind(void)
{
   fseek(TSC, 0, SEEK_SET);
}

S32 find_line(C8 *substring, S32 errors_are_fatal)
{
   C8 buff[MAX_LINE_CHARS+1];

   for (;;)
      {
      if (feof(TSC) || ferror(TSC)) 
         {
         if (errors_are_fatal)
            {
            abend("Error: couldn't find line containing '%s'\n",substring);
            }

         return 0;
         }

      memset(buff, 0, sizeof(buff));
      fgets(buff, sizeof(buff)-1, TSC);

      if (strstr(buff, substring) != NULL)
         {
         return 1;
         }
      }
}

S32 seek_required_line(C8 *substring)
{
   rewind();
   return find_line(substring, TRUE);
}

S32 seek_optional_line(C8 *substring)
{
   rewind();
   return find_line(substring, FALSE);
}

C8 *next_line(C8 *containing_substring, S32 errors_are_fatal, C8 *excluding_substring = NULL)
{
   static C8 buff[MAX_LINE_CHARS+1];

   for (;;)
      {
      if (feof(TSC) || ferror(TSC)) 
         {
         if (errors_are_fatal)
            {
            abend("Error: couldn't find next line\n");
            }

         return NULL;
         }

      memset(buff, 0, sizeof(buff));
      fgets(buff, sizeof(buff)-1, TSC);

      S32 i = strlen(buff)-1;
      while ((i >= 0) && (isspace(buff[i])))
         {
         buff[i--] = 0;             // remove trailing CR/LF and any other whitespace
         }

      if (excluding_substring != NULL)
         {
         C8 *src = strstr(buff, excluding_substring);

         if (src != NULL)
            {
            continue;
            }
         }

      if (containing_substring == NULL)
         {
         C8 *src = buff;

         while (*src)
            {
            if (!isspace(*src))        
               {
               return src;          // return pointer to first nonspace character, if there is one...
               }

            ++src;
            }
         }
      else
         {
         C8 *src = strstr(buff, containing_substring);

         if (src != NULL)
            {
            S32 len = strlen(containing_substring);

            return &src[len];       // ...else if line contains specified substring, return pointer to
            }                       // first character after it
         }
      }
}

C8 *next_required_line(C8 *containing_substring)
{
   return next_line(containing_substring, TRUE);
}

C8 *next_optional_line(C8 *containing_substring)
{
   return next_line(containing_substring, FALSE);
}

C8 *next_required_line_excluding(C8 *excluding_substring)
{
   return next_line(NULL, TRUE, excluding_substring);
}

static S32 round_to_nearest(DOUBLE f)
{
   if (f >= 0.0) 
      f += 0.5;
   else
      f -= 0.5;

   return S32(f);
}

void main(S32 argc, C8 **argv)
{
   if (argc < 3)
      {
      printf("\n");
      printf("This program converts .txt data files from TSC2PNP.BAT to the .PNP\n");
      printf("(Phase Noise Plot) format used by the KE5FX GPIB Toolkit's phase-noise \n");
      printf("measurement application, PN.EXE.\n\n");
      printf("Usage: txt2pnp logfile.txt outfile.pnp [min_offset_Hz] [max_offset_Hz]\n\n");
      printf("Example:\n");
      printf("         txt2pnp netlog.txt noise_test.pnp 100 100000\n\n");
      printf("The default minimum and maximum offsets are 0.1 Hz and 100000 Hz, respectively.\n");
      exit(1);
      }

   TSC = fopen(argv[1],"rt");

   if (TSC == NULL)
      {
      printf("Error: Couldn't open %s\n",argv[1]);
      exit(1);
      }

   PNP = fopen(argv[2],"wt");

   if (PNP == NULL)
      {
      fclose(TSC);
      printf("Error: Couldn't open %s\n",argv[2]);
      exit(1);
      }

   DOUBLE min_offset_Hz = 0.1;
   DOUBLE max_offset_Hz = 100000.0;

   if (argc >= 4)
      {
      sscanf(argv[3],"%lf",&min_offset_Hz);
      }

   if (argc >= 5)
      {
      sscanf(argv[4],"%lf",&max_offset_Hz);
      }

   min_decade_exp = round_to_nearest(log10(min_offset_Hz));
   max_decade_exp = round_to_nearest(log10(max_offset_Hz)) - 1;

   if (min_decade_exp < MIN_DECADE) min_decade_exp = MIN_DECADE;
   if (max_decade_exp > MAX_DECADE) max_decade_exp = MAX_DECADE;

   atexit(shutdown);

   fprintf(PNP,";\n");
   fprintf(PNP,";Composite-noise plot file %s\n",argv[2]);
   fprintf(PNP,";from original TSC logfile %s\n",argv[1]);
   fprintf(PNP,";\n");
   fprintf(PNP,";Generated by TXT2PNP.EXE V"VERSION" of "__DATE__"\n");
   fprintf(PNP,";by John Miles, KE5FX (john@miles.io)\n");
   fprintf(PNP,";\n\n");

   if (seek_optional_line("show title"))
      {
      C8 *text = next_required_line_excluding("Welcome to the TSC");

      if ((text[0] == '=') && (strstr(text," >") != NULL))
         {
         fprintf(PNP,"CAP %s\n",strstr(text," >")+2);
         }                                            
      else
         {
         fprintf(PNP,"CAP %s\n", text);
         }
      }

   C8 timestamp[MAX_LINE_CHARS+1] = "N/A";

   if (seek_optional_line("show date"))
      {
      strcpy(timestamp, next_required_line(": "));

      fprintf(PNP,"TIM %s\n",timestamp);
      }

   if (seek_optional_line("show version"))
      {
      fprintf(PNP,"IMO %s\n",next_required_line("Model: "));
      fprintf(PNP,"IRV %s\n",next_required_line("Software: "));
      }

   fprintf(PNP,"\nDRG %d,%d\n",min_decade_exp,max_decade_exp);
   fprintf(PNP,"DCC %d\n",N_DEST_POINTS);
   fprintf(PNP,"OVC 0\n\n");

   fprintf(PNP,"EIF -1 Hz\n");
   fprintf(PNP,"ELO -1 Hz\n");
   fprintf(PNP,"MUL 1.000000\n\n");

   fprintf(PNP,"VBF 1.00\n\n");

   //
   // Parse carrier frequency/amplitude
   //

   DOUBLE car_Hz = -1.0;
   DOUBLE car_dBm = -1.0;

   seek_required_line("show inputs");
   next_required_line("Current:");
   C8 *t = next_required_line("Input: ");

   if (strstr(t," MHz ") != NULL)
      {
      sscanf(t,"Frequency %lf MHz Amplitude %lf dBm",&car_Hz, &car_dBm);
      car_Hz *= 1E6;    // distinguish MHz (megahertz) from mHz (millihertz)
      }
   else
      {
      _strlwr(t);

      if (strstr(t," ghz ") != NULL)
         {
         sscanf(t,"frequency %lf ghz amplitude %lf dbm",&car_Hz, &car_dBm);
         car_Hz *= 1E9;
         }
      else if (strstr(t," khz ") != NULL)
         {
         sscanf(t,"frequency %lf khz amplitude %lf dbm",&car_Hz, &car_dBm);
         car_Hz *= 1E3;
         }
      else if (strstr(t," hz ") != NULL)
         {
         sscanf(t,"frequency %lf hz amplitude %lf dbm",&car_Hz, &car_dBm);
         }
      else
         {
         abend("Couldn't parse line: \"%s\"\n",t);
         }
      }

   fprintf(PNP,"CAR %I64d Hz, %lf dBm\n",(S64) (car_Hz + 0.5), car_dBm);
   fprintf(PNP,"ATT 0 dB\n\n");  

   //
   // Read PSD entries, logging decade indexes between min_decade and max_decade
   //

   seek_required_line("show spectrum");
   next_required_line("Frequency (Hz)");

   n_input_points = 0;

   S32 cur_decade_exp = min_decade_exp;

   memset(first_point_in_decade, -1, sizeof(first_point_in_decade));
   memset(last_point_in_decade,  -1, sizeof(last_point_in_decade));

   for (;;)
      {
      if (feof(TSC) || ferror(TSC) || (n_input_points == MAX_INPUT_POINTS))
         {
         break;
         }

      C8 buff[MAX_LINE_CHARS+1];

      memset(buff, 0, sizeof(buff));
      fgets(buff, sizeof(buff)-1, TSC);

      S32 i = strlen(buff)-1;
      while ((i >= 0) && (isspace(buff[i])))
         {
         buff[i--] = 0;             // remove trailing CR/LF and any other whitespace
         }

      if (i == -1)
         {
         continue;                  // skip blank lines
         }
   
      //
      // Log the point, exiting at end of PSD list
      //

      DOUBLE f=0.0,a=0.0;
      sscanf(buff,"%lf %lf", &f,&a);

      if (f == 0.0)  
         {
         break;
         }

      F[n_input_points] = f;
      A[n_input_points] = a;

      //
      // Classify this point according to the decade it falls into
      //

      while (cur_decade_exp <= max_decade_exp)
         {
         S32 cur_decade_index = cur_decade_exp - min_decade_exp;

         DOUBLE cur_decade_begins = pow(10.0,cur_decade_exp);
         DOUBLE nxt_decade_begins = cur_decade_begins * 10.0;

         if (f < nxt_decade_begins)
            {
            if (f >= cur_decade_begins)
               {
               if (first_point_in_decade[cur_decade_index] == -1)
                  {
                  first_point_in_decade[cur_decade_index] = n_input_points;
                  }

               last_point_in_decade[cur_decade_index] = n_input_points;
               }

            break;
            }

         cur_decade_exp++;
         }

      ++n_input_points;
      }

   printf("\nProcessing %d input points...\n",n_input_points);

   for (S32 i=0; i < NUM_DECADES; i++)
      {
      if (first_point_in_decade[i] != -1)
         {
         printf("  Decade %d: %5d to %5d (offsets %9.2f Hz to %9.2f Hz)\n",
            i + min_decade_exp,
            first_point_in_decade[i],
            last_point_in_decade[i],
            F[first_point_in_decade[i]],
            F[last_point_in_decade[i]]);
         }
      }

   //
   // Resample TSC trace data to destination (fixed-width-per-decade) traces
   //
   // We lerp the frequency offsets, while point-sampling the PSD values
   //

   for (S32 de=min_decade_exp; de <= max_decade_exp; de++)
      {
      S32 decade_index = de - min_decade_exp;

      S32 s0 = first_point_in_decade[decade_index];
      S32 s1 = last_point_in_decade [decade_index];
      S32 n_src_points = s1-s0+1;

      DOUBLE f0 = F[s0];
      DOUBLE f1 = F[s1];
      DOUBLE df = f1-f0;

      DOUBLE di_dp = ((DOUBLE) n_src_points) / N_DEST_POINTS;
      DOUBLE index = (DOUBLE) s0;

      DOUBLE df_dp = df / N_DEST_POINTS;
      DOUBLE f     = f0;

      for (S32 i=0; i < N_DEST_POINTS; i++)
         {
         S32 src_index = (S32) (index + 0.5);

         if (src_index >= n_input_points)
            {
            abend("Error: Insufficient trace data available for the specified decade range\n");
            }

         dest_F[decade_index][i] = (f < f1) ? f : f1;
         dest_A[decade_index][i] = A[src_index];

         index += di_dp;
         f     += df_dp;
         }
      }

   //
   // Write the resampled decade data
   //

   for (S32 de=min_decade_exp; de <= max_decade_exp; de++)
      {
      S32 decade_index = de - min_decade_exp;

      fprintf(PNP,"DEC %d\n", de);
      fprintf(PNP,"NCF 0.0 dB\n");

      for (S32 i=0; i < N_DEST_POINTS; i++)
         {
         fprintf(PNP,"   TRA %d: %lf Hz, %lf dBc\n",
            i,
            dest_F[decade_index][i] + car_Hz,
            dest_A[decade_index][i]);
         }
      
      fprintf(PNP,"\n");
      }

   //
   // Terminate the file
   //

   if (!seek_optional_line("Collecting ("))
      {
      fprintf(PNP,"ELP N/A\n");
      }
   else
      {
      rewind();
      C8 *text = next_optional_line("Collecting (");

      C8 *term = strchr(text,')');

      if (term != NULL)
         {
         *term = NULL;
         }

      fprintf(PNP,"ELP %s\n",text);
      }

   fprintf(PNP,"EOF %s\n",timestamp);

   printf("Done\n");
   exit(0);
}

