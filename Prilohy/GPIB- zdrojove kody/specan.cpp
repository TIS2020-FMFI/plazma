//
// SPECAN.CPP: Spectrum-analyzer trace acquisition library
//
// Author: John Miles, KE5FX (john@miles.io)
//

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <tlhelp32.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <io.h>
#include <fcntl.h>
#include <mmsystem.h>
#include <time.h>
#include <assert.h>
#include <float.h>
#include <limits.h>
#include <malloc.h>
#include <setupapi.h>

#include "typedefs.h"

#include "spline.cpp"

#define BUILDING_SPECAN
#include "specan.h"

#define ASCII_ACQ  0          // 1 to use ASCII mode to fetch data, when supported (very slow in some cases)
#define N_SIM_PTS  500        // Points for simulated data source (GPIB address = -1)
#define MAX_N_PTS  65536      // Max # of trace points supported 

SA_STATE state;               // Global state accessible to application

S32 disable_GPIB_timeouts;
S32 orig_n_points;            // Valid for Advantest R3267 family only
S32 cur_n_points;             // Valid for Advantest analyzers only 

C8 connect_cmd    [1024];
C8 disconnect_cmd [1024];
C8 startup_cmd    [1024];
C8 shutdown_cmd   [1024];

C8 temp_buffer[262144];

struct TEK_PREAMBLE     // 49x/271x preamble
{
   S32    nr_pt;
   S32    pt_off; 
   DOUBLE xincr;
   DOUBLE xzero;
   DOUBLE yoff;
   DOUBLE ymult;
   DOUBLE yzero;
   C8     expected_curve[512];
};

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

//***************************************************************************
//
// kill_trailing_whitespace()
//
//***************************************************************************

void kill_trailing_whitespace(C8 *dest)
{
   S32 l = strlen(dest);

   while (--l >= 0)
      {
      if (!isspace((U8) dest[l]))
         {
         dest[l+1] = 0;
         break;
         }
      }
}

/*********************************************************************/
//
// Sleep() equivalent that checks periodically for a space-bar keypress
//
/*********************************************************************/

S32 interruptable_sleep(S32 ms)
{
   S32 itcnt = ms / 10;       // check every 10 milliseconds

   for (S32 i=0; i < itcnt; i++)
      {
      if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
         {
         return TRUE;
         }                    

      Sleep(10);
      }

   return FALSE;
}

/*********************************************************************/
//
// Kill any existing Hound2Pipe.exe processes and wait for OS to clean up the
// associated I/O state
//
/*********************************************************************/

void HOUND_kill_process(bool delay)
{
   S32 killed = 0;

   HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

   if (hProcessSnap == INVALID_HANDLE_VALUE)
      {
      return;
      }

   PROCESSENTRY32 pe32 = { 0 };
   pe32.dwSize = sizeof(PROCESSENTRY32);

   if (Process32First(hProcessSnap, &pe32))
      {
      do
         {
         if (!_stricmp(pe32.szExeFile, "HOUND2PIPE.EXE"))
            {
            HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);

            if (hProcess != NULL)
               {
               TerminateProcess(hProcess, 0);
               WaitForSingleObject(hProcess, 5000);
               CloseHandle(hProcess);
               ++killed;
               }
            }
         }
      while (Process32Next(hProcessSnap, &pe32));
      }

   CloseHandle(hProcessSnap);

   if (killed && delay)
      {
      Sleep(2000);
      }
}

//****************************************************************************
//
// 32/64-bit hardware abstraction
//
//****************************************************************************

#ifdef _WIN64

#include <ni488.h>
#define NO_FTDI
#include "gpibport.cpp"
#undef ERR

GPIBPORT GPIB;

void __cdecl alert(C8 *caption, C8 *fmt, ...)
{
   state.error_text[sizeof(state.error_text)-1] = 0;
   state.error = TRUE;

   va_list ap;

   va_start(ap, 
            fmt);

   _vsnprintf(state.error_text, 
              sizeof(state.error_text)-1,
              fmt, 
              ap);
       
   va_end (ap);
}

void fail(S32 exit_code)
{
}

void WINAPI GPIB_error(GPIBPORT *P, 
                       C8       *msg)
{
   if (state.error)
      {
      return;
      }

   alert("", msg);
}

bool GPIB_connect  (S32        device_address, 
                    GPIBERR    handler, 
                    bool       clear                  = FALSE, 
                    S32        timeout_msecs          = 100000,  // use -1 to disable timeout checks
                    S32        board_address          = -1, 
                    bool       release_system_control = FALSE,
                    S32        max_read_buffer_len    = 4096,    // >4K will not work with NI GPIB-232! 
                    S32        force_auto_read        = -1)      // -1 = use INI_force_auto_read setting
{
   if (state.error)
      {
      return FALSE;
      }

   if (!GPIB.connect (device_address,
                      handler,
                      clear,
                      timeout_msecs,         
                      board_address,         
                      release_system_control,
                      max_read_buffer_len,   
                      FALSE))
      {
      return FALSE;
      }

   if (force_auto_read > 0)
      {
      GPIB.auto_read_mode (TRUE);
      }

   return TRUE;
}

bool GPIB_disconnect (bool reset_to_local = TRUE)
{
   if (state.error)
      {
      return FALSE;
      }

   return GPIB.disconnect(reset_to_local);
}

S32 GPIB_set_EOS_mode (S32  new_EOS_char, 
                       bool send_EOI_at_EOT            = TRUE, 
                       bool enable_board_configuration = TRUE)
{
   if (state.error)
      {
      return -1;
      }

   return GPIB.set_EOS_mode (new_EOS_char,
                             send_EOI_at_EOT,
                             enable_board_configuration);
}

S32 GPIB_set_serial_read_dropout (S32 ms)
{
   if (state.error)
      {
      return FALSE;
      }

   return GPIB.set_serial_read_dropout(ms);
}

U8 GPIB_serial_poll (void)
{
   if (state.error)
      {
      return FALSE;
      }

   return GPIB.serial_poll();
}

C8 *GPIB_query (C8  *string, 
                bool from_board = FALSE)
{
   temp_buffer[0] = 0;

   if (state.error)
      {
      return temp_buffer;
      }

   C8 *result = GPIB.query(string, from_board);

   if (state.error)
      {
      GPIB.disconnect(FALSE); 
      return temp_buffer;
      }

   return result;
}

S32 GPIB_puts (C8  *string)
{
   if (state.error)
      {
      return FALSE;
      }

   return GPIB.puts(string);
}

C8 *GPIB_read_ASC (S32  max_len        = -1, 
                   bool report_timeout =  TRUE, 
                   bool from_board     =  FALSE)
{
   temp_buffer[0] = 0;

   if (state.error)
      {
      return temp_buffer;
      }

   return GPIB.read_ASC(max_len, report_timeout, from_board);
}

C8 *GPIB_read_BIN (S32   max_len        = -1, 
                   bool  report_timeout = TRUE, 
                   bool  from_board     = FALSE, 
                   S32  *actual_len     = NULL)
{
   temp_buffer[0] = 0;

   if (state.error)
      {
      if (actual_len != NULL) *actual_len = 0;
      return temp_buffer;
      }

   return GPIB.read_BIN(max_len, report_timeout, from_board, actual_len);
}

S32 GPIB_flush_receive_buffers (void)
{
   if (state.error)
      {
      return 0;
      }

   return GPIB.flush_receive_buffers();
}

S32 GPIB_auto_read_mode (S32 set_mode = -1)
{
   if (state.error)
      {
      return FALSE;
      }

   return GPIB.auto_read_mode(set_mode);
}

#else

#include "shapi.h"
#include "gpiblib.h"

void __cdecl alert(C8 *caption, C8 *fmt, ...)
{
   state.error_text[sizeof(state.error_text)-1] = 0;
   state.error = TRUE;

   if ((fmt == NULL) || (strlen(fmt) > sizeof(state.error_text)))
      {
      strcpy(state.error_text, "(String missing or too large)");
      }
   else
      {
      va_list ap;

      va_start(ap, 
               fmt);

      _vsnprintf(state.error_text, 
                 sizeof(state.error_text)-1,
                 fmt, 
                 ap);

      va_end  (ap);
      }

   if (caption == NULL)
      {
      MessageBox(NULL, 
                 state.error_text,
                "Error", 
                 MB_OK);
      }
   else
      {
      MessageBox(NULL,
                 state.error_text,
                 caption,
                 MB_OK);
      }
}

void fail(S32 exit_code)
{
   exit(exit_code);
}

void WINAPI GPIB_error(C8 *msg, S32 _ibsta, S32 _iberr, S32 _ibcntl)
{
   alert("Acquisition stopped due to GPIB error",msg);
   exit(1);
}

#endif

//****************************************************************************
//
// Establish connection with instrument and read its controls
//
//****************************************************************************

