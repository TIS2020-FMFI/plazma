//
// SGENTEST: Automated testing for HP signal generators using an 8566/8568
// or 8560A/E-series spectrum analyzer
//
// Author: John Miles, KE5FX (john@miles.io)
//

#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include "gpiblib.h"

#define VERSION "1.13"

FILE  *logfile;
S32    debug_start_time;

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

   if (isspace(((U8 *) dest)[0]))
      {
      dest[0] = 0;
      }
}

void WINAPI GPIB_error(C8 *msg, S32 ibsta, S32 iberr, S32 ibcntl)
{
   printf("%s",msg);

   exit(1);
}

void shutdown(void)
{
   _fcloseall();
   GPIB_disconnect();
}

void timestamp(void)
{
   U32 t = timeGetTime() - debug_start_time;

   printf("[%.02d:%.02d:%.02d.%.03d] ",
      (t / 3600000L),
      ((t / 60000L) % 60L),
      ((t / 1000L)  % 60L),
      (t % 1000L));

   if (logfile != NULL)
      {
      fprintf(logfile,"[%.02d:%.02d:%.02d.%.03d] ",
         (t / 3600000L),
         ((t / 60000L) % 60L),
         ((t / 1000L)  % 60L),
         (t % 1000L));
      }
}

void __cdecl log_printf(C8 *fmt, ...)
{
   C8 work_string[512] = { 0 };

   if ((fmt == NULL) || (strlen(fmt) > sizeof(work_string)))
      {
      strcpy(work_string, "(String missing or too large)");
      }
   else
      {
      va_list ap;

      va_start(ap, 
               fmt);

      _vsnprintf(work_string, 
                 sizeof(work_string)-1,
                 fmt, 
                 ap);

      va_end(ap);
      }

   printf("%s",work_string);

   if (logfile != NULL)
      {
      fprintf(logfile,"%s",work_string);
      }
}