bool WINAPI GPIB_connect_to_analyzer(S32 address)
{
   if (!state.SA44_mode)      // (uses USB)
      {
      GPIB_connect(address,
                   GPIB_error,
                   0,
                   disable_GPIB_timeouts ? -1 : 10000);

      GPIB_set_serial_read_dropout(2000);

      //
      // Transmit any user setup commands
      //
      // Because we can't read acknowledgements from the Tek 49x analyzers, we need
      // to flush the Prologix board's receive buffers explicitly
      //

      if (startup_cmd[0])
         {
         GPIB_puts(startup_cmd);
         GPIB_flush_receive_buffers();

         startup_cmd[0] = 0;
         }

      if (connect_cmd[0])
         {
         GPIB_puts(connect_cmd);
         GPIB_flush_receive_buffers();
         }
      }

   //
   // Is this an auto-detectable instrument?  If not, special-case it based on the user's
   // selection
   //

   if (state.SA44_mode)
      {
#ifdef _WIN64
      SA_disconnect();
      state.type = SATYPE_INVALID;

      alert("GPIB_connect_to_analyzer() error", "Signal Hound not supported in 64-bit Windows");
      return FALSE;
#else
      state.type = SATYPE_SA44;

      if (state.CTRL_n_divs > 0)
         state.amplitude_levels = state.CTRL_n_divs;
      else
         state.amplitude_levels = 10;

      if (state.CTRL_dB_div > 0)
         state.dB_division = state.CTRL_dB_div;
      else
         state.dB_division = 10;

      strcpy(state.ID_string, "USB Signal Hound");

      if (!state.SH_initialized)
         {
         HCURSOR wait_cursor = LoadCursor(0, IDC_WAIT);
         HCURSOR norm = SetCursor(wait_cursor);

         HOUND_kill_process(TRUE);

         //
         // Set CWD to .exe's path so SHAPI_Initialize() will find Hound2Pipe.exe, then restore it
         // (Not thread-safe!)
         //

         C8 old_path[MAX_PATH] = { 0 };
         GetCurrentDirectory(sizeof(old_path), old_path);

         C8 exe_path[MAX_PATH] = { 0 };

         GetModuleFileName(NULL, exe_path, MAX_PATH);

         C8 *bs = strrchr(exe_path,'\\');

         if (bs != NULL)
            {
            bs[1] = 0;
            }

         SetCurrentDirectory(exe_path);
         S32 err = SHAPI_Initialize();
         SetCurrentDirectory(old_path);

         SetCursor(norm);

         if (err)
            {
            SA_disconnect();
            state.type = SATYPE_INVALID;

            alert("SHAPI_Initialize() error", "Signal Hound not found, code %d\n\n(If the Signal Hound is currently plugged in, try disconnecting and reconnecting it.)", err);
            return FALSE;
            }

         state.SH_initialized = TRUE;
         }
      
      if (state.CTRL_span_Hz >= 0.0)
         {
         if (state.CTRL_span_Hz < 1.0)
            {                         
            SA_disconnect();
            state.type = SATYPE_INVALID;

            alert("Error","Zero span not supported -- must specify a valid nonzero span/div");
            return FALSE;
            }
         }

      DOUBLE ref_lvl  = state.CTRL_RL_dBm;  
      DOUBLE start_Hz = state.CTRL_start_Hz;
      DOUBLE stop_Hz  = state.CTRL_stop_Hz; 
      S32    sens     = state.CTRL_sens;    
      DOUBLE atten_dB = state.CTRL_atten_dB;
      S32    FFT_size = state.CTRL_FFT_size;

      S32    above_150_MHz = (start_Hz > 150E6);
      S32    decim = 1;          // 1 required for fast sweeps
      S32    IF_path = 0;        // 0 required for fast sweeps
      S32    ADC_clock = 0;      // 0 required for fast sweeps

      S32 err = SHAPI_Configure(atten_dB,
                                above_150_MHz,
                                sens,
                                decim,
                                IF_path,
                                ADC_clock);
      if (err)
         {
         SA_disconnect();
         state.type = SATYPE_INVALID;

         alert("SHAPI_Configure() error", "Signal Hound could not be configured, code %d", err);
         return FALSE;
         }

      S32 cnt = SHAPI_GetFastSweepCount(start_Hz,
                                        stop_Hz,
                                        FFT_size);
                            
      S32 slice_cnt = (S32) ((stop_Hz - start_Hz) / 200E3) + 1;
      state.sweep_secs = (40.0 + (1.2 * slice_cnt)) / 1000.0;     // (approx)

      state.RBW_Hz   = SHAPI_GetRBW(FFT_size, decim);
      state.max_dBm  = ref_lvl;
      state.RFATT_dB = atten_dB;
      state.FFT_size = FFT_size;
      state.min_Hz   = start_Hz;
      state.max_Hz   = stop_Hz;
      state.n_trace_points = cnt;

      if ((cnt > MAX_N_PTS) || (cnt < 0))
         {
         SA_disconnect();
         state.type = SATYPE_INVALID;

         alert("Error","Specified parameters result in excessive span buffer size\n(%u points, up to %d allowed)",
            cnt, MAX_N_PTS);
         return FALSE;
         } 

      return TRUE;
#endif
      }
   else if (state.HP8569B_mode)
      {
      state.type = SATYPE_HP8569B;
      state.amplitude_levels = 8;

      strcpy(state.ID_string, "HP 8569B/8570A");

      GPIB_set_EOS_mode(10, FALSE);

      //
      // (We can't force the 8569B to single-sweep mode)
      //
      // Get dB/division scale, and make sure we're in log mode
      //

      state.dB_division = 10000;

      while (state.dB_division == 10000)
         {
         C8 *text = GPIB_query("LG");

         sscanf(text,"%d",&state.dB_division); 

         if (state.dB_division == 0)
            {
            SA_disconnect();
            state.type = SATYPE_INVALID;

            alert("Error","LIN mode not supported -- must select logarithmic vertical display (dB/div)");
            return FALSE;
            }
         }

      //
      // Query HP 8569B reference level
      //

      state.max_dBm = 10000.0;

      while (state.max_dBm == 10000.0)
         {
         C8 *text = GPIB_query("RL");
         sscanf(text,"%lf",&state.max_dBm); 
         }

      //
      // Query HP 8569B frequency limits
      //

      DOUBLE span = -10000.0;

      while (span == -10000.0)
         {
         C8 *text = GPIB_query("SP");

         sscanf(text,"%lf",&span); 

         if (span == -2.0)
            {
            span = 22E9 - 1.7E9;
            }

         if (span < 0.0)
            {
            SA_disconnect();
            state.type = SATYPE_INVALID;

            alert("Error","Must select a valid numeric span (PER DIV or 1.7-22 GHz)");
            return FALSE;
            }
         }

      DOUBLE CF = -FLT_MAX;

      while (CF == -FLT_MAX)
         {
         C8 *text = GPIB_query("CF");
         sscanf(text,"%lf",&CF); 
         }

      state.min_Hz = CF - (span * 5.0);
      state.max_Hz = CF + (span * 5.0);

      //
      // Return success
      //

      state.n_trace_points = 481;
      return TRUE;
      }
   else if (state.HP3585_mode)
      {
      state.type = SATYPE_HP3585;
      state.amplitude_levels = 10;

      strcpy(state.ID_string, "HP 3585A/B");

      GPIB_set_EOS_mode(10, FALSE);

      SA_printf("SV4");       // Disable autocalibration (which prevents end-of-sweep bit from going high)
      SA_printf("FA");        // Force start/stop annotation mode 
      SA_printf("S2");        // Select single-sweep mode

      //
      // Perform "D7T4" query to obtain display annotation with immediate triggering
      // (EOS is set to 10 to separate the strings)
      //

      static C8 annotation[10][64];
      memset(annotation, 0, sizeof(annotation));

      SA_printf("D7T4");

      S32 i,j;
      for (i=0; i < 10; i++)
         {
         C8 *result = GPIB_read_ASC(sizeof(annotation[i])-1);
         assert(result != NULL);

         strcpy(annotation[i], result);
         _strupr(annotation[i]);

         //
         // Remove spaces from strings so numeric values can be 
         // parsed with sscanf()
         //

         for (j=strlen(annotation[i])-1; j >= 0; j--)    
            {
            if (isspace((U8) annotation[i][j]))
               {
               strcpy(&annotation[i][j], &annotation[i][j+1]);
               }
            }
         }

      //
      // Determine HP 3585 reference level and scale
      //

      state.dB_division = 10000;

      sscanf(annotation[2], "%dDB/DIV", &state.dB_division);

      if (state.dB_division == 10000)
         {
         SA_disconnect();
         state.type = SATYPE_INVALID;

         alert("Error","Must select logarithmic vertical display (dB/div)");
         return FALSE;
         }

      state.max_dBm = 10000.0;
      sscanf(annotation[0], "REF%lfDBM", &state.max_dBm);

      //
      // Determine HP 3585 frequency limits
      //

      state.min_Hz = -FLT_MAX;
      state.max_Hz = -FLT_MAX;

      sscanf(annotation[5], "START%lf", &state.min_Hz);
      sscanf(annotation[6], "STOP%lf",  &state.max_Hz);

           if (strstr(annotation[5], "GHZ")) state.min_Hz *= 1.0E9;
      else if (strstr(annotation[5], "MHZ")) state.min_Hz *= 1.0E6;
      else if (strstr(annotation[5], "KHZ")) state.min_Hz *= 1.0E3;
      
           if (strstr(annotation[6], "GHZ")) state.max_Hz *= 1.0E9;
      else if (strstr(annotation[6], "MHZ")) state.max_Hz *= 1.0E6;
      else if (strstr(annotation[6], "KHZ")) state.max_Hz *= 1.0E3;

      //
      // Return success
      //

      state.n_trace_points = 1001;
      return TRUE;
      }
   else if (state.HP856XA_mode)
      {
      state.type = SATYPE_HP8566A_8568A;
      state.amplitude_levels = 10;

      strcpy(state.ID_string, "HP 8566A/8567A/8568A");

      GPIB_set_EOS_mode(10, FALSE);

      SA_printf("S2;");        // Select single-sweep mode
      SA_printf("FA;");        // Force start/stop annotation mode

      //
      // Perform "OT" query to obtain display annotation
      // (EOS is set to 10 to separate the strings)
      //

      static C8 OT_buffer[32][65];
      memset(OT_buffer, 0, sizeof(OT_buffer));

      SA_printf("OT");

      S32 i;
      for (i=0; i < 32; i++)
         {
         C8 *result = GPIB_read_ASC(sizeof(OT_buffer[i])-1);
         assert(result != NULL);

         strcpy(OT_buffer[i], result);
         }

      //
      // Determine HP 8566A/8568A reference level and scale
      //

      state.dB_division = 10000;

      for (i=0; i < 8; i++)      // Skip garbage characters (08 20) appearing before scale text
         {
         C8 *scale_text = &OT_buffer[7][i];

         if (isdigit((U8) *scale_text))
            {
            sscanf(scale_text, "%d dB/", &state.dB_division);
            break;
            }
         }

      if (state.dB_division == 10000)
         {
         SA_disconnect();
         state.type = SATYPE_INVALID;

         alert("Error","Must select logarithmic vertical display (dB/div)");
         return FALSE;
         }

      state.max_dBm = 10000.0;
      sscanf(OT_buffer[6], "REF %lf dBm", &state.max_dBm);

      //
      // Determine HP 8566A/8568A frequency limits
      //

      state.min_Hz = -FLT_MAX;
      state.max_Hz = -FLT_MAX;

      sscanf(OT_buffer[9], "START %lf", &state.min_Hz);
      sscanf(OT_buffer[10], "STOP %lf", &state.max_Hz);

           if (strstr(OT_buffer[9], "GHz")) state.min_Hz *= 1.0E9;
      else if (strstr(OT_buffer[9], "MHz")) state.min_Hz *= 1.0E6;
      else if (strstr(OT_buffer[9], "kHz")) state.min_Hz *= 1.0E3;
      
           if (strstr(OT_buffer[10], "GHz")) state.max_Hz *= 1.0E9;
      else if (strstr(OT_buffer[10], "MHz")) state.max_Hz *= 1.0E6;
      else if (strstr(OT_buffer[10], "kHz")) state.max_Hz *= 1.0E3;

      //
      // Return success
      //

      state.n_trace_points = 1001;
      return TRUE;
      }
   else if (state.TR417X_mode)
      {
      state.type = SATYPE_TR4172_TR4173;
      state.amplitude_levels = 10;

      GPIB_set_EOS_mode(10, TRUE);   // (assume EOIs not used for ASCII reads)

      SA_printf("SRDR");
      GPIB_flush_receive_buffers();    // if we don't do this, the queries below return one string behind (from PN)

      sprintf(state.ID_string,"TR4172/TR4173"); // seems to have been made by at least 3 manufacturers? (T-R, Advantest, Anritsu)

      SA_printf("AW");               // Currently supports only 1001-point mode

      SA_printf("SI");               // Single-sweep mode
      SA_printf("SH7");              // Currently supports only 10 dB/division log scale

      GPIB_flush_receive_buffers();

      state.max_dBm = 10000.0;

      while (state.max_dBm == 10000.0)
         {
         C8 *RL_text = GPIB_query("REOA");
         sscanf(RL_text,"%lf",&state.max_dBm);
         }

      // TODO

      }
   else if (state.R3261_mode)
      {
      GPIB_set_EOS_mode(10, TRUE);          // (Under Prologix, ASCII reads won't terminate on EOI reception, since we never set ++eot_enable and ++eot_char) 

      GPIB_puts("DL0 SI S1 S2 HD0");        // Expect (and return) CR+LF|EOI delimiters, use single-sweep mode, disable SRQs, and clear status byte 
      GPIB_flush_receive_buffers();         // (Needed on R3271A at least)
      
      GPIB_puts("TYP?");                    
      sprintf(state.ID_string,"Advantest %s", GPIB_read_ASC());   // (Reads 'R3361A\r\r\n' under Prologix)
      kill_trailing_whitespace(state.ID_string);

      if ((strstr(state.ID_string,"3261") != NULL) || 
          (strstr(state.ID_string,"3361") != NULL))
         {
         state.type = SATYPE_R3261_R3361;
         }
      else
         {
         state.type = SATYPE_R3265_R3271;
         GPIB_puts("TPC");                   // Select 401-point vertical resolution for compatibility with R3261/R3361
         }

      //
      // Query R3261/R3361 reference level and scale
      //

      state.max_dBm = 10000.0;
      C8 *RL = GPIB_query("RL?");
      sscanf(RL,"%lf", &state.max_dBm);

      S32 DD_result = -1;
      C8 *DD = GPIB_query("DD?");
      sscanf(DD,"%d",&DD_result);

      switch (DD_result)
         {
         case 0:  state.dB_division = 10; break;
         case 1:  state.dB_division = 5;  break;
         case 2:  state.dB_division = 2;  break;
         case 3:  state.dB_division = 1;  break;
         default: state.dB_division = 0;  break;  // (we don't support 0.5 dB/div)
         }
      
      if ((state.max_dBm == 10000.0) || (state.dB_division == 0))
         {
         SA_disconnect();
         state.type = SATYPE_INVALID;

         alert("Error","Must select logarithmic vertical display in dBm (1-10 dB/div), '%s' returned",DD);
         return FALSE;
         }

      if (state.type == SATYPE_R3265_R3271)
         {
         //
         // R3265/R3271 always uses 10 divisions
         //

         state.amplitude_levels = 10;
         }
      else
         {
         //
         // R3261/R3361 support 8/12 division displays at 10 dB/division -- find out
         // which
         //

         state.amplitude_levels = 8;

         if (state.dB_division == 10)
            {
            C8 *DV = GPIB_query("DV?");

            if (DV[0] == '1')
               {
               state.amplitude_levels = 12;
               }
            }
         }

      //
      // Query R3261/R3361 frequency limits
      //

      DOUBLE start = -FLT_MAX;

      while (start == -FLT_MAX)
         {
         C8 *text = GPIB_query("FA?");
         sscanf(text,"%lf",&start); 
         }
      
      DOUBLE stop = -FLT_MAX;

      while (stop == -FLT_MAX)
         {
         C8 *text = GPIB_query("FB?");
         sscanf(text,"%lf",&stop); 
         }

      state.min_Hz = start;
      state.max_Hz = stop;

      //
      // Binary trace queries should not be terminated in CR/LF, only EOI
      //
      // (Note, however, that ++spoll results will still come back with CR/LFs added
      // by the Prologix firmware)
      //

      SA_printf("DL2");   

      GPIB_auto_read_mode(FALSE);

      state.n_trace_points = 701;
      return TRUE;
      }

   //
   // Otherwise, send ID? query to instrument and kill any trailing whitespace
   //
   // This process is somewhat complicated by the fact that CR/LF/0xFF
   // acknowledgements from previous commands sent to a Tek 490/2750-series 
   // analyzer may not have been read due to network or bus latency in
   // excess of the GPIBLIB layer's 100-millisecond buffer-flush time.
   //
   // As a heuristic, we'll reject any ID? results less than 4 bytes long,
   // retrying for 10 seconds or until we receive a valid-looking ID string
   //

   GPIB_set_EOS_mode(10);

   S32 start_time = timeGetTime();

   do
      {
      if ((timeGetTime() - start_time) > 5000)
         {
         GPIB_disconnect();                  
         alert("Error","No valid response to ID? or *IDN? query");
         return FALSE;
         }

      if (state.SCPI_mode || state.HP358XA_mode || state.Advantest_mode)   // TODO: -advantest should just set SCPI mode?
         {                          
         if (state.ID_string[0] == 0)  
            {
            strcpy(state.ID_string, GPIB_query("*IDN?"));
            }
         }
      else
         {
         if (state.ID_string[0] == 0)
            {
            strcpy(state.ID_string, GPIB_query("ID?"));
            }
         }

      for (S32 i=strlen(state.ID_string)-1; i >= 0; i--)
         {
         if (!isspace((U8) state.ID_string[i])) break;
         state.ID_string[i] = 0;
         }
      }
   while (strlen(state.ID_string) <= 3);

   if ((!_strnicmp(state.ID_string,"ID TEK/49",9))   ||
       (!_strnicmp(state.ID_string,"ID TEK/2AP",10)) ||
       (!_strnicmp(state.ID_string,"ID TEK/5AP",10)) ||
       (!_strnicmp(state.ID_string,"ID TEK/275",10)))
      {
      state.type = SATYPE_TEK_490P;
      state.amplitude_levels = 8;

      //
      // Set up Tek 490 required parameters
      //

      state.n_trace_points = state.hi_speed_acq ? 500 : 1000;

      SA_printf("SIGSWP;");           // Arm single-sweep mode

      //
      // Get dB/division scale, and make sure we're in log mode
      //

      state.dB_division = 10000;

      while (state.dB_division == 10000)
         {
         C8 *text = GPIB_query("VRTDSP?");

         if (strstr(text,"LIN") != NULL)
            {
            SA_disconnect();
            state.type = SATYPE_INVALID;

            alert("Error","LIN mode not supported -- must select logarithmic vertical display (dB/div)");
            return FALSE;
            }

         sscanf(text,"VRTDSP LOG:%d",&state.dB_division); 
         }

      //
      // Query Tek 490 reference level
      //

      SA_printf("RLUNIT DBM;");       // Units in dBm (not supported on 1984-level instruments)

      state.max_dBm = 10000.0;

      while (state.max_dBm == 10000.0)
         {
         C8 *text = GPIB_query("REFLVL?");
         sscanf(text,"REFLVL %lf",&state.max_dBm); 
         }

      //
      // Query Tek 490 center frequency (which may differ
      // from the midspan value in MAX SPAN mode)
      //

      state.CF_Hz = -FLT_MAX;

      while (state.CF_Hz == -FLT_MAX)
         {
         C8 *text = GPIB_query("FREQ?");
         sscanf(text,"FREQ %lf", &state.CF_Hz);
         }

      //
      // Query Tek 490 frequency limits
      //

      state.min_Hz = -FLT_MAX;
      state.max_Hz = -FLT_MAX;

      DOUBLE span = -10000.0;

      while (span == -10000.0)
         {
         C8 *text = GPIB_query("SPAN?");

         if (strstr(text,"SPAN MAX") != NULL) 
            {
            span = FLT_MAX;
            SA_fetch_trace();               // Get min/max frequency from trace header in MAX SPAN mode...
            break;                          // rather than CF +/- (SPAN/2) calculation below
            }

         sscanf(text,"SPAN %lf",&span); 
         }

      if (span < 1.0)                       // Firmware bug on all 49x versions returns meaningless CURVE data in zero-span mode
         {                                  // (also ~30 dB too low in bands > 60 GHz?)
         SA_disconnect();
         state.type = SATYPE_INVALID;

         alert("Error","ZERO SPAN not supported -- must select a valid nonzero span/div");
         return FALSE;
         }

      if (state.min_Hz == -FLT_MAX)         // i.e., if not determined by SA_fetch_trace() call above
         {
         state.min_Hz = state.CF_Hz - (span * 5.0);
         state.max_Hz = state.CF_Hz + (span * 5.0);
         }

      //
      // Query other Tek 490-series parameters
      //

      state.RBW_Hz = -1.0;

      while (state.RBW_Hz == -1.0)
         {
         C8 *text = GPIB_query("RESBW?");
         sscanf(text,"RESBW %lf",&state.RBW_Hz);
         }

      C8 *vflt = GPIB_query("VIDFLT?");

      state.VBW_Hz = -1.0;

      if (strstr(vflt,"WIDE") != NULL)   
         {
              if (state.RBW_Hz >= 300000) state.VBW_Hz = 30000.0;
         else if (state.RBW_Hz >= 30000)  state.VBW_Hz = 3000.0;
         else if (state.RBW_Hz >= 3000)   state.VBW_Hz = 300.0;
         else if (state.RBW_Hz >= 300)    state.VBW_Hz = 30.0;
         else                             state.VBW_Hz = 3.0;
         }
      else if (strstr(vflt,"NARROW") != NULL) 
         {
              if (state.RBW_Hz >= 300000) state.VBW_Hz = 3000.0;
         else if (state.RBW_Hz >= 30000)  state.VBW_Hz = 300.0;
         else if (state.RBW_Hz >= 3000)   state.VBW_Hz = 30.0;
         else if (state.RBW_Hz >= 300)    state.VBW_Hz = 3.0;
         else                             state.VBW_Hz = 0.3F;
         }

      C8 *stim = GPIB_query("TIME?");

      DOUBLE tval = -1.0;
      sscanf(stim, "TIME %lf", &tval);

      state.sweep_secs = -1.0;

      if (tval > 0.0) 
         {
         state.sweep_secs = tval * 10.0;
         }
      }
   else if ((!_strnicmp(state.ID_string,"ID TEK/271",10)))
      {
      state.type = SATYPE_TEK_271X;
      state.amplitude_levels = 8;
      state.n_trace_points = 512;

      //
      // Set up Tek 271X required parameters
      //

      SA_printf("SIGSWP;");           // Arm single-sweep mode

      //
      // Get dB/division scale, and make sure we're in log mode
      //

      state.dB_division = 10000;

      while (state.dB_division == 10000)
         {
         C8 *text = GPIB_query("VRTDSP?");

         if ((strstr(text,"LIN") != NULL) ||
             (strstr(text,"FM")  != NULL) ||
             (strstr(text,"EXT") != NULL))
            {
            SA_disconnect();
            state.type = SATYPE_INVALID;

            alert("Error","Must select logarithmic vertical display (dB/div)");
            return FALSE;
            }

         sscanf(text,"VRTDSP LOG:%d",&state.dB_division); 
         }

      //
      // Query Tek 271X reference level
      //

      SA_printf("RLUNIT DBM;");       // Units in dBm 

      state.max_dBm = 10000.0;

      while (state.max_dBm == 10000.0)
         {
         C8 *text = GPIB_query("REFLVL?");
         sscanf(text,"REFLVL %lf",&state.max_dBm); 
         }

      //
      // Query Tek 271X frequency limits
      //

      DOUBLE span = -10000.0;

      while (span == -10000.0)
         {
         C8 *text = GPIB_query("SPAN?");

         if (strstr(text,"SPAN MAX") != NULL)
            {
            SA_fetch_trace();               // Get min/max frequency from trace header in MAX SPAN mode...
            return TRUE;                    // rather than CF +/- (SPAN/2) calculation below
            }

         sscanf(text,"SPAN %lf",&span); 
         }

      DOUBLE CF = -FLT_MAX;

      while (CF == -FLT_MAX)
         {
         C8 *text = GPIB_query("FREQ?");
         sscanf(text,"FREQ %lf",&CF); 
         }

      state.min_Hz = CF - (span * 5.0);
      state.max_Hz = CF + (span * 5.0);
      }
   else if ((!_strnicmp(state.ID_string,"HP8566",6)) ||
            (!_strnicmp(state.ID_string,"HP8576",6)) ||
            (!_strnicmp(state.ID_string,"HP8568",6)))
      {
      state.type = SATYPE_HP8566B_8568B;
      state.amplitude_levels = 10;

      //
      // Set up HP 8566B/8568B required parameters
      //

      SA_printf("SNGLS;");     // Select single-sweep mode

      //
      // Query HP 8566B/8568B reference level and scale
      //

      state.dB_division = 10000;

      while (state.dB_division == 10000)
         {
         C8 *text = GPIB_query("LG?");
         sscanf(text,"%d",&state.dB_division); 
         }

      if ((state.dB_division < 1) || (state.dB_division > 10))
         {
         SA_disconnect();
         state.type = SATYPE_INVALID;

         alert("Error","Must select logarithmic vertical display (1-10 dB/div)");
         return FALSE;
         }

      state.max_dBm = 10000.0;

      while (state.max_dBm == 10000.0)
         {
         C8 *text = GPIB_query("RL?");
         sscanf(text,"%lf",&state.max_dBm); 
         }

      //
      // Query HP 8566B/8568B frequency limits
      //

      DOUBLE start = -FLT_MAX;

      while (start == -FLT_MAX)
         {
         C8 *text = GPIB_query("FA?");
         sscanf(text,"%lf",&start); 
         }
      
      DOUBLE stop = -FLT_MAX;

      while (stop == -FLT_MAX)
         {
         C8 *text = GPIB_query("FB?");
         sscanf(text,"%lf",&stop); 
         }

      state.min_Hz = start;
      state.max_Hz = stop;

      state.n_trace_points = 1001;

      //
      // Query other HP 8566B/8568B parameters
      //

      state.RBW_Hz     = -1.0;
      state.VBW_Hz     = -1.0;
      state.vid_avgs   = -1;
      state.sweep_secs = -1.0;
      state.RFATT_dB   = -99999.0;

      while (state.RBW_Hz < 0.0)
         {
         C8 *text = GPIB_query("RB?");
         sscanf(text,"%lf",&state.RBW_Hz); 
         }

      while (state.VBW_Hz < 0.0)
         {
         C8 *text = GPIB_query("VB?");
         sscanf(text,"%lf",&state.VBW_Hz); 
         }

      while (state.vid_avgs < 0)
         {
         C8 *text = GPIB_query("VAVG?");

         _strlwr(text);

         if (strstr(text,"off") != NULL)  // (8566B reports 0 when VAVG turned off)
            state.vid_avgs = 0;           
         else
            sscanf(text,"%d",&state.vid_avgs);
         }

      while (state.sweep_secs < 0.0)
         {
         C8 *text = GPIB_query("ST?");
         sscanf(text,"%lf",&state.sweep_secs);
         }

      while (state.RFATT_dB < -10000.0)
         {
         C8 *text = GPIB_query("AT?");
         sscanf(text,"%lf",&state.RFATT_dB);
         }
      }
   else if ((!_strnicmp(state.ID_string,"TEK/278",7)))
      {
      state.type = SATYPE_TEK_278X;
      state.amplitude_levels = 10;

      //
      // Set up Tek 2782/2784 required parameters
      //

      SA_printf("SNGLS;");     // Select single-sweep mode

      //
      // Query Tek 278x reference level and scale
      //

      state.dB_division = 10000;

      while (state.dB_division == 10000)
         {
         C8 *text = GPIB_query("LG?");
         sscanf(text,"%d",&state.dB_division); 
         }

      if ((state.dB_division < 1) || (state.dB_division > 10))
         {
         SA_disconnect();
         state.type = SATYPE_INVALID;

         alert("Error","Must select logarithmic vertical display (1-10 dB/div)");
         return FALSE;
         }

      state.max_dBm = 10000.0;

      while (state.max_dBm == 10000.0)
         {
         C8 *text = GPIB_query("RL?");
         sscanf(text,"%lf",&state.max_dBm); 
         }

      //
      // Query Tek 278x frequency limits
      //

      DOUBLE start = -FLT_MAX;

      while (start == -FLT_MAX)
         {
         C8 *text = GPIB_query("FA?");
         sscanf(text,"%lf",&start); 
         }
      
      DOUBLE stop = -FLT_MAX;

      while (stop == -FLT_MAX)
         {
         C8 *text = GPIB_query("FB?");
         sscanf(text,"%lf",&stop); 
         }

      state.min_Hz = start;
      state.max_Hz = stop;

      state.n_trace_points = 1001;
      }
   else if (!_strnicmp(state.ID_string,"HP856",5))
      {
      state.type = SATYPE_HP8560;
      state.amplitude_levels = 10;

      //
      // Set up HP 8560 required parameters
      //

      SA_printf("SNGLS;");     // Select single-sweep mode

      //
      // Query HP 8560 reference level and scale
      //

      state.dB_division = 10000;

      while (state.dB_division == 10000)
         {
         C8 *text = GPIB_query("LG?");
         sscanf(text,"%d",&state.dB_division); 
         }

      if ((state.dB_division < 1) || (state.dB_division > 10))
         {
         SA_disconnect();
         state.type = SATYPE_INVALID;

         alert("Error","Must select logarithmic vertical display (1-10 dB/div)");
         return FALSE;
         }

      state.max_dBm = 10000.0;

      while (state.max_dBm == 10000.0)
         {
         C8 *text = GPIB_query("RL?");
         sscanf(text,"%lf",&state.max_dBm); 
         }

      //
      // Query HP 8560 frequency limits
      //

      DOUBLE start = -FLT_MAX;

      while (start == -FLT_MAX)
         {
         C8 *text = GPIB_query("FA?");
         sscanf(text,"%lf",&start); 
         }
      
      DOUBLE stop = -FLT_MAX;

      while (stop == -FLT_MAX)
         {
         C8 *text = GPIB_query("FB?");
         sscanf(text,"%lf",&stop); 
         }

      state.min_Hz = start;
      state.max_Hz = stop;

      state.n_trace_points = 601;

      //
      // Query other HP 8560 parameters
      //

      state.RBW_Hz     = -1.0;
      state.VBW_Hz     = -1.0;
      state.vid_avgs   = -1;
      state.sweep_secs = -1.0;
      state.RFATT_dB   = -99999.0;

      while (state.RBW_Hz < 0.0)
         {
         C8 *text = GPIB_query("RB?");
         sscanf(text,"%lf",&state.RBW_Hz); 
         }

      while (state.VBW_Hz < 0.0)
         {
         C8 *text = GPIB_query("VB?");
         sscanf(text,"%lf",&state.VBW_Hz); 
         }

      while (state.vid_avgs < 0)
         {
         C8 *text = GPIB_query("VAVG?");

         _strlwr(text);

         if (strstr(text,"off") != NULL)     // (8561E always reports 100 after PRESET, even
            state.vid_avgs = 0;              // with vid averaging turned off)
         else
            sscanf(text,"%d",&state.vid_avgs);
         }

      while (state.sweep_secs < 0.0)
         {
         C8 *text = GPIB_query("ST?");
         sscanf(text,"%lf",&state.sweep_secs);
         }

      while (state.RFATT_dB < -10000.0)
         {
         C8 *text = GPIB_query("AT?");
         sscanf(text,"%lf",&state.RFATT_dB);
         }
      }
   else if (!_strnicmp(state.ID_string,"HP859",5))
      {
      state.type = SATYPE_HP8590;
      state.amplitude_levels = 8;

      //
      // Set up HP 8590 required parameters
      //

      SA_printf("SNGLS;");     // Select single-sweep mode

      //
      // Query HP 8590 reference level and scale
      // (8590Es support fractional dB/division settings, but we don't)
      //

      state.dB_division = 10000;

      while (state.dB_division == 10000)
         {
         C8 *text = GPIB_query("LG?");
         sscanf(text,"%d",&state.dB_division); 
         }

      if ((state.dB_division < 1) || (state.dB_division > 20))
         {
         SA_disconnect();
         state.type = SATYPE_INVALID;

         alert("Error","Must select integer log vertical display (1-20 dB/div)");
         return FALSE;
         }

      state.max_dBm = 10000.0;

      while (state.max_dBm == 10000.0)
         {
         C8 *text = GPIB_query("RL?");
         sscanf(text,"%lf",&state.max_dBm); 
         }

      //
      // Query HP 8590 frequency limits
      //

      DOUBLE start = -FLT_MAX;

      while (start == -FLT_MAX)
         {
         C8 *text = GPIB_query("FA?");
         sscanf(text,"%lf",&start); 
         }
      
      DOUBLE stop = -FLT_MAX;

      while (stop == -FLT_MAX)
         {
         C8 *text = GPIB_query("FB?");
         sscanf(text,"%lf",&stop); 
         }

      state.min_Hz = start;
      state.max_Hz = stop;

      state.n_trace_points = 401;
      }
   else if ((!_strnicmp(state.ID_string,"HP70",4)) ||
            (!_strnicmp(state.ID_string,"HP71",4)))
      {
      state.type = SATYPE_HP70000;
      state.amplitude_levels = 10;

      //
      // Set up HP 70000-series required parameters
      //

      SA_printf("SNGLS;");     // Select single-sweep mode

      //
      // Query HP 70000 reference level and scale
      //

      state.dB_division = 10000;

      while (state.dB_division == 10000)
         {
         C8 *text = GPIB_query("LG?");
         sscanf(text,"%d",&state.dB_division); 
         }

      if ((state.dB_division < 1) || (state.dB_division > 10))
         {
         SA_disconnect();
         state.type = SATYPE_INVALID;

         alert("Error","Must select logarithmic vertical display (1-10 dB/div)");
         return FALSE;
         }

      state.max_dBm = 10000.0;

      while (state.max_dBm == 10000.0)
         {
         C8 *text = GPIB_query("RL?");
         sscanf(text,"%lf",&state.max_dBm); 
         }

      //
      // Query HP 70000 frequency limits
      //

      DOUBLE start = -FLT_MAX;

      while (start == -FLT_MAX)
         {
         C8 *text = GPIB_query("FA?");
         sscanf(text,"%lf",&start); 
         }
      
      DOUBLE stop = -FLT_MAX;

      while (stop == -FLT_MAX)
         {
         C8 *text = GPIB_query("FB?");
         sscanf(text,"%lf",&stop); 
         }

      state.min_Hz = start;
      state.max_Hz = stop;

      //
      // Query other HP 70000 parameters
      //

      state.RBW_Hz     = -1.0;
      state.VBW_Hz     = -1.0;
      state.vid_avgs   = -1;
      state.sweep_secs = -1.0;
      state.RFATT_dB   = -99999.0;

      while (state.RBW_Hz < 0.0)
         {
         C8 *text = GPIB_query("RB?");
         sscanf(text,"%lf",&state.RBW_Hz); 
         }

      while (state.VBW_Hz < 0.0)
         {
         C8 *text = GPIB_query("VB?");
         sscanf(text,"%lf",&state.VBW_Hz); 
         }

      while (state.vid_avgs < 0)
         {
         C8 *text = GPIB_query("VAVG?");

         _strlwr(text);

         if (strstr(text,"off") != NULL)  
            state.vid_avgs = 0;           
         else
            sscanf(text,"%d",&state.vid_avgs);
         }

      while (state.sweep_secs < 0.0)
         {
         C8 *text = GPIB_query("ST?");
         sscanf(text,"%lf",&state.sweep_secs);
         }

      while (state.RFATT_dB < -10000.0)
         {
         C8 *text = GPIB_query("AT?");
         sscanf(text,"%lf",&state.RFATT_dB);
         }

      state.n_trace_points = 800;
      }
   else if ((!_strnicmp(state.ID_string,"ADVANTEST,R3264",15)) || 
            (!_strnicmp(state.ID_string,"ADVANTEST,R3267",15)) || 
            (!_strnicmp(state.ID_string,"ADVANTEST,R3273",15))) 
      {
      state.type = SATYPE_R3267_SERIES;
      state.amplitude_levels = 10;

      //
      // Set up R3267 required parameters
      //

      SA_printf("DL0;SI");        // Select CR+LF+EOI query termination and single-sweep mode
      GPIB_flush_receive_buffers();

      //
      // Query R3267 reference level and scale
      //
      // Note that it's possible to set this analyzer to LIN mode without
      // being able to detect this condition via GPIB queries -- DD and 
      // AUNITS can still return 0 after LL1 is executed! 
      //

      S32 AUNITS = -1;

      while (AUNITS == -1)
         {
         C8 *text = GPIB_query("AUNITS?");
         sscanf(text,"%d",&AUNITS);
         }

      state.dB_division = 10000;

      while (state.dB_division == 10000)
         {
         S32 DD_result = -1;

         C8 *text = GPIB_query("DD?");
         sscanf(text,"%d",&DD_result);

         switch (DD_result)
            {
            case 0:  state.dB_division = 10; break;
            case 1:  state.dB_division = 5;  break;
            case 2:  state.dB_division = 2;  break;
            case 3:  state.dB_division = 1;  break;
            default: state.dB_division = 0;  break;  // (we don't support 0.5 dB/div)
            }
         }

      if ((state.dB_division == 0) || (AUNITS != 0))
         {
         SA_disconnect();
         state.type = SATYPE_INVALID;

         alert("Error","Must select logarithmic vertical display in dBm (1-10 dB/div)");
         return FALSE;
         }

      state.max_dBm = 10000.0;

      while (state.max_dBm == 10000.0)
         {
         C8 *text = GPIB_query("RL?");
         sscanf(text,"%lf",&state.max_dBm); 
         }

      //
      // Query R3267 frequency limits
      //

      DOUBLE start = -FLT_MAX;

      while (start == -FLT_MAX)
         {
         C8 *text = GPIB_query("FA?");
         sscanf(text,"%lf",&start); 
         }
      
      DOUBLE stop = -FLT_MAX;

      while (stop == -FLT_MAX)
         {
         C8 *text = GPIB_query("FB?");
         sscanf(text,"%lf",&stop); 
         }

      state.min_Hz = start;
      state.max_Hz = stop;

      //
      // Log current # of points per trace, and force update
      // before first sweep
      //
      // (R3267 can be set to either 501 or 1001 points)
      //

      S32 precision = -1;

      while (precision == -1)
         {
         C8 *text = GPIB_query("TP?");
         sscanf(text,"%d",&precision);
         }

      orig_n_points = (precision != 0) ? 1001 : 501;
      cur_n_points  = -1;

      state.n_trace_points = state.hi_speed_acq ? 501 : 1001;

      //
      // Binary trace queries should not be terminated in CR/LF, only EOI
      //

      SA_printf("DL2");
      GPIB_flush_receive_buffers();
      }
   else if ((    strstr(state.ID_string,"ADVANTEST,R3477") != NULL) || 
            (!_strnicmp(state.ID_string,"ADVANTEST,R3463",15))      || 
            (!_strnicmp(state.ID_string,"ADVANTEST,R3465",15))      || 
            (!_strnicmp(state.ID_string,"ADVANTEST,R3263",15))      ||
            (!_strnicmp(state.ID_string,"ADVANTEST,R3272",15))      ||
            (!_strnicmp(state.ID_string,"ADVANTEST,R3131",15))      || 
            (!_strnicmp(state.ID_string,"ADVANTEST,R3132",15))      || 
            (!_strnicmp(state.ID_string,"ADVANTEST,R3162",15))      ||
            (!_strnicmp(state.ID_string,"ADVANTEST,R3172",15))      ||
            (!_strnicmp(state.ID_string,"ADVANTEST,R3182",15))) 
      {
      if ((!_strnicmp(state.ID_string,"ADVANTEST,R3132",15)) || 
          (!_strnicmp(state.ID_string,"ADVANTEST,R3162",15)) ||
          (!_strnicmp(state.ID_string,"ADVANTEST,R3172",15)) ||
          (!_strnicmp(state.ID_string,"ADVANTEST,R3182",15))) 
         {
         state.type = SATYPE_R3132_SERIES;
         }
      else if (!_strnicmp(state.ID_string,"ADVANTEST,R3131",15))
         {
         state.type = SATYPE_R3131;
         }
      else
         {
         state.type = SATYPE_R3465_SERIES;
         }

      state.amplitude_levels = (state.type == SATYPE_R3131) ? 8 : 10;

      //
      // Set up R3465/R3131/R3132 required parameters
      //

      SA_printf("DL0;SI");        // Select CR+LF+EOI query termination and single-sweep mode
      GPIB_flush_receive_buffers();

      //
      // Query R3465/R3131/R3132 reference level and scale
      //

      S32 AUNITS = -1;

      while (AUNITS == -1)
         {
         C8 *text = GPIB_query("AUNITS?");
         sscanf(text,"%d",&AUNITS);
         }

      state.dB_division = 10000;

      while (state.dB_division == 10000)
         {
         S32 DD_result = -1;

         C8 *text = GPIB_query("DD?");
         sscanf(text,"%d",&DD_result);

         switch (DD_result)
            {
            case 0:  state.dB_division = 10; break;
            case 1:  state.dB_division = 5;  break;
            case 2:  state.dB_division = 2;  break;
            case 3:  state.dB_division = 1;  break;
            default: state.dB_division = 0;  break;  // (we don't support 0.5 dB/div)
            }
         }

      if ((state.dB_division == 0) || (AUNITS != 0))
         {
         SA_disconnect();
         state.type = SATYPE_INVALID;

         alert("Error","Must select logarithmic vertical display in dBm (1-10 dB/div)");
         return FALSE;
         }

      state.max_dBm = 10000.0;

      while (state.max_dBm == 10000.0)
         {
         C8 *text = GPIB_query("RL?");
         sscanf(text,"%lf",&state.max_dBm); 
         }

      //
      // Query R3465/R3131/R3132 frequency limits
      //

      DOUBLE start = -FLT_MAX;

      while (start == -FLT_MAX)
         {
         C8 *text = GPIB_query("FA?");
         sscanf(text,"%lf",&start); 
         }
      
      DOUBLE stop = -FLT_MAX;

      while (stop == -FLT_MAX)
         {
         C8 *text = GPIB_query("FB?");
         sscanf(text,"%lf",&stop); 
         }

      state.min_Hz = start;
      state.max_Hz = stop;

      //
      // Force TPS/TPL update during first sweep period
      //
      // R3465/R3132 can be set to either 501 or 1001 points, but on the R3465 family
      // there's no TP? query; the default is 1001 points.  R3131 supports 501 points only
      //

      orig_n_points = -1;
      cur_n_points  = -1;

      state.n_trace_points = (state.hi_speed_acq || state.type == SATYPE_R3131) ? 501 : 1001;

      //
      // Binary trace queries should not be terminated in CR/LF, only EOI
      //

      SA_printf("DL2");
      GPIB_flush_receive_buffers();
      }
   else if (strstr(state.ID_string,"MS860") != NULL)      
      {
      state.amplitude_levels = 10;
      state.type = SATYPE_MS8604A;

      GPIB_query("*SRE 0;*CLS;*OPC?");

      if (strstr(GPIB_query("PNLMD?"),"SPECT") == NULL)
         {
         SA_disconnect();
         state.type = SATYPE_INVALID;

         alert("Error","Instrument does not appear to be in spectrum analyzer mode");
         return FALSE;
         }

      GPIB_query("SNGLS;*OPC?");
      
      //
      // Query Anritsu MS8604A parameters
      //

      state.dB_division = 10000;
      state.RBW_Hz      = -1.0;
      state.VBW_Hz      = -1.0;
      state.sweep_secs  = -1.0;
      state.RFATT_dB    = -99999.0;

      while (state.dB_division == 10000)
         {
         C8 *text = GPIB_query("LG?");
         sscanf(text,"%d",&state.dB_division); 
         }

      if ((state.dB_division < 1) || (state.dB_division > 10))
         {
         SA_disconnect();
         state.type = SATYPE_INVALID;

         alert("Error","Must select logarithmic vertical display (1-10 dB/div)");
         return FALSE;
         }

      state.max_dBm = 10000.0;

      while (state.max_dBm == 10000.0)
         {
         C8 *text = GPIB_query("RL?");
         sscanf(text,"%lf",&state.max_dBm); 
         }

      while (state.RBW_Hz < 0.0)
         {
         C8 *text = GPIB_query("RB?");
         sscanf(text,"%lf",&state.RBW_Hz); 
         }

      while (state.VBW_Hz < 0.0)
         {
         C8 *text = GPIB_query("VB?");
         sscanf(text,"%lf",&state.VBW_Hz); 
         }

      while (state.sweep_secs < 0.0)
         {
         C8 *text = GPIB_query("ST?");
         sscanf(text,"%lf",&state.sweep_secs);
         state.sweep_secs /= 1000000.0;
         }

      while (state.RFATT_dB < -10000.0)
         {
         C8 *text = GPIB_query("AT?");
         sscanf(text,"%lf",&state.RFATT_dB);
         }

      DOUBLE start = -FLT_MAX;

      while (start == -FLT_MAX)
         {
         C8 *text = GPIB_query("FA?");
         sscanf(text,"%lf",&start); 
         }
      
      DOUBLE stop = -FLT_MAX;

      while (stop == -FLT_MAX)
         {
         C8 *text = GPIB_query("FB?");
         sscanf(text,"%lf",&stop); 
         }

      state.min_Hz = start;
      state.max_Hz = stop;

      //
      // MS8604A can be set to either 501 or 1002 points
      //

      if (strstr(GPIB_query("DPOINT?"),"DOUBLE") != NULL)
         orig_n_points = 1002;
      else
         orig_n_points = 501;

      cur_n_points = -1;

      state.n_trace_points = state.hi_speed_acq ? 501 : 1002;

      //
      // Binary trace queries are terminated with a single LF
      //

      SA_printf("TRM 0;BIN 1");
      GPIB_flush_receive_buffers();
      }
   else if ((strstr(state.ID_string,"MS265") != NULL) ||
            (strstr(state.ID_string,"MS266") != NULL))
      {
      state.amplitude_levels = 10;
      state.type = SATYPE_MS265X_MS266X;


      GPIB_query("SNGLS;*OPC?");
      
      //
      // Query Anritsu MS2650/MS2660 parameters
      //

      state.dB_division = 10000;
      state.RBW_Hz      = -1.0;
      state.VBW_Hz      = -1.0;
      state.sweep_secs  = -1.0;
      state.RFATT_dB    = -99999.0;

      while (state.dB_division == 10000)
         {
         C8 *text = GPIB_query("LG?");
         sscanf(text,"%d",&state.dB_division); 
         }

      if ((state.dB_division < 1) || (state.dB_division > 10))
         {
         SA_disconnect();
         state.type = SATYPE_INVALID;

         alert("Error","Must select logarithmic vertical display (1-10 dB/div)");
         return FALSE;
         }

      state.max_dBm = 10000.0;

      while (state.max_dBm == 10000.0)
         {
         C8 *text = GPIB_query("RL?");
         sscanf(text,"%lf",&state.max_dBm); 
         }

      while (state.RBW_Hz < 0.0)
         {
         C8 *text = GPIB_query("RB?");
         sscanf(text,"%lf",&state.RBW_Hz); 
         }

      while (state.VBW_Hz < 0.0)
         {
         C8 *text = GPIB_query("VB?");
         sscanf(text,"%lf",&state.VBW_Hz); 
         }

      while (state.sweep_secs < 0.0)
         {
         C8 *text = GPIB_query("ST?");
         sscanf(text,"%lf",&state.sweep_secs);
         state.sweep_secs /= 1000000.0;
         }

      while (state.RFATT_dB < -10000.0)
         {
         C8 *text = GPIB_query("AT?");
         sscanf(text,"%lf",&state.RFATT_dB);
         }

      DOUBLE start = -FLT_MAX;

      while (start == -FLT_MAX)
         {
         C8 *text = GPIB_query("FA?");
         sscanf(text,"%lf",&start); 
         }
      
      DOUBLE stop = -FLT_MAX;

      while (stop == -FLT_MAX)
         {
         C8 *text = GPIB_query("FB?");
         sscanf(text,"%lf",&stop); 
         }

      state.min_Hz = start;
      state.max_Hz = stop;

      state.n_trace_points = 501;

      //
      // Binary trace queries are terminated with a single LF
      //

      SA_printf("TRM 0;BIN 1");
      GPIB_flush_receive_buffers();
      }
   else if (state.SCPI_mode                      ||
            state.HP358XA_mode                   ||
           (!_strnicmp(state.ID_string,"E44",3)) ||
           (!_strnicmp(state.ID_string,"P44",3)) ||
           (!_strnicmp(state.ID_string,"HP358",5)))
      {
      state.amplitude_levels = 10;

      //
      // Set up SCPI required parameters
      //
      // HP 3588A/3589A is not 100% SCPI-compliant (or even TMSL-compliant), so we
      // need to identify it separately.  Likewise, the R&S FSE-series analyzers are not
      // identical in operation to the Agilent E4400 series, so a distinction must be
      // made.  The same is true of the specialized E4406A transmitter tester.
      //
      // (Use of *OPC? query at the end of every non-query command string eliminates unterminated-query
      // warning messages seen with Prologix boards)
      //

      if ((strstr(state.ID_string,"3588A") != NULL) ||
          (strstr(state.ID_string,"3589A") != NULL))
         {
         GPIB_query("*SRE 0;*CLS;*OPC?"); 
         state.type = SATYPE_HP358XA;
         }
      else if ((strstr(state.ID_string,"FSIQ") != NULL) ||
               (strstr(state.ID_string,"FSEA") != NULL) || 
               (strstr(state.ID_string,"FSEB") != NULL) || 
               (strstr(state.ID_string,"FSEM") != NULL) ||
               (strstr(state.ID_string,"FSEK") != NULL))
         {
         GPIB_query("*SRE 0;*CLS;:INIT:CONT 0;*OPC?"); 
         state.type = SATYPE_FSE;
         }
      else if (strstr(state.ID_string,"FSU") != NULL)
         {
         GPIB_query("*SRE 0;*CLS;:INIT:CONT 0;*OPC?"); 
         state.type = SATYPE_FSU;
         }
      else if ((strstr(state.ID_string,"FSP") != NULL))
         {
         GPIB_query("*SRE 0;*CLS;:INIT:CONT 0;*OPC?"); 
         state.type = SATYPE_FSP;
         }
      else if (strstr(state.ID_string,"N99") != NULL)
         {
         GPIB_query("*SRE 0;*CLS;:INIT:CONT 0;*OPC?"); 
         state.type = SATYPE_N9900;
         }
      else if (strstr(state.ID_string,"E4406") != NULL)
         {
         GPIB_query("*SRE 0;*CLS;:INIT:CONT 0;*OPC?");   

         if (strstr(GPIB_query(":INST:SEL?"),"BASIC") == NULL)
            {
            SA_disconnect();
            state.type = SATYPE_INVALID;

            alert("Error","Instrument does not appear to be in BASIC spectrum analyzer mode");
            return FALSE;
            }

         state.VSA_orig_display_state = (GPIB_query(":DISP:ENAB?")          [0] != '0');
         state.VSA_orig_avg_state     = (GPIB_query(":SENS:SPEC:AVER:STAT?")[0] != '0');

         state.VSA_display_state = state.VSA_orig_display_state;

         GPIB_query(":SENS:SPEC:AVER:STAT 0;*OPC?");     // Disable averaging
         GPIB_query(":DISP:FORM:ZOOM1;*OPC?");           // Zoom to spectrum analyzer display window
         state.type = SATYPE_E4406A;
         }
      else
         {
         GPIB_query("*SRE 0;*CLS;:INIT:CONT 0;*OPC?"); 
         state.type = SATYPE_SCPI;
         }

      state.dB_division = 10000;
      state.max_dBm = 10000.0;

      state.min_Hz = -FLT_MAX;
      state.max_Hz = -FLT_MAX;

      if (state.type == SATYPE_E4406A)
         {
         //
         // Query E4406A VSA reference level and scale
         //

#if 0
         GPIB_query(":FORM:DATA ASC;:INIT:IMM;*WAI;*OPC?");
         C8 *text = GPIB_query("FETCH:SPEC1?");

         DOUBLE FFT_peak_dBm = 0.0;
         DOUBLE FFT_peak_freq = 0.0;
         S32    FFT_n_points = 1;
         DOUBLE FFT_start = 0.0;
         DOUBLE FFT_spacing = 0.0;
         S32    TD_record_points = 1;     // x2 points if complex data
         DOUBLE TD_trigger_relative_time = 0.0;
         DOUBLE TD_time_spacing = 0.0;
         S32    TD_is_complex = 0;
         DOUBLE TD_scan_time = 0;         // TD_time_spacing * (TD_record_points-1)
         S32    AVG_cur_count = 0;
         
         printf("Got %s\n",text);

         sscanf(text,"%lf,%lf,%d,%lf,%lf,%d,%lf,%lf,%d,%lf,%d", 
            &FFT_peak_dBm,
            &FFT_peak_freq,
            &FFT_n_points,
            &FFT_start,
            &FFT_spacing,
            &TD_record_points,
            &TD_trigger_relative_time,
            &TD_time_spacing,
            &TD_is_complex,
            &TD_scan_time,
            &AVG_cur_count);
#endif

         while (state.dB_division == 10000)
            {                 
            DOUBLE temp = 0.0;
            C8 *text = GPIB_query(":DISP:SPEC1:WIND1:TRAC:Y:SCAL:PDIV?");
            sscanf(text,"%lf",&temp);
            state.dB_division = (S32) (temp + 0.5);
            }

         while (state.max_dBm == 10000.0)
            {
            C8 *text = GPIB_query(":DISP:SPEC1:WIND1:TRAC:Y:SCAL:RLEV?");
            sscanf(text,"%lf",&state.max_dBm); 
            }

         //
         // Query E4406A VSA frequency limits
         //

         DOUBLE center = -10000.0;

         while (center == -10000.0)
            {
            C8 *text = GPIB_query(":SENS:FREQ:CENT?");
            sscanf(text,"%lf",&center); 
            }

         DOUBLE span = -10000.0;

         while (span == -10000.0)
            {
            C8 *text = GPIB_query(":SENS:SPEC:FREQ:SPAN?");
            sscanf(text,"%lf",&span); 
            }

         state.CF_Hz = center;
         state.min_Hz = state.CF_Hz - (span * 0.5);
         state.max_Hz = state.CF_Hz + (span * 0.5);

         //
         // Query other E4406A VSA parameters
         //

         state.RBW_Hz = -10000.0;

         while (state.RBW_Hz == -10000.0)
            {
            C8 *text = GPIB_query(":SENS:SPEC:BAND:RES?");
            sscanf(text,"%lf",&state.RBW_Hz); 
            }
         }
      else if ((state.type == SATYPE_FSE) ||
               (state.type == SATYPE_FSU) ||
               (state.type == SATYPE_FSP))
         {
         if (state.type == SATYPE_FSP)
            {
            GPIB_query(":SENS:SWE:POIN 501;*OPC?");      // FSP point count is configurable, but we only support the default

            state.RBW_Hz = -10000.0;

            while (state.RBW_Hz == -10000.0)
               {
               C8 *text = GPIB_query(":SENS:BAND:RES?");
               sscanf(text,"%lf",&state.RBW_Hz); 
               }

            state.sweep_secs = -10000.0;

            while (state.sweep_secs == -10000.0)
               {
               C8 *text = GPIB_query(":SENS:SWEEP:TIME?");
               sscanf(text,"%lf",&state.sweep_secs); 
               }
            }

         //
         // Query FSE/FSU/FSP reference level and scale
         //

         C8 *text = GPIB_query(":DISP:WIND:TRAC:Y:SPAC?");

         if (_strnicmp(text,"LOG",3))
            {
            SA_disconnect();
            state.type = SATYPE_INVALID;

            alert("Error","Must select logarithmic vertical display (1-10 dB/div)");
            return FALSE;
            }

         while (state.dB_division == 10000)
            {                 
            DOUBLE fdB = 0.0;
            C8 *scale = GPIB_query(":DISP:WIND:TRAC:Y:SCAL?");
            sscanf(scale,"%lf",&fdB);
            state.dB_division = (S32) (fdB + 0.5F) / state.amplitude_levels; 
            }

         while (state.max_dBm == 10000.0)
            {
            C8 *level = GPIB_query(":DISP:WIND:TRAC:Y:RLEV?");
            sscanf(level,"%lf",&state.max_dBm); 
            }

         //
         // Query FSE/FSU/FSP frequency limits
         //

         DOUBLE start = -FLT_MAX;

         while (start == -FLT_MAX)
            {
            C8 *fa = GPIB_query(":SENS:FREQ:START?");
            sscanf(fa,"%lf",&start); 
            }
         
         DOUBLE stop = -FLT_MAX;

         while (stop == -FLT_MAX)
            {
            C8 *fb = GPIB_query(":SENS:FREQ:STOP?");
            sscanf(fb,"%lf",&stop); 
            }

         state.min_Hz = start;
         state.max_Hz = stop;

         //
         // Query FSE/FSU/FSP display state
         //

         state.FSP_orig_display_state = (GPIB_query(":SYST:DISP:UPD?")[0] != '0');
         state.FSP_display_state = state.FSP_orig_display_state;
         }
      else if (state.type == SATYPE_N9900)
         {
         //
         // Query N9900-series reference level and scale
         //

         while (state.dB_division == 10000)
            {                 
            DOUBLE fdB = 0.0;
            C8 *text = GPIB_query(":DISP:WIND:TRAC:Y:PDIV?");
            sscanf(text,"%lf",&fdB);
            state.dB_division = (S32) (fdB + 0.5F); 
            }

         if ((state.dB_division < 1) || (state.dB_division > 10))
            {
            SA_disconnect();
            state.type = SATYPE_INVALID;

            alert("Error","Must select logarithmic vertical display (1-10 dB/div)");
            return FALSE;
            }

         while (state.max_dBm == 10000.0)
            {
            C8 *text = GPIB_query(":DISP:WIND:TRAC:Y:RLEV?");
            sscanf(text,"%lf",&state.max_dBm); 
            }

         //
         // Query N9900-series frequency limits
         //

         DOUBLE start = -FLT_MAX;

         while (start == -FLT_MAX)
            {
            C8 *text = GPIB_query(":SENS:FREQ:START?");
            sscanf(text,"%lf",&start); 
            }
         
         DOUBLE stop = -FLT_MAX;

         while (stop == -FLT_MAX)
            {
            C8 *text = GPIB_query(":SENS:FREQ:STOP?");
            sscanf(text,"%lf",&stop); 
            }

         state.min_Hz = start;
         state.max_Hz = stop;
         }
      else
         {
         //
         // Query other SCPI analyzers' reference level and scale
         //

         while (state.dB_division == 10000)
            {                 
            DOUBLE fdB = 0.0;
            C8 *text = GPIB_query(state.type == SATYPE_SCPI ? ":DISP:WIND:TRAC:Y:PDIV?" : ":DISP:Y:SCAL:PDIV?");
            sscanf(text,"%lf",&fdB);
            state.dB_division = (S32) (fdB + 0.5F); 
            }

         if ((state.dB_division < 1) || (state.dB_division > 10))
            {
            SA_disconnect();
            state.type = SATYPE_INVALID;

            alert("Error","Must select logarithmic vertical display (1-10 dB/div)");
            return FALSE;
            }

         while (state.max_dBm == 10000.0)
            {
            C8 *text = GPIB_query(state.type == SATYPE_SCPI ? ":DISP:WIND:TRAC:Y:RLEV?" : ":DISP:Y:SCAL:MAX?");
            sscanf(text,"%lf",&state.max_dBm); 
            }

         //
         // Query SCPI frequency limits
         //

         DOUBLE start = -FLT_MAX;

         while (start == -FLT_MAX)
            {
            C8 *text = GPIB_query(":SENS:FREQ:START?");
            sscanf(text,"%lf",&start); 
            }
         
         DOUBLE stop = -FLT_MAX;

         while (stop == -FLT_MAX)
            {
            C8 *text = GPIB_query(":SENS:FREQ:STOP?");
            sscanf(text,"%lf",&stop); 
            }

         state.min_Hz = start;
         state.max_Hz = stop;
         }

      //
      // Request 32-bit little-endian integer trace format for E4400-style SCPI, or
      // 32-bit IEEE754 reals for HP 3588A/3589A, E4406A, FSx, and the N9900 series handhelds
      //

      if (state.type == SATYPE_HP358XA)
         {
         GPIB_query(":CALC1:FORM MLOG;:FORM:DATA REAL,32;*OPC?");
         }
      else if ((state.type == SATYPE_FSE) ||
               (state.type == SATYPE_FSU) ||
               (state.type == SATYPE_FSP))
         {
         GPIB_query(":FORM:DATA REAL,32;*OPC?");
         }
      else if (state.type == SATYPE_N9900)
         {
         GPIB_query(":FORM:DATA REAL,32;:FORM:BORD NORM;*OPC?");     // NORM is LE on N9900
         }
      else if (state.type == SATYPE_E4406A)
         {
         GPIB_query(":FORM:DATA REAL,32;:FORM:BORD SWAP;*OPC?");     // SWAP is LE on E4406A (and presumably other SCPI instruments...? TODO)
         }
      else
         {
         GPIB_query(":FORM:TRACE:DATA INT,32;:FORM:BORD SWAP;*OPC?");
         }

      //
      // Get # of points per trace supported by this instrument
      //
      // E4406A VSA appears to have no way to query the # of points that will actually be returned
      // by a :FETCH:SPEC4 query, so we must request a trace here and see how many points it gives us
      //

      if (state.type == SATYPE_HP358XA)
         {
         state.n_trace_points = 401;
         }
      else if (state.type == SATYPE_FSE)
         {
         state.n_trace_points = 500;
         }
      else if (state.type == SATYPE_FSU)
         {
         state.n_trace_points = 501;
         }
      else if (state.type == SATYPE_FSP)
         {
         state.n_trace_points = 501;
         }
      else if (state.type == SATYPE_E4406A)
         {
         state.n_trace_points = 0;
         SA_fetch_trace();
         }
      else
         {
         state.n_trace_points = 0;

         while (state.n_trace_points == 0)
            {
            C8 *text = GPIB_query(":SENS:SWEEP:POINTS?");
            sscanf(text,"%d",&state.n_trace_points);
            }                  
         }
      }
   else  
      {
      GPIB_disconnect();                    // return instrument to local control
      alert("Error","Instrument '%s' not supported",state.ID_string);
      return FALSE;
      }

   return TRUE;
}

//****************************************************************************
//
// Send formatted string to GPIB
//
// This version doesn't poll for acknowledgement from Tek 49x/271x analyzers;
// it needs to be called when issuing queries to those instruments
//
//****************************************************************************

void __cdecl SA_query_printf(C8 *fmt, ...)
{
   static C8 work_string[16384];

   va_list ap;

   va_start(ap, 
            fmt);

   vsprintf(work_string, 
            fmt,  
            ap);

   va_end(ap);

   //
   // Remove any trailing whitespace
   //

   S32 l = strlen(work_string);

   while (l > 0)
      {
      --l;

      if (!isspace((U8) work_string[l]))
         {
         break;
         }

      work_string[l] = 0;
      }

   GPIB_puts(work_string);
}              

//****************************************************************************
//
// Send formatted string to GPIB, then wait for a response from all 
// analyzer types
//
//****************************************************************************

C8 * __cdecl SA_query(C8 *fmt, ...)
{
   static C8 work_string[16384];

   va_list ap;

   va_start(ap, 
            fmt);

   vsprintf(work_string, 
            fmt,  
            ap);

   va_end(ap);

   //
   // Remove any trailing whitespace
   //

   S32 l = strlen(work_string);

   while (l > 0)
      {
      --l;

      if (!isspace((U8) work_string[l]))
         {
         break;
         }

      work_string[l] = 0;
      }

   return GPIB_query(work_string);
}