void main(S32 argc, C8 **argv)
{
   setbuf(stdout,NULL);

   if (argc < 3)
      {
      printf("\nSGENTEST version "VERSION" of "__DATE__" by John Miles, KE5FX\n\n");
      printf("This program tests HP signal generators (8656A/B, 8757A-D, 8642B/M,\n"
             "8662A/8663A, 8672A and others) by programming randomly-chosen frequency\n"
             "and amplitude settings.  Output signals are then verified by an\n"
             "HP 8566A/B, 8568A/B, or 8560A/E-series spectrum analyzer.\n\n");
      
      printf("Usage: sgentest <analyzer address> <generator address> [<options> ...]\n\n");
      printf("Available options (with default or example values):\n\n");
      printf("         -fmax:1.2E9   .... Maximum test frequency = 1200 MHz\n"
             "         -fmin:10E3    .... Minimum test frequency = 10 kHz\n"
             "         -amin:-90     .... Minimum test power = -90 dBm\n"
             "         -amax:10      .... Maximum test power = 10 dBm\n"
             "         -loss:1.0     .... Interconnect loss in dB at fmax\n"
             "         -ftol:200     .... Frequency tolerance = +/- 200 Hz\n"
             "         -atol:2       .... Amplitude tolerance = +/- 2 dB\n"
             "         -wait:0.25    .... Wait 250 milliseconds after programming generator\n"
             "         -8662         .... Check for HP 8662A/8663A hardware errors\n"
             "         -8560         .... 8560-series compatibility mode\n"
             "         -8672         .... 8672-series compatibility mode\n"
             "         -log:filename .... Log output to filename\n"
             "         -trials:1000  .... Stop after 1000 trials\n"
             "         -verbose      .... Display/log all trials, not just failures\n"
             "         -srand:500    .... Seed random # generator with integer\n"
             "         -stime        .... Seed random # generator with time() function\n\n");
      printf("Notes:\n\n");
      printf("- Minimum test power must be sufficient to yield good amplitude measurements\n");
      printf("- Numeric arguments are reals, and may use scientific notation\n");
      printf("- 8662A/8663A hardware error checks are disabled by default\n");
      printf("- Some 8560-series analyzers may require -8560 option for compatibility\n");
      printf("- Test will run indefinitely if -trials is 0 (or not specified)\n");
      printf("- Default -loss value is based on 15 feet of LMR-400 and two N connectors\n");
      printf("  at fmax=1.2E9\n");
      printf("\nExample to test HP 8663A at address 19 with analyzer at address 18, using\n"
             "15 feet of LMR-400 cable (1.0 dB loss) with N connectors (0.15 dB each):\n\n");
      printf("  sgentest 18 19 -fmax:2559E6 -amax:19 -8662 -loss:1.3\n");
      printf("\nExample to test HP 8672A:\n\n");
      printf("  sgentest 18 19 -8672 -fmax:18E9 -fmin:2E9 -amax:8 -amin:-50 -atol:4 -loss:4\n");
      printf("  (Set output level range to -110 dBm before running)\n");

      exit(1);
      }

   //
   // Check for CLI options
   //

   S32 addr_specan = atoi(argv[1]);    // mandatory
   S32 addr_siggen = atoi(argv[2]);    // mandatory

   DOUBLE  max_Hz         = 1.2E9;        // optional args begin at argv[3]
   DOUBLE  min_Hz         = 10.0E3;
   DOUBLE  min_dBm        = -90.0;
   DOUBLE  max_dBm        = 10.0;
   DOUBLE  loss_at_fmax   = 0.0;
   DOUBLE  ampl_tolerance = 2.0;
   DOUBLE  freq_tolerance = 200.0;
   DOUBLE  tune_secs      = 0.25;
   S32     poll_8662A     = FALSE;
   S32     trials         = 0;
   U32     rand_seed      = 0;
   bool    use_8560       = FALSE;
   bool    use_8672       = FALSE;
   bool    verbose        = FALSE;
   C8      log_filename[MAX_PATH] = { 0 };

   C8 lpCmdLine[MAX_PATH] = "";

   for (S32 i=3; i < argc; i++)
      {
      strcat(lpCmdLine, argv[i]);
      strcat(lpCmdLine, " ");
      }

   C8 *option = NULL;
   C8 *end    = NULL;
   C8 *beg    = NULL;
   C8 *ptr    = NULL;
   S32 len    = 0;

   while (1)
      {
      //
      // Remove leading and trailing spaces from command line
      //

      for (U32 i=0; i < strlen(lpCmdLine); i++)
         {
         if (!isspace((U8) lpCmdLine[i]))
            {
            memmove(lpCmdLine, &lpCmdLine[i], strlen(&lpCmdLine[i])+1);
            break;
            }
         }

      C8 *p = &lpCmdLine[strlen(lpCmdLine)-1];

      while (p >= lpCmdLine)
         {
         if (!isspace((U8) *p))
            {
            break;
            }

         *p-- = 0;
         }

      //
      // -loss: Loss at fmax
      //

      option = strstr(lpCmdLine,"-loss:");

      if (option != NULL)
         {
         sscanf(option,"-loss:%lf%n", &loss_at_fmax, &len);

         end = option+len;
         memmove(option, end, strlen(end)+1);
         continue;
         }

      //
      // -fmax: Max freq
      //

      option = strstr(lpCmdLine,"-fmax:");

      if (option != NULL)
         {
         sscanf(option,"-fmax:%lf%n", &max_Hz, &len);

         end = option+len;
         memmove(option, end, strlen(end)+1);
         continue;
         }

      //
      // -fmin: Min freq
      //

      option = strstr(lpCmdLine,"-fmin:");

      if (option != NULL)
         {
         sscanf(option,"-fmin:%lf%n", &min_Hz, &len);

         end = option+len;
         memmove(option, end, strlen(end)+1);
         continue;
         }

      //
      // -amax: Max dBm
      //

      option = strstr(lpCmdLine,"-amax:");

      if (option != NULL)
         {
         sscanf(option,"-amax:%lf%n", &max_dBm, &len);

         end = option+len;
         memmove(option, end, strlen(end)+1);
         continue;
         }

      //
      // -amin: Min dBm
      //

      option = strstr(lpCmdLine,"-amin:");

      if (option != NULL)
         {
         sscanf(option,"-amin:%lf%n", &min_dBm, &len);

         end = option+len;
         memmove(option, end, strlen(end)+1);
         continue;
         }

      //
      // -ftol: Frequency tolerance in Hz
      //

      option = strstr(lpCmdLine,"-ftol:");

      if (option != NULL)
         {
         sscanf(option,"-ftol:%lf%n", &freq_tolerance, &len);

         end = option+len;
         memmove(option, end, strlen(end)+1);
         continue;
         }

      //
      // -atol: Amplitude tolerance in dB
      //

      option = strstr(lpCmdLine,"-atol:");

      if (option != NULL)
         {
         sscanf(option,"-atol:%lf%n", &ampl_tolerance, &len);

         end = option+len;
         memmove(option, end, strlen(end)+1);
         continue;
         }


      //
      // -trials: Stop after specified # of trials
      //

      option = strstr(lpCmdLine,"-trials:");

      if (option != NULL)
         {
         DOUBLE float_trials = 0.0;
         sscanf(option,"-trials:%lf%n", &float_trials, &len);
         trials = (S32) (float_trials + 0.5);

         end = option+len;
         memmove(option, end, strlen(end)+1);
         continue;
         }

      //
      // -srand: Seed rand()
      //

      option = strstr(lpCmdLine,"-srand:");

      if (option != NULL)
         {
         DOUBLE float_seed = 0.0;
         sscanf(option,"-srand:%lf%n", &float_seed, &len);
         rand_seed = (U32) (float_seed + 0.5);

         end = option+len;
         memmove(option, end, strlen(end)+1);
         continue;
         }

      //
      // -stime: Seed rand() with time()
      //

      option = strstr(lpCmdLine,"-stime");

      if (option != NULL)
         {
         rand_seed = time(NULL);
         memmove(option, &option[6], strlen(&option[6])+1);
         continue;
         }

      //
      // -wait: Delay after tuning generator before measuring
      //

      option = strstr(lpCmdLine,"-wait:");

      if (option != NULL)
         {
         sscanf(option,"-wait:%lf%n", &tune_secs, &len);

         end = option+len;
         memmove(option, end, strlen(end)+1);
         continue;
         }

      //
      // -8662: HP 8662A/8663A serial polling
      //

      option = strstr(lpCmdLine,"-8662");

      if (option != NULL)
         {
         poll_8662A = TRUE;
         memmove(option, &option[5], strlen(&option[5])+1);
         continue;
         }

      //
      // -8560: HP 8560A compatibilty mode (no KSb or KSx instructions)
      //

      option = strstr(lpCmdLine,"-8560");

      if (option != NULL)
         {
         use_8560 = TRUE;
         memmove(option, &option[5], strlen(&option[5])+1);
         continue;
         }

      //
      // -8672: HP 8672A compatibilty mode (R2D2-type frequency/amplitude HP-IB commands)
      //

      option = strstr(lpCmdLine,"-8672");

      if (option != NULL)
         {
         use_8672 = TRUE;
         memmove(option, &option[5], strlen(&option[5])+1);
         continue;
         }

      //
      // -verbose: Log all attempts, not just errors
      //

      option = strstr(lpCmdLine,"-verbose");

      if (option != NULL)
         {
         verbose = TRUE;
         memmove(option, &option[8], strlen(&option[8])+1);
         continue;
         }

      //
      // -log: Log to file 
      //

      option = strstr(lpCmdLine,"-log:");

      if (option != NULL)
         {
         sscanf(option,"-log:%s%n", &log_filename, &len);

         end = option+len;
         memmove(option, end, strlen(end)+1);
         continue;
         }

      //
      // Exit loop when no more CLI options are recognizable
      //

      break;
      }

   //
   // Rest of command line should be empty
   //

   if (strlen(lpCmdLine) > 0)
      {
      printf("Unrecognized option: %s\n",lpCmdLine);
      exit(1);
      }

   //
   // Validate params
   //

   if (min_dBm > max_dBm)
      {
      printf("Error: Minimum test power limit exceeds maximum limit\n");
      exit(1);
      }

   if (min_Hz > max_Hz)
      {
      printf("Error: Minimum test frequency limit exceeds maximum limit\n");
      exit(1);
      }

   //
   // Seed random number generator and open optional logfile
   //

   srand(rand_seed);
   
   logfile = NULL;

   if (log_filename[0]) 
      {
      logfile = fopen(log_filename,"wt");

      if (logfile == NULL)
         {
         printf("Couldn't open %s for writing\n", log_filename);
         exit(1);
         }
      }

   //
   // Write header
   //  

   time_t     elapstime;
   struct tm *adjtime;
   C8         loctime[32];

   time(&elapstime);
   adjtime = localtime(&elapstime);
   strcpy(loctime,asctime(adjtime));
   loctime[24] = 0;

   debug_start_time = timeGetTime();

   log_printf("\nSGENTEST version "VERSION" of "__DATE__" by John Miles, KE5FX\n"
             "-------------------------------------------------------------------------------\n\n"
            " Start time: %s\n"
            "     Trials: %d%s\n"
            "Random seed: %u\n\n",
            (C8 *) loctime, 
            trials, trials == 0 ? " (Run until keypress)" : "",
            rand_seed);

   //
   // Set up HP 856x spectrum analyzer
   //
   // Set 2 dB/division, positive-peak detection, enable amplitude correction factors 
   // and single-sweep mode
   //
   // Set span and RBW so that frequency tolerance is equal to +/- one
   // minor division
   //
   // 8560A series does not support KSb and KSX (pos peak detection and amplitude correction),
   // so we allow these to be turned off with -8560 switch
   // 

   atexit(shutdown);

   GPIB_connect(addr_specan,
                GPIB_error,
                0,
                10000);

   if (use_8560)
      {
      GPIB_printf("IP;SP %dHZ;RB %dHZ;LG %dDB;DET POS;S2", 
         (S32) ((freq_tolerance + 0.5) * 10.0),
         (S32) ((freq_tolerance + 0.5) / 2),
         (S32) ((loss_at_fmax > 5) ? 5 : 2));   // 2 dB/div unless lots of cable loss, then 5 dB/div
      }
   else  
      {
      GPIB_printf("IP;SP %dHZ;RB %dHZ;LG %dDB;KSb;KSX;S2", 
         (S32) ((freq_tolerance + 0.5) * 10.0),
         (S32) ((freq_tolerance + 0.5) / 2),
         (S32) ((loss_at_fmax > 5) ? 5 : 2));   // 2 dB/div unless lots of cable loss, then 5 dB/div
      }

   DOUBLE sweep_secs = 0.0;

   if (use_8672)
      {
      GPIB_printf("ST OA");
      C8 *read_result = GPIB_read_ASC();
      sscanf(read_result,"%lf", &sweep_secs);

      if (sweep_secs < 3.0)
         {
         GPIB_printf("ST 3.0SC OA");
         read_result = GPIB_read_ASC();
         sscanf(read_result,"%lf", &sweep_secs);
         }
      }
   
   GPIB_disconnect();
   
   printf("Press any key to terminate test ...\n\n"); 

   log_printf("     Time          Event              Command                  Result\n"
              "_______________________________________________________________________________\n\n");

   S32 n_f_violations = 0;
   S32 n_a_violations = 0;
   S32 n_hard_errs = 0;
   DOUBLE max_f_err = 0;
   DOUBLE max_a_err = 0;
   DOUBLE abs_max_f_err = 0;
   DOUBLE abs_max_a_err = 0;
   DOUBLE avg_f_err = 0;
   DOUBLE avg_a_err = 0;
   S32 trial_num = 1;
   S32 n_readings = 0;
   S32 n_passed = 0;

   while ((trials==0) || (trial_num <= trials))
      {
      if (_kbhit())
         {
         _getch();
         break;
         }

      DOUBLE MHz  = (DOUBLE) ((rand() % ((S32)(max_Hz / 1000)))+1) * 1.0E6;
      DOUBLE Hz   = (DOUBLE) (rand() * (rand() % 100));
      S64 freq = (S64) (MHz + Hz);

      if ((freq < min_Hz) || (freq > max_Hz))
         {
         continue;
         }

      S32 ampl = (S32) min_dBm + (rand() % (S32) ((max_dBm - min_dBm) + 1));
   
      //
      // Calculate expected loss due to skin effect (we ignore dielectric loss for now)
      //

      DOUBLE loss = loss_at_fmax / sqrt(max_Hz / freq);

      //
      // For 8672A, tune spectrum analyzer to quantized frequency and start a sweep before programming the 
      // generator, then disconnect as soon as the command has been issued (via 300-ms timeout)
      //
      // This is necessary because the 8672A will revert to its front-panel settings upon disconnection,
      // unlike most other signal generators
      //

      if (use_8672)  
         {
         S64 qfreq = (freq / 1000LL) * 1000LL;                 // quantize to whole-number kHz for 8672A
            
         if (qfreq > 6199999000LL && qfreq < 12400000000LL)    // band 2 sets in 2 kHz increments
            {
            qfreq = (S64) (qfreq / 2000LL) * 2000LL;
            }
            
         if (qfreq > 12399999999LL)                            // band 3 sets in 3 kHz increments
            {
            qfreq = (S64) (qfreq / 3000LL) * 3000LL;
            }

         freq = qfreq;

         GPIB_connect(addr_specan,
                      GPIB_error,
                      0,
                      300);

         GPIB_printf("CF %I64dHZ;RL %dDM;TS", freq, ampl + 10);

         GPIB_disconnect();
         }

      //
      // Tune generator
      //

      GPIB_connect(addr_siggen,
                   GPIB_error,
                   0,
                   10000);

      if (!use_8672)
         {
         GPIB_printf("FR %I64dHZ;AP %dDM", freq, ampl);
         }
      else
         {
         GPIB_printf("P%08I64dZ1M0N6",                                    // "P" loads freq, Z1 executes freq, M0=AM off, N6=FM off
                (S64) max(2000000LL, min((freq/1000LL), 18599997LL)));    // freq in 8-digits of kHz, min 2 GHz, max 18.6 GHz

         if (ampl > 0)
            {
            GPIB_printf("K0L%cO3", '=' - ampl);
            }
         else  
            {
            GPIB_printf("K%cL%cO1",                            // "P" loads freq, Z1 executes freq, K: step atten, L: vernier, M: AM, N: FM, O: ALC
                  C8( '0' + (abs(ampl) / 10) % 12 ),           // K-command: 0 to -110 dB step attenuation ('0','1',...';')
                  C8( '3' + (abs(ampl) % 10 )));               // L-command: 0 to -9 dB vernier, offset by 3 ('3','4',...'<')
            }
         }

      Sleep((S32) (tune_secs * 1000.0));

      if (use_8672)
         {
         //
         // Ensure the analyzer has had time to finish sweeping before we disconnect from
         // the 8672A
         //

         Sleep((S32) (1000.0 * (sweep_secs + 1.0)));
         }

      //
      // Optionally check for hardware error conditions
      // on HP 8662A / 8663A (flashing STATUS button)
      //

      bool av = FALSE;
      bool fv = FALSE;
      bool hf = FALSE;

      C8 fault_text[128] = { 0 };

      if (poll_8662A)
         {
         if (GPIB_serial_poll() & 0x04)
            {
            GPIB_printf("MS");
            _snprintf(fault_text,sizeof(fault_text)-1,"%s",GPIB_read_ASC());
            kill_trailing_whitespace(fault_text);

            ++n_hard_errs;
            hf = TRUE;

            //
            // Clear the error by resetting the generator,
            // then poll for up to 5 seconds until the bit actually goes low
            //

            GPIB_printf("SPECIAL 00");

            for (S32 i=0; i < 10; i++)
               {
               if (!(GPIB_serial_poll() & 0x04))
                  {
                  break;
                  }

               GPIB_printf("MS");
               GPIB_read_ASC();
               Sleep(500);

               if (_kbhit())
                  {
                  _getch();
                  printf("Status poll loop exited manually\n");
                  break;
                  }
               }
            }
         }

      GPIB_disconnect();

      //
      // Put marker on signal and compare its amplitude to what we programmed
      // above
      //

      DOUBLE read_ampl = -9999.0;
      DOUBLE read_freq = -9999.0;

      if (!hf)
         {
         GPIB_connect(addr_specan,
                      GPIB_error,
                      0,
                      30000);

         if (!use_8672)  
            {
            //
            // For signal generators other than the 8672A, tune the analyzer after
            // the generator
            //

            GPIB_printf("CF %I64dHZ;RL %dDM;TS", freq, ampl + 10);
            }

         GPIB_printf("E1");

         GPIB_printf("MA"); 
         C8 *read_result = GPIB_read_ASC();
         sscanf(read_result,"%lf", &read_ampl);

         GPIB_printf("MF"); 
         read_result = GPIB_read_ASC();
         sscanf(read_result,"%lf", &read_freq);

         GPIB_disconnect();

         //
         // Compensate for skin-effect loss
         //

         read_ampl += loss;

         //
         // Log statistics
         //

         DOUBLE a_err = fabs(read_ampl - ampl);
         DOUBLE f_err = fabs(read_freq - freq);

         DOUBLE k = 1.0 / (DOUBLE) trial_num;

         avg_f_err += k * (f_err - avg_f_err);
         avg_a_err += k * (a_err - avg_a_err);

         if (a_err >= abs_max_a_err) 
            {
            abs_max_a_err = a_err;
            max_a_err = read_ampl - ampl;
            }

         if (f_err >= abs_max_f_err) 
            {
            abs_max_f_err = f_err;
            max_f_err = read_freq - freq;
            }

         if (a_err > ampl_tolerance)
            {
            ++n_a_violations;
            av = TRUE;
            }

         if (f_err > freq_tolerance)
            {
            ++n_f_violations;
            fv = TRUE;
            }

         ++n_readings;
         }

      if (hf)
         {
         timestamp();

         log_printf("Trial %.06d %13I64d Hz %+3d dBm      %s\n",
            trial_num,
            freq,
            ampl,
            fault_text);
         }
      else if (av || fv || verbose)
         {
         timestamp();

         log_printf("Trial %.06d %13I64d Hz %+3d dBm %+8d Hz %+0.2lf dB\n",
            trial_num,
            freq,
            ampl,
            (S32) (read_freq - freq),
            (DOUBLE) (read_ampl - ampl));
         }

           if (hf)        log_printf("(Hardware error)\n");
      else if (av && !fv) log_printf("(Amplitude error)\n");
      else if (fv && !av) log_printf("(Frequency error)\n");
      else if (fv && av)  log_printf("(Amplitude and frequency error)\n");
      else ++n_passed;

      trial_num++;
      }

   S32 n_attempts = trial_num-1;

   if (n_attempts > 0)
      {
      log_printf("\n%d trials (%2.1lf%% passed)",
         n_attempts,
         (100.0 * (DOUBLE) n_passed) / (DOUBLE) n_attempts);
      
      if (n_readings > 0)
         {
         log_printf(":\n   Max freq err = %d Hz\n   Max ampl err = %.02lf dB\n   Avg freq err = %d Hz\n   Avg ampl err = %.02lf dB",
            (S32) max_f_err,
            max_a_err,
            (S32) avg_f_err,
            avg_a_err);
         }

      log_printf("\n\n");

      log_printf("Frequency violation(s): %d\n",n_f_violations);
      log_printf("Amplitude violation(s): %d\n",n_a_violations);

      if (poll_8662A)
         {
         log_printf("     Hardware error(s): %d\n",n_hard_errs);
         }
      }

}