//****************************************************************************
//
// Send formatted string to GPIB, then wait for CR/LF to be transmitted
// as an acknowledgement from the Tek 49x/271x analyzer
//
// Tek 49x commands (other than queries) need to go through here -- otherwise, 
// they'll remain unacknowledged if a Prologix adapter is in use.  An
// unacknowledged command can corrupt subsequent query commands and/or prevent
// further commands from executing. 
//
// (What the 494AP actually returns is 0xff 0x0d 0x0a, for some unknown reason...)
//
//****************************************************************************

void __cdecl SA_printf(C8 *fmt, ...)
{
   static C8 work_string[16384];

   va_list ap;

   va_start(ap, 
            fmt);

   vsprintf(work_string, 
            fmt,  
            ap);

   va_end(ap);

   //
   // Remove any trailing whitespace
   //

   S32 l = strlen(work_string);

   while (l > 0)
      {
      --l;

      if (!isspace((U8) work_string[l]))
         {
         break;
         }

      work_string[l] = 0;
      }

   GPIB_puts(work_string);

   if ((state.type == SATYPE_TEK_490P) ||
       (state.type == SATYPE_TEK_271X))
      {
      GPIB_read_ASC();
      }
}              

// -------------------------------------------------------------------------
// SA_startup()
// -------------------------------------------------------------------------

SA_STATE * WINAPI SA_startup(void)
{
   //
   // Initialize global state
   //

   memset(&state, 0, sizeof(state));

   state.error = FALSE;

   state.interface_name_specified = FALSE;
   state.addr_specified           = FALSE;
   state.interface_name[0]        = 0;
   state.addr          [0]        = 0;

   state.type             =  SATYPE_INVALID; 
   state.min_Hz           =  0.0;          
   state.max_Hz           =  1800000000.0; 
   state.CF_Hz            =  900000000.0;
   state.min_dBm          = -100.0;        
   state.max_dBm          =  0.0;          
   state.amplitude_levels =  10;
   state.dB_division      =  10;
   state.F_offset         =  0.0;
   state.A_offset         =  0.0;
   state.n_trace_points   =  0;
   state.FFT_size         =  0;

   state.HP856XA_mode     = FALSE;
   state.HP8569B_mode     = FALSE;
   state.HP3585_mode      = FALSE;
   state.HP358XA_mode     = FALSE;
   state.SCPI_mode        = FALSE;
   state.Advantest_mode   = FALSE;
   state.R3261_mode       = FALSE;
   state.hi_speed_acq     = FALSE;

   state.blind_data             = NULL;
   state.FSP_display_state      = TRUE;
   state.FSP_orig_display_state = TRUE;
   state.VSA_display_state      = TRUE;
   state.VSA_orig_avg_state     = TRUE;
   state.VSA_orig_display_state = TRUE;

   state.SH_initialized = FALSE;

   state.CTRL_RL_dBm         =  -30.0;
   state.CTRL_CF_Hz          =  0.0;
   state.CTRL_span_Hz        =  -1.0;
   state.CTRL_start_Hz       =  88E6;
   state.CTRL_stop_Hz        =  108E6;
   state.CTRL_freq_specified =  FALSE;
   state.CTRL_sens           =  2;
   state.CTRL_atten_dB       =  0.0;
   state.CTRL_FFT_size       =  128;
   state.CTRL_n_divs         =  0;
   state.CTRL_dB_div         =  0;

   orig_n_points          = 0;    
   cur_n_points           = 0;

   disable_GPIB_timeouts  = 0;
   connect_cmd   [0]      = 0;
   disconnect_cmd[0]      = 0;
   startup_cmd   [0]      = 0;
   shutdown_cmd  [0]      = 0;

   return &state;
}

// -------------------------------------------------------------------------
// SA_shutdown()
// -------------------------------------------------------------------------

void WINAPI SA_shutdown(void)
{
   if (state.type != SATYPE_INVALID)
      {
      SA_disconnect(TRUE);
      }

   HOUND_kill_process(FALSE);
}

// -------------------------------------------------------------------------
// SA_setup()
// -------------------------------------------------------------------------

void WINAPI SA_parse_command_line(C8 *command_line)
{
   //
   // Parse command line
   //

   C8 lpCmdLine [MAX_PATH] = { 0 };
   C8 lpUCmdLine[MAX_PATH] = { 0 };

   _snprintf(lpCmdLine,  sizeof(lpCmdLine)-1, "%s", command_line);
   strcpy(lpUCmdLine, lpCmdLine);
   _strupr(lpUCmdLine);

   //
   // Check for CLI options
   //

   disable_GPIB_timeouts = 0;
   
   connect_cmd   [0] = 0;
   disconnect_cmd[0] = 0;
   startup_cmd   [0] = 0;
   shutdown_cmd  [0] = 0;

   C8 *option = NULL;
   C8 *end    = NULL;
   C8 *beg    = NULL;
   C8 *ptr    = NULL;

   for (;;)
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
      // -interface: Interface specifier (e.g., GPIB0)
      //

      option = strstr(lpCmdLine, "-interface:");
      
      if (option != NULL)
         {
         S32 len = 0;
         sscanf(option,"-interface:%s%n", state.interface_name, &len);
         end = &option[len];
         memmove(option, end, strlen(end)+1);

#ifdef _WIN64
         GPIB.set_connection_properties(state.interface_name);
#endif

         state.interface_name_specified = TRUE;
         continue;
         }

      //
      // -addr: Address specifier (e.g., 18)
      //

      option = strstr(lpCmdLine, "-addr:");
      
      if (option != NULL)
         {
         S32 len = 0;
         sscanf(option,"-addr:%s%n", state.addr, &len);
         end = &option[len];
         memmove(option, end, strlen(end)+1);

         state.addr_specified = TRUE;
         continue;
         }

      option = strstr(lpCmdLine, "-address:");
      
      if (option != NULL)
         {
         S32 len = 0;
         sscanf(option,"-address:%s%n", state.addr, &len);
         end = &option[len];
         memmove(option, end, strlen(end)+1);

         state.addr_specified = TRUE;
         continue;
         }

      //
      // -startup: GPIB command to be transmitted when launched
      //

      option = strstr(lpCmdLine, "-startup:\"");

      if (option != NULL)
         {
         beg = &option[10];
         end = beg;
         ptr = startup_cmd;

         while ((*end) && (*end != '\"'))
            {
            *ptr++ = *end;
            *ptr = 0;

            end++;
            }

         if (*end == '\"') end++;

         memmove(option, end, strlen(end)+1);
         continue;
         }

      //
      // -shutdown: GPIB command to be transmitted at exit time
      //

      option = strstr(lpCmdLine, "-shutdown:\"");

      if (option != NULL)
         {
         beg = &option[11];
         end = beg;
         ptr = shutdown_cmd;

         while ((*end) && (*end != '\"'))
            {
            *ptr++ = *end;
            *ptr = 0;

            end++;
            }

         if (*end == '\"') end++;

         memmove(option, end, strlen(end)+1);
         continue;
         }

      //
      // -connect: GPIB command to be transmitted at connection time
      //

      option = strstr(lpCmdLine, "-connect:\"");

      if (option != NULL)
         {
         beg = &option[10];
         end = beg;
         ptr = connect_cmd;

         while ((*end) && (*end != '\"'))
            {
            *ptr++ = *end;
            *ptr = 0;

            end++;
            }

         if (*end == '\"') end++;

         memmove(option, end, strlen(end)+1);
         continue;
         }

      //
      // -disconnect: GPIB command to be transmitted at disconnection time
      //

      option = strstr(lpCmdLine, "-disconnect:\"");

      if (option != NULL)
         {
         beg = &option[13];
         end = beg;
         ptr = disconnect_cmd;

         while ((*end) && (*end != '\"'))
            {
            *ptr++ = *end;
            *ptr = 0;

            end++;
            }

         if (*end == '\"') end++;

         memmove(option, end, strlen(end)+1);
         continue;
         }

      //
      // -fo: Set frequency offset in Hz
      //

      option = strstr(lpCmdLine,"-fo:");

      if (option != NULL)
         {
         S32 len = 0;
         sscanf(option,"-fo:%lf%n",&state.F_offset,&len); 
         end = &option[len];
         memmove(option, end, strlen(end)+1);
         continue;
         }

      //
      // -ao: Set amplitude offset in dB
      //

      option = strstr(lpCmdLine,"-ao:");

      if (option != NULL)
         {
         S32 len = 0;
         sscanf(option,"-ao:%lf%n",&state.A_offset,&len); 
         end = &option[len];
         memmove(option, end, strlen(end)+1);
         continue;
         }

      //
      // -t: Disable GPIB read timeouts
      //

      option = strstr(lpCmdLine,"-t");

      if (option != NULL)
         {
         disable_GPIB_timeouts = TRUE;
         memmove(option, &option[2], strlen(&option[2])+1);
         continue;
         }

      //
      // -856XA: Enable HP 8568A/8566A mode
      //

      option = strstr(lpUCmdLine,"-856XA");

      if (option != NULL)
         {
         state.HP856XA_mode = TRUE;
         memmove(option, &option[6], strlen(&option[6])+1);
         continue;
         }

      //
      // -358XA: Enable HP 358XA mode
      //

      option = strstr(lpUCmdLine,"-358XA");

      if (option != NULL)
         {
         state.HP358XA_mode = TRUE;
         memmove(option, &option[6], strlen(&option[6])+1);
         continue;
         }

      //
      // -3585: Enable HP 3585A/B mode
      //

      option = strstr(lpUCmdLine,"-3585");

      if (option != NULL)
         {
         state.HP3585_mode = TRUE;
         memmove(option, &option[5], strlen(&option[5])+1);
         continue;
         }

      //
      // -8569B: Enable HP 8569B mode
      //

      option = strstr(lpUCmdLine,"-8569B");

      if (option != NULL)
         {
         state.HP8569B_mode = TRUE;
         memmove(option, &option[6], strlen(&option[6])+1);
         continue;
         }

      //
      // -SCPI: Use SCPI-compatible analyzer
      //

      option = strstr(lpUCmdLine,"-SCPI");

      if (option != NULL)
         {
         state.SCPI_mode = TRUE;
         memmove(option, &option[5], strlen(&option[5])+1);
         continue;
         }

      //
      // -advantest: Enable Advantest R3264/R3267/R3273, R3463/R3465/R3272/R3263, or 
      // R3131/R3132/R3162/R3172/R3182 mode
      //
      // (i.e., any R31xx, R32xx, or R34xx analyzer that recognizes *IDN query)
      //
      
      option = strstr(lpUCmdLine,"-ADVANTEST");

      if (option != NULL)
         {
         state.Advantest_mode = TRUE;
         memmove(option, &option[10], strlen(&option[10])+1);
         continue;
         }

      //
      // -r3261: Enable Advantest R3261/R3361 mode
      //

      option = strstr(lpUCmdLine,"-R3261");

      if (option != NULL)
         {
         state.R3261_mode = TRUE;
         state.Advantest_mode = FALSE;
         memmove(option, &option[6], strlen(&option[6])+1);
         continue;
         }

      //
      // -sa44: Enable Signal Hound USB-SA44 mode
      //

      option = strstr(lpUCmdLine,"-SA44");

      if (option != NULL)
         {
         state.SA44_mode = TRUE;
         memmove(option, &option[5], strlen(&option[5])+1);
         continue;
         }

      //
      // -forceid: Preset ID string to avoid executing an ID? query
      // (which crashes some HP 8562As per Leon Markel)
      //

      option = strstr(lpCmdLine,"-forceid:");

      if (option != NULL)
         {
         S32 len = 0;
         sscanf(option,"-forceid:%s%n", state.ID_string, &len);
         end = &option[len];
         memmove(option, end, strlen(end)+1);

         for (S32 i=0; i < strlen(state.ID_string); i++)
            {
            if (state.ID_string[i] == '_')
               {
               state.ID_string[i] = ' ';
               }
            }

         printf("Forced ID: [%s]\n", state.ID_string);
         continue;
         }

      //
      // -f: Enable fastest possible acquisition (lower resolution or precision, disable screen, etc.)
      //

      option = strstr(lpCmdLine,"-f");

      if (option != NULL)
         {
         state.hi_speed_acq = TRUE;
         memmove(option, &option[2], strlen(&option[2])+1);
         continue;
         }

      //
      // -RL, -CF, -span, -start, -stop, -bins, -RFATT, -sens, -divs, -logdiv: Signal Hound control options
      //

      option = strstr(lpUCmdLine, "-RL");

      if (option != NULL)
         {
         S32 len = 0;
         sscanf(option,"-RL:%lf%n",&state.CTRL_RL_dBm,&len); 
         end = &option[len];
         memmove(option, end, strlen(end)+1);
         continue;
         }

      option = strstr(lpUCmdLine, "-CF");

      if (option != NULL)
         {
         S32 len = 0;
         sscanf(option,"-CF:%lf%n",&state.CTRL_CF_Hz,&len); 
         end = &option[len];
         memmove(option, end, strlen(end)+1);
         state.CTRL_freq_specified = TRUE;
         continue;
         }

      option = strstr(lpUCmdLine, "-SPAN");

      if (option != NULL)
         {
         S32 len = 0;
         sscanf(option,"-SPAN:%lf%n",&state.CTRL_span_Hz,&len); 
         end = &option[len];
         memmove(option, end, strlen(end)+1);
         state.CTRL_freq_specified = TRUE;
         continue;
         }

      option = strstr(lpUCmdLine, "-START");

      if (option != NULL)
         {
         S32 len = 0;
         sscanf(option,"-START:%lf%n",&state.CTRL_start_Hz,&len); 
         end = &option[len];
         memmove(option, end, strlen(end)+1);
         state.CTRL_freq_specified = TRUE;
         continue;
         }

      option = strstr(lpUCmdLine, "-STOP");

      if (option != NULL)
         {
         S32 len = 0;
         sscanf(option,"-STOP:%lf%n",&state.CTRL_stop_Hz,&len); 
         end = &option[len];
         memmove(option, end, strlen(end)+1);
         state.CTRL_freq_specified = TRUE;
         continue;
         }

      option = strstr(lpUCmdLine, "-RFATT");

      if (option != NULL)
         {
         S32 len = 0;
         sscanf(option,"-RFATT:%lf%n",&state.CTRL_atten_dB,&len); 
         end = &option[len];
         memmove(option, end, strlen(end)+1);
         continue;
         }

      option = strstr(lpUCmdLine, "-SENS");

      if (option != NULL)
         {
         S32 len = 0;
         sscanf(option,"-SENS:%d%n",&state.CTRL_sens,&len); 
         end = &option[len];
         memmove(option, end, strlen(end)+1);
         continue;
         }

      option = strstr(lpUCmdLine, "-BINS");

      if (option != NULL)
         {
         S32 len = 0;
         sscanf(option,"-BINS:%d%n",&state.CTRL_FFT_size,&len); 
         end = &option[len];
         memmove(option, end, strlen(end)+1);
         continue;
         }

      option = strstr(lpUCmdLine, "-DIVS");

      if (option != NULL)
         {
         S32 len = 0;
         sscanf(option,"-DIVS:%d%n",&state.CTRL_n_divs,&len); 
         end = &option[len];
         memmove(option, end, strlen(end)+1);
         continue;
         }

      option = strstr(lpUCmdLine, "-LOGDIV");

      if (option != NULL)
         {
         S32 len = 0;
         sscanf(option,"-LOGDIV:%d%n",&state.CTRL_dB_div,&len); 
         end = &option[len];
         memmove(option, end, strlen(end)+1);
         continue;
         }

      //
      // Exit loop when no more CLI options are recognizable
      //

      break;
      }

   //
   // If span was specified, set start/stop accordingly
   // Otherwise, derive CF/span from start/stop
   //
   // All supported combinations yield valid start, stop, span, and CF values
   //

   if (state.CTRL_span_Hz >= 0.0)
      {
      if (state.CTRL_CF_Hz > 0.0)
         {
         //
         // CF,span specified
         //

         state.CTRL_start_Hz = state.CTRL_CF_Hz - (state.CTRL_span_Hz / 2.0);
         state.CTRL_stop_Hz  = state.CTRL_CF_Hz + (state.CTRL_span_Hz / 2.0);
         }
      else
         {
         //
         // start,span specified
         //

         state.CTRL_stop_Hz = state.CTRL_start_Hz + state.CTRL_span_Hz;
         }
      }
   else
      {
      if (state.CTRL_CF_Hz > 0.0)
         {
         //
         // CF,start specified
         //

         state.CTRL_span_Hz = (state.CTRL_CF_Hz - state.CTRL_start_Hz) * 2.0;
         state.CTRL_stop_Hz =  state.CTRL_start_Hz + state.CTRL_span_Hz;
         }
      else
         {
         //
         // start,stop specified
         //

         state.CTRL_span_Hz =  state.CTRL_stop_Hz - state.CTRL_start_Hz;
         state.CTRL_CF_Hz   = (state.CTRL_stop_Hz + state.CTRL_start_Hz) / 2.0;
         }
      }
}

// -------------------------------------------------------------------------
// SA_connect()
// -------------------------------------------------------------------------

bool WINAPI SA_connect(S32 address)
{
   bool result = TRUE;

   state.type = SATYPE_INVALID;

   state.RBW_Hz     = -1.0;
   state.VBW_Hz     = -1.0;
   state.vid_avgs   = -1;
   state.sweep_secs = -1.0;
   state.RFATT_dB   = -99999.0;
   state.CF_Hz      = -FLT_MAX;

   if ((address != -1) ||                          // if GPIB address valid...
       (state.SA44_mode))                          // or USB device...
      {
      strcpy(state.ID_string,"");                  // (Default "Test mode" string causes crash in -scpi mode)

      result = GPIB_connect_to_analyzer(address);  // ... then connect to it
      }
   else
      {
      strcpy(state.ID_string,"Test mode");

      state.min_Hz           =  0.0;
      state.max_Hz           =  1800000000.0;
      state.max_dBm          =  0.0;
      state.min_dBm          = -100.0;
      state.dB_division      =  10;
      state.amplitude_levels =  10;
      }

   //
   // Apply any user offsets, calculate display range, and return
   //

   if (state.CF_Hz == -FLT_MAX)
      {
      state.CF_Hz = (state.max_Hz + state.min_Hz) / 2.0;
      }

   state.max_dBm += state.A_offset;
   state.min_Hz  += state.F_offset;
   state.max_Hz  += state.F_offset;
   state.CF_Hz   += state.F_offset;

   state.min_dBm = state.max_dBm - (state.dB_division * state.amplitude_levels);

   return result;
}

// -------------------------------------------------------------------------
// SA_disconnect()
// -------------------------------------------------------------------------

void WINAPI SA_disconnect(bool final_exit)
{
   //
   // Issue command to return to continuous-sweep mode 
   //

   bool reset_to_local = TRUE;

   if (state.type == SATYPE_TEK_490P)
      {
      SA_printf("SIGSWP OFF;");   
      SA_printf("TRIG FRERUN;");
      }
   else if (state.type == SATYPE_TEK_271X)
      {
      SA_printf("TRIGGER FRERUN;");
      }
   else if (state.type == SATYPE_HP8569B) 
      {
      //
      // Nothing to do...
      //
      }
   else if (state.type == SATYPE_HP3585) 
      {
      SA_printf("TB0S1RC4");      // Hide trace 'B', continuous sweep, re-enable autocal
      }
   else if (state.type == SATYPE_HP8566A_8568A)
      {
      //
      // Resetting the rev-B 8568A to local under Prologix addresses it to talk,
      // which floods the connection with zeroes and hoses the serial port driver
      //

      SA_printf("S1;");        
      reset_to_local = FALSE;
      }
   else if (state.type == SATYPE_TEK_278X)
      {
      SA_printf("CONTS;");        
      }
   else if (state.type == SATYPE_HP8566B_8568B)
      {
      SA_printf("CONTS;");        
      }
   else if (state.type == SATYPE_HP70000)
      {
      SA_printf("CONTS;");        
      }
   else if ((state.type == SATYPE_HP8560) || (state.type == SATYPE_HP8590))
      {
      SA_printf("CONTS;");      
      }
   else if (state.type == SATYPE_R3267_SERIES)
      {
      SA_printf((orig_n_points == 501) ? "TPS" : "TPL");
      SA_printf("SN;DL0");            // Restore original resolution, continuous sweep, default <CR><LF><EOI> response delimiter 
      }
   else if ((state.type == SATYPE_R3465_SERIES) ||
            (state.type == SATYPE_R3132_SERIES))
      {
      SA_printf("TPL;SN;DL0");        // Restore default 1001-point mode, continuous sweep, default <CR><LF><EOI> response delimiter
      }
   else if ((state.type == SATYPE_R3261_R3361) ||
            (state.type == SATYPE_R3265_R3271))
      {
      GPIB_auto_read_mode(TRUE);
      SA_printf("SN FR DL0");         // Normal sweep, free-run, default <CR><LF><EOI> response delimiter 
      }
   else if (state.type == SATYPE_R3131)
      {
      SA_printf("SN FR DL0");         // Normal sweep, free-run, default <CR><LF><EOI> response delimiter 
      }
   else if (state.type == SATYPE_MS8604A)
      {
      if (orig_n_points == 1002)
         GPIB_query("CONTS;DPOINT DOUBLE;*OPC?");
      else
         GPIB_query("CONTS;DPOINT NRM;*OPC?");
      }
   else if (state.type == SATYPE_MS265X_MS266X)
      {
      GPIB_query("CONTS;");
      }
   else if ((state.type == SATYPE_FSE)   ||
            (state.type == SATYPE_FSP)   ||
            (state.type == SATYPE_FSU)   ||
            (state.type == SATYPE_N9900) ||
            (state.type == SATYPE_SCPI))
      {
      GPIB_query(":INIT:CONT 1;*OPC?");
      }
   else if (state.type == SATYPE_E4406A)
      {
      GPIB_query(":DISP:ENAB 1;:INIT:CONT 1;*OPC?");

      if (state.VSA_orig_avg_state)   
         GPIB_query(":SENS:SPEC:AVER:STAT 1;*OPC?");
      else
         GPIB_query(":SENS:SPEC:AVER:STAT 0;*OPC?");
      }
   else if (state.type == SATYPE_SA44)
      {
      //
      // Nothing to do...
      //
      }

   //
   // Send disconnection and exit commands, if any
   //

   if (state.type != SATYPE_SA44)
      {
      if (state.type != SATYPE_INVALID)
         {
         if (disconnect_cmd[0])
            {
            GPIB_puts(disconnect_cmd);
            }

         if (final_exit && shutdown_cmd[0])
            {
            if (disconnect_cmd[0])
               {
               GPIB_flush_receive_buffers();
               }

            GPIB_puts(shutdown_cmd);
            shutdown_cmd[0] = 0;
            }
         }

      GPIB_disconnect(reset_to_local);
      }

   if (state.blind_data != NULL)
      {
      free(state.blind_data);
      state.blind_data = NULL;
      }

   state.type = SATYPE_INVALID;
}

// -------------------------------------------------------------------------
// SA_fetch_trace()
// -------------------------------------------------------------------------

void WINAPI SA_fetch_trace(void)
{
   S32 n_expected_trace_points = state.n_trace_points;
   state.n_trace_points = 0;

   if (state.type == SATYPE_SA44)
      {
#ifndef _WIN64
      //
      // Take sweep
      //

      static DOUBLE trace[MAX_N_PTS];
      memset(trace, 0, sizeof(trace[0]) * n_expected_trace_points);

      SHAPI_GetFastSweep(trace,
                         state.min_Hz,
                         state.max_Hz,
                         n_expected_trace_points,
                         state.FFT_size,
                         0);                // 0=default, image rejection on

      state.dBm_values = trace;
      state.n_trace_points = n_expected_trace_points;
#endif
      }
   else if (state.type == SATYPE_TEK_490P)
      {
      //
      // Take a single sweep, then fetch and parse waveform preamble 
      // for our desired format (both traces, binary encoded)
      //
      // Cache the preamble so we don't have to request it for every trace
      //

      SA_printf("SIGSWP;WAIT;"); 

      S32 pts = (state.hi_speed_acq) ? 500 : 1000;

      TEK_PREAMBLE *P = (TEK_PREAMBLE *) state.blind_data;

      if (P != NULL)
         {
         if (P->nr_pt != pts)
            {
            free(state.blind_data);
            state.blind_data = NULL;
            }
         }

      if (state.blind_data == NULL)
         {
         P = (TEK_PREAMBLE *) calloc(1, sizeof(TEK_PREAMBLE));

         if (P == NULL)
            {
            alert("Error","Out of memory");
            fail(1);
            return;
            }

         state.blind_data = P;

         C8 *preamble_header;

         if (pts == 1000)
            {
            SA_printf("WFMPRE WFID:FULL,ENC:BIN;");
            preamble_header = "WFMPRE WFID:FULL,ENCDG:BIN,NR.PT:%d,PT.FMT:Y,PT.OFF:%d,XINCR:%lf,XZERO:%lf,XUNIT:HZ,YOFF:%lf,YMULT:%lf,YZERO:%lf";
            strcpy(P->expected_curve,"CURVE CRVID:FULL,%");
            }
         else
            {
            SA_printf("WFMPRE WFID:A,ENC:BIN;");
            preamble_header = "WFMPRE WFID:A,ENCDG:BIN,NR.PT:%d,PT.FMT:Y,PT.OFF:%d,XINCR:%lf,XZERO:%lf,XUNIT:HZ,YOFF:%lf,YMULT:%lf,YZERO:%lf";
            strcpy(P->expected_curve,"CURVE CRVID:A,%");
            }

         SA_query_printf("WFMPRE?");

         C8 *preamble = GPIB_read_ASC();

         sscanf(preamble, 
                preamble_header, 
               &P->nr_pt,
               &P->pt_off,
               &P->xincr,
               &P->xzero,
               &P->yoff,
               &P->ymult,
               &P->yzero);

         assert(P->nr_pt == pts);
         }

      //
      // Fetch binary data from display traces A+B and 
      // verify its checksum
      //

      SA_query_printf("CURVE?");

      C8 *curve_result = GPIB_read_ASC(strlen(P->expected_curve), FALSE);

      assert(!_stricmp(P->expected_curve,curve_result));

      C8 *cnt = GPIB_read_BIN(2, TRUE);

      U8 n_MSD = ((U8 *) cnt)[0];
      U8 n_LSD = ((U8 *) cnt)[1];

      S32 n_binary_nums = (256*S32(n_MSD)) + S32(n_LSD);
      assert(n_binary_nums == pts+1);          
      
      static U8 curve[1000];

      memset(curve, 0, pts);
      memcpy(curve, GPIB_read_BIN(pts,TRUE,FALSE), pts);

      U8 chksum = ((U8 *) GPIB_read_BIN(1,TRUE,FALSE))[0];

      chksum += n_MSD;
      chksum += n_LSD;

      for (S32 i=0; i < pts; i++)
         {
         chksum += curve[i];
         }

      assert(chksum == 0);

#if 1
      GPIB_read_ASC();   // swallow trailing CR/LF
#else 
      S32 actual=0;      // display actual trailing character(s) for diagnostic purposes
      U8 *t = (U8 *) GPIB_read_BIN(-1,FALSE,FALSE,&actual);
      SAL_debug_printf("%d: ",actual);
      for (S32 i=0; i < actual; i++)
         SAL_debug_printf("%X ",t[i]);
      SAL_debug_printf("\n");
#endif      
      //
      // Fill in trace array with actual dBm/frequency values
      //
      // Log the min/max frequency endpoints here as well -- they can't be easily
      // obtained from the span because the MAX SPAN extents are different
      // across a wide variety of models
      //

      static DOUBLE trace[1000];
      static DOUBLE freq [1000];

      for (S32 p=0; p < pts; p++)
         {
         freq [p] =  P->xzero + P->xincr * (p - P->pt_off);
         trace[p] = (P->yzero + state.A_offset) + (P->ymult * (S32(curve[p]) - P->yoff));
         }

      state.dBm_values     = trace;
      state.n_trace_points = pts;

      state.min_Hz = P->xzero + P->xincr * (-P->pt_off);
      state.max_Hz = P->xzero + P->xincr *   P->pt_off;
      }
   else if (state.type == SATYPE_TEK_271X)
      {
      //
      // Take a single sweep, then fetch and parse waveform preamble 
      // for our desired format
      //

      SA_printf("SIGSWP;WAIT;"); 

      //
      // Cache the preamble so we don't have to request it for every trace
      //

      S32 pts = 512;

      TEK_PREAMBLE *P = (TEK_PREAMBLE *) state.blind_data;

      if (P == NULL)
         {
         P = (TEK_PREAMBLE *) calloc(1, sizeof(TEK_PREAMBLE));

         if (P == NULL)
            {
            alert("Error","Out of memory");
            fail(1);
            return;
            }

         state.blind_data = P;

         SA_printf("WFMPRE WFID:A,ENC:BIN;");

         C8 *preamble_header = "WFMPRE WFID:A,ENCDG:BIN,NR.PT:%d,PT.FMT:Y,PT.OFF:%d,XINCR:%lf,XZERO:%lf,XUNIT:HZ,YOFF:%lf,YMULT:%lf,YZERO:%lf";
         strcpy(P->expected_curve, "CURVE %");

         SA_query_printf("WFMPRE?");

         C8 *preamble = GPIB_read_ASC();

         sscanf(preamble, 
                preamble_header, 
               &P->nr_pt,
               &P->pt_off,
               &P->xincr,
               &P->xzero,
               &P->yoff,
               &P->ymult,
               &P->yzero);

         assert(P->nr_pt == pts);
         }

      //
      // Fetch binary data from display trace A and 
      // verify its checksum
      //

      SA_query_printf("CURVE? A;");

      C8 *curve_result = GPIB_read_ASC(strlen(P->expected_curve), FALSE);

      assert(!_stricmp(P->expected_curve,curve_result));

      C8 *cnt = GPIB_read_BIN(2, TRUE);

      U8 n_MSD = ((U8 *) cnt)[0];
      U8 n_LSD = ((U8 *) cnt)[1];

      S32 n_binary_nums = (256*S32(n_MSD)) + S32(n_LSD);
      assert(n_binary_nums == pts+1);          
      
      static U8 curve[512];

      memset(curve, 0, pts);                   
      memcpy(curve, GPIB_read_BIN(pts,TRUE,FALSE), pts);

      U8 chksum = ((U8 *) GPIB_read_BIN(1,TRUE,FALSE))[0];

      chksum += n_MSD;
      chksum += n_LSD;

      for (S32 i=0; i < pts; i++)
         {
         chksum += curve[i];
         }

      assert(chksum == 0);
 
      GPIB_read_ASC();   // swallow trailing CR/LF
      
      //
      // Fill in trace array with actual dBm/frequency values
      //
      // Log the min/max frequency endpoints here as well -- they can't be easily
      // obtained from the span because the MAX SPAN extents are different
      // across a wide variety of models
      //

      static DOUBLE trace[512];
      static DOUBLE freq [512];

      for (S32 p=0; p < pts; p++)
         {
         freq [p] = P->xzero + P->xincr * (p - P->pt_off);
         trace[p] = (P->yzero + state.A_offset) + (P->ymult * (S32(curve[p]) - P->yoff));
         }

      state.dBm_values     = trace;
      state.n_trace_points = pts;

      state.min_Hz = P->xzero + P->xincr * (-P->pt_off);
      state.max_Hz = P->xzero + P->xincr *   P->pt_off;
      }
   else if (state.type == SATYPE_HP8566A_8568A)
      {
      //
      // Take sweep, and acquire data from display trace A
      //

      static DOUBLE trace[1001];
      SA_printf("TS;");

#if ASCII_ACQ
      SA_printf("O3;TA");      // Request ASCII dBm values from trace A
                               // (Slow)
      for (S32 i=0; i < 1001; i++)
         {
         do
            {
            C8 *read_result = GPIB_read_ASC(16, FALSE);

            if ((read_result == NULL) || (read_result[0] == 0))
               {
               alert("Error","Sweep read terminated prematurely at column %d\n",i);
               fail(1);
               return;
               }

            trace[i] = 10000.0;
            sscanf(read_result,"%lf",&trace[i]); 
            if (trace[i] != 10000.0) trace[i] += state.A_offset;
            }
         while (trace[i] == 10000.0);
         }
#else
      if (!state.hi_speed_acq)
         {
         SA_printf("O2;TA");      // Request 16-bit binary display values from trace A
                                  // (Slightly slower but more precise than 8-bit binary mode)
         static U16 curve[1001];     

         memset(curve, 0, 1001 * sizeof(S16));
         memcpy(curve, GPIB_read_BIN(1001 * sizeof(S16),TRUE,FALSE), 1001 * sizeof(S16));

         DOUBLE offset      =  state.min_dBm;
         DOUBLE scale       = (state.max_dBm - state.min_dBm) / 1000;
         DOUBLE grat_offset = 0.0;

         for (S32 i=0; i < 1001; i++)
            {
            curve[i] = ((curve[i] & 0xff00) >> 8) + ((curve[i] & 0x00ff) << 8);
            trace[i] = ((DOUBLE(curve[i]) + grat_offset) * scale) + offset;
            }
         }
      else
         {
         SA_printf("O4;TA");      // Request 8-bit binary display values from trace A
                                  // (Fast, but less precise)
         static U8 curve[1001];     

         memset(curve, 0, 1001);
         memcpy(curve, GPIB_read_BIN(1001,TRUE,FALSE), 1001);

         DOUBLE offset      =  state.min_dBm;
         DOUBLE scale       = (state.max_dBm - state.min_dBm) / 250;
         DOUBLE grat_offset = 0.0;

         for (S32 i=0; i < 1001; i++)
            {
            trace[i] = ((DOUBLE(curve[i]) + grat_offset) * scale) + offset;
            }
         }
#endif

      state.dBm_values     = trace;
      state.n_trace_points = 1001;
      }
   else if (state.type == SATYPE_HP8566B_8568B)
      {
      //
      // Take sweep, and acquire data from display trace A
      //

      static DOUBLE trace[1001];
      SA_printf("TS;");

#if ASCII_ACQ
      SA_printf("O3;TA");      // Request ASCII dBm values from trace A
                               // (Slow)
      for (S32 i=0; i < 1001; i++)
         {
         do
            {
            C8 *read_result = GPIB_read_ASC(16, FALSE);

            if ((read_result == NULL) || (read_result[0] == 0))
               {
               alert("Error","Sweep read terminated prematurely at column %d\n",i);
               fail(1);
               return;
               }

            trace[i] = 10000.0;
            sscanf(read_result,"%lf",&trace[i]); 
            if (trace[i] != 10000.0) trace[i] += state.A_offset;
            }
         while (trace[i] == 10000.0);
         }
#else
      if (!state.hi_speed_acq)
         {
         SA_printf("O2;TA");      // Request 16-bit binary display values from trace A
                                  // (Slightly slower but more precise than 8-bit binary mode)
         static U16 curve[1001];     

         memset(curve, 0, 1001 * sizeof(S16));
         memcpy(curve, GPIB_read_BIN(1001 * sizeof(S16),TRUE,FALSE), 1001 * sizeof(S16));

         DOUBLE offset      =  state.min_dBm;
         DOUBLE scale       = (state.max_dBm - state.min_dBm) / 1000;
         DOUBLE grat_offset = 0.0;

         for (S32 i=0; i < 1001; i++)
            {
            curve[i] = ((curve[i] & 0xff00) >> 8) + ((curve[i] & 0x00ff) << 8);
            trace[i] = ((DOUBLE(curve[i]) + grat_offset) * scale) + offset;
            }
         }
      else
         {
         SA_printf("O4;TA");      // Request 8-bit binary display values from trace A
                                  // (Fast, but less precise)
         static U8 curve[1001];     

         memset(curve, 0, 1001);
         memcpy(curve, GPIB_read_BIN(1001,TRUE,FALSE), 1001);

         DOUBLE offset      =  state.min_dBm;
         DOUBLE scale       = (state.max_dBm - state.min_dBm) / 250;
         DOUBLE grat_offset = 0.0;

         for (S32 i=0; i < 1001; i++)
            {
            trace[i] = ((DOUBLE(curve[i]) + grat_offset) * scale) + offset;
            }
         }
#endif

      state.dBm_values     = trace;
      state.n_trace_points = 1001;
      }
   else if (state.type == SATYPE_TEK_278X)
      {
      //
      // Take sweep, and acquire data from display trace A
      //

      static DOUBLE trace[1001];
      SA_printf("TS;");

      if (!state.hi_speed_acq)
         {
         SA_printf("O2;TROUT TRNOR");   // Request 16-bit binary display values from trace A
                                        // (Slightly slower but more precise than 8-bit binary mode)
         static U16 curve[1001];     

         memset(curve, 0, 1001 * sizeof(S16));
         memcpy(curve, GPIB_read_BIN(1001 * sizeof(S16),TRUE,FALSE), 1001 * sizeof(S16));

         GPIB_read_ASC();   // swallow trailing CR/LF

         DOUBLE offset      =  state.min_dBm;
         DOUBLE scale       = (state.max_dBm - state.min_dBm) / 1000;
         DOUBLE grat_offset = 0.0;

         for (S32 i=0; i < 1001; i++)
            {
            curve[i] = ((curve[i] & 0xff00) >> 8) + ((curve[i] & 0x00ff) << 8);
            trace[i] = ((DOUBLE(curve[i]) + grat_offset) * scale) + offset;
            }
         }
      else
         {
         SA_printf("O4;TROUT TRNOR");      // Request 8-bit binary display values from trace A
                                           // (Fast, but less precise)
         static U8 curve[1001];     

         memset(curve, 0, 1001);
         memcpy(curve, GPIB_read_BIN(1001,TRUE,FALSE), 1001);

         GPIB_read_ASC();   // swallow trailing CR/LF

         DOUBLE offset      =  state.min_dBm;
         DOUBLE scale       = (state.max_dBm - state.min_dBm) / 250;
         DOUBLE grat_offset = 0.0;

         for (S32 i=0; i < 1001; i++)
            {
            trace[i] = ((DOUBLE(curve[i]) + grat_offset) * scale) + offset;
            }
         }

      state.dBm_values     = trace;
      state.n_trace_points = 1001;
      }
   else if (state.type == SATYPE_HP8560)
      {
      //
      // Take sweep, and acquire 16-bit binary data from display trace A
      //

      static U16    curve[601];     
      static DOUBLE trace[601];

      SA_printf("TS;");
      SA_printf("TDF B;TRA?");     // Request 16-bit binary display values from trace A

      memset(curve, 0, 601 * sizeof(S16));
      memcpy(curve, GPIB_read_BIN(601 * sizeof(S16),TRUE,FALSE), 601 * sizeof(S16));

      DOUBLE offset      =  state.min_dBm;
      DOUBLE scale       = (state.max_dBm - state.min_dBm) / 600.0;
      DOUBLE grat_offset = 0.0;

      for (S32 i=0; i < 601; i++)     
         {
         curve[i] = ((curve[i] & 0xff00) >> 8) + ((curve[i] & 0x00ff) << 8);
         trace[i] = (((DOUBLE(curve[i]) + grat_offset) * scale) + offset);
         }

      state.dBm_values     = trace;
      state.n_trace_points = 601;
      }
   else if (state.type == SATYPE_HP8590)
      {
      //
      // Take sweep, and acquire 16-bit binary data from display trace A
      //

      static U16    curve[401];     
      static DOUBLE trace[401];

      SA_printf("TS;");
      SA_printf("TDF B;MDS W;TA;");     // Request 16-bit binary display values from trace A

      memset(curve, 0, 401 * sizeof(S16));
      memcpy(curve, GPIB_read_BIN(401 * sizeof(S16),TRUE,FALSE), 401 * sizeof(S16));

      for (S32 i=0; i < 401; i++)     
         {
         curve[i] = ((curve[i] & 0xff00) >> 8) + ((curve[i] & 0x00ff) << 8);
         trace[i] = state.max_dBm - ((8000.0 - ((DOUBLE) curve[i])) * 0.01);
         }

      state.dBm_values     = trace;
      state.n_trace_points = 401;
      }
   else if (state.type == SATYPE_HP70000)
      {
      //
      // Take sweep, and acquire 16-bit binary data from display trace A
      //

      static U16    curve[800];     
      static DOUBLE trace[800];

      SA_printf("TS;");
      SA_printf("MDS W;TDF B;TRA?");     // Request 16-bit binary measurement uints from trace A

      memset(curve, 0, 800 * sizeof(S16));
      memcpy(curve, GPIB_read_BIN(800 * sizeof(S16),TRUE,FALSE), 800 * sizeof(S16));

      for (S32 i=0; i < 800; i++)     
         {
         curve[i] = ((curve[i] & 0xff00) >> 8) + ((curve[i] & 0x00ff) << 8);
         trace[i] = (DOUBLE(S16(curve[i])) / 100.0) + state.A_offset; // HP 70K measurement units are dBm * 100 
         }

      state.dBm_values     = trace;
      state.n_trace_points = 800;
      }
   else if (state.type == SATYPE_HP8569B)
      {
      //
      // Take sweep, and acquire 16-bit binary data from display trace A
      //
      // This code doesn't work reliably on Prologix GPIB-Ethernet adapters,
      // although it's fine on GPIB-USB.  It's even less reliable if TS;BA are
      // transmitted on a single line, or if the Sleep(100) is omitted.  Any
      // delay between TS and BA seems to prevent the code from working at all
      //

      static DOUBLE trace[481];
      Sleep(100);           // improves reliability on GPIB-Ethernet
      SA_printf("TS");
      SA_printf("BA");      // (TS has no effect unless SS is selected manually)
                                 
      static U16 curve[481];     

      memset(curve, 0, 481 * sizeof(S16));
      memcpy(curve, GPIB_read_BIN(481 * sizeof(S16),TRUE,FALSE), 481 * sizeof(S16));

      DOUBLE offset      =  state.min_dBm;
      DOUBLE scale       = (state.max_dBm - state.min_dBm) / 800;
      DOUBLE grat_offset = 0.0;

      for (S32 i=0; i < 481; i++)
         {
         curve[i] = ((curve[i] & 0xff00) >> 8) + ((curve[i] & 0x00ff) << 8);
         trace[i] = ((DOUBLE(curve[i]) + grat_offset) * scale) + offset;
         }

      state.dBm_values     = trace;
      state.n_trace_points = 481;
      }
   else if (state.type == SATYPE_HP3585)
      {
      //
      // Take sweep, and acquire binary data from display trace B
      //

      SA_printf("TB0S2");      // Disable trace 'B' visibility from last sweep, and execute the next sweep

      for (;;)                 // Poll status word to detect end-of-sweep condition
         {
         if (interruptable_sleep(100)) break;

         SA_printf("D6T4");

         U16 status = 0;
         memcpy(&status, GPIB_read_BIN(sizeof(status)), sizeof(status));

         if (!(status & 0x80))
            {
            break;
            }

         if (interruptable_sleep(100)) break;
         }

      SA_printf("SABO");       // Store trace A to trace B, then initiate binary dump of trace B

      static U16 curve[1002];  // first 16-bit word ignored (all 1s) 
      static DOUBLE trace[1002];
                                 
      memset(curve, 0, 1002 * sizeof(S16));
      memcpy(curve, GPIB_read_BIN(1002 * sizeof(S16),TRUE,FALSE), 1002 * sizeof(S16));

      DOUBLE offset      =  state.min_dBm;
      DOUBLE scale       = (state.max_dBm - state.min_dBm) / 1000;    // bottom=0, ref=1000, top=1023
      DOUBLE grat_offset = 0.0;

      for (S32 i=0; i < 1001; i++)
         {
         curve[i+1] = ((curve[i+1] & 0xff00) >> 8) + ((curve[i+1] & 0x0003) << 8);
         trace[i] = ((DOUBLE(curve[i+1]) + grat_offset) * scale) + offset;
         }

      state.dBm_values     = trace;
      state.n_trace_points = 1001;
      }
   else if ((state.type == SATYPE_HP358XA) || 
            (state.type == SATYPE_E4406A)  ||
            (state.type == SATYPE_FSE)     ||
            (state.type == SATYPE_FSU)     ||
            (state.type == SATYPE_FSP))
      {
      //
      // Abort any sweep in progress and trigger the next one, then wait
      // for it to complete and request a trace dump
      //
      // On supported models, turn off the front-panel display if high-speed acquisition
      // is requested
      //

      if ((state.type == SATYPE_FSE) ||
          (state.type == SATYPE_FSU) ||
          (state.type == SATYPE_FSP))
         {
         if (state.hi_speed_acq == state.FSP_display_state)
            {
            state.FSP_display_state = !state.hi_speed_acq;

            if (state.FSP_display_state)
               GPIB_query(":SYST:DISP:UPD ON;*OPC?");          
            else
               GPIB_query(":SYST:DISP:UPD OFF;*OPC?");          
            }

         GPIB_puts(":ABOR;:INIT:IMM;*WAI;:TRAC:DATA? TRACE1");
         }
      else if (state.type == SATYPE_E4406A)
         {
         if (state.hi_speed_acq == state.VSA_display_state)
            {
            state.VSA_display_state = !state.hi_speed_acq;

            if (state.VSA_display_state)
               GPIB_query(":DISP:ENAB 1;*OPC?");
            else
               GPIB_query(":DISP:ENAB 0;*OPC?");
            }

         GPIB_puts(":ABOR;:INIT:IMM;*WAI;:FETCH:SPEC4?");
         }
      else
         {
         GPIB_puts(":ABOR;:INIT:IMM;*WAI;:CALC1:DATA?");
         }

      //
      // Read preamble, and verify that the data size
      // is what we expect
      //
      // (If n_expected_trace_points is zero, we're making an initial call to see how many
      // points the E4406A VSA will actually return, so don't flag as an error)
      //

      S8 *preamble = (S8 *) GPIB_read_BIN(2);
      assert(preamble[0] == '#');
      S32 n_preamble_bytes = preamble[1] - '0';

      C8 *size_text = GPIB_read_ASC(n_preamble_bytes);

      S32 size_bytes = atoi(size_text);

      if (n_expected_trace_points == 0)
         {
         n_expected_trace_points = size_bytes / sizeof(SINGLE);
         }

      assert(size_bytes == n_expected_trace_points * sizeof(SINGLE));

      //
      // Acquire 32-bit IEEE754 float data from display trace A
      //
      // On the HP 3588A/3589A, trace data arrives in big-endian format, so we'll 
      // store it in a 32-bit int and byte-swap it
      //

      static DOUBLE trace[MAX_N_PTS];

      if ((state.type == SATYPE_FSE) ||
          (state.type == SATYPE_FSU) ||
          (state.type == SATYPE_FSP) ||
          (state.type == SATYPE_E4406A))
         {
         static SINGLE curve[MAX_N_PTS];

         memset(curve, 
                0, 
                n_expected_trace_points * sizeof(SINGLE));

         memcpy(curve, 
                GPIB_read_BIN(n_expected_trace_points * sizeof(SINGLE),
                              TRUE,
                              FALSE), 
                n_expected_trace_points * sizeof(SINGLE));

         for (S32 i=0; i < n_expected_trace_points; i++)     
            {
            trace[i] = curve[i] + state.A_offset;
            }
         }
      else
         {
         static U32 curve[MAX_N_PTS];

         memset(curve, 
                0, 
                n_expected_trace_points * sizeof(U32));

         memcpy(curve, 
                GPIB_read_BIN(n_expected_trace_points * sizeof(U32),
                              TRUE,
                              FALSE), 
                n_expected_trace_points * sizeof(U32));

         for (S32 i=0; i < n_expected_trace_points; i++)     
            {
            U32 swapped = (((curve[i] & 0x000000ff) << 24) |
                           ((curve[i] & 0x0000ff00) << 8)  |
                           ((curve[i] & 0x00ff0000) >> 8)  |
                           ((curve[i] & 0xff000000) >> 24));

            trace[i] = (*(SINGLE *) &swapped) + state.A_offset;
            }
         }

      //
      // Swallow trailing CR/LF
      //

      GPIB_read_ASC();
//      C8 *foo = GPIB_read_BIN(1);
//      printf("Term = %d %d %d\n", foo[0], foo[1], foo[2]);

      state.dBm_values     = trace;
      state.n_trace_points = n_expected_trace_points;
      }
   else if ((state.type == SATYPE_R3267_SERIES) || 
            (state.type == SATYPE_R3132_SERIES) ||
            (state.type == SATYPE_R3465_SERIES))
      {
      //
      // Ensure analyzer will generate the expected # of points, based on the
      // user's trace-resolution setting
      //

      S32 n_src_points = state.hi_speed_acq ? 501 : 1001;

      if (n_src_points != cur_n_points)
         {
         cur_n_points = n_src_points;

         SA_printf((cur_n_points == 501) ? "TPS" : "TPL");
         GPIB_flush_receive_buffers();
         }

      //
      // Take sweep, and acquire 16-bit binary data from display trace A
      // (No CR/LF termination is expected here, since DL2 mode has been set)
      //

      static U16    curve[1001];     
      static DOUBLE trace[1001];

      SA_printf("TS;TBA?");

      memset(curve, 0, n_src_points * sizeof(S16));
      memcpy(curve, GPIB_read_BIN(n_src_points * sizeof(S16),TRUE,FALSE), n_src_points * sizeof(S16));

      DOUBLE offset      =  state.min_dBm;
      DOUBLE scale       = (state.max_dBm - state.min_dBm) / 12800;   // bottom=1792, ref=14592, top=14592+
      DOUBLE grat_offset = -1792.0;

      for (S32 i=0; i < n_src_points; i++)     
         {
         curve[i] = ((curve[i] & 0xff00) >> 8) + ((curve[i] & 0x00ff) << 8);
         trace[i] = ((DOUBLE(curve[i]) + grat_offset) * scale) + offset;
         }

      state.dBm_values     = trace;
      state.n_trace_points = n_src_points;
      }
   else if (state.type == SATYPE_R3131)
      {
      //
      // Take sweep, and acquire 16-bit binary data from display trace A
      // (No CR/LF termination is expected here, since DL2 mode has been set)
      //

      static U16    curve[501];     
      static DOUBLE trace[501];

      SA_printf("TS;TBA?");

      memset(curve, 0, 501 * sizeof(S16));
      memcpy(curve, GPIB_read_BIN(501 * sizeof(S16),TRUE,FALSE), 501 * sizeof(S16));

      DOUBLE offset      =  state.min_dBm;
      DOUBLE scale       = (state.max_dBm - state.min_dBm) / 2880;   // bottom=512, ref=3392, top=3392+
      DOUBLE grat_offset = -512.0;

      for (S32 i=0; i < 501; i++)     
         {
         curve[i] = ((curve[i] & 0xff00) >> 8) + ((curve[i] & 0x00ff) << 8);
         trace[i] = ((DOUBLE(curve[i]) + grat_offset) * scale) + offset;
         }

      state.dBm_values     = trace;
      state.n_trace_points = 501;
      }
   else if ((state.type == SATYPE_R3261_R3361) ||
            (state.type == SATYPE_R3265_R3271))
      {
      //
      // Take sweep, and acquire 16-bit binary data from display trace A
      // (No CR/LF termination is expected here, since DL2 mode has been set)
      //

      static U16    curve[701];     
      static DOUBLE trace[701];

      S32 result = 0;

      SA_printf("S2");     // Clear status byte and start sweep  
      SA_printf("SR");     // (must be on separate lines)

      do                   // Wait for end of sweep
         {
         result = GPIB_serial_poll();

         if (interruptable_sleep(30))
            {
            state.n_trace_points = 0;
            state.dBm_values = NULL;
            return;
            }
         }
      while (!result);

      SA_printf("TBA?");   // Request binary trace data transfer

      memset(curve, 0, 701 * sizeof(S16));
      memcpy(curve, GPIB_read_BIN(701 * sizeof(S16),TRUE,FALSE), 701 * sizeof(S16));

      DOUBLE offset      =  state.min_dBm;
      DOUBLE scale       = (state.max_dBm - state.min_dBm) / 400.0;   // bottom=0, ref=400
      DOUBLE grat_offset =  0.0;

      for (S32 i=0; i < 701; i++)     
         {
         curve[i] = ((curve[i] & 0xff00) >> 8) + ((curve[i] & 0x00ff) << 8);
         trace[i] = ((DOUBLE(curve[i]) + grat_offset) * scale) + offset;
         }

      state.dBm_values     = trace;
      state.n_trace_points = 701;
      }
   else if (state.type == SATYPE_MS8604A)
      {
      //
      // Ensure analyzer will generate the expected # of points, based on the
      // user's trace-resolution setting
      //

      S32 n_src_points = state.hi_speed_acq ? 501 : 1002;

      if (n_src_points != cur_n_points)
         {
         cur_n_points = n_src_points;

         SA_printf((cur_n_points == 501) ? "DPOINT NRM" : "DPOINT DOUBLE");
         GPIB_flush_receive_buffers();
         }

      //
      // Take sweep, and acquire 16-bit binary data
      //

      static U16    curve[1002];     
      static DOUBLE trace[1002];

      SA_printf("SWP;XMA? 0,%d", n_src_points);

      memset(curve, 0, n_src_points * sizeof(S16));
      memcpy(curve, GPIB_read_BIN(n_src_points * sizeof(S16)), n_src_points * sizeof(S16));

      GPIB_read_BIN(1);    // swallow LF terminator

      for (S32 i=0; i < n_src_points; i++)     
         {
         curve[i] = ((curve[i] & 0xff00) >> 8) + ((curve[i] & 0x00ff) << 8);
         trace[i] = (DOUBLE(S16(curve[i])) / 100.0) + state.A_offset; // MS8604A measurement units are dBm * 100 
         }

      state.dBm_values     = trace;
      state.n_trace_points = n_src_points;
      }
   else if (state.type == SATYPE_MS265X_MS266X)
      {
      //
      // Take sweep, and acquire 16-bit binary data
      //

      static U16    curve[501];     
      static DOUBLE trace[501];

      SA_printf("SWP;XMA? 0,501");

      memset(curve, 0, sizeof(curve));
      memcpy(curve, GPIB_read_BIN(sizeof(curve)), sizeof(curve));

      GPIB_read_BIN(1);    // swallow LF terminator

      for (S32 i=0; i < 501; i++)     
         {
         curve[i] = ((curve[i] & 0xff00) >> 8) + ((curve[i] & 0x00ff) << 8);
         trace[i] = (DOUBLE(S16(curve[i])) / 100.0) + state.A_offset; // MS256X/MS266X measurement units are dBm * 100 
         }

      state.dBm_values     = trace;
      state.n_trace_points = 501;
      }
   else if (state.type == SATYPE_N9900)
      {
      //
      // Trigger a sweep and request trace dump when it's finished
      //

      SA_printf(":INIT:IMM;*WAI;:TRAC1:DATA?");

      //
      // Read preamble, and verify that the data size
      // is what we expect
      //

      S8 *preamble = (S8 *) GPIB_read_BIN(2);
      assert(preamble[0] == '#');
      S32 n_preamble_bytes = preamble[1] - '0';

      C8 *size_text = GPIB_read_ASC(n_preamble_bytes);

      assert(atoi(size_text) == (S32) (n_expected_trace_points * sizeof(SINGLE)));

      //
      // Acquire 32-bit LE IEEE754 float data from trace and swallow trailing newline
      //

      static DOUBLE trace[MAX_N_PTS];
      static SINGLE curve[MAX_N_PTS];

      memset(curve, 
             0, 
             n_expected_trace_points * sizeof(SINGLE));

      memcpy(curve,
             GPIB_read_BIN(n_expected_trace_points * sizeof(SINGLE),
                           TRUE,
                           FALSE), 
             n_expected_trace_points * sizeof(SINGLE));

      GPIB_read_ASC();

      for (S32 i=0; i < n_expected_trace_points; i++)     
         {
         trace[i] = curve[i] + state.A_offset;
         }

      state.dBm_values     = trace;
      state.n_trace_points = n_expected_trace_points;
      }
   else if (state.type == SATYPE_SCPI)
      {
      //
      // Trigger a sweep and request trace dump when it's finished
      //

      SA_printf(":INIT:IMM;*WAI;:TRAC:DATA? TRACE1");

      //
      // Read preamble, and verify that the data size
      // is what we expect
      //

      S8 *preamble = (S8 *) GPIB_read_BIN(2);
      assert(preamble[0] == '#');
      S32 n_preamble_bytes = preamble[1] - '0';

      C8 *size_text = GPIB_read_ASC(n_preamble_bytes);

      assert(atoi(size_text) == (S32) (n_expected_trace_points * sizeof(S32)));

      //
      // Acquire 32-bit integer data from display trace A
      // 

      static S32 curve[MAX_N_PTS];

      memset(curve, 
             0, 
             n_expected_trace_points * sizeof(S32));

      memcpy(curve, 
             GPIB_read_BIN(n_expected_trace_points * sizeof(S32),
                           TRUE,
                           FALSE), 
             n_expected_trace_points * sizeof(S32));

      //
      // Swallow trailing newline
      //

      GPIB_read_ASC();

      //
      // Convert trace to floating-point format (much faster on host PC
      // than on target analyzer, hence our use of the integer format)
      //

      static DOUBLE trace[MAX_N_PTS];

      for (S32 i=0; i < n_expected_trace_points; i++)     
         {
         trace[i] = (DOUBLE(curve[i]) / 1000.0) + state.A_offset;
         }

      state.dBm_values     = trace;
      state.n_trace_points = n_expected_trace_points;
      }
   else
      {
      //
      // Simulated data source for testing
      //

      static DOUBLE trace[N_SIM_PTS];
      static DOUBLE rads = 0.0;

      DOUBLE r = rads;

      //
      // Advance source phase
      //

      rads += 0.06283F;

      if (rads > 6.283F)
         {
         rads -= 6.283F;
         }

      //
      // Fill array with sine waveform beginning at current phase
      //

      for (S32 i=0; i < N_SIM_PTS; i++)
         {
         trace[i] = DOUBLE(((sin(r) + 1.0) * 40.0) - 90.0) + state.A_offset;

         r += (62.83F / DOUBLE(N_SIM_PTS));

         if (r > 6.283F)
            {
            r -= 6.283F;
            }
         }

      state.n_trace_points = N_SIM_PTS;
      state.dBm_values     = trace;
      }
}

//****************************************************************************
//
// Resample analyzer trace array to specified # of data points
//
//****************************************************************************

void WINAPI SA_resample_data(DOUBLE *src,  S32 ns, //)
                             DOUBLE *dest, S32 nd,
                             RESAMPLE_OP operation)
{
   if (nd == ns)
      {
      //
      // If unity sampling, use point-sampled case
      //

      operation = RT_POINT;
      }
   else if ((nd > ns) && (operation != RT_POINT))
      {
      //
      // Max/min/avg combining works only when one or more source points 
      // contribute to each destination point
      //

      operation = RT_SPLINE;
      }

   switch (operation)
      {
      case RT_POINT:
         {
         DOUBLE dp = DOUBLE(ns-1) / DOUBLE(nd);
         DOUBLE s  = 0.5F;

         for (S32 d=0; d < nd; d++)
            {
            S32 bin = S32(s);

            dest[d] = src[bin];
            s += dp;
            }
         break;
         }

      case RT_MAX:
         {
         DOUBLE dp = DOUBLE(nd) / DOUBLE(ns);
         DOUBLE d  = 0.5F;

         DOUBLE m        = -FLT_MAX;
         S32    last_bin = 0;

         S32 s = 0;

         while (1)
            {
            if (s >= ns)
               {
               s = ns-1;
               }

            if (src[s] > m)
               {
               m = src[s];
               }

            s++;

            S32 bin = S32(d);
            d += dp;

            if (bin != last_bin)
               {
               dest[last_bin] = m;
               m = -FLT_MAX;

               if (last_bin == nd-1)
                  {
                  break;
                  }

               last_bin = bin;
               }
            }

         break;
         }

      case RT_MIN:
         {
         DOUBLE dp = DOUBLE(nd) / DOUBLE(ns);
         DOUBLE d  = 0.5F;

         DOUBLE m        = FLT_MAX;
         S32    last_bin = 0;

         S32 s = 0;

         while (1)
            {
            if (s >= ns)
               {
               s = ns-1;
               }

            if (src[s] < m)
               {
               m = src[s];
               }

            s++;

            S32 bin = S32(d);
            d += dp;

            if (bin != last_bin)
               {
               dest[last_bin] = m; 
               m = FLT_MAX;

               if (last_bin == nd-1)
                  {
                  break;
                  }

               last_bin = bin;
               }
            }

         break;
         }

      case RT_AVG:
         {
         DOUBLE dp = DOUBLE(nd) / DOUBLE(ns);
         DOUBLE d  = 0.5F;

         S32    n        = 0;
         DOUBLE m        = 0.0;
         S32    last_bin = 0;

         S32 s = 0;

         while (1)
            {
            if (s >= ns)
               {
               s = ns-1;
               }

            m += src[s];
            n++;
            s++;

            S32 bin = S32(d);
            d += dp;

            if (bin != last_bin)
               {
               dest[last_bin] = m / n;
               m = 0.0;
               n = 0;

               if (last_bin == nd-1)
                  {
                  break;
                  }

               last_bin = bin;
               }
            }

         break;
         }

      case RT_SPLINE:
         {
         ispline(src, ns, dest, nd);
         break;
         }

      default:
         break;
      }
}

