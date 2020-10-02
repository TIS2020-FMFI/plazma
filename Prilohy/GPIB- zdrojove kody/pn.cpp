//
// PN: HP 85671A-style noise measurement utility
//
// 13-Jul-05 John Miles, KE5FX
// john@miles.io
//

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <mmsystem.h>
#include <shellapi.h>
#include <shlobj.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <io.h>
#include <fcntl.h>
#include <assert.h>
#include <limits.h>

#include "typedefs.h"
#include "pnres.h"
#include "w32sal.h"
#include "winvfx.h"
#include "gpiblib.h"
#include "chartype.h"
#include "gfxfile.cpp"

#define VERSION "1.51"

C8 szAppName[] = "KE5FX Noise Measurement Utility";

//
// Configuration equates
//

const S32 MAX_SOURCES       = 8;
const S32 MAX_DECADE_EXP    = 7;          // 10 MHz-100 MHz
const S32 MIN_DECADE_EXP    = -6;         // 1 uHz-10 uHz
const S32 NUM_DECADES       = (MAX_DECADE_EXP - MIN_DECADE_EXP + 1);
const S32 MAX_POINTS        = 2000;
const S32 MAX_GRAPH_WIDTH   = 1280;
const S32 MAX_LEGEND_FIELDS = 20;
const S32 MAX_TMPFILES      = 8;
const S32 MAX_SCPI_POINTS   = 8192;

#define MIN_SPUR_CLIP_DB  3
#define MAX_SPUR_CLIP_DB  100

#define BINARY_ACQ        1

//
// Window handle created by SAL
//

HWND hWnd;
HINSTANCE hInstance;

//
// App's target window resolution
//

S32 RES_X = 800;
S32 RES_Y = 600;

S32 requested_RES_X = 800;
S32 requested_RES_Y = 600;

//
// Windows/panes used by app
//

VFX_WINDOW *stage_window = NULL;
PANE       *stage = NULL;
PANE       *trace_area = NULL;
PANE       *image = NULL;

VFX_WINDOW *screen_window = NULL;
PANE       *screen = NULL;

S32 font_height;
S32 em_width;

U16 transparent_font_CLUT[256];

//
// PN.INI
//

C8 INI_path[MAX_PATH];

S32 INI_res_x = 640;
S32 INI_res_y = 480;
S32 INI_auto_print_mode = 0;
C8  INI_auto_save[MAX_PATH] = "";
C8  INI_save_directory[MAX_PATH] = "";
S32 INI_default_smoothing = 32;
SINGLE INI_default_spot_offset_Hz = 10000;
SINGLE INI_default_L_offset_Hz = 10000;
SINGLE INI_default_U_offset_Hz = 100000;
S32 INI_default_contrast = 1;
S32 INI_default_ghost_sample = 1;
S32 INI_default_thick_trace = 1;
S32 INI_default_allow_clipping = 1;
S32 INI_default_alt_smoothing = 0;
S32 INI_default_spur_reduction = 0;
S32 INI_default_spur_dB = 20;
S32 INI_default_sort_modify_date = 0;

S32 INI_show_legend_field[MAX_LEGEND_FIELDS];

S32 INI_DLG_source_ID = -1;
C8  INI_DLG_caption[256] = "";
C8  INI_DLG_carrier[256] = "";
C8  INI_DLG_LO[256] = "";
C8  INI_DLG_IF[256] = "";
C8  INI_DLG_carrier_ampl[256] = "";
C8  INI_DLG_VBF[256] = "";
C8  INI_DLG_NF[256] = "";
C8  INI_DLG_min[256] = "";
C8  INI_DLG_max[256] = "";
C8  INI_DLG_mult[256] = "";
S32 INI_DLG_GPIB = 18;
S32 INI_DLG_clip = 10;
S32 INI_DLG_alt = 0;
S32 INI_DLG_0dB = 0;

//
// Misc. globals
//

#define PI 3.1415926535

S32 mouse_x = 0;
S32 mouse_y = 0;
S32 left_button = 0;
S32 right_button = 0;
S32 last_left_button = 0;
S32 last_right_button = 0;

BOOL RMS_needed = FALSE;

S32 sort_modify_date;
S32 contrast;
S32 ghost_sample;
S32 thick_trace;
S32 allow_clipping;
S32 alt_smoothing;
S32 spur_reduction;
S32 spur_dB;
S32 background_invalid;
S32 force_redraw;
S32 invalid_trace_cache;

S32 GPIB_in_progress = 0;
C8  cleanup_filename[MAX_PATH] = "";

S32 refresh_cnt = 0;

C8 text[1024];

S32 smooth_samples;
SINGLE spot_offset_Hz;
SINGLE L_offset_Hz;
SINGLE U_offset_Hz;

FILE *out;
BOOL key_hit = FALSE;

//
// UI state
//

S32 spot_range_x0 = 0;
S32 spot_range_y0 = 0;
S32 spot_range_x1 = 0;
S32 spot_range_y1 = 0;
SINGLE spot_col_freq[MAX_GRAPH_WIDTH];
S32    spot_col_x   [MAX_GRAPH_WIDTH];
S32 n_spot_cols = 0;

HMENU hmenu;

enum
{
   M_OVERLAY,        // Overlay multiple sources in list 
   M_BROWSE          // Browse multiple sources          
};

S32 mode = M_OVERLAY;    

//
// Colors for each successive graph
// First array element is saturation (2=darkest/most saturated, 0=lightest/least saturated)
//

#define N_PEN_COLORS 8
#define N_PEN_LEVELS 3

S32 pen_colors[N_PEN_LEVELS][N_PEN_COLORS] = 
                  {{ 
                   RGB_TRIPLET(230,230,255),     // Blue        
                   RGB_TRIPLET(255,230,255),     // Magenta     
                   RGB_TRIPLET(230,255,230),     // Dark green  
                   RGB_TRIPLET(255,230,230),     // Red         
                   RGB_TRIPLET(200,240,255),     // Dark cyan   
                   RGB_TRIPLET(255,240,200),     // Dark yellow 
                   RGB_TRIPLET(200,240,200),     // Lime green  
                   RGB_TRIPLET(230,230,230)      // Gray        
                   },
                   { 
                   RGB_TRIPLET(200,200,225),     
                   RGB_TRIPLET(225,200,225),     
                   RGB_TRIPLET(200,225,200),     
                   RGB_TRIPLET(225,200,200),     
                   RGB_TRIPLET(170,210,225),     
                   RGB_TRIPLET(225,210,170),     
                   RGB_TRIPLET(170,210,170), 
                   RGB_TRIPLET(200,200,200) 
                   },
                   { 
                   RGB_TRIPLET(0,0,255),         
                   RGB_TRIPLET(255,0,255),       
                   RGB_TRIPLET(20,147,20),       
                   RGB_TRIPLET(255,0,0),         
                   RGB_TRIPLET(0,192,255),       
                   RGB_TRIPLET(255,192,0),       
                   RGB_TRIPLET(0,255,0),         
                   RGB_TRIPLET(120,120,120)      
                   }};

//
// Utility macros
//

extern "C" DXDEC void WINAPI SAL_set_application_icon (C8 *resource);

#define FTOI(x) (S32((x)+0.5F))

//
// File menu
//

#define IDM_LOAD_PNP     1
#define IDM_SAVE         2
#define IDM_EXPORT       3
                         
#define IDM_PRINT_IMAGE  4

#define IDM_SORT_MODIFY  5

#define IDM_CLOSE        6
#define IDM_CLOSE_ALL    7
#define IDM_DELETE       8
                         
#define IDM_QUIT         9
                         
//                       
// View menu             
//                       
                         
#define IDM_REFRESH      100
                         
#define IDM_BROWSE       101
#define IDM_OVERLAY      102
                         
#define IDM_NEXT         103
#define IDM_PREV         104

#define IDM_MOVE_UP      105
#define IDM_MOVE_DOWN    106

#define IDM_VIS          107

//                       
// Display menu
//

#define IDM_384             200
#define IDM_400             201
#define IDM_480             202
#define IDM_600             203
#define IDM_768             204
#define IDM_864             205
#define IDM_1024            206
#define IDM_LOW_CONTRAST    210
#define IDM_MED_CONTRAST    211
#define IDM_HIGH_CONTRAST   212

//
// Trace menu
//

#define IDM_S_NONE        300
#define IDM_S_2           301
#define IDM_S_4           302
#define IDM_S_8           303
#define IDM_S_16          304
#define IDM_S_24          305
#define IDM_S_32          306
#define IDM_S_48          307
#define IDM_S_64          308
#define IDM_S_96          309
#define IDM_S_ALT         315
#define IDM_SPUR          316
#define IDM_EDIT_SPUR     317

#define IDM_GHOST          320
#define IDM_THICK_TRACE    321
#define IDM_ALLOW_CLIPPING 322
#define IDM_EDIT_CAPTION   330

//
// Legend menu and associated definitions
// Must increase MAX_LEGEND_FIELDS when adding entries! 
//

enum LEGEND_FIELD
{
   T_CAPTION,
   T_FILENAME,
   T_CARRIER_FREQ,
   T_CARRIER_AMP,
   T_SPOT_AMP,
   T_RESID_FM,
   T_JITTER_DEGS,
   T_JITTER_RADS,
   T_JITTER_SECS,
   T_CNR,
   T_EXT_MULT,
   T_EXT_LO,
   T_EXT_IF,
   T_RF_ATTEN,
   T_CLIP_DB,
   T_VB_FACTOR,
   T_SWEEP_TIME,
   T_TIMESTAMP,
   T_INSTRUMENT,
   T_INSTRUMENT_REV,
   T_END_OF_TABLE
};

C8 legend_captions[MAX_LEGEND_FIELDS][64] = 
{ 
  "Trace",
  "Filename",
  "Carrier Hz",
  "Carrier dBm",
  "dBc/Hz at Spot Offset",
  "Residual FM Hz",
  "RMS Integrated Noise (Degs)",
  "RMS Integrated Noise (Rads)",
  "RMS Time Jitter",
  "SSB Carrier/Noise dB",
  "Ext Mult",
  "Ext LO Hz",
  "Ext IF Hz",
  "RF Atten dB",
  "Clip dB",
  "VBW/RBW",
  "Sweep",
  "Time/Date",
  "Instrument",
  "Rev",
};

C8 working_captions[MAX_LEGEND_FIELDS][64];

#define IDM_S_CAPTION        400
#define IDM_S_FILENAME       401
#define IDM_S_CARRIER_FREQ   402
#define IDM_S_CARRIER_AMP    403   
#define IDM_S_SPOT_AMP       404 
#define IDM_S_RESID_FM       405
#define IDM_S_J_DEGS         406
#define IDM_S_J_RADS         407 
#define IDM_S_J_SECS         408 
#define IDM_CNR              409 
#define IDM_S_EXT_MULT       410 
#define IDM_S_EXT_LO         411
#define IDM_S_EXT_IF         412
#define IDM_S_RF_ATTEN       413 
#define IDM_S_CLIP           414
#define IDM_S_VB_FACTOR      415   
#define IDM_S_SWEEP_TIME     416   
#define IDM_S_TIMESTAMP      417   
#define IDM_S_INSTRUMENT     418 
#define IDM_S_INSTRUMENT_REV 419 

#define IDM_ALL              450
#define IDM_NONE             451

C8 *legend_INI_names[MAX_LEGEND_FIELDS] = 
{ 
  "show_trace",
  "show_filename",
  "show_carrier_freq",
  "show_carrier_ampl",
  "show_spot_cursor",
  "show_resid_FM",
  "show_jitter_degs",
  "show_jitter_rads",
  "show_jitter_secs",
  "show_CNR",
  "show_external_mult",
  "show_external_LO",
  "show_external_IF",
  "show_RF_atten",
  "show_clip_dB",
  "show_VB_factor",
  "show_sweep_time",
  "show_timestamp",
  "show_instrument",
  "show_instrument_rev",
};

//
// Acquire menu
//

#define FIRST_ACQ_SOURCE  500
#define IDM_TEK490        500
#define IDM_TEK2780       501
#define IDM_HP8566A       502
#define IDM_HP8567A       503
#define IDM_HP8568A       504
#define IDM_HP8566B       505
#define IDM_HP8567B       506
#define IDM_HP8568B       507
#define IDM_HP8560A       508
#define IDM_HP8560E       509
#define IDM_HP8590        510
#define IDM_HP70000       511
#define IDM_HP3585        512
#define IDM_HP358XA       513
#define IDM_HP4195A       514
#define IDM_HP4396A       515
#define IDM_E4400         516
#define IDM_E4406A        517
#define IDM_R3131         518
#define IDM_R3132         519
#define IDM_R3261         520
#define IDM_R3465         521
#define IDM_R3267         522
#define IDM_TR417X        523
#define IDM_MS8604A       524
#define IDM_MS2650        525
#define IDM_FSIQ          526
#define IDM_FSP           527
#define IDM_FSQ           528
#define IDM_FSU           529
#define IDM_REPEAT_LAST   550

#define MAX_ACQUIRE_OPTIONS 30

C8 *acquire_options[MAX_ACQUIRE_OPTIONS] =
{
   "Tektronix 490P or 2750P series ...", 
   "Tektronix 2780 series ...",
   "HP 8566A ...",   
   "HP 8567A ...",              
   "HP 8568A ...",              
   "HP 8566B ...",   
   "HP 8567B ...",              
   "HP 8568B ...",              
   "HP 8560A-series portables ...",         
   "HP 8560E/EC-series portables ...",      
   "HP 8590-series portables ...",          
   "HP 70000 series ...",
   "HP 3585A/B ...",
   "HP 3588A or 3589A ...",
   "HP 4195A ...",
   "HP 4396A/B ...",
   "Agilent E4400 PSA/ESA series...",
   "Agilent E4406A VSA ...",
   "Advantest R3131 ...",
   "Advantest R3132, R3162, R3172, or R3182 ...",
   "Advantest R3261, R3361, R3265, or R3271 ...",
   "Advantest R3463, R3465, R3263, or R3272 ...",
   "Advantest R3264, R3267, or R3273 ...",
   "Advantest TR417x series ...",
   "Anritsu MS8600A series ...",
   "Anritsu MS2650/MS2660 series ...",
   "Rohde && Schwarz FSEA/B/M/K or FSIQ ...",
   "Rohde && Schwarz FSP ...",
   "Rohde && Schwarz FSQ ...",
   "Rohde && Schwarz FSU ...",
};

//
// Help menu
//

#define IDM_USER_GUIDE    600
#define IDM_ABOUT         601

//****************************************************************************
//*                                                                          *
//* Utility functions                                                        *
//*                                                                          *
//****************************************************************************

S32 round_to_nearest(DOUBLE f)
{
   if (f >= 0.0) 
      f += 0.5;
   else
      f -= 0.5;

   return S32(f);
}

bool epsilon_match(DOUBLE a, DOUBLE b)
{
   DOUBLE absa = fabs(a);
   DOUBLE absb = fabs(b);

   DOUBLE epsilon = min(absa,absb) / 100.0;

   if (epsilon < 1E-9) epsilon = 1E-9;      // arbitrary threshold in case a or b is 0
   
   return (fabs(a-b) < epsilon);
}

DOUBLE round_to_nearest_double(DOUBLE val, S32 places)    // e.g., places=2 rounds to nearest 0.01, 0 rounds to nearest int, -2 rounds to nearest 100
{
   DOUBLE sd  = pow(10.0, places);

   if (val < 0)
      return ceil((val * sd) - 0.5) / sd; 
   else
      return floor((val * sd) + 0.5) / sd; 
}

//
// Simple and fast atof (ascii to float) function.
//
// - Executes about 5x faster than standard MSCRT library atof().
// - An attractive alternative if the number of calls is in the millions.
// - Assumes input is a proper integer, fraction, or scientific format.
// - Matches library atof() to 15 digits (except at extreme exponents).
// - Only enough error checking to flag obvious non-numeric values.
//
// 09-May-2009 Tom Van Baak (tvb) www.LeapSecond.com
//

double fast_atof (char **src, bool *valid = NULL)
{
    unsigned char *p = (unsigned char *) *src;
    if (valid) *valid = false;

    // Skip leading white space, if any.

    while (is_whitespace[*p] ) {
        p += 1;
    }

    // Get sign, if any.

    double sign = 1.0;
    if (*p == '-') {
        sign = -1.0;
        p += 1;

    } else if (*p == '+') {
        p += 1;
    }

    // Get digits before decimal point or exponent, if any.

    unsigned char *check = p;

    double value = 0.0;
    while (is_digit[*p]) {
        value = value * 10.0 + (*p - '0');
        p += 1;
    }

    // Get digits after decimal point, if any.

    if (*p == '.') {
        double pow10 = 10.0;
        p += 1;

        while (is_digit[*p]) {
            value += (*p - '0') / pow10;
            pow10 *= 10.0;
            p += 1;
        }
    }

    // If neither of the above loops fetched any characters, this isn't
    // a valid real

    if (p == check)
      {
      return 0.0;
      }

    if (valid) *valid = true;

    // Handle exponent, if any.

    int frac = 0;
    double scale = 1.0;
    if ((*p == 'e') || (*p == 'E')) {
        unsigned int expon;
        p += 1;

        // Get sign of exponent, if any.

        frac = 0;
        if (*p == '-') {
            frac = 1;
            p += 1;

        } else if (*p == '+') {
            p += 1;
        }

        // Get digits of exponent, if any.

        expon = 0;
        while (is_digit[*p]) {
            expon = expon * 10 + (*p - '0');
            p += 1;
        }
        if (expon > 308) expon = 308;

        // Calculate scaling factor.

        while (expon >= 50) { scale *= 1E50; expon -= 50; }
        while (expon >=  8) { scale *= 1E8;  expon -=  8; }
        while (expon >   0) { scale *= 10.0; expon -=  1; }
    }

    // Skip trailing white space, if any.

    while (is_whitespace[*p] ) {
        p += 1;
    }

    // Return signed and scaled floating point result, and a
    // pointer to the next field on the line (or its null 
    // terminator)

    *src = (char *) p;

    return sign * (frac ? (value / scale) : (value * scale));
}

S32 string_width(C8 *text)
{
   S32 len = 0;

   C8 ch;

   while ((ch = *text++) != 0)
      {
      len += VFX_character_width(VFX_default_system_font(), 
                                 ch);
      }

   return len;
}

C8 *Hz_string(DOUBLE val)
{
   //
   // First, round positive frequency to 64-bit int
   //

   S64 Hz = (S64) (val + 0.5);

   //
   // Convert to integer by shifting digits off the LSD end, inserting a space
   // after every three 
   // 

   static C8 text[256];
   text[255] = 0;
   C8 *ptr = &text[254];

   *ptr = '0';

   S32 digits = 0;

   while (Hz > 0)
      {
      *ptr = (C8) ((Hz % 10) + '0');
      Hz /= 10;

      if (Hz > 0)
         {
         ptr--;
         digits++;

         if (digits == 3)
            {
            *ptr = ' ';
            ptr--;
            digits = 0;
            }
         }
      }

   return ptr;
}

C8 *log_Hz_string(DOUBLE Hz)
{
   static C8 text[256];
   memset(text, 0, sizeof(text));

   if (fabs(Hz) < 1E-9) 
      {
      return "0";
      }

   //
   // Identify decade to which frequency value belongs (e.g, 1234 = decade 3),
   // then round value down to nearest multiple of N, where N is 10^(decade-1).
   // E.g., N(1234)=100, so values from 1000-9999 are rounded to 1000, 1100 ... 9900
   //

   S32 dec;
   
   if (Hz > 0.0)
      dec = round_to_nearest(ceil(log10(Hz)))-1;
   else
      dec = -round_to_nearest(ceil(log10(-Hz)))-1;

   sprintf(text,"%lG",round_to_nearest_double(Hz, -dec));

   return text;
}

//***************************************************************************
//
// kill_trailing_whitespace()
//
//***************************************************************************

void kill_trailing_whitespace(C8 *dest)
{
   S32 l = strlen(dest);      // NB: old function left string intact if it consisted entirely of whitespace...
                              // was there a reason for that?
   while (--l >= 0)
      {
      if (!isspace(dest[l]))
         {
         dest[l+1] = 0;
         break;
         }
      }
}

//***************************************************************************
//
// elapsed_uS()
//
//***************************************************************************

S64 elapsed_uS(void)
{
   static S32 query_OK    = -1;
   static S64 q_freq      = 0;
   static S64 q_first     = 0;
   static S64 q_adjust    = 0;
   static S64 t_last      = 0;
   static S64 last_result = 0;

   if (query_OK == -1)
      {
      query_OK = 0;

      if (QueryPerformanceFrequency((LARGE_INTEGER *) &q_freq))
         {
         query_OK = 1;
         QueryPerformanceCounter((LARGE_INTEGER *) &q_first);

         t_last = (S64) (S32) GetTickCount();
         }
      }

   if (!query_OK)
      {
      //
      // Platform doesn't support high-res counter; return millisecond-precise count 
      // instead
      //

      return timeGetTime() * 1000;
      }

    //
    // Derive microsecond timer from QueryPerformanceCount() result
    //

    S64 q_time;
    S64 t_time;

    QueryPerformanceCounter((LARGE_INTEGER *) &q_time);

    S64 result = q_adjust + (((q_time - q_first) * (S64) 1000000) / q_freq);

    //
    // Get # of low-res and QPC ticks since last call
    //

    t_time = (S64) (S32) GetTickCount();

    S64 dt = (t_time - t_last) * (S64) 1000;
    S64 dq = result - last_result;

    //
    // Correct any large slippages in QPC, where dq and dt differ by more than 200 mS
    //

    S64 d = dt - dq;

    if (d < 0) 
      {
      d = -d;
      }

    if (d > 200000)
      {
      dt -= dq;

      q_adjust += dt;
      result   += dt;
      }

   //
   // Disallow any retrograde jumps
   //

   if ((result - last_result) < 0)
      {
      return last_result;
      }

   t_last      = t_time;
   last_result = result;

   return result;
}                             

//***************************************************************************
//
// system_time_uS()
//
//***************************************************************************

S64 system_time_uS (C8 *filename = NULL)
{
   //
   // Returns # of 1000-ns intervals since 1-Jan-1601 UTC
   //

   static union
      {
      FILETIME ftime;
      S64      itime;
      }
   T;

   T.itime = 0;

   //
   // If no filename supplied, return system time in UTC
   // Do this without making a filesystem call whenever possible
   //

   if (filename == NULL)
      {
      S64 tick = elapsed_uS();

      static S64 initial_file_system_time = -1;

      if (initial_file_system_time == -1)
         {
         GetSystemTimeAsFileTime(&T.ftime);

         initial_file_system_time = (T.itime / 10) - tick;
         }

      return initial_file_system_time + tick;
      }

   //
   // Filename provided
   //

   HANDLE infile = CreateFile(filename,
                              GENERIC_READ,
                              0,
                              NULL,
                              OPEN_EXISTING,
                              FILE_FLAG_SEQUENTIAL_SCAN,
                              NULL);

   if (infile == INVALID_HANDLE_VALUE)
      {
      return -1;
      }

   if (!GetFileTime(infile, 
                    NULL, 
                    NULL, 
                   &T.ftime))
      {
      return -1;
      }

   CloseHandle(infile);

   return T.itime / 10;
}

//***************************************************************************
//
// time_text()
//
//***************************************************************************

C8 *time_text  (S64 time_uS)
{
   FILETIME   ftime;
   FILETIME   lftime;
   SYSTEMTIME stime;

   S64 file_time = time_uS * 10;

   ftime.dwLowDateTime = S32(file_time & 0xffffffff);
   ftime.dwHighDateTime = S32(U64(file_time) >> 32);

   FileTimeToLocalFileTime(&ftime, &lftime);

   FileTimeToSystemTime(&lftime, &stime);

   static C8 text[1024];

   GetTimeFormat(LOCALE_SYSTEM_DEFAULT,
                 0,
                &stime,
                 NULL,
                 text,
                 sizeof(text));

   return text;
}

//***************************************************************************
//
// date_text()
//
//***************************************************************************

C8 *date_text (S64 time_uS)
{
   FILETIME   ftime;
   FILETIME   lftime;
   SYSTEMTIME stime;

   S64 file_time = time_uS * 10;

   ftime.dwLowDateTime = S32(file_time & 0xffffffff);
   ftime.dwHighDateTime = S32(U64(file_time) >> 32);

   FileTimeToLocalFileTime(&ftime, &lftime);

   FileTimeToSystemTime(&lftime, &stime);

   static C8 text[1024];

   GetDateFormat(LOCALE_SYSTEM_DEFAULT,
                 0,
                &stime,
                 NULL,
                 text,
                 sizeof(text));

   return text;
}

//****************************************************************************
//*                                                                          *
//*  Text timestamp                                                          *
//*                                                                          *
//****************************************************************************

C8 *timestamp(S64 at_time = 0)
{
   if (!at_time)
      {
      at_time = system_time_uS();
      }

   static C8 text[1024];

   sprintf(text,
          "%s %s",
           date_text(at_time),
           time_text(at_time));

   return text;
}

//****************************************************************************
//
// Turn string into valid MS-DOS filespec
//
//****************************************************************************

C8 *sanitize_path(C8 *input, S32 allow_colons_quotes_and_backslashes, S32 allow_periods)
{
   static C8 output[MAX_PATH];

   C8 *ptr = output;

   S32 len = strlen(input);

   if (len >= sizeof(output))
      {
      len = sizeof(output)-1;
      }

   for (S32 i=0; i < len; i++)
      {
      C8 s = input[i];

      if (((U8) s) >= 0x80) 
         {
         s = ' ';
         }

      if ((s == ' ') && (ptr == output))
         {
         continue;
         }

      C8 d = s;

      S32 ok = 0;

      switch (s)
         {
         case 'Ä': d = 'C'; break;
         case 'Å': d = 'u'; break;
         case 'Ç': d = 'e'; break;
         case 'É': d = 'a'; break;
         case 'Ñ': d = 'a'; break;
         case 'Ö': d = 'a'; break;
         case 'Ü': d = 'a'; break;
         case 'á': d = 'c'; break;
         case 'à': d = 'e'; break;
         case 'â': d = 'e'; break;
         case 'ä': d = 'e'; break;
         case 'ã': d = 'i'; break;
         case 'å': d = 'i'; break;
         case 'ç': d = 'i'; break;
         case 'é': d = 'A'; break;
         case 'è': d = 'A'; break;
         case 'ê': d = 'E'; break;
         case 'ì': d = 'o'; break;
         case 'î': d = 'o'; break;
         case 'ï': d = 'o'; break;
         case 'ñ': d = 'u'; break;
         case 'ó': d = 'u'; break;
         case 'ò': d = 'y'; break;
         case 'ô': d = 'o'; break;
         case 'ö': d = 'u'; break;
         case 'õ': d = 'c'; break;
         case 'ú': d = '$'; break;
         case 'ù': d = 'Y'; break;
         case 'ü': d = 'f'; break;
         case '†': d = 'a'; break;
         case '°': d = 'i'; break;
         case '¢': d = 'o'; break;
         case '£': d = 'u'; break;
         case '§': d = 'n'; break;
         case '•': d = 'n'; break;    
         case '-':            ok = 1; break;
         case '_':            ok = 1; break;
         case '~':            ok = 1; break;
         case '&':            ok = 1; break;
         case '\'':           ok = 1; break;
         case ')':            ok = 1; break;
         case '(':            ok = 1; break;
         case ' ':            ok = 1; break;

         case '.':      
            {
            if (!allow_periods)
               {
               d = '-';
               }

            ok = 1;
            break;
            }

         case '\"':
         case '\\':
         case ':':  
            {
            if (!allow_colons_quotes_and_backslashes)
               {
               d = '-';
               }

            ok = 1; 
            break;
            }
         }

      if (isalnum(d) || (ok))
         {
         *ptr++ = d;
         }
      }

   *ptr++ = 0;

   return output;
}

//****************************************************************************
//
// Launch HTML page from program directory
//
//****************************************************************************

void WINAPI launch_page(C8 *filename)
{
   C8 path[MAX_PATH];

   GetModuleFileName(NULL, path, sizeof(path)-1);

   _strlwr(path);

   C8 *exe = strstr(path,"pn.exe");

   if (exe != NULL)
      {
      strcpy(exe, filename);
      }
   else
      {
      strcpy(path, filename);
      }

   ShellExecute(NULL,    // hwnd
               "open",   // verb
                path,    // filename
                NULL,    // parms
                NULL,    // dir
                SW_SHOWNORMAL);
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
      if (SAL_is_app_active() && (GetKeyState(VK_SPACE) & 0x8000))
         {
         return TRUE;
         }                    

      Sleep(10);
      }

   return FALSE;
}

/*********************************************************************/
//
// Error/confirmation dialogs
//
/*********************************************************************/

S32 __cdecl option_box(C8 *caption, C8 *fmt, ...)
{
   static char work_string[4096];

   va_list ap;

   va_start(ap, 
            fmt);

   _vsnprintf(work_string, 
              sizeof(work_string)-1,
              fmt, 
              ap);

   va_end  (ap);

   SAL_FlipToGDISurface();

   S32 result = MessageBox(hWnd,
                           work_string,
                           caption,
                           MB_YESNO | MB_ICONQUESTION); // MB_ICONWARNING beeps

   return (result != IDNO);
}

//****************************************************************************
//
// Error handling
//
//****************************************************************************

void WINAPI GPIB_error(C8 *msg, S32 ibsta, S32 iberr, S32 ibcntl)
{
   SAL_alert_box("GPIB Error",msg);

   exit(1);
}

//****************************************************************************
//
// Send formatted string to GPIB, then wait for CR/LF to be transmitted
// as an acknowledgement
//
// Tek 49x commands (other than queries) need to go through here -- otherwise, 
// they'll remain unacknowledged if a Prologix adapter is in use.  An
// unacknowledged command prevents subsequent commands from executing.
//
//****************************************************************************

void __cdecl GPIB_cmd_printf(C8 *fmt, ...)
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

      if (!isspace(work_string[l]))
         {
         break;
         }

      work_string[l] = 0;
      }

   GPIB_puts(work_string);

   GPIB_read_ASC();
}              

//
// In-memory representation of .PNP file
// 

enum SRCLINESTYLE
{
   SLS_SOLID = 0,
   SLS_BROKEN,
};

struct SOURCE
{
   U32 color;
   U32 sat_color;

   C8 filename    [MAX_PATH];
   C8 caption     [1024];
   C8 timestamp   [1024];
   C8 instrument  [1024];
   C8 rev         [1024];
   C8 elapsed_time[1024];

   S32 min_decade_exp;
   S32 max_decade_exp;

   S32 global_n_trace_points;
   S32 global_n_overlap_points;

   S32 n_trace_points  [NUM_DECADES];
   S32 n_overlap_points[NUM_DECADES];

   DOUBLE carrier_Hz;
   DOUBLE carrier_dBm;
   DOUBLE VBW_factor;
   DOUBLE noise_correction_factor;
   DOUBLE ext_IF_Hz;
   DOUBLE ext_LO_Hz;
   DOUBLE ext_mult;
   S32    smoothing_limit;
   S32    RF_atten;
   S32    clip_dB;

   DOUBLE spot_dBc_Hz;
   DOUBLE RMS_rads;
   DOUBLE CNR;
   DOUBLE resid_FM;
   DOUBLE jitter_s;

   DOUBLE TF[NUM_DECADES][MAX_POINTS];
   DOUBLE TA[NUM_DECADES][MAX_POINTS];
           
   DOUBLE OF[NUM_DECADES][MAX_POINTS];
   DOUBLE OA[NUM_DECADES][MAX_POINTS];

   S32 last_sampled_y;
   S32 last_smoothed_y;

   SRCLINESTYLE style;
   S32          visible;

   DOUBLE VD[MAX_GRAPH_WIDTH];         // Trace video cache: Sampled point amplitude in dB
   DOUBLE VW[MAX_GRAPH_WIDTH];         // Trace video cache: Sampled point amplitude in watts
   S32    VV[MAX_GRAPH_WIDTH];         // Trace video cache: Sampled point is valid 
   DOUBLE max_dBc_Hz;
   DOUBLE min_dBc_Hz;

   DOUBLE offset  [MAX_GRAPH_WIDTH];   // Frequency offset in Hz at each graph point
   DOUBLE smoothed[MAX_GRAPH_WIDTH];   // Smoothed trace amplitude at each graph point

   //
   // Construction
   // No virtuals allowed!
   //

   SOURCE::SOURCE(void)
      {
      memset(this, 0, sizeof(*this));
      }

   SOURCE::~SOURCE()
      {
      }

   // ------------------------------------------
   // Read .PNP file
   // ------------------------------------------

   BOOL load(C8 *_filename, BOOL report_errors)
      {
      S32 i;
      FILE *in = fopen(_filename,"rt");

      if (in == NULL)
         {
         if (report_errors)
            {
            SAL_alert_box("Error","Could not load %s\n",_filename);
            }

         return FALSE;
         }

      memset(this, 0, sizeof(*this));
      strcpy(filename, _filename);

      min_dBc_Hz = 300;
      max_dBc_Hz = -300;

      ext_mult  = 1;
      ext_IF_Hz = -1;
      ext_LO_Hz = -1;

      clip_dB = -1;

      smoothing_limit = 1024;

      style = SLS_SOLID;
      visible = TRUE;

      S32 decade_index = -1;
      S32 decade_exp = 0;
      S32 overlap_index = -1;
      S32 overlap_exp = 0;

      BOOL att_set = FALSE;      // (These can vary from one decade to the next, but we only
      BOOL clip_set = FALSE;     // display the first entry encountered)

      while (1)
         {
         //
         // Read input line
         //

         C8 linbuf[512];

         memset(linbuf,
                0,
                sizeof(linbuf));

         C8 *result = fgets(linbuf, 
                            sizeof(linbuf) - 1, 
                            in);

         if ((result == NULL) || (feof(in)))
            {
            break;
            }

         //
         // Convert any characters not supported by WinVFX to ' '
         //

         S32 l = strlen(linbuf);

         for (i=0; i < l; i++)
            {
            if (((U8) linbuf[i]) >= 0x80)
               {
               linbuf[i] = ' ';
               }
            }

         //
         // Skip blank lines, and kill trailing and leading spaces
         //

         if ((!l) || (linbuf[0] == ';'))
            {
            continue;
            }

         C8 *lexeme  = linbuf;
         C8 *end     = linbuf;
         S32 leading = 1;

         for (i=0; i < l; i++)
            {
            if (!isspace(linbuf[i]))
               {
               if (leading)
                  {
                  lexeme = &linbuf[i];
                  leading = 0;
                  }

               end = &linbuf[i];
               }
            }

         end[1] = 0;

         if ((leading) || (!strlen(lexeme)))
            {
            continue;
            }

         //
         // Separate tag and value components of input line by terminating
         // the tag substring at the first space
         //

         C8 *tag   = lexeme;
         C8 *value = strchr(lexeme,' ');

         if (value == NULL)
            {
            continue;
            }

         *value = 0;
         value++;

         //
         // Process tags
         //

         if (!_stricmp(tag,"NTR"))
            {
            S32 cnt = 0;
            sscanf(value,"%d",&cnt);

            if (cnt != 1)
               {
               SAL_alert_box("Error", "Multitrace files not supported by PN.EXE");
               return FALSE;
               }
            }

         if (!_stricmp(tag,"CAP")) 
            { 
            strcpy(caption, value); 

            if (strstr(caption,"(Broken trace)") != NULL)
               {
               style = SLS_BROKEN;
               }

            continue; 
            }

         if (!_stricmp(tag,"TIM")) { strcpy(timestamp,  value); continue; }
         if (!_stricmp(tag,"IMO")) { strcpy(instrument, value); continue; }
         if (!_stricmp(tag,"IRV")) { strcpy(rev,        value); continue; }

         if (!_stricmp(tag,"DRG"))
            {
            sscanf(value,"%d,%d",&min_decade_exp,&max_decade_exp);
            continue;
            }

         if (!_stricmp(tag,"DCC"))
            {
            sscanf(value,"%d",&global_n_trace_points);

            if (decade_index >= 0)
               {
               n_trace_points[decade_index] = global_n_trace_points;
               }

            continue;
            }
            
         if (!_stricmp(tag,"OVC"))
            {
            sscanf(value,"%d",&global_n_overlap_points);

            if (overlap_index >= 0)
               {
               n_overlap_points[overlap_index] = global_n_overlap_points;
               }

            continue;
            }

         if (!_stricmp(tag,"MUL"))
            {
            sscanf(value,"%lf",&ext_mult);
            continue;
            }

         if (!_stricmp(tag,"EIF"))
            {
            sscanf(value,"%lf Hz",&ext_IF_Hz);
            continue;
            }

         if (!_stricmp(tag,"ELO"))
            {
            sscanf(value,"%lf Hz",&ext_LO_Hz);
            continue;
            }

         if ((!_stricmp(tag,"ATT")) && (!att_set))
            {
            att_set = TRUE;
            sscanf(value,"%d",&RF_atten);
            continue;
            }

         if ((!_stricmp(tag,"CLP")) && (!clip_set))
            {
            clip_set = TRUE;
            sscanf(value,"%d",&clip_dB);
            continue;
            }

         if (!_stricmp(tag,"VBF"))
            {
            sscanf(value,"%lf",&VBW_factor);
            continue;
            }

         if (!_stricmp(tag,"SFL"))
            {
            sscanf(value,"%d",&smoothing_limit);
            continue;
            }

         if (!_stricmp(tag,"ELP"))
            {
            strncpy(elapsed_time, value, sizeof(elapsed_time));
            continue;
            }

         if (!_stricmp(tag,"CAR"))
            {
            sscanf(value,"%lf Hz, %lf dBm",
               &carrier_Hz,
               &carrier_dBm);
            continue;
            }

         if (!_stricmp(tag,"DEC"))
            {
            sscanf(value,"%d",&decade_exp);
            decade_index = decade_exp - min_decade_exp;

            n_trace_points[decade_index] = global_n_trace_points;
            overlap_index = -1;
            continue;
            }

         if (!_stricmp(tag,"OVL"))
            {
            sscanf(value,"%d",&overlap_exp);
            overlap_index = overlap_exp - min_decade_exp;

            n_overlap_points[overlap_index] = global_n_overlap_points;
            decade_index = -1;
            continue;
            }

         if (!_stricmp(tag,"NCF"))
            {
            sscanf(value,"%lf",&noise_correction_factor);
            continue;
            }

         if (!_stricmp(tag,"TRA"))
            {
            S32    n = 0;
            DOUBLE F = 0.0F;
            DOUBLE A = 0.0F;

            sscanf(value,"%d: %lf Hz, %lf dBc",
               &n,
               &F,
               &A);

            if (ext_IF_Hz >= 0)                       // If trace was taken at an IF, rebase it at the
               {                                      // original carrier frequency (analogous to what's done
               F -= ext_IF_Hz;                        // at the beginning of the acquisition routines)
               F += carrier_Hz;
               }

            A += noise_correction_factor;             // Apply filter/detector correction factor

            if (F < 0.0)      F = 0.0;                // Clamp erroneous value to avoid crashes
            if (A > 0.0)      A = 0.0;
            if (F > 999.0E12) F = 999.0E12;
            if (A < -300.0)   A = -300.0;

            if (decade_index != -1)
               {
               TF[decade_index][n] = F;
               TA[decade_index][n] = A;
               }
            else if (overlap_index != -1)
               {
               OF[overlap_index][n] = F;
               OA[overlap_index][n] = A;
               }
            }
         }

      fclose(in);

      //
      // For each decade except the first, alpha-blend the overlapped data from the
      // previous decade with the first subdecade
      //
      // Overlap-blending can be inhibited for testing purposes by holding down the SHIFT key
      //

      for (S32 de=min_decade_exp+1; de <= max_decade_exp; de++)
         {
         S32 decade_index = de - min_decade_exp;

         if ((n_overlap_points[decade_index-1] > 0) && (!(GetAsyncKeyState(VK_SHIFT) & 0x8000)))
            {
            static DOUBLE blend_source[MAX_POINTS];

            S32 n_blend_points = n_overlap_points[decade_index-1] / 10;
            assert(n_blend_points == n_trace_points[decade_index-1] / 9);

            DOUBLE *ovl_src  = &OA[decade_index-1][0];
            DOUBLE *ovl_dest = &blend_source[0];

            for (i=0; i < n_blend_points; i++)
               {
               //
               // Point-sample data from the overlap decade (it's noise, after all)...
               //

               *ovl_dest++ = *ovl_src;
               ovl_src += 10;
               }

            for (i=0; i < n_blend_points; i++)
               {
               DOUBLE alpha = DOUBLE(i) / DOUBLE(n_blend_points);

               TA[decade_index][i] = (TA[decade_index][i] * alpha) + (blend_source[i] * (1.0 - alpha));
               }
            }
         }

      return TRUE;
      }

   // ------------------------------------------
   // Modify caption field for existing file
   // ------------------------------------------

   BOOL set_new_caption(C8 *new_caption)
      {
      S32 i;
      strcpy(caption, new_caption);

      style = SLS_SOLID;

      if (strstr(caption,"(Broken trace)") != NULL)
         {
         style = SLS_BROKEN;
         }

      FILE *in = fopen(filename,"rt");

      if (in == NULL)
         {
         SAL_alert_box("Error","Could not read %s\n",filename);
         exit(1);
         }

      C8 *write_filename = tmpnam(NULL);

      FILE *out = fopen(write_filename,"wt");

      if (out == NULL)
         {
         SAL_alert_box("Error","Could not write to %s\n",write_filename);
         exit(1);
         }

      BOOL caption_was_replaced = FALSE;
      
      while (1)
         {
         //
         // Read input line and make a copy of its original text
         //

         C8 linbuf     [512];
         C8 orig_linbuf[512];

         memset(linbuf,
                0,
                sizeof(linbuf));

         C8 *result = fgets(linbuf, 
                            sizeof(linbuf) - 1, 
                            in);

         if ((result == NULL) || (feof(in)))
            {
            break;
            }

         strcpy(orig_linbuf, linbuf);

         //
         // Kill trailing and leading spaces before parsing input line
         //

         C8 *tag = linbuf;

         S32 l = strlen(linbuf);

         if ((l) && (linbuf[0] != ';'))
            {
            C8 *end     = linbuf;
            S32 leading = 1;

            for (i=0; i < l; i++)
               {
               if (!isspace(linbuf[i]))
                  {
                  if (leading)
                     {
                     tag     = &linbuf[i];
                     leading = 0;
                     }

                  end = &linbuf[i];
                  }
               }

            end[1] = 0;
            }

         C8 *value = strchr(tag,' ');

         if (value != NULL)
            {
            *value = 0;
            value++;
            }

         //
         // Process lines with tags we want to modify, writing all others unchanged
         //

         if (!_stricmp(tag,"CAP")) 
            { 
            fprintf(out,"CAP %s\n",caption);
            caption_was_replaced = TRUE;
            }
         else
            {
            fputs(orig_linbuf, out);
            }
         }

      if (!caption_was_replaced)
         {
         //
         // File didn't originally have a CAP field; write one now
         //

         fprintf(out,"CAP %s\n",caption);
         }

      //
      // Overwrite original .PNP file with our modified copy
      //

      _fcloseall();

      CopyFile(write_filename,
               filename,
               FALSE);

      DeleteFile(write_filename);

      return TRUE;
      }

   // ------------------------------------------
   // Return point-sampled value from trace
   // ------------------------------------------

   static int search_point_list(const void *keyval, const void *datum)
      {
      DOUBLE key = *(DOUBLE *) keyval;

      if (key <  ((DOUBLE *) datum)[0]) return -1;
      if (key >= ((DOUBLE *) datum)[1]) return  1;

      return 0;
      }

   BOOL sampled_value(DOUBLE Hz, DOUBLE *result)
      {
      S32 i,d = round_to_nearest(floor(log10(Hz))) - min_decade_exp;

      if ((d < 0) || (d > max_decade_exp - min_decade_exp))
         {
         *result = 0.0F;
         return FALSE;
         }

      DOUBLE abs_freq = Hz + carrier_Hz;

      if (abs_freq <= TF[d][0])
         {
         *result = TA[d][0];     
         return TRUE;            
         }

      if (abs_freq >= TF[d][n_trace_points[d]-1])
         {
         *result = TA[d][n_trace_points[d]-1];     
         return TRUE;            
         }
      
      DOUBLE *ptr = (DOUBLE *) bsearch(&abs_freq,
                                        TF[d],
                                        n_trace_points[d],
                                        sizeof(DOUBLE),
                                        SOURCE::search_point_list);
      if (ptr == NULL)  
         {
         *result = 0.0;
         return FALSE;
         }

      i = ptr - TF[d];

      *result = TA[d][i];
      return TRUE;
      }
};

//
// Array of data sources to overlay and/or browse
//

SOURCE data_source_list[MAX_SOURCES];
S32 n_data_sources;
S32 current_data_source;

//****************************************************************************
//*                                                                          *
//* Acquire data from HP 8566A/8567A/8568A                                   *
//*                                                                          *
//****************************************************************************

void EXIT_HP8566A_HP8567A_HP8568A(S32 clip, //)
                                  S64 carrier_Hz)
{
   if (out != NULL)
      {
      fclose(out);
      out = NULL;
      }

   //
   // Put clipped peak back onscreen
   //

   for (S32 cp=0; cp < clip; cp += 10)
      {
      GPIB_printf("RL UP;");
      }

   GPIB_printf("M1;");                      // No marker
   GPIB_printf("S1;");                      // Return to continuous-sweep mode
   GPIB_printf("CR;");                      // Autoselect RBW
   GPIB_printf("CV;");                      // Autoselect VBW
   GPIB_printf("CA;");                      // Autoselect RF attenuator
   GPIB_printf("KSZ 0DB;");                 // No reference offset
   GPIB_printf("KSa;");                     // Normal detection
   GPIB_printf("CF %I64dHZ;",carrier_Hz);   // Set CF

   GPIB_disconnect(FALSE);                  // Don't return to local if Prologix adapter in use (causes flooding)
}

BOOL ACQ_HP8566A_HP8567A_HP8568A(C8    *filename,
                                 S32    GPIB_address,
                                 C8    *caption,
                                 S64    carrier,           
                                 S64    ext_IF,
                                 S64    ext_LO,
                                 SINGLE VBW_factor,
                                 DOUBLE ext_mult,          
                                 SINGLE NF_characteristic,
                                 SINGLE min_offset,        
                                 SINGLE max_offset,
                                 S32    clip,
                                 SINGLE ref,
                                 S32    instrument_model,
                                 S32    use_quartiles,
                                 S32    force_0dB)
{
   S32 i;

   S32    min_decade = S32(log10(min_offset)+0.5);
   S32    max_decade = S32(log10(max_offset)+0.5) - 1;
   SINGLE ext_mult_dB = 0.0F;

   SAL_debug_printf("Minimum offset = %d Hz\n",S32(pow(10.0,min_decade)+0.5));
   SAL_debug_printf("Maximum offset = %d Hz\n",S32(pow(10.0,max_decade+1)+0.5));

   ext_mult_dB = -20.0F * (SINGLE) log10(ext_mult);

   SAL_debug_printf("External frequency multiplier = %.06lfx (%.02f dB)\n",
                     ext_mult,ext_mult_dB);

   //
   // Start up GPIB
   //

   GPIB_connect (GPIB_address,
                 GPIB_error,
                 0,
                 1000000);

   GPIB_set_EOS_mode(10, FALSE);
   GPIB_set_serial_read_dropout(500);

   //
   // Open output file
   //

   out = fopen(filename,"wt");

   if (out == NULL)
      {
      GPIB_disconnect();

      SAL_alert_box("Error","Could not open %s\n",filename);
      return FALSE;
      }

   fprintf(out,";\n");
   fprintf(out,";Composite-noise plot file %s\n",filename);
   fprintf(out,";\n");
   fprintf(out,";Generated by PN.EXE V"VERSION" of "__DATE__"\n");
   fprintf(out,";by John Miles, KE5FX (john@miles.io)\n");
   fprintf(out,";\n\n");

   //
   // Log caption, if present
   //

   if (caption[0])
      {
      fprintf(out,"CAP %s\n",caption);
      }

   //
   // Log timestamp and instrument ID
   //

   static C8 ID[1024];
   static C8 REV[1024];

   switch (instrument_model)
      {
      case IDM_HP8566A: strcpy(ID,"HP8566A"); break;
      case IDM_HP8567A: strcpy(ID,"HP8567A"); break;
      case IDM_HP8568A: strcpy(ID,"HP8568A"); break;
      default: assert(0);
      }
   
   strcpy(REV,"N/A");

   kill_trailing_whitespace(ID);
   kill_trailing_whitespace(REV);

   fprintf(out,"TIM %s\n",timestamp());
   fprintf(out,"IMO %s\n", ID);
   fprintf(out,"IRV %s\n\n", REV);

   //
   // Log decade statistics
   //

   fprintf(out,"DRG %d,%d\n", min_decade,max_decade);
   fprintf(out,"DCC 900\n");    // Decade point count=900 points
   fprintf(out,"OVC 1000\n\n"); // Overlap point count=1000 points into next decade

   fprintf(out,"EIF %I64d Hz\n",ext_IF);
   fprintf(out,"ELO %I64d Hz\n",ext_LO);
   fprintf(out,"MUL %lf\n\n",ext_mult);

   fprintf(out,"VBF %.02f\n",VBW_factor);
   fprintf(out,"CLP %d\n\n",clip);


   //
   // Set basic analyzer state needed for operation of program
   //

   GPIB_printf("KSX;");       // Enable corrections
   GPIB_printf("S2;");        // Select single-sweep mode
   GPIB_printf("M1;");        // No markers by default
   GPIB_printf("LG 10DB;");   // Need 10 dB/division log scale
   GPIB_printf("CR;");        // Autoselect RBW
   GPIB_printf("CV;");        // Autoselect VBW
   GPIB_printf("CA;");        // Autoselect RF attenuation
   GPIB_printf("CT;");        // Autoselect sweep time
   GPIB_printf("O3;");        // ASCII queries

   //
   // If no carrier specified, find the strongest signal in the 
   // currently-selected view
   //
   // This routine will generally land within 10-20 Hz.  We round
   // the result to the nearest minimum-decade multiple
   //

   if (carrier < 0)
      {
      SAL_debug_printf("Measuring carrier frequency... ");
      assert(ext_LO == -1);

      //
      // Zero in on signal, dividing span by 10 at each step until span width is <= 5 kHz
      //

      while (1)
         {
         GPIB_printf("TS;");        // Take sweep
         GPIB_printf("E1;");        // Put marker on highest signal
         GPIB_printf("E2;");        // Marker -> center frequency
         GPIB_printf("SP;");        // Active function = span width

         DOUBLE span = -10000.0;

         while (span == -10000.0)
            {
            C8 *span_text = GPIB_query("OA");
            sscanf(span_text,"%lf",&span); 
            }

         if (span <= 5000.0)
            {
            break;
            }

         span /= 10.0;              // Select next-narrower span and repeat measurement  
         GPIB_printf("SP %lfHZ;",span);
         }

      GPIB_flush_receive_buffers();

      DOUBLE C = -10000.0;

      while (C == -10000.0)         // Query final marker frequency
         {
         C8 *CF_text = GPIB_query("O3;MF");
         sscanf(CF_text,"%lf",&C); 
         }

      if (C < 0.0F)
         {
         C = -C;
         }

      GPIB_printf("M1;");           // Kill marker

      carrier = S64(C + 0.5);

      S64 decade_start = (S64) (pow(10.0, min_decade) + 0.5F);

      carrier = ((carrier + (decade_start / 2)) / decade_start) * decade_start;

      carrier = S64(((DOUBLE) carrier / ext_mult) + 0.5);  
      
      SAL_debug_printf("Result = %I64d Hz\n",carrier);
      }

   //
   // Perform actual measurement at external IF, if one is specified
   //

   S64 source_carrier = carrier;

   if (ext_IF >= 0)
      {
      carrier = ext_IF;
      }

   //
   // Log beginning of sweep series
   //

   S64 start = elapsed_uS();

   //
   // For each decade...
   //

   bool reference_level_set = FALSE;

   S64 mult_delta_Hz = S64(((DOUBLE) carrier * ext_mult) + 0.5) - carrier;  

   for (S32 d=min_decade; d <= max_decade; d++)
      {
      static SINGLE trace_A   [2][1001];
      static DOUBLE freq      [2][1001];
      SINGLE        correction[2];

      S32 decade_start = (S32) (pow(10.0, d) + 0.5F);
      S32 next_decade  = decade_start * 10;

      GPIB_printf("M1;");                       // Kill marker

      // 
      // If reference level not already determined, establish it
      // using default Rosenfell detector
      //

      if (!reference_level_set)
         {
         reference_level_set = TRUE;

         if (ref == 10000.0F)
            {
            //
            // Program a reasonable span/RBW combination for peak-level measurement, and 
            // declare a high reference level that is guaranteed to place the signal 
            // peak onscreen
            //

            GPIB_printf("SP 100KZ;");
            GPIB_printf("RB 10KZ;");
            GPIB_printf("VB 10KZ;");

            GPIB_printf("LG 10DB;");                  // Use dBm units
            GPIB_printf("KSa;");                      // Rosenfell detection

            GPIB_printf("RL 30DM;");                  // +30 dBm reference level
            GPIB_printf("KSZ 0DB;");                  // Clear reference offset

            //
            // Tune to specified center frequency and take sweep
            //

            GPIB_printf("CF %I64dHZ;",carrier + mult_delta_Hz);   // Set CF
            GPIB_printf("TS;");                                   // Take sweep
            GPIB_printf("E1;");                                   // Put marker on highest signal
            GPIB_printf("E4;");                                   // Set reference level = marker amplitude

            //
            // Query reference level and use it as the offset 
            // for subsequent dBm/Hz measurements
            //

            GPIB_printf("RL;");                    // Active function = reference level

            while (ref == 10000.0F)
               {
               C8 *RL_text = GPIB_query("OA");
               sscanf(RL_text,"%f",&ref); 
               }
            }

         GPIB_printf("KSZ %fDM;",-ref);            // Establish reference offset
         GPIB_printf("RL 0DM;");                   // Set relative reference level = 0 dBm

         //
         // Apply reference-level clipping if requested
         //

         for (S32 cp=0; cp < clip; cp += 10)
            {
            GPIB_printf("RL DN;");
            }

         if (force_0dB)
            {
            GPIB_printf("AT 0DB;");

            GPIB_printf("RL;");                    // Active function = reference level

            SINGLE new_clip = 10000.0F;

            while (new_clip == 10000.0F)
               {
               C8 *RL_text = GPIB_query("OA");
               sscanf(RL_text,"%f",&new_clip); 
               }

            clip = (S32) ((-new_clip) + 0.5F);
            }
         }

      //
      // Enable sample detection for noise-like signals
      //

      GPIB_printf("KSe;");

      GPIB_printf("M1;");                       // No marker needed

      //
      // Log RF attenuation used for this sweep
      //
      // We obtain this by examining the CRT annotation field for the RF attenuation setting,
      // rather than by issuing an AT/OA pair.  The latter disturbs the coupled/manual state
      // of the attenuator control.
      //

      SINGLE att = 10000.0F;

      //
      // Perform "OT" query to obtain display annotation
      // (EOS is set to 10 to separate the strings)
      //

      static C8 OT_buffer[32][65];
      memset(OT_buffer, 0, sizeof(OT_buffer));

      GPIB_printf("OT");

      for (i=0; i < 32; i++)
         {
         C8 *result = GPIB_read_ASC(sizeof(OT_buffer[i])-1);
         assert(result != NULL);

         strcpy(OT_buffer[i], result);
         }

      //
      // Determine HP 8566A/8568A attenuator setting
      //

      sscanf(OT_buffer[5], "ATTEN %f dB", &att);

      SAL_debug_printf("Carrier level = %.02f dBm, RF atten = %d dB\n",ref,S32(att + 0.5F));

      //
      // Log carrier amplitude used for this sweep
      //

      fprintf(out,"CAR %I64d Hz, %.02f dBm\n",
         source_carrier,
         ref);

      fprintf(out,"ATT %d dB\n",
         S32(att + 0.5F));

      S32 tune_chunks  = 0;
      S32 tune_span    = 0;
      S32 using_RBW    = 0;

      for (S32 half=0; half < 2; half++)
         {
         SAL_serve_message_queue();

         //
         // Take a reading at each half-decade, with the marker positioned
         // at the right edge of the display for automatic noise-characteristic
         // measurement (if enabled)
         //

         SAL_debug_printf("Analyzing decade 10^%d, sweep %d of 2...\n",d,half+1);

         if (key_hit)
            {
            EXIT_HP8566A_HP8567A_HP8568A(clip, carrier + mult_delta_Hz);
            key_hit = FALSE;

            SAL_alert_box("Error","Aborted by keypress");
            return FALSE;
            }

         //
         // Noise measurements made in spans >= 26 kHz have a higher PN floor on 
         // the 8566 due to gain variations in the YTO loop.  This shows up 
         // as an unexpected "bump" around 20 kHz.  We can work around 
         // this effect by breaking both the normal and overlap halves of the 
         // 10 kHz - 100 kHz decade into four quartiles each
         //

         SINGLE *trace_dest = trace_A[half];
         DOUBLE *freq_dest  = freq   [half];

         if ((instrument_model == IDM_HP8566A) && (decade_start == 10000) && (use_quartiles))
            {
            tune_chunks  = 4;
            tune_span    = next_decade / 4;
            using_RBW    = 300;

            assert(tune_span == 25000);
            }
         else
            {
            tune_chunks  = 1;
            tune_span    = next_decade;
            using_RBW    = decade_start / 10;
            }

         GPIB_printf("SP %dHZ;",tune_span);                                // Set total span
         GPIB_printf("RB %dHZ;",using_RBW);                                // Set RBW
         GPIB_printf("VB %dHZ;",(S32) ((using_RBW * VBW_factor) + 0.5F));  // Set VBW

         SAL_debug_printf("Span %d Hz, RBW %d Hz\n",tune_span,using_RBW);

         for (S32 chunk=0; chunk < tune_chunks; chunk++)
            {
            S64 tune_freq = carrier + S64(tune_span / 2) + S64(chunk * tune_span) + S64(next_decade * half);

            SAL_debug_printf("  %d: CF=%I64d Hz\n",chunk,tune_freq + mult_delta_Hz);

            GPIB_printf("CF %I64dHZ;",tune_freq + mult_delta_Hz);    // Set center freq
            GPIB_printf("TS;");                                      // Take a sweep

            //
            // Fetch data from display trace A
            // Both binary and ASCII versions supported (binary is faster)
            //
            // (TODO: ASCII version hangs/reboots/BSODs here with ICS 488-USB) 
            //

            DOUBLE df_dp = ((DOUBLE) tune_span) / 1000.0;
            DOUBLE F     = ((DOUBLE) tune_freq) - (500.0 * df_dp);
#if BINARY_ACQ
            GPIB_printf("O2;TA");         // Request 16-bit binary display values
                                         
            static U16 curve[1001];     
     
            memset(curve, 0, 1001 * sizeof(S16));
            memcpy(curve, GPIB_read_BIN(1001 * sizeof(S16),TRUE,FALSE), 1001 * sizeof(S16));
     
            SINGLE offset      = -100.0F;
            SINGLE scale       = 100.0F / 1000;
            SINGLE grat_offset = 0.0F;
     
            for (i=0; i < 1001; i++)
               {
               if (!(i % tune_chunks))
                  {
                  if ((trace_dest - trace_A[half]) < ARY_CNT(trace_A[half]))
                     {
                     U16 scrn_Y = ((curve[i] & 0xff00) >> 8) + ((curve[i] & 0x00ff) << 8);

                     *trace_dest++ = (((SINGLE(scrn_Y) + grat_offset) * scale) + offset) - (SINGLE) clip;
                     *freq_dest++  = F;
                     }
                  }
     
               F += df_dp;
               }

            GPIB_printf("O3;");
#else
            GPIB_printf("O3;TA");      // Request ASCII dBm values

            for (i=0; i < 1001; i++)
               {
               SINGLE trace_point;

               do
                  {
                  C8 *read_result = GPIB_read_ASC(16, FALSE);

                  if ((read_result == NULL) || (read_result[0] == 0))
                     {
                     EXIT_HP8566A_HP8567A_HP8568A(clip, carrier + mult_delta_Hz);

                     SAL_alert_box("Error","Sweep read terminated prematurely at column %d\n",i);
                     return FALSE;
                     }

                  trace_point = 10000.0F; 
                  sscanf(read_result,"%f",&trace_point);
                  }
               while (trace_point == 10000.0F);

               if (!(i % tune_chunks))
                  {
                  if ((trace_dest - trace_A[half]) < ARY_CNT(trace_A[half]))
                     {
                     *trace_dest++ = trace_point;
                     *freq_dest++  = F;
                     }
                  }

               F += df_dp;
               }
#endif
            }

         //
         // Log the noise normalization factor for this RBW
         //

         SINGLE RB_factor = -10.0F * (SINGLE) log10((DOUBLE) using_RBW);

         correction[half] = RB_factor + NF_characteristic;
         }

      //
      // Display results
      //

      SAL_debug_printf("\n%8d Hz: %.02f dBc/Hz\n",
         decade_start,
         trace_A[0][100] + correction[0] + ext_mult_dB);

      SAL_debug_printf("%8d Hz: %.02f dBc/Hz / %.02f dBc/Hz\n\n",
         next_decade,
         trace_A[0][1000] + correction[0] + ext_mult_dB,
         trace_A[1][100]  + correction[1] + ext_mult_dB);

      //
      // Send decade and overlap data to output file
      //

      fprintf(out,"DEC %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[0]);

      for (i=100; i < 1000; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i - 100,
            freq[0][i],
            trace_A[0][i] + ext_mult_dB);
         }

      fprintf(out,"OVL %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[1]);

      for (i=0; i < 1000; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i,
            freq[1][i],
            trace_A[1][i] + ext_mult_dB);
         }
      }

   //
   // Write cumulative sweep time and EOF timestamp
   //

   S64 end = elapsed_uS();

   fprintf(out,"ELP %I64ds\n",((end - start) + 500000) / 1000000);
   fprintf(out,"EOF %s\n",timestamp());

   EXIT_HP8566A_HP8567A_HP8568A(clip, carrier + mult_delta_Hz);
   return TRUE;
}

//****************************************************************************
//*                                                                          *
//* Acquire data from HP 8566B/8567B/8568B                                   *
//*                                                                          *
//****************************************************************************

void EXIT_HP8566B_HP8567B_HP8568B(S32 clip, //)
                                  S64 carrier_Hz)
{
   if (out != NULL)
      {
      fclose(out);
      out = NULL;
      }

   //
   // Put clipped peak back onscreen
   //

   for (S32 cp=0; cp < clip; cp += 10)
      {
      GPIB_printf("RL UP;");
      }

   GPIB_printf("MKOFF ALL;");             // No marker
   GPIB_printf("CONTS;");                 // Return to continuous-sweep mode
   GPIB_printf("CR;");                    // Autoselect RBW
   GPIB_printf("CV;");                    // Autoselect VBW
   GPIB_printf("CA;");                    // Autoselect RF attenuator
   GPIB_printf("ROFFSET 0DM;");           // No reference offset
   GPIB_printf("DET NRM;");               // Normal detection
   GPIB_printf("CF %I64dHZ;",carrier_Hz); // Center at carrier frequency

   GPIB_disconnect();
}

BOOL ACQ_HP8566B_HP8567B_HP8568B(C8    *filename,
                                 S32    GPIB_address,
                                 C8    *caption,
                                 S64    carrier,           
                                 S64    ext_IF,
                                 S64    ext_LO,
                                 SINGLE VBW_factor,
                                 DOUBLE ext_mult,          
                                 SINGLE NF_characteristic,
                                 SINGLE min_offset,        
                                 SINGLE max_offset,
                                 S32    clip,
                                 SINGLE ref,
                                 S32    instrument_model,
                                 S32    use_quartiles,
                                 S32    force_0dB)
{
   S32    i;
   S32    min_decade = S32(log10(min_offset)+0.5);
   S32    max_decade = S32(log10(max_offset)+0.5) - 1;
   SINGLE ext_mult_dB = 0.0F;

   SAL_debug_printf("Minimum offset = %d Hz\n",S32(pow(10.0,min_decade)+0.5));
   SAL_debug_printf("Maximum offset = %d Hz\n",S32(pow(10.0,max_decade+1)+0.5));

   ext_mult_dB = -20.0F * (SINGLE) log10(ext_mult);

   SAL_debug_printf("External frequency multiplier = %.06lfx (%.02f dB)\n",
                     ext_mult,ext_mult_dB);

   //
   // Start up GPIB
   //

   GPIB_connect (GPIB_address,
                 GPIB_error,
                 0,
                 1000000);

   GPIB_set_EOS_mode(10);
   GPIB_set_serial_read_dropout(500);

   //
   // Open output file
   //

   out = fopen(filename,"wt");

   if (out == NULL)
      {
      GPIB_disconnect();

      SAL_alert_box("Error","Could not open %s\n",filename);
      return FALSE;
      }

   fprintf(out,";\n");
   fprintf(out,";Composite-noise plot file %s\n",filename);
   fprintf(out,";\n");
   fprintf(out,";Generated by PN.EXE V"VERSION" of "__DATE__"\n");
   fprintf(out,";by John Miles, KE5FX (john@miles.io)\n");
   fprintf(out,";\n\n");

   //
   // Log caption, if present
   //

   if (caption[0])
      {
      fprintf(out,"CAP %s\n",caption);
      }

   //
   // Log timestamp and instrument ID
   //

   static C8 ID[1024];
   static C8 REV[1024];

   strcpy(ID,GPIB_query("ID?"));
   strcpy(REV,GPIB_query("REV?"));

   kill_trailing_whitespace(ID);
   kill_trailing_whitespace(REV);

   fprintf(out,"TIM %s\n",timestamp());
   fprintf(out,"IMO %s\n", ID);
   fprintf(out,"IRV %s\n\n", REV);

   //
   // Log decade statistics
   //

   fprintf(out,"DRG %d,%d\n", min_decade,max_decade);
   fprintf(out,"DCC 900\n");    // Decade point count=900 points
   fprintf(out,"OVC 1000\n\n"); // Overlap point count=1000 points into next decade

   fprintf(out,"EIF %I64d Hz\n",ext_IF);
   fprintf(out,"ELO %I64d Hz\n",ext_LO);
   fprintf(out,"MUL %lf\n\n",ext_mult);

   fprintf(out,"VBF %.02f\n",VBW_factor);
   fprintf(out,"CLP %d\n\n",clip);

   //
   // Set basic analyzer state needed for operation of program
   //

   GPIB_printf("KSX;");       // Enable corrections       
   GPIB_printf("SNGLS;");     // Select single-sweep mode
   GPIB_printf("MKOFF ALL;"); // No markers by default
   GPIB_printf("LG 10DM;");   // Need 10 dB/division log scale
   GPIB_printf("CR;");        // Autoselect RBW
   GPIB_printf("CV;");        // Autoselect VBW
   GPIB_printf("CA;");        // Autoselect RF attenuator
   GPIB_printf("CT;");        // Autoselect sweep time
   GPIB_printf("O3;");        // ASCII queries

   //
   // If no carrier specified, find the strongest signal in the 
   // currently-selected view
   //
   // 8566 analyzers don't have a counter, so we'll need to 
   // zoom in on the peak signal manually.  We round
   // the result to the nearest minimum-decade multiple (x100 Hz for 
   // the 8566)
   //

   if (carrier < 0)
      {
      SAL_debug_printf("Measuring carrier frequency... ");
      assert(ext_LO == -1);

      //
      // Zero in on signal, dividing span by 10 at each step until span width is <= 5 kHz
      //

      while (1)
         {
         GPIB_printf("TS;");        // Take sweep
         GPIB_printf("MKPK HI;");   // Put marker on highest signal
         GPIB_printf("MKCF;");      // Marker -> center frequency

         DOUBLE span = -10000.0;

         while (span == -10000.0)   
            {
            C8 *span_text = GPIB_query("SP?");
            sscanf(span_text,"%lf",&span); 
            }

         if (span <= 5000.0)
            {
            break;
            }

         span /= 10.0;              // Select next-narrower span and repeat measurement  
         GPIB_printf("SP %lfHZ;",span);  
         }

      GPIB_flush_receive_buffers();

      DOUBLE C = -10000.0;

      while (C == -10000.0)         // Query final marker frequency
         {
         C8 *CF_text = GPIB_query("MKF?");
         sscanf(CF_text,"%lf",&C); 
         }

      if (C < 0.0F)
         {
         C = -C;
         }

      GPIB_printf("MKOFF ALL;");    // Kill marker

      carrier = S64(C + 0.5);

      S64 decade_start = (S64) (pow(10.0, min_decade) + 0.5F);

      carrier = ((carrier + (decade_start / 2)) / decade_start) * decade_start;

      carrier = S64(((DOUBLE) carrier / ext_mult) + 0.5);  

      SAL_debug_printf("Result = %I64d Hz\n",carrier);
      }

   //
   // Perform actual measurement at external IF, if one is specified
   //

   S64 source_carrier = carrier;

   if (ext_IF >= 0)
      {
      carrier = ext_IF;
      }

   //
   // Log beginning of sweep series
   //

   S64 start = elapsed_uS();

   //
   // For each decade...
   //

   bool reference_level_set = FALSE;

   S64 mult_delta_Hz = S64(((DOUBLE) carrier * ext_mult) + 0.5) - carrier;  

   for (S32 d=min_decade; d <= max_decade; d++)
      {
      static SINGLE trace_A   [2][1001];
      static DOUBLE freq      [2][1001];
      SINGLE        correction[2];

      S32 decade_start = (S32) (pow(10.0, d) + 0.5F);
      S32 next_decade  = decade_start * 10;

      GPIB_printf("MKOFF ALL;");                // Kill marker

      //
      // If reference level not already determined, establish it
      // using default Rosenfell detector
      //

      if (!reference_level_set)
         {
         reference_level_set = TRUE;

         if (ref == 10000.0F)
            {
            //
            // Program a reasonable span/RBW combination for peak-level measurement, and 
            // declare a high reference level that is guaranteed to place the signal 
            // peak onscreen
            //

            GPIB_printf("SP 100KZ;");     
            GPIB_printf("RB 10KZ;");      
            GPIB_printf("VB 10KZ;");               

            GPIB_printf("AUNITS DBM;");               // Use dBm units
            GPIB_printf("DET NRM;");                  // Rosenfell detection

            GPIB_printf("RL 30DM;");                  // +30 dBm reference level
            GPIB_printf("ROFFSET 0DM;");              // Clear reference offset

            //
            // Tune to specified center frequency and take sweep
            //

            GPIB_printf("CF %I64dHZ;",carrier + mult_delta_Hz);   // Set CF
            GPIB_printf("TS;");                                   // Take sweep
            GPIB_printf("MKPK HI;");                              // Put marker on highest signal
            GPIB_printf("MKRL;");                                 // Set reference level = marker amplitude

            //
            // Query reference level and use it as the offset 
            // for subsequent dBm/Hz measurements
            //

            while (ref == 10000.0F)
               {
               C8 *RL_text = GPIB_query("RL?");
               sscanf(RL_text,"%f",&ref); 
               }
            }

         GPIB_printf("ROFFSET %fDM;",-ref);        // Establish reference offset
         GPIB_printf("RL 0DM;");                   // Set relative reference level = 0 dBm

         //
         // Apply reference-level clipping if requested
         //

         for (S32 cp=0; cp < clip; cp += 10)
            {
            GPIB_printf("RL DN;");
            }

         if (force_0dB)
            {
            GPIB_printf("AT 0DB;");

            SINGLE new_clip = 10000.0F;

            while (new_clip == 10000.0F)
               {
               C8 *RL_text = GPIB_query("RL?");
               sscanf(RL_text,"%f",&new_clip); 
               }

            clip = (S32) ((-new_clip) + 0.5F);
            }
         }

      //
      // Enable sample detection for noise-like signals
      //

      GPIB_printf("DET SMP;");

      GPIB_printf("MKOFF ALL;");                // No marker needed

      //
      // Log RF attenuation used for this sweep
      //

      SINGLE att = 10000.0F;

      while (att == 10000.0F)
         {
         C8 *AT_text = GPIB_query("AT?");
         sscanf(AT_text,"%f",&att);
         }

      SAL_debug_printf("Carrier level = %.02f dBm, RF atten = %d dB\n",ref,S32(att + 0.5F));

      //
      // Log carrier amplitude used for this sweep
      //

      fprintf(out,"CAR %I64d Hz, %.02f dBm\n",
         source_carrier,
         ref);

      fprintf(out,"ATT %d dB\n",
         S32(att + 0.5F));

      S32 tune_chunks  = 0;
      S32 tune_span    = 0;
      S32 using_RBW    = 0;

      for (S32 half=0; half < 2; half++)
         {
         SAL_serve_message_queue();

         //
         // Take a reading at each half-decade, with the marker positioned
         // at the right edge of the display for automatic noise-characteristic
         // measurement (if enabled)
         //

         SAL_debug_printf("Analyzing decade 10^%d, sweep %d of 2...\n",d,half+1);

         if (key_hit)
            {
            EXIT_HP8566B_HP8567B_HP8568B(clip, carrier + mult_delta_Hz);
            key_hit = FALSE;

            SAL_alert_box("Error","Aborted by keypress");
            return FALSE;
            }

         //
         // Noise measurements made in spans >= 26 kHz have a higher PN floor on 
         // the 8566 due to gain variations in the YTO loop.  This shows up 
         // as an unexpected "bump" around 20 kHz.  We can work around 
         // this effect by breaking both the normal and overlap halves of the 
         // 10 kHz - 100 kHz decade into four quartiles each
         //

         SINGLE *trace_dest = trace_A[half];
         DOUBLE *freq_dest  = freq   [half];

         if ((instrument_model == IDM_HP8566B) && (decade_start == 10000) && (use_quartiles))
            {
            tune_chunks  = 4;
            tune_span    = next_decade / 4;
            using_RBW    = 300;

            assert(tune_span == 25000);
            }
         else
            {
            tune_chunks  = 1;
            tune_span    = next_decade;
            using_RBW    = decade_start / 10;
            }

         GPIB_printf("SP %dHZ;",tune_span);                                // Set total span
         GPIB_printf("RB %dHZ;",using_RBW);                                // Set RBW
         GPIB_printf("VB %dHZ;",(S32) ((using_RBW * VBW_factor) + 0.5F));  // Set VBW

         SAL_debug_printf("Span %d Hz, RBW %d Hz\n",tune_span,using_RBW);

         for (S32 chunk=0; chunk < tune_chunks; chunk++)
            {
            S64 tune_freq = carrier + S64(tune_span / 2) + S64(chunk * tune_span) + S64(next_decade * half);

            SAL_debug_printf("  %d: CF=%I64d Hz\n",chunk,tune_freq + mult_delta_Hz);

            GPIB_printf("CF %I64dHZ;",tune_freq + mult_delta_Hz);    // Set center freq
            GPIB_printf("TS;");                                      // Take a sweep

            //
            // Fetch data from display trace A
            // Both binary and ASCII versions supported (binary is faster)
            //
            // (TODO: ASCII version hangs/reboots/BSODs here with ICS 488-USB) 
            //

            DOUBLE df_dp = ((DOUBLE) tune_span) / 1000.0;
            DOUBLE F     = ((DOUBLE) tune_freq) - (500.0 * df_dp);
#if BINARY_ACQ
            GPIB_printf("O2;TA");         // Request 16-bit binary display values
                                         
            static U16 curve[1001];     
     
            memset(curve, 0, 1001 * sizeof(S16));
            memcpy(curve, GPIB_read_BIN(1001 * sizeof(S16),TRUE,FALSE), 1001 * sizeof(S16));
     
            SINGLE offset      = -100.0F;
            SINGLE scale       = 100.0F / 1000;
            SINGLE grat_offset = 0.0F;
     
            for (i=0; i < 1001; i++)
               {
               if (!(i % tune_chunks))
                  {
                  if ((trace_dest - trace_A[half]) < ARY_CNT(trace_A[half]))
                     {
                     U16 scrn_Y = ((curve[i] & 0xff00) >> 8) + ((curve[i] & 0x00ff) << 8);

                     *trace_dest++ = (((SINGLE(scrn_Y) + grat_offset) * scale) + offset) - (SINGLE) clip;
                     *freq_dest++  = F;
                     }
                  }
     
               F += df_dp;
               }

            GPIB_printf("O3;");
#else
            GPIB_printf("O3;TA");      // Request dBm values from trace A 

            for (i=0; i < 1001; i++)
               {
               SINGLE trace_point;

               do
                  {
                  C8 *read_result = GPIB_read_ASC(16, FALSE);

                  if ((read_result == NULL) || (read_result[0] == 0))
                     {
                     EXIT_HP8566B_HP8567B_HP8568B(clip, carrier + mult_delta_Hz);

                     SAL_alert_box("Error","Sweep read terminated prematurely at column %d\n",i);
                     return FALSE;
                     }

                  trace_point = 10000.0F; 
                  sscanf(read_result,"%f",&trace_point);
                  }
               while (trace_point == 10000.0F);

               if (!(i % tune_chunks))
                  {
                  if ((trace_dest - trace_A[half]) < ARY_CNT(trace_A[half]))
                     {
                     *trace_dest++ = trace_point;
                     *freq_dest++  = F;
                     }
                  }

               F += df_dp;
               }
#endif
            }

         //
         // Log the noise normalization factor for this RBW
         //

         SINGLE RB_factor = -10.0F * (SINGLE) log10((DOUBLE) using_RBW);

         correction[half] = RB_factor + NF_characteristic;
         }

      //
      // Display results
      //

      SAL_debug_printf("\n%8d Hz: %.02f dBc/Hz\n",
         decade_start,
         trace_A[0][100] + correction[0] + ext_mult_dB);

      SAL_debug_printf("%8d Hz: %.02f dBc/Hz / %.02f dBc/Hz\n\n",
         next_decade,
         trace_A[0][1000] + correction[0] + ext_mult_dB,
         trace_A[1][100]  + correction[1] + ext_mult_dB);

      //
      // Send decade and overlap data to output file
      //

      fprintf(out,"DEC %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[0]);

      for (i=100; i < 1000; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i - 100,
            freq[0][i],
            trace_A[0][i] + ext_mult_dB);
         }

      fprintf(out,"OVL %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[1]);

      for (i=0; i < 1000; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i,
            freq[1][i],
            trace_A[1][i] + ext_mult_dB);
         }
      }

   //
   // Write cumulative sweep time and EOF timestamp
   //

   S64 end = elapsed_uS();

   fprintf(out,"ELP %I64ds\n",((end - start) + 500000) / 1000000);
   fprintf(out,"EOF %s\n",timestamp());

   EXIT_HP8566B_HP8567B_HP8568B(clip, carrier + mult_delta_Hz);
   return TRUE;
}

//****************************************************************************
//*                                                                          *
//* Acquire data from HP 8560A/E                                             *
//*                                                                          *
//****************************************************************************

void EXIT_HP8560(S32 clip, //)
                 S64 carrier_Hz)
{
   if (out != NULL)
      {
      fclose(out);
      out = NULL;
      }

   //
   // Put clipped peak back onscreen
   //

   for (S32 cp=0; cp < clip; cp += 10)
      {
      GPIB_printf("RL UP;");
      }

   GPIB_printf("MKOFF ALL;");               // No marker
   GPIB_printf("CONTS;");                   // Return to continuous-sweep mode
   GPIB_printf("RB AUTO;");                 // Autoselect RBW
   GPIB_printf("VB AUTO;");                 // Autoselect VBW
   GPIB_printf("ROFFSET 0DB;");             // No reference offset
   GPIB_printf("DET NRM;");                 // Normal detection
   GPIB_printf("CF %I64dHZ;",carrier_Hz);   // Set CF

   GPIB_disconnect();
}

BOOL ACQ_HP8560(C8    *filename,
                S32    GPIB_address,
                C8    *caption,
                S64    carrier,           
                S64    ext_IF,
                S64    ext_LO,
                SINGLE VBW_factor,
                DOUBLE ext_mult,          
                SINGLE NF_characteristic,
                SINGLE min_offset,        
                SINGLE max_offset,
                S32    clip,
                SINGLE ref = 10000.0F)
{
   S32 i;

   S32    min_decade = S32(log10(min_offset)+0.5);
   S32    max_decade = S32(log10(max_offset)+0.5) - 1;
   SINGLE ext_mult_dB = 0.0F;

   SAL_debug_printf("Minimum offset = %d Hz\n",S32(pow(10.0,min_decade)+0.5));
   SAL_debug_printf("Maximum offset = %d Hz\n",S32(pow(10.0,max_decade+1)+0.5));

   ext_mult_dB = -20.0F * (SINGLE) log10(ext_mult);

   SAL_debug_printf("External frequency multiplier = %.06lfx (%.02f dB)\n",
                     ext_mult,ext_mult_dB);

   //
   // Start up GPIB
   //

   GPIB_connect (GPIB_address,
                 GPIB_error,
                 0,
                 1000000);

   GPIB_set_EOS_mode(10);
   GPIB_set_serial_read_dropout(500);

   //
   // Open output file
   //

   out = fopen(filename,"wt");

   if (out == NULL)
      {
      GPIB_disconnect();

      SAL_alert_box("Error","Could not open %s\n",filename);
      return FALSE;
      }

   fprintf(out,";\n");
   fprintf(out,";Composite-noise plot file %s\n",filename);
   fprintf(out,";\n");
   fprintf(out,";Generated by PN.EXE V"VERSION" of "__DATE__"\n");
   fprintf(out,";by John Miles, KE5FX (john@miles.io)\n");
   fprintf(out,";\n\n");

   //
   // Log caption, if present
   //

   if (caption[0])
      {
      fprintf(out,"CAP %s\n",caption);
      }

   //
   // Log timestamp and instrument ID
   //

   static C8 ID[1024];
   static C8 REV[1024];

   strcpy(ID,GPIB_query("ID?"));
   strcpy(REV,GPIB_query("REV?"));

   kill_trailing_whitespace(ID);
   kill_trailing_whitespace(REV);

   fprintf(out,"TIM %s\n",timestamp());
   fprintf(out,"IMO %s\n", ID);
   fprintf(out,"IRV %s\n\n", REV);

   S32 is_8562A = (strstr(ID,"8562A") != NULL);

   //
   // Log decade statistics
   //

   fprintf(out,"DRG %d,%d\n", min_decade,max_decade);
   fprintf(out,"DCC 540\n");   // Decade point count=540 points (60/division)
   fprintf(out,"OVC 600\n\n"); // Overlap point count=600 points into next decade

   fprintf(out,"EIF %I64d Hz\n",ext_IF);
   fprintf(out,"ELO %I64d Hz\n",ext_LO);
   fprintf(out,"MUL %lf\n\n",ext_mult);

   fprintf(out,"VBF %.02f\n",VBW_factor);
   fprintf(out,"CLP %d\n\n",clip);

   //
   // Set basic analyzer state needed for operation of program
   //

   GPIB_printf("SNGLS;");     // Select single-sweep mode
   GPIB_printf("MKOFF ALL;"); // No markers by default
   GPIB_printf("LG 10DB;");   // Need 10 dB/division log scale
   GPIB_printf("RB AUTO;");   // Autoselect RBW
   GPIB_printf("VB AUTO;");   // Autoselect VBW
   GPIB_printf("ST AUTO;");   // Autoselect sweep time
   GPIB_printf("TDF P;");     // ASCII queries

   //
   // If no carrier specified, find the strongest signal in the 
   // currently-selected view
   //
   // The marker-based version of this routine will generally land 
   // within 10-20 Hz.  The use of the counter marker should improve 
   // accuracy to +/- 1 Hz.  We then round the result to the nearest 
   // minimum-decade multiple
   //

   if (carrier < 0)
      {
      SAL_debug_printf("Measuring carrier frequency... ");
      assert(ext_LO == -1);

      //
      // Zero in on signal, dividing span by 10 at each step until span width is <= 5 kHz
      //

      while (1)
         {
         GPIB_printf("TS;");        // Take sweep
         GPIB_printf("MKPK HI;");   // Put marker on highest signal
         GPIB_printf("MKCF;");      // Marker -> center frequency

         DOUBLE span = -10000.0;

         while (span == -10000.0)
            {
            C8 *span_text = GPIB_query("SP?");
            sscanf(span_text,"%lf",&span); 
            }

         if (span <= 5000.0)
            {
            break;
            }

         span /= 10.0;              // Select next-narrower span and repeat measurement  
         GPIB_printf("SP %lfHZ;",span);
         }

      GPIB_printf(is_8562A ? "MKFCR 10HZ;" : "MKFCR 1HZ;"); // Measure at maximum supported resolution
      GPIB_printf("TS;");        // Take sweep
      GPIB_printf("MKPK HI;");   // Put marker on highest signal
      GPIB_printf("MKFC ON;");   // Turn on counter
      GPIB_printf("TS;");        // Take sweep

      GPIB_flush_receive_buffers();

      DOUBLE C = -10000.0;

      while (C == -10000.0)         // Query final marker frequency
         {
         C8 *CF_text = GPIB_query("MKF?");
         sscanf(CF_text,"%lf",&C); 
         }

      if (C < 0.0F)
         {
         C = -C;
         }

      GPIB_printf("MKOFF ALL;");    // Kill marker

      carrier = S64(C + 0.5);

      S64 decade_start = (S64) (pow(10.0, min_decade) + 0.5F);

      carrier = ((carrier + (decade_start / 2)) / decade_start) * decade_start;

      carrier = S64(((DOUBLE) carrier / ext_mult) + 0.5);  
      
      SAL_debug_printf("Result = %I64d Hz\n",carrier);
      }

   //
   // Perform actual measurement at external IF, if one is specified
   //

   S64 source_carrier = carrier;

   if (ext_IF >= 0)
      {
      carrier = ext_IF;
      }

   //
   // Log beginning of sweep series
   //

   S64 start = elapsed_uS();

   //
   // For each decade...
   //

   bool reference_level_set = FALSE;

   S64 mult_delta_Hz = S64(((DOUBLE) carrier * ext_mult) + 0.5) - carrier;  

   for (S32 d=min_decade; d <= max_decade; d++)
      {
      static SINGLE trace_A   [2][601];
      static DOUBLE freq      [2][601];
      SINGLE        correction[2];

      S32 decade_start = (S32) (pow(10.0, d) + 0.5F);
      S32 next_decade  = decade_start * 10;
      S32 total_span   = decade_start * 10;

      //
      // Select RBW and span
      //

      S32 RBW = decade_start / 10;
      S32 VBW = (S32) ((RBW * VBW_factor) + 0.5F);

      GPIB_printf("MKOFF ALL;");                // Kill marker

      GPIB_printf("SP %dHZ;",total_span);       // Set total span
      GPIB_printf("RB %dHZ;",RBW);              // Set RBW

      //
      // If reference level not explicitly specified, establish fullscreen level 
      // using positive-peak detector and no additional video filtering
      //

      if (!reference_level_set)
         {
         reference_level_set = TRUE;

         if (ref == 10000.0F)
            {
            //
            // Tune to specified center frequency and establish fullscreen 
            // reference level using positive-peak detector and no 
            // additional video filtering
            //
            // Start by declaring a high reference level that is guaranteed to
            // place the signal peak onscreen
            //

            if (VBW > RBW)
               {
               GPIB_printf("VB %dHZ;",VBW);           // Set specified video BW
               }
            else
               {
               GPIB_printf("VBR 1;");                 // Set 1:1 VBW/RBW ratio
               }

            GPIB_printf("AUNITS DBM;");               // Use dBm units
            GPIB_printf("DET POS;");                  // POS detection

            GPIB_printf("RL 30DBM;");                 // +30 dBm reference level
            GPIB_printf("ROFFSET 0DB;");              // Clear reference offset

            //
            // Tune to specified center frequency and take sweep
            //

            GPIB_printf("CF %I64dHZ;",carrier + mult_delta_Hz);   // Set CF
            GPIB_printf("TS;");                                   // Take sweep
            GPIB_printf("MKPK HI;");                              // Put marker on highest signal
            GPIB_printf("MKRL;");                                 // Set reference level = marker amplitude

            //
            // Query reference level and use it as the offset 
            // for subsequent dBm/Hz measurements
            //

            while (ref == 10000.0F)
               {
               C8 *RL_text = GPIB_query("RL?");
               sscanf(RL_text,"%f",&ref); 
               }
            }

         GPIB_printf("ROFFSET %fDB;",-ref);        // Establish reference offset
         GPIB_printf("RL 0DBM;");                  // Set relative reference level = 0 dBm

         //
         // Apply reference-level clipping if requested
         //

         for (S32 cp=0; cp < clip; cp += 10)
            {
            GPIB_printf("RL DN;");
            }
         }

      //
      // Enable video filtering
      //

      GPIB_printf("MKOFF ALL;");                // No markers
      GPIB_printf("VAVG OFF;");                 // No video averaging
      GPIB_printf("DET SMP;");                  // We use sampled detection for noise-like signals...

      GPIB_printf("VB %dHZ;",VBW);              // Set video bandwidth

      S64 tune_freq = carrier + (decade_start * 5);

      //
      // Log RF attenuation used for this sweep
      //

      SINGLE att = 10000.0F;

      while (att == 10000.0F)
         {
         C8 *AT_text = GPIB_query("AT?");
         sscanf(AT_text,"%f",&att);
         }

      SAL_debug_printf("Carrier level = %.02f dBm, RF atten = %d dB\n",ref,S32(att + 0.5F));

      //
      // Log carrier amplitude used for this sweep
      //

      fprintf(out,"CAR %I64d Hz, %.02f dBm\n",
         source_carrier,
         ref);

      fprintf(out,"ATT %d dB\n",
         S32(att + 0.5F));

      for (S32 half=0; half < 2; half++)
         {
         SAL_serve_message_queue();

         //
         // Take a reading at each half-decade
         //

         SAL_debug_printf("Analyzing decade 10^%d, sweep %d of 2...\n",d,half+1);

         if (key_hit)
            {
            key_hit = FALSE;
            EXIT_HP8560(clip, carrier + mult_delta_Hz);

            SAL_alert_box("Error","Aborted by keypress");
            return FALSE;
            }

         GPIB_printf("CF %I64dHZ;",tune_freq + mult_delta_Hz);    // Set center freq
         GPIB_printf("TS;");                                      // Take a sweep

         //
         // Fetch data from display trace A
         //

         DOUBLE df_dp = ((DOUBLE) total_span) / 600.0;
         DOUBLE F     = ((DOUBLE) tune_freq) - (300.0 * df_dp);

#if BINARY_ACQ
         GPIB_printf("TDF B;TRA?");     // Request 16-bit binary display values from trace A

         static U16 curve[601];     

         memset(curve, 0, 601 * sizeof(S16));
         memcpy(curve, GPIB_read_BIN(601 * sizeof(S16),TRUE,FALSE), 601 * sizeof(S16));

         SINGLE offset      = -100.0F;
         SINGLE scale       = 100.0F / 600.0F;
         SINGLE grat_offset = 0.0F;

         for (i=0; i < 601; i++)
            {
            U16 scrn_Y = ((curve[i] & 0xff00) >> 8) + ((curve[i] & 0x00ff) << 8);

            trace_A[half][i] = (((SINGLE(scrn_Y) + grat_offset) * scale) + offset) - (SINGLE) clip;
            freq[half][i]    = F;
     
            F += df_dp;
            }

         GPIB_printf("TDF P;");              // Re-enable ASCII queries
#else
         GPIB_set_serial_read_dropout(2000); // 500 ms isn't long enough for 8561E; use 2000 instead for ASCII traces

         GPIB_printf("TDF P;TRA?");         // Request dBm values from trace A
                                             // (8560E ASCII data is comma-separated rather than CR/LF separated)
         static C8 input_line[32768];
         input_line[0] = 0;           

         while (1)
            {
            C8 *read_result = GPIB_read_ASC(1024, TRUE);

            if ((read_result == NULL) || (read_result[0] == 0))
               {
               EXIT_HP8560(clip, carrier + mult_delta_Hz);

               SAL_alert_box("Error","Sweep read terminated prematurely at column %d\n",i);
               return FALSE;
               }

            strcat(input_line, read_result);

            if ((strchr(read_result, 10) != NULL) ||
                (strlen(read_result) != 1024)     ||
                (strlen(input_line) >= 32767-1024))
               {
               break;
               }
            }

         GPIB_set_serial_read_dropout(500);

         //
         // Parse input line
         // 

         C8 *ptr = input_line;
         i = 0;

         while (1)
            {
            sscanf(ptr,"%f",&trace_A[half][i]);

            freq[half][i] = F;
            F += df_dp;

            i++;
            if (i == 601)
               {
               break;
               }

            ptr = strchr(ptr,',');

            if (ptr == NULL)
               {
               EXIT_HP8560(clip, carrier + mult_delta_Hz);

               SAL_alert_box("Error","Input line terminated prematurely at column %d, offset %d\n",i,ptr-input_line);
               return FALSE;
               }

            ptr++;
            }
#endif
         //
         // Apply noise-normalization factor
         //
         // This is typically 2 dB greater than the baseline
         // 10*log(RBW) normalization factor due to filter
         // and detector noise-response characteristics
         // 

         SINGLE RB_factor = -10.0F * (SINGLE) log10((DOUBLE) RBW);

         correction[half] = RB_factor + NF_characteristic;

         tune_freq += next_decade;
         }

      //
      // Display results
      //

      SAL_debug_printf("\n%8d Hz: %.02f dBc/Hz\n",
         decade_start,
         trace_A[0][60] + correction[0] + ext_mult_dB);

      SAL_debug_printf("%8d Hz: %.02f dBc/Hz / %.02f dBc/Hz\n\n",
         next_decade,
         trace_A[0][600] + correction[0] + ext_mult_dB,
         trace_A[1][60]  + correction[1] + ext_mult_dB);

      //
      // Send decade and overlap data to output file
      //

      fprintf(out,"DEC %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[0]);

      for (i=60; i < 600; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i - 60,
            freq[0][i],
            trace_A[0][i] + ext_mult_dB);
         }

      fprintf(out,"OVL %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[1]);

      for (i=0; i < 600; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i,
            freq[1][i],
            trace_A[1][i] + ext_mult_dB);
         }
      }

   //
   // Write cumulative sweep time and EOF timestamp
   //

   S64 end = elapsed_uS();

   fprintf(out,"ELP %I64ds\n",((end - start) + 500000) / 1000000);
   fprintf(out,"EOF %s\n",timestamp());

   EXIT_HP8560(clip, carrier + mult_delta_Hz);
   return TRUE;
}

//****************************************************************************
//*                                                                          *
//* Acquire data from HP 8590 series                                         *
//*                                                                          *
//****************************************************************************

void EXIT_HP8590(S32 clip, //)
                 S64 carrier_Hz)
{
   if (out != NULL)
      {
      fclose(out);
      out = NULL;
      }

   //
   // Put clipped peak back onscreen
   //

   for (S32 cp=0; cp < clip; cp += 10)
      {
      GPIB_printf("RL UP;");
      }

   GPIB_printf("MKOFF ALL;");               // No marker
   GPIB_printf("CONTS;");                   // Return to continuous-sweep mode
   GPIB_printf("RB AUTO;");                 // Autoselect RBW
   GPIB_printf("VB AUTO;");                 // Autoselect VBW
   GPIB_printf("ROFFSET 0DB;");             // No reference offset
   GPIB_printf("DET SMP;");                 // Normal (sample) detection
   GPIB_printf("CF %I64dHZ;",carrier_Hz);   // Set CF

   GPIB_disconnect();
}

BOOL ACQ_HP8590(C8    *filename,
                S32    GPIB_address,
                C8    *caption,
                S64    carrier,           
                S64    ext_IF,
                S64    ext_LO,
                SINGLE VBW_factor,
                DOUBLE ext_mult,          
                SINGLE NF_characteristic,
                SINGLE min_offset,        
                SINGLE max_offset,
                S32    clip,
                SINGLE ref = 10000.0F)
{
   S32 i;

   S32    min_decade = S32(log10(min_offset)+0.5);
   S32    max_decade = S32(log10(max_offset)+0.5) - 1;
   SINGLE ext_mult_dB = 0.0F;

   SAL_debug_printf("Minimum offset = %d Hz\n",S32(pow(10.0,min_decade)+0.5));
   SAL_debug_printf("Maximum offset = %d Hz\n",S32(pow(10.0,max_decade+1)+0.5));

   ext_mult_dB = -20.0F * (SINGLE) log10(ext_mult);

   SAL_debug_printf("External frequency multiplier = %.06lfx (%.02f dB)\n",
                     ext_mult,ext_mult_dB);

   //
   // Start up GPIB
   //

   GPIB_connect (GPIB_address,
                 GPIB_error,
                 0,
                 1000000);

   GPIB_set_EOS_mode(10);
   GPIB_set_serial_read_dropout(500);

   //
   // Open output file
   //

   out = fopen(filename,"wt");

   if (out == NULL)
      {
      GPIB_disconnect();

      SAL_alert_box("Error","Could not open %s\n",filename);
      return FALSE;
      }

   fprintf(out,";\n");
   fprintf(out,";Composite-noise plot file %s\n",filename);
   fprintf(out,";\n");
   fprintf(out,";Generated by PN.EXE V"VERSION" of "__DATE__"\n");
   fprintf(out,";by John Miles, KE5FX (john@miles.io)\n");
   fprintf(out,";\n\n");

   //
   // Log caption, if present
   //

   if (caption[0])
      {
      fprintf(out,"CAP %s\n",caption);
      }

   //
   // Log timestamp and instrument ID
   //

   static C8 ID[1024];
   static C8 REV[1024];

   strcpy(ID,GPIB_query("ID?"));
   strcpy(REV,GPIB_query("REV?"));

   kill_trailing_whitespace(ID);
   kill_trailing_whitespace(REV);

   fprintf(out,"TIM %s\n",timestamp());
   fprintf(out,"IMO %s\n", ID);
   fprintf(out,"IRV %s\n\n", REV);

   //
   // Log decade statistics
   //

   fprintf(out,"DRG %d,%d\n", min_decade,max_decade);
   fprintf(out,"DCC 360\n");   // Decade point count=360 points (40/division)
   fprintf(out,"OVC 400\n\n"); // Overlap point count=400 points into next decade

   fprintf(out,"EIF %I64d Hz\n",ext_IF);
   fprintf(out,"ELO %I64d Hz\n",ext_LO);
   fprintf(out,"MUL %lf\n\n",ext_mult);

   fprintf(out,"VBF %.02f\n",VBW_factor);
   fprintf(out,"CLP %d\n\n",clip);

   //
   // Set basic analyzer state needed for operation of program
   //

   GPIB_printf("SNGLS;");     // Select single-sweep mode
   GPIB_printf("MKOFF ALL;"); // No markers by default
   GPIB_printf("LG 10DB;");   // Need 10 dB/division log scale
   GPIB_printf("RB AUTO;");   // Autoselect RBW
   GPIB_printf("VB AUTO;");   // Autoselect VBW
   GPIB_printf("ST AUTO;");   // Autoselect sweep time
   GPIB_printf("TDF P;");     // ASCII queries

   //
   // If no carrier specified, find the strongest signal in the 
   // currently-selected view
   //
   // This routine will generally land within 10-20 Hz.  We round
   // the result to the nearest minimum-decade multiple
   //

   if (carrier < 0)
      {
      SAL_debug_printf("Measuring carrier frequency... ");
      assert(ext_LO == -1);

      //
      // Zero in on signal, dividing span by 10 at each step until span width is <= 5 kHz
      //

      while (1)
         {
         GPIB_printf("TS;");        // Take sweep
         GPIB_printf("MKPK HI;");   // Put marker on highest signal
         GPIB_printf("MKCF;");      // Marker -> center frequency

         DOUBLE span = -10000.0;

         while (span == -10000.0)
            {
            C8 *span_text = GPIB_query("SP?");
            sscanf(span_text,"%lf",&span); 
            }

         if (span <= 5000.0)
            {
            break;
            }

         span /= 10.0;              // Select next-narrower span and repeat measurement  
         GPIB_printf("SP %lfHZ;",span);
         }

      GPIB_flush_receive_buffers();

      DOUBLE C = -10000.0;

      while (C == -10000.0)         // Query final marker frequency
         {
         C8 *CF_text = GPIB_query("MKF?");
         sscanf(CF_text,"%lf",&C); 
         }

      if (C < 0.0F)
         {
         C = -C;
         }

      GPIB_printf("MKOFF ALL;");    // Kill marker

      carrier = S64(C + 0.5);

      S64 decade_start = (S64) (pow(10.0, min_decade) + 0.5F);

      carrier = ((carrier + (decade_start / 2)) / decade_start) * decade_start;

      carrier = S64(((DOUBLE) carrier / ext_mult) + 0.5);  
      
      SAL_debug_printf("Result = %I64d Hz\n",carrier);
      }

   //
   // Perform actual measurement at external IF, if one is specified
   //

   S64 source_carrier = carrier;

   if (ext_IF >= 0)
      {
      carrier = ext_IF;
      }

   //
   // Log beginning of sweep series
   //

   S64 start = elapsed_uS();

   //
   // For each decade...
   //

   bool reference_level_set = FALSE;

   S64 mult_delta_Hz = S64(((DOUBLE) carrier * ext_mult) + 0.5) - carrier;  

   for (S32 d=min_decade; d <= max_decade; d++)
      {
      static SINGLE trace_A   [2][401];
      static DOUBLE freq      [2][401];
      SINGLE        correction[2];

      S32 decade_start = (S32) (pow(10.0, d) + 0.5F);
      S32 next_decade  = decade_start * 10;
      S32 total_span   = decade_start * 10;

      //
      // Select RBW and span
      //

      S32 RBW = decade_start / 10;
      S32 VBW = (S32) ((RBW * VBW_factor) + 0.5F);

      GPIB_printf("MKOFF ALL;");                // Kill marker

      GPIB_printf("SP %dHZ;",total_span);       // Set total span
      GPIB_printf("RB %dHZ;",RBW);              // Set RBW

      //
      // If reference level not explicitly specified, establish fullscreen level 
      // using positive-peak detector and no additional video filtering
      //

      if (!reference_level_set)
         {
         reference_level_set = TRUE;

         if (ref == 10000.0F)
            {
            //
            // Tune to specified center frequency and establish fullscreen 
            // reference level using positive-peak detector and no 
            // additional video filtering
            //
            // Start by declaring a high reference level that is guaranteed to
            // place the signal peak onscreen
            //

            if (VBW > RBW)
               {
               GPIB_printf("VB %dHZ;",VBW);           // Set specified video BW
               }
            else
               {
               GPIB_printf("VBR 1;");                 // Set 1:1 VBW/RBW ratio
               }

            GPIB_printf("AUNITS DBM;");               // Use dBm units
            GPIB_printf("DET POS;");                  // POS detection (8590 doesn't support rosenfell NRM mode)

            GPIB_printf("RL 30DM;");                  // +30 dBm reference level
            GPIB_printf("ROFFSET 0DB;");              // Clear reference offset

            //
            // Tune to specified center frequency and take sweep
            //

            GPIB_printf("CF %I64dHZ;",carrier + mult_delta_Hz);   // Set CF
            GPIB_printf("TS;");                                   // Take sweep
            GPIB_printf("MKPK HI;");                              // Put marker on highest signal
            GPIB_printf("MKRL;");                                 // Set reference level = marker amplitude

            //
            // Query reference level and use it as the offset 
            // for subsequent dBm/Hz measurements
            //

            while (ref == 10000.0F)
               {
               C8 *RL_text = GPIB_query("RL?");
               sscanf(RL_text,"%f",&ref); 
               }
            }

         GPIB_printf("ROFFSET %fDB;",-ref);        // Establish reference offset
         GPIB_printf("RL 0DM;");                   // Set relative reference level = 0 dBm

         //
         // Apply reference-level clipping if requested
         //

         for (S32 cp=0; cp < clip; cp += 10)
            {
            GPIB_printf("RL DN;");
            }
         }

      //
      // Enable video filtering
      //

      GPIB_printf("MKOFF ALL;");                // No markers
      GPIB_printf("DET SMP;");                  // We use sampled detection for noise-like signals...

      GPIB_printf("VB %dHZ;",VBW);              // Set video bandwidth

      S64 tune_freq = carrier + (decade_start * 5);

      //
      // Log RF attenuation used for this sweep
      //

      SINGLE att = 10000.0F;

      while (att == 10000.0F)
         {
         C8 *AT_text = GPIB_query("AT?");
         sscanf(AT_text,"%f",&att);
         }

      SAL_debug_printf("Carrier level = %.02f dBm, RF atten = %d dB\n",ref,S32(att + 0.5F));

      //
      // Log carrier amplitude used for this sweep
      //

      fprintf(out,"CAR %I64d Hz, %.02f dBm\n",
         source_carrier,
         ref);

      fprintf(out,"ATT %d dB\n",
         S32(att + 0.5F));

      for (S32 half=0; half < 2; half++)
         {
         SAL_serve_message_queue();

         //
         // Take a reading at each half-decade
         //

         SAL_debug_printf("Analyzing decade 10^%d, sweep %d of 2...\n",d,half+1);

         if (key_hit)
            {
            key_hit = FALSE;
            EXIT_HP8590(clip, carrier + mult_delta_Hz);

            SAL_alert_box("Error","Aborted by keypress");
            return FALSE;
            }

         GPIB_printf("CF %I64dHZ;",tune_freq + mult_delta_Hz);    // Set center freq

         GPIB_printf("TS;");                                      // Take a sweep

         //
         // Fetch data from display trace A
         //

         DOUBLE df_dp = ((DOUBLE) total_span) / 400.0;
         DOUBLE F     = ((DOUBLE) tune_freq) - (200.0 * df_dp);

#if BINARY_ACQ
         GPIB_printf("TDF B;TRA?");     // Request 16-bit binary display values from trace A

         static U16 curve[401];     

         memset(curve, 0, 401 * sizeof(S16));
         memcpy(curve, GPIB_read_BIN(401 * sizeof(S16),TRUE,FALSE), 401 * sizeof(S16));

         SINGLE offset      = -80.0F;
         SINGLE scale       = 80.0F / 8000.0F;
         SINGLE grat_offset = 0.0F;

         for (i=0; i < 401; i++)
            {
            U16 scrn_Y = ((curve[i] & 0xff00) >> 8) + ((curve[i] & 0x00ff) << 8);

            trace_A[half][i] = (((SINGLE(scrn_Y) + grat_offset) * scale) + offset) - (SINGLE) clip;
            freq[half][i]    = F;
     
            F += df_dp;
            }

         GPIB_printf("TDF P;");          // Re-enable ASCII queries
#else
         GPIB_set_serial_read_dropout(2000);

         GPIB_printf("TDF P;TRA?");     // Request dBm values from trace A                                   
                                         // (8590E ASCII data is comma-separated rather than CR/LF separated)  
         static C8 input_line[32768];
         input_line[0] = 0;

         while (1)
            {
            C8 *read_result = GPIB_read_ASC(1024, TRUE);

            if ((read_result == NULL) || (read_result[0] == 0))
               {
               EXIT_HP8590(clip, carrier + mult_delta_Hz);

               SAL_alert_box("Error","Sweep read terminated prematurely at column %d\n",i);
               return FALSE;
               }

            strcat(input_line, read_result);

            if ((strchr(read_result, 10) != NULL) ||
                (strlen(read_result) != 1024)     ||
                (strlen(input_line) >= 32767-1024))
               {
               break;
               }
            }

         GPIB_set_serial_read_dropout(500);

         //
         // Parse input line
         // 

         C8 *ptr = input_line;
         i = 0;

         while (1)
            {
            sscanf(ptr,"%f",&trace_A[half][i]);

            freq[half][i] = F;
            F += df_dp;

            i++;
            if (i == 401)
               {
               break;
               }

            ptr = strchr(ptr,',');

            if (ptr == NULL)
               {
               EXIT_HP8590(clip, carrier + mult_delta_Hz);

               SAL_alert_box("Error","Input line terminated prematurely at column %d, offset %d\n",i,ptr-input_line);
               return FALSE;
               }

            ptr++;
            }
#endif
         //
         // Apply noise-normalization factor
         //
         // This is typically 2 dB greater than the baseline
         // 10*log(RBW) normalization factor due to filter
         // and detector noise-response characteristics
         // 

         SINGLE RB_factor = -10.0F * (SINGLE) log10((DOUBLE) RBW);

         correction[half] = RB_factor + NF_characteristic;

         tune_freq += next_decade;
         }

      //
      // Display results
      //

      SAL_debug_printf("\n%8d Hz: %.02f dBc/Hz\n",
         decade_start,
         trace_A[0][40] + correction[0] + ext_mult_dB);

      SAL_debug_printf("%8d Hz: %.02f dBc/Hz / %.02f dBc/Hz\n\n",
         next_decade,
         trace_A[0][400] + correction[0] + ext_mult_dB,
         trace_A[1][40]  + correction[1] + ext_mult_dB);

      //
      // Send decade and overlap data to output file
      //

      fprintf(out,"DEC %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[0]);

      for (i=40; i < 400; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i - 40,
            freq[0][i],
            trace_A[0][i] + ext_mult_dB);
         }

      fprintf(out,"OVL %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[1]);

      for (i=0; i < 400; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i,
            freq[1][i],
            trace_A[1][i] + ext_mult_dB);
         }
      }

   //
   // Write cumulative sweep time and EOF timestamp
   //

   S64 end = elapsed_uS();

   fprintf(out,"ELP %I64ds\n",((end - start) + 500000) / 1000000);
   fprintf(out,"EOF %s\n",timestamp());

   EXIT_HP8590(clip, carrier + mult_delta_Hz);
   return TRUE;
}

//****************************************************************************
//*                                                                          *
//* Acquire data from HP 4195A                                               *
//*                                                                          *
//****************************************************************************

void EXIT_HP4195(S32 clip, //)
                 S64 carrier_Hz)
{
   if (out != NULL)
      {
      fclose(out);
      out = NULL;
      }

   GPIB_printf("MCF1");                         // No marker
   GPIB_printf("SWM1");                         // Return to continuous-sweep mode
   GPIB_printf("CPL1");                         // Autoselect RBW
   GPIB_printf("VFTR1");                        // Autoselect VBW
   GPIB_printf("CENTER=%I64dHZ",carrier_Hz);    // Set CF

   GPIB_disconnect();
}

void WAIT_HP4195(S64 carrier)
{
   GPIB_auto_read_mode(FALSE);

   for (;;)      // wait for trace complete
      {
      C8 stat = GPIB_serial_poll();
     
      if (stat & 0x02)
         break;  // trace complete
      else
         Sleep(100);
      }

   GPIB_auto_read_mode(TRUE);

   GPIB_printf("CLS");   // Clear Status byte
}

BOOL ACQ_HP4195(C8    *filename,
                S32    GPIB_address,
                C8    *caption,
                S64    carrier,           
                S64    ext_IF,
                S64    ext_LO,
                SINGLE VBW_factor,
                DOUBLE ext_mult,          
                SINGLE NF_characteristic,
                SINGLE min_offset,        
                SINGLE max_offset,
                S32    clip,
                SINGLE ref = 10000.0F)
{
   S32 i;

   S32    min_decade = S32(log10(min_offset)+0.5);
   S32    max_decade = S32(log10(max_offset)+0.5) - 1;
   SINGLE ext_mult_dB = 0.0F;

   SAL_debug_printf("Minimum offset = %d Hz\n",S32(pow(10.0,min_decade)+0.5));
   SAL_debug_printf("Maximum offset = %d Hz\n",S32(pow(10.0,max_decade+1)+0.5));

   ext_mult_dB = -20.0F * (SINGLE) log10(ext_mult);

   SAL_debug_printf("External frequency multiplier = %.06lfx (%.02f dB)\n",
                     ext_mult,ext_mult_dB);

   //
   // Start up GPIB
   //

   GPIB_connect (GPIB_address,
                 GPIB_error,
                 0,
                 1000000);

   GPIB_set_EOS_mode(10);
   GPIB_set_serial_read_dropout(500);

   //
   // Open output file
   //

   out = fopen(filename,"wt");

   if (out == NULL)
      {
      GPIB_disconnect();

      SAL_alert_box("Error","Could not open %s\n",filename);
      return FALSE;
      }

   fprintf(out,";\n");
   fprintf(out,";Composite-noise plot file %s\n",filename);
   fprintf(out,";\n");
   fprintf(out,";Generated by PN.EXE V"VERSION" of "__DATE__"\n");
   fprintf(out,";by John Miles, KE5FX (john@miles.io)\n");
   fprintf(out,";\n\n");

   //
   // Log caption, if present
   //

   if (caption[0])
      {
      fprintf(out,"CAP %s\n",caption);
      }

   //
   // Log timestamp and instrument ID
   //

   static C8 ID[1024];
   static C8 REV[1024];

   strcpy(ID,GPIB_query("ID?"));
   strcpy(REV,GPIB_query("REV?"));

   kill_trailing_whitespace(ID);
   kill_trailing_whitespace(REV);

   fprintf(out,"TIM %s\n",timestamp());
   fprintf(out,"IMO %s\n", ID);
   fprintf(out,"IRV %s\n\n", REV);

   //
   // Log decade statistics
   //

   fprintf(out,"DRG %d,%d\n", min_decade,max_decade);
   fprintf(out,"DCC 360\n");   // Decade point count=360 points (40/division)
   fprintf(out,"OVC 400\n\n"); // Overlap point count=400 points into next decade

   fprintf(out,"EIF %I64d Hz\n",ext_IF);
   fprintf(out,"ELO %I64d Hz\n",ext_LO);
   fprintf(out,"MUL %lf\n\n",ext_mult);

   fprintf(out,"VBF %.02f\n",VBW_factor);
   fprintf(out,"CLP %d\n\n",clip);

   //
   // Set basic analyzer state needed for operation of program
   //

   GPIB_printf("FNC2");       // Spectrum mode
   GPIB_printf("CLS");        // Clear Status byte
   GPIB_printf("RQS=2");      // Enable End-Of-Sweep status
   GPIB_printf("PORT2");      // Spectrum analysis of port T1
   GPIB_printf("IRNG1");      // 1= Normal / 2= Low Dist / 3= High Sense IF range select
   GPIB_printf("SWT1");       // Linear sweep type
   GPIB_printf("SWM2;TRGM1"); // Select single-sweep mode
   GPIB_printf("MCF1");       // Markers on
   GPIB_printf("SCT1");       // Need 10 dB/division log scale
   GPIB_printf("CPL1");       // Autoselect RBW
   GPIB_printf("VFTR0");      // Autoselect VBW
   GPIB_printf("FMT1");       // ASCII queries

   //
   // If no carrier specified, find the strongest signal in the 
   // currently-selected view
   //
   // This routine will generally land within 10-20 Hz.  We round
   // the result to the nearest minimum-decade multiple
   //

   if (carrier < 0)
      {
      SAL_debug_printf("Measuring carrier frequency... ");
      assert(ext_LO == -1);

      //
      // Zero in on signal, dividing span by 10 at each step until span width is <= 5 kHz
      //

      while (1)
         {
         GPIB_printf("SWTRG");   // Take sweep
         WAIT_HP4195(carrier);   // Wait until sweep done
         GPIB_printf("MKMX");    // Put marker on highest signal
         GPIB_printf("MKCTR");   // Marker -> center frequency

         DOUBLE span = -10000.0;

         while (span == -10000.0)
            {
            C8 *span_text = GPIB_query("SPAN?");
            sscanf(span_text,"%lf",&span); 
            }

         if (span <= 5000.0)
            {
            break;
            }

         span /= 10.0;              // Select next-narrower span and repeat measurement  
         GPIB_printf("SPAN=%lf",span);
         }

      GPIB_flush_receive_buffers();

      DOUBLE C = -10000.0;

      while (C == -10000.0)         // Query final marker frequency
         {
         C8 *CF_text = GPIB_query("CENTER?");
         sscanf(CF_text,"%lf",&C); 
         }

      if (C < 0.0F)
         {
         C = -C;
         }

      GPIB_printf("MCF1");    // Kill marker

      carrier = S64(C + 0.5);

      S64 decade_start = (S64) (pow(10.0, min_decade) + 0.5F);

      carrier = ((carrier + (decade_start / 2)) / decade_start) * decade_start;

      carrier = S64(((DOUBLE) carrier / ext_mult) + 0.5);  
      
      SAL_debug_printf("Result = %I64d Hz\n",carrier);
      }

   //
   // Perform actual measurement at external IF, if one is specified
   //

   S64 source_carrier = carrier;

   if (ext_IF >= 0)
      {
      carrier = ext_IF;
      }

   //
   // Log beginning of sweep series
   //

   S64 start = elapsed_uS();

   //
   // For each decade...
   //

   bool reference_level_set = FALSE;

   S64 mult_delta_Hz = S64(((DOUBLE) carrier * ext_mult) + 0.5) - carrier;  

   for (S32 d=min_decade; d <= max_decade; d++)
      {
      static SINGLE trace_A   [2][401];
      static DOUBLE freq      [2][401];
      SINGLE        correction[2];

      S32 decade_start = (S32) (pow(10.0, d) + 0.5F);
      S32 next_decade  = decade_start * 10;
      S32 total_span   = decade_start * 10;

      //
      // Select RBW and span
      //

      S32 RBW = decade_start / 10;
      S32 VBW = (S32) ((RBW * VBW_factor) + 0.5F);

      GPIB_printf("MCF1");                      // Kill marker

      GPIB_printf("SPAN=%d",total_span);        // Set total span
      GPIB_printf("RBW=%d",RBW);                // Set RBW

      //
      // If reference level not explicitly specified, establish fullscreen level 
      // using positive-peak detector and no additional video filtering
      //

      if (!reference_level_set)
         {
         reference_level_set = TRUE;

         if (ref == 10000.0F)
            {
            //
            // Tune to specified center frequency and establish fullscreen 
            // reference level using positive-peak detector and no 
            // additional video filtering
            //
            // Start by declaring a high reference level that is guaranteed to
            // place the signal peak onscreen
            //

            if (VBW > RBW)
               {
               GPIB_printf("VFTR0");
               }
            else
               {
               GPIB_printf("VFTR0");                  // Set 1:1 VBW/RBW ratio
               }

            GPIB_printf("SAP1");                      // Use dBm units

            GPIB_printf("REF=30DM");                  // +30 dBm reference level

            //
            // Tune to specified center frequency and take sweep
            //

            GPIB_printf("CENTER=%I64d",carrier + mult_delta_Hz);   // Set CF
            GPIB_printf("SWTRG");   // Take sweep
            WAIT_HP4195(carrier);   // Wait until sweep done
            GPIB_printf("MKMX");    // Put marker on highest signal
            GPIB_printf("MKREF");   // Set reference level = marker amplitude

            //
            // Query reference level and use it as the offset 
            // for subsequent dBm/Hz measurements
            //

            while (ref == 10000.0F)
               {
               C8 *RL_text = GPIB_query("REF?");
               sscanf(RL_text,"%f",&ref); 
               }
            }
         }

      //
      // Enable video filtering
      //

      GPIB_printf("MCF1");                // No markers
      GPIB_printf("VFTR0");

      S64 tune_freq = carrier + (decade_start * 5);

      //
      // Log RF attenuation used for this sweep
      //

      SINGLE att = 10000.0F;

      while (att == 10000.0F)
         {
         C8 *AT_text = GPIB_query("ATT1?");  // ATR1 = port 1, ATT1 = port 2
         sscanf(AT_text,"%f",&att);
         }

      SAL_debug_printf("Carrier level = %.02f dBm, RF atten = %d dB\n",ref,S32(att + 0.5F));

      //
      // Log carrier amplitude used for this sweep
      //

      fprintf(out,"CAR %I64d Hz, %.02f dBm\n",
         source_carrier,
         ref);

      fprintf(out,"ATT %d dB\n",
         S32(att + 0.5F));

      for (S32 half=0; half < 2; half++)
         {
         SAL_serve_message_queue();

         //
         // Take a reading at each half-decade
         //

         SAL_debug_printf("Analyzing decade 10^%d, sweep %d of 2...\n",d,half+1);

         if (key_hit)
            {
            key_hit = FALSE;
            EXIT_HP4195(clip, carrier + mult_delta_Hz);

            SAL_alert_box("Error","Aborted by keypress");
            return FALSE;
            }

         GPIB_printf("CENTER=%I64d",tune_freq + mult_delta_Hz);    // Set center freq

         GPIB_printf("SWTRG");  // Take a sweep
         WAIT_HP4195(carrier);   // Wait until sweep done

         //
         // Fetch data from display trace A
         //

         DOUBLE df_dp = ((DOUBLE) total_span) / 400.0;
         DOUBLE F     = ((DOUBLE) tune_freq) - (200.0 * df_dp);

#if BINARY_ACQ
         GPIB_printf("SAP4;FMT3;A?");     // Request 32-bit binary IEEE.754 display values from trace A

         static U32 curve[402];     

         memset(curve, 0, 402 * sizeof(S32));
         memcpy(curve, GPIB_read_BIN(402 * sizeof(S32),TRUE,FALSE), 402 * sizeof(S32));

         SINGLE offset      = -80.0F;
         SINGLE scale       = 80.0F / 8000.0F;
         SINGLE grat_offset = 0.0F;

         for (i=0; i < 401; i++)
            {
            U32 Ybig, Ysmall;
            float scrn_Y = (float) curve[i+1];

            Ybig = curve[i+1];
            Ysmall = (Ybig >> 24) & 255;
            Ysmall |= ((Ybig >> 16) & 255) << 8;
            Ysmall |= ((Ybig >> 8) & 255) << 16;
            Ysmall |= (Ybig & 255) << 24;
            scrn_Y = *(float *)&Ysmall;

            trace_A[half][i] = scrn_Y;
            freq[half][i]    = F;
     
            F += df_dp;
            }

         GPIB_printf("FMT1");          // Re-enable ASCII queries
#else
         GPIB_set_serial_read_dropout(2000);

         GPIB_printf("SAP4;FMT1;A?");     // Request dBm values from trace A                                   

         static C8 input_line[32768];
         input_line[0] = 0;

         while (1)
            {
            C8 *read_result = GPIB_read_ASC(1024, TRUE);

            if ((read_result == NULL) || (read_result[0] == 0))
               {
               EXIT_HP4195(clip, carrier + mult_delta_Hz);

               SAL_alert_box("Error","Sweep read terminated prematurely at column %d\n",i);
               return FALSE;
               }

            strcat(input_line, read_result);

            if ((strchr(read_result, 10) != NULL) ||
                (strlen(read_result) != 1024)     ||
                (strlen(input_line) >= 32767-1024))
               {
               break;
               }
            }

         GPIB_set_serial_read_dropout(500);

         //
         // Parse input line
         // 

         C8 *ptr = input_line;
         i = 0;

         while (1)
            {
            sscanf(ptr,"%f",&trace_A[half][i]);

            freq[half][i] = F;
            F += df_dp;

            i++;
            if (i == 401)
               {
               break;
               }

            ptr = strchr(ptr,',');

            if (ptr == NULL)
               {
               EXIT_HP4195(clip, carrier + mult_delta_Hz);

               SAL_alert_box("Error","Input line terminated prematurely at column %d, offset %d\n",i,ptr-input_line);
               return FALSE;
               }

            ptr++;
            }
#endif
         //
         // Apply noise-normalization factor
         //
         // The default NF characteristic for the 4195A should be 0 dB,
         // since noise-normalization is handled by the use of SAP4 mode
         // 

         correction[half] = NF_characteristic - ref;

         tune_freq += next_decade;
         }

      //
      // Display results
      //

      SAL_debug_printf("\n%8d Hz: %.02f dBc/Hz\n",
         decade_start,
         trace_A[0][40] + correction[0] + ext_mult_dB);

      SAL_debug_printf("%8d Hz: %.02f dBc/Hz / %.02f dBc/Hz\n\n",
         next_decade,
         trace_A[0][400] + correction[0] + ext_mult_dB,
         trace_A[1][40]  + correction[1] + ext_mult_dB);

      //
      // Send decade and overlap data to output file
      //

      fprintf(out,"DEC %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[0]);

      for (i=40; i < 400; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i - 40,
            freq[0][i],
            trace_A[0][i] + ext_mult_dB);
         }

      fprintf(out,"OVL %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[1]);

      for (i=0; i < 400; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i,
            freq[1][i],
            trace_A[1][i] + ext_mult_dB);
         }
      }

   //
   // Write cumulative sweep time and EOF timestamp
   //

   S64 end = elapsed_uS();

   fprintf(out,"ELP %I64ds\n",((end - start) + 500000) / 1000000);
   fprintf(out,"EOF %s\n",timestamp());

   EXIT_HP4195(clip, carrier + mult_delta_Hz);
   return TRUE;
}

//****************************************************************************
//*                                                                          *
//* Acquire data from HP 70000 series                                        *
//*                                                                          *
//****************************************************************************

void EXIT_HP70000(S32 clip, //)
                  S64 carrier_Hz)
{
   if (out != NULL)
      {
      fclose(out);
      out = NULL;
      }

   //
   // Put clipped peak back onscreen
   //

   for (S32 cp=0; cp < clip; cp += 10)
      {
      GPIB_printf("RL UP;");
      }

   GPIB_printf("MKOFF ALL;");               // No marker
   GPIB_printf("CONTS;");                   // Return to continuous-sweep mode
   GPIB_printf("RB AUTO;");                 // Autoselect RBW
   GPIB_printf("VB AUTO;");                 // Autoselect VBW
   GPIB_printf("ROFFSET 0DB;");             // No reference offset
   GPIB_printf("DET AUTO;");                // Auto detection (normally Rosenfell)
   GPIB_printf("CF %I64dHZ;",carrier_Hz);   // Set CF

   GPIB_disconnect();
}

BOOL ACQ_HP70000(C8    *filename,
                 S32    GPIB_address,
                 C8    *caption,
                 S64    carrier,           
                 S64    ext_IF,
                 S64    ext_LO,
                 SINGLE VBW_factor,
                 DOUBLE ext_mult,          
                 SINGLE NF_characteristic,
                 SINGLE min_offset,        
                 SINGLE max_offset,
                 S32    clip,
                 SINGLE ref = 10000.0F)
{
   S32 i;

   S32    min_decade = S32(log10(min_offset)+0.5);
   S32    max_decade = S32(log10(max_offset)+0.5) - 1;
   SINGLE ext_mult_dB = 0.0F;

   SAL_debug_printf("Minimum offset = %d Hz\n",S32(pow(10.0,min_decade)+0.5));
   SAL_debug_printf("Maximum offset = %d Hz\n",S32(pow(10.0,max_decade+1)+0.5));

   ext_mult_dB = -20.0F * (SINGLE) log10(ext_mult);

   SAL_debug_printf("External frequency multiplier = %.06lfx (%.02f dB)\n",
                     ext_mult,ext_mult_dB);

   //
   // Start up GPIB
   //

   GPIB_connect (GPIB_address,
                 GPIB_error,
                 0,
                 1000000);

   GPIB_set_EOS_mode(10);
   GPIB_set_serial_read_dropout(500);

   //
   // Open output file
   //

   out = fopen(filename,"wt");

   if (out == NULL)
      {
      GPIB_disconnect();

      SAL_alert_box("Error","Could not open %s\n",filename);
      return FALSE;
      }

   fprintf(out,";\n");
   fprintf(out,";Composite-noise plot file %s\n",filename);
   fprintf(out,";\n");
   fprintf(out,";Generated by PN.EXE V"VERSION" of "__DATE__"\n");
   fprintf(out,";by John Miles, KE5FX (john@miles.io)\n");
   fprintf(out,";\n\n");

   //
   // Log caption, if present
   //

   if (caption[0])
      {
      fprintf(out,"CAP %s\n",caption);
      }

   //
   // Log timestamp and instrument ID
   //

   static C8 ID[1024];
   static C8 REV[1024];

   strcpy(ID,GPIB_query("ID?"));
   strcpy(REV,GPIB_query("REV?"));

   kill_trailing_whitespace(ID);
   kill_trailing_whitespace(REV);

   fprintf(out,"TIM %s\n",timestamp());
   fprintf(out,"IMO %s\n", ID);
   fprintf(out,"IRV %s\n\n", REV);

   //
   // Log decade statistics
   //

   fprintf(out,"DRG %d,%d\n", min_decade,max_decade);
   fprintf(out,"DCC 720\n");   // Decade point count=720 points (80/division)
   fprintf(out,"OVC 800\n\n"); // Overlap point count=800 points into next decade

   fprintf(out,"EIF %I64d Hz\n",ext_IF);
   fprintf(out,"ELO %I64d Hz\n",ext_LO);
   fprintf(out,"MUL %lf\n\n",ext_mult);

   fprintf(out,"VBF %.02f\n",VBW_factor);
   fprintf(out,"CLP %d\n\n",clip);

   //
   // Set basic analyzer state needed for operation of program
   //

   GPIB_printf("SNGLS;");     // Select single-sweep mode
   GPIB_printf("MKOFF ALL;"); // No markers by default
   GPIB_printf("LG 10DB;");   // Need 10 dB/division log scale
   GPIB_printf("RB AUTO;");   // Autoselect RBW
   GPIB_printf("VB AUTO;");   // Autoselect VBW
   GPIB_printf("ST AUTO;");   // Autoselect sweep time
   GPIB_printf("TDF P;");     // ASCII queries

   //
   // If no carrier specified, find the strongest signal in the 
   // currently-selected view
   //
   // This routine will generally land within 10-20 Hz.  We round
   // the result to the nearest minimum-decade multiple
   //

   if (carrier < 0)
      {
      SAL_debug_printf("Measuring carrier frequency... ");
      assert(ext_LO == -1);

      //
      // Zero in on signal, dividing span by 10 at each step until span width is <= 5 kHz
      //

      while (1)
         {
         GPIB_printf("TS;");        // Take sweep
         GPIB_printf("MKPK HI;");   // Put marker on highest signal
         GPIB_printf("MKCF;");      // Marker -> center frequency

         DOUBLE span = -10000.0;

         while (span == -10000.0)
            {
            C8 *span_text = GPIB_query("SP?");
            sscanf(span_text,"%lf",&span); 
            }

         if (span <= 5000.0)
            {
            break;
            }

         span /= 10.0;              // Select next-narrower span and repeat measurement  
         GPIB_printf("SP %lfHZ;",span);
         }

      GPIB_flush_receive_buffers();

      DOUBLE C = -10000.0;

      while (C == -10000.0)         // Query final marker frequency
         {
         C8 *CF_text = GPIB_query("MKF?");
         sscanf(CF_text,"%lf",&C); 
         }

      if (C < 0.0F)
         {
         C = -C;
         }

      GPIB_printf("MKOFF ALL;");    // Kill marker

      carrier = S64(C + 0.5);

      S64 decade_start = (S64) (pow(10.0, min_decade) + 0.5F);

      carrier = ((carrier + (decade_start / 2)) / decade_start) * decade_start;

      carrier = S64(((DOUBLE) carrier / ext_mult) + 0.5);  
      
      SAL_debug_printf("Result = %I64d Hz\n",carrier);
      }

   //
   // Perform actual measurement at external IF, if one is specified
   //

   S64 source_carrier = carrier;

   if (ext_IF >= 0)
      {
      carrier = ext_IF;
      }

   //
   // Log beginning of sweep series
   //

   S64 start = elapsed_uS();

   //
   // For each decade...
   //

   bool reference_level_set = FALSE;

   S64 mult_delta_Hz = S64(((DOUBLE) carrier * ext_mult) + 0.5) - carrier;  

   for (S32 d=min_decade; d <= max_decade; d++)
      {
      static SINGLE trace_A   [2][800];
      static DOUBLE freq      [2][800];
      SINGLE        correction[2];

      S32 decade_start = (S32) (pow(10.0, d) + 0.5F);
      S32 next_decade  = decade_start * 10;
      S32 total_span   = decade_start * 10;

      //
      // Select RBW and span
      //

      S32 RBW = decade_start / 10;
      S32 VBW = (S32) ((RBW * VBW_factor) + 0.5F);

      GPIB_printf("MKOFF ALL;");                // Kill marker

      GPIB_printf("SP %dHZ;",total_span);       // Set total span
      GPIB_printf("RB %dHZ;",RBW);              // Set RBW

      //
      // If reference level not explicitly specified, establish fullscreen level 
      // using positive-peak detector and no additional video filtering
      //

      if (!reference_level_set)
         {
         reference_level_set = TRUE;

         if (ref == 10000.0F)
            {
            //
            // Tune to specified center frequency and establish fullscreen 
            // reference level using positive-peak detector and no 
            // additional video filtering
            //
            // Start by declaring a high reference level that is guaranteed to
            // place the signal peak onscreen
            //

            if (VBW > RBW)
               {
               GPIB_printf("VB %dHZ;",VBW);           // Set specified video BW
               }
            else
               {
               GPIB_printf("VBR 1;");                 // Set 1:1 VBW/RBW ratio
               }

            GPIB_printf("AUNITS DBM;");               // Use dBm units
            GPIB_printf("DET NRM;");                  // Rosenfell detection

            GPIB_printf("RL 30DBM;");                 // +30 dBm reference level
            GPIB_printf("ROFFSET 0DB;");              // Clear reference offset

            //
            // Tune to specified center frequency and take sweep
            //

            GPIB_printf("CF %I64dHZ;",carrier + mult_delta_Hz);   // Set CF
            GPIB_printf("TS;");                                   // Take sweep
            GPIB_printf("MKPK HI;");                              // Put marker on highest signal
            GPIB_printf("MKRL;");                                 // Set reference level = marker amplitude

            //
            // Query reference level and use it as the offset 
            // for subsequent dBm/Hz measurements
            //

            while (ref == 10000.0F)
               {
               C8 *RL_text = GPIB_query("RL?");
               sscanf(RL_text,"%f",&ref); 
               }
            }

         GPIB_printf("ROFFSET %fDB;",-ref);        // Establish reference offset
         GPIB_printf("RL 0DBM;");                  // Set relative reference level = 0 dBm

         //
         // Apply reference-level clipping if requested
         //

         for (S32 cp=0; cp < clip; cp += 10)
            {
            GPIB_printf("RL DN;");
            }
         }

      //
      // Enable video filtering
      //

      GPIB_printf("MKOFF ALL;");                // No markers
      GPIB_printf("DET SMP;");                  // We use sampled detection for noise-like signals...

      GPIB_printf("VB %dHZ;",VBW);              // Set video bandwidth

      S64 tune_freq = carrier + (decade_start * 5);

      //
      // Log RF attenuation used for this sweep
      //

      SINGLE att = 10000.0F;

      while (att == 10000.0F)
         {
         C8 *AT_text = GPIB_query("AT?");
         sscanf(AT_text,"%f",&att);
         }

      SAL_debug_printf("Carrier level = %.02f dBm, RF atten = %d dB\n",ref,S32(att + 0.5F));

      //
      // Log carrier amplitude used for this sweep
      //

      fprintf(out,"CAR %I64d Hz, %.02f dBm\n",
         source_carrier,
         ref);

      fprintf(out,"ATT %d dB\n",
         S32(att + 0.5F));

      for (S32 half=0; half < 2; half++)
         {
         SAL_serve_message_queue();

         //
         // Take a reading at each half-decade
         //

         SAL_debug_printf("Analyzing decade 10^%d, sweep %d of 2...\n",d,half+1);

         if (key_hit)
            {
            key_hit = FALSE;
            EXIT_HP70000(clip, carrier + mult_delta_Hz);

            SAL_alert_box("Error","Aborted by keypress");
            return FALSE;
            }

         GPIB_printf("CF %I64dHZ;",tune_freq + mult_delta_Hz);    // Set center freq

         GPIB_printf("TS;");                                      // Take a sweep

         //
         // Fetch data from display trace A
         // 70000 data is comma-separated rather than CR/LF separated
         //

         GPIB_set_serial_read_dropout(2000);

         GPIB_printf("TDF P;TRA?");     // Request ASCII dBm values from trace A

         static C8 input_line[32768];
         input_line[0] = 0;

         while (1)
            {
            C8 *read_result = GPIB_read_ASC(1024, TRUE);

            if ((read_result == NULL) || (read_result[0] == 0))
               {
               EXIT_HP70000(clip, carrier + mult_delta_Hz);

               SAL_alert_box("Error","Sweep read terminated prematurely\n");
               return FALSE;
               }

            strcat(input_line, read_result);

            if ((strchr(read_result, 10) != NULL) ||
                (strlen(read_result) != 1024)     ||
                (strlen(input_line) >= 32767-1024))
               {
               break;
               }
            }

         GPIB_set_serial_read_dropout(500);

         //
         // Parse input line
         // 

         DOUBLE df_dp = ((DOUBLE) total_span) / 800.0;
         DOUBLE F     = ((DOUBLE) tune_freq) - (400.0 * df_dp);

         C8 *ptr = input_line;
         i = 0;

         while (1)
            {
            sscanf(ptr,"%f",&trace_A[half][i]);

            freq[half][i] = F;
            F += df_dp;

            i++;
            if (i == 800)
               {
               break;
               }

            ptr = strchr(ptr,',');

            if (ptr == NULL)
               {
               EXIT_HP70000(clip, carrier + mult_delta_Hz);

               SAL_alert_box("Error","Input line terminated prematurely at column %d, offset %d\n",i,ptr-input_line);
               return FALSE;
               }

            ptr++;
            }

         //
         // Apply noise-normalization factor
         //
         // This is typically 2 dB greater than the baseline
         // 10*log(RBW) normalization factor due to filter
         // and detector noise-response characteristics
         // 

         SINGLE RB_factor = -10.0F * (SINGLE) log10((DOUBLE) RBW);

         correction[half] = RB_factor + NF_characteristic;

         tune_freq += next_decade;
         }

      //
      // Display results
      //

      SAL_debug_printf("\n%8d Hz: %.02f dBc/Hz\n",
         decade_start,
         trace_A[0][80] + correction[0] + ext_mult_dB);

      SAL_debug_printf("%8d Hz: %.02f dBc/Hz / %.02f dBc/Hz\n\n",
         next_decade,
         trace_A[0][799] + correction[0] + ext_mult_dB,
         trace_A[1][80]  + correction[1] + ext_mult_dB);

      //
      // Send decade and overlap data to output file
      //

      fprintf(out,"DEC %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[0]);

      for (i=80; i < 800; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i - 80,
            freq[0][i],
            trace_A[0][i] + ext_mult_dB);
         }

      fprintf(out,"OVL %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[1]);

      for (i=0; i < 800; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i,
            freq[1][i],
            trace_A[1][i] + ext_mult_dB);
         }
      }

   //
   // Write cumulative sweep time and EOF timestamp
   //

   S64 end = elapsed_uS();

   fprintf(out,"ELP %I64ds\n",((end - start) + 500000) / 1000000);
   fprintf(out,"EOF %s\n",timestamp());

   EXIT_HP70000(clip, carrier + mult_delta_Hz);
   return TRUE;
}

//****************************************************************************
//*                                                                          *
//* Acquire data from Tektronix 490P/2750P series                            *
//*                                                                          *
//****************************************************************************

void EXIT_TEK490(S32 clip, //)
                 S64 tune_Hz)
{
   if (out != NULL)
      {
      fclose(out);
      out = NULL;
      }

   //
   // Put clipped peak back onscreen
   //

   for (S32 cp=0; cp < clip; cp += 10)
      {
      GPIB_cmd_printf("REFLVL INC;");
      }

   GPIB_cmd_printf("SIGSWP OFF;");              // Return to continuous-sweep mode
   GPIB_cmd_printf("CRSOR KNOB;");              // Return peak/average cursor to local control
   GPIB_cmd_printf("TRIG FRERUN;");          
   GPIB_cmd_printf("ARES ON;");                 // Autoselect RBW
   GPIB_cmd_printf("TUNE %I64dHZ;",tune_Hz);    // Recenter at carrier*ext_mult

   GPIB_disconnect();
}

BOOL ACQ_TEK490(C8    *filename,
                S32    GPIB_address,
                C8    *caption,
                S64    carrier,           
                S64    ext_IF,
                S64    ext_LO,
                SINGLE VBW_factor,
                DOUBLE ext_mult,          
                SINGLE NF_characteristic,
                SINGLE min_offset,        
                SINGLE max_offset,
                S32    clip,
                SINGLE ref = 10000.0F)
{
   S32 i;

   S32    min_decade = S32(log10(min_offset)+0.5);
   S32    max_decade = S32(log10(max_offset)+0.5) - 1;
   SINGLE ext_mult_dB = 0.0F;

   SAL_debug_printf("Minimum offset = %d Hz\n",S32(pow(10.0,min_decade)+0.5));
   SAL_debug_printf("Maximum offset = %d Hz\n",S32(pow(10.0,max_decade+1)+0.5));

   ext_mult_dB = -20.0F * (SINGLE) log10(ext_mult);

   SAL_debug_printf("External frequency multiplier = %.06lfx (%.02f dB)\n",
                     ext_mult,ext_mult_dB);

   //
   // Start up GPIB
   //
   // Since we're doing binary transfers, the LF OR EOI switch on the analyzer must 
   // be set to '1'!
   //

   GPIB_connect (GPIB_address,
                 GPIB_error,
                 0,
                 1000000);

   GPIB_set_EOS_mode(10);
   GPIB_set_serial_read_dropout(500);

   //
   // Identify the exact analyzer model
   //

   static C8 ID[1024]  = "";
   static C8 REV[1024] = "";

   strcpy(ID,GPIB_query("ID?"));

   kill_trailing_whitespace(ID);

   C8 *r = strchr(ID,',');

   if (r != NULL)
      {
      *r = 0;
      strcpy(REV, &r[1]);
      }

   r = strchr(ID,'/');

   if (r != NULL)
      {
      *r = ' ';
      }

   //
   // Older instruments do not support the RFATT query, and the
   // 492P/496P models are not fully synthesized
   //

   BOOL RFATT_supported = TRUE;
   BOOL synthesized_model = TRUE;

   if ((strstr(ID,"494P")   != NULL) ||
       (strstr(ID,"492P")   != NULL) ||
       (strstr(ID,"496P")   != NULL) ||
       (strstr(ID,"492/6P") != NULL))
      {
      RFATT_supported = FALSE;
      }

   if ((strstr(ID,"492P")   != NULL) ||
       (strstr(ID,"496P")   != NULL) ||
       (strstr(ID,"492/6P") != NULL))
      {
      synthesized_model = FALSE;
      }

   //
   // If the user supplied a carrier frequency, tune the analyzer to it,
   // either automatically or with manual assistance
   //
   // (Perform actual measurement at external IF, if one is specified)
   //

   S64 logged_carrier_Hz = carrier;

   if (ext_IF >= 0)
      {
      carrier = ext_IF;
      }

   if (carrier >= 0)
      {
      carrier = (S64) (((DOUBLE) carrier * ext_mult) + 0.5);

      GPIB_cmd_printf("FREQ %I64dHZ;", carrier);
      GPIB_cmd_printf("DEGAUS;");
      }

   if ((!synthesized_model) || (carrier < 0))
      {
      //
      // This is either a 492P/496P, or the carrier was not entered explicitly.
      // FREQ has already tuned as close to the carrier as possible; temporarily 
      // disconnect and prompt the user to center the desired carrier 
      // at the lowest RBW of interest, then reconnect
      //

      GPIB_disconnect();

      C8 confirm[1024];

      wsprintf(confirm,"Either no carrier frequency was entered, or this analyzer does not support synthesized tuning.\n\n"
                       "Tune the analyzer or source to position the carrier at midscreen, then decrease "
                       "resolution bandwidth to %d Hz or less while keeping the carrier centered.\n\n"
                       "Select OK when ready to proceed, or Cancel to abort the measurement.",
                        ((S32) (pow(10.0, min_decade) + 0.5F)) / 10);

      if (MessageBox(hWnd,
                     confirm,
                    "Manual tuning required",
                     MB_OKCANCEL | MB_ICONASTERISK) != IDOK)
         {
         return FALSE;
         }

      GPIB_connect(GPIB_address,
                   GPIB_error,
                   0,
                   1000000);

      GPIB_set_EOS_mode(10);
      GPIB_set_serial_read_dropout(500);

      //
      // Query the carrier frequency, if not provided by the user
      // 

      if (logged_carrier_Hz < 0)
         {
         logged_carrier_Hz = -LONG_MAX;

         while (logged_carrier_Hz == -LONG_MAX)
            {
            DOUBLE temp = 0.0;

            C8 *text = GPIB_query("FREQ?");
            sscanf(text,"FREQ %lf",&temp);

            logged_carrier_Hz = (S64) (temp + 0.5);
            }

         carrier = (S64) (((DOUBLE) logged_carrier_Hz * ext_mult) + 0.5);
         }
      }

   //
   // There should now be a 0-Hz delta between where the analyzer is tuned and
   // the signal at the input jack.  Any further tuning must be performed relative
   // to the analyzer's current center frequency (since it isn't known precisely 
   // on the 492P/496P.)
   //

   S64 cur_Hz = carrier;

   //
   // Open output file
   //

   out = fopen(filename,"wt");

   if (out == NULL)
      {
      GPIB_disconnect();

      SAL_alert_box("Error","Could not open %s\n",filename);
      return FALSE;
      }

   fprintf(out,";\n");
   fprintf(out,";Composite-noise plot file %s\n",filename);
   fprintf(out,";\n");
   fprintf(out,";Generated by PN.EXE V"VERSION" of "__DATE__"\n");
   fprintf(out,";by John Miles, KE5FX (john@miles.io)\n");
   fprintf(out,";\n\n");

   //
   // Log caption, if present
   //

   if (caption[0])
      {
      fprintf(out,"CAP %s\n",caption);
      }

   //
   // Log timestamp and instrument ID
   //

   fprintf(out,"TIM %s\n",timestamp());
   fprintf(out,"IMO %s\n", &ID[3]);
   fprintf(out,"IRV %s\n", REV);

   //
   // Log decade statistics
   //

   fprintf(out,"DRG %d,%d\n", min_decade,max_decade);
   fprintf(out,"DCC 450\n");         // Decade point count=450 points
   fprintf(out,"OVC 500\n\n");       // Overlap point count=500 points into next decade

   fprintf(out,"EIF %I64d Hz\n",ext_IF);
   fprintf(out,"ELO %I64d Hz\n",ext_LO);
   fprintf(out,"MUL %lf\n\n",ext_mult);

   fprintf(out,"CLP %d\n\n",clip);

   //
   // Set basic analyzer state needed for operation of program
   //

   GPIB_cmd_printf("SIGSWP;");           // Arm single-sweep mode
   GPIB_cmd_printf("VRTDSP LOG:10 DB;"); // Need 10 dB/division log scale
   GPIB_cmd_printf("RLUNIT DBM;");       // Units in dBm (not supported, but not harmful, on 1984-level instruments)
   GPIB_cmd_printf("RLMODE MNOISE;");    // Optimize signal chain for minimum noise
   GPIB_cmd_printf("FINE OFF;");         // Normal REFLVL step size
   GPIB_cmd_printf("ARES ON;");          // Autoselect RBW
   GPIB_cmd_printf("TIME AUTO;");        // Autoselect sweep time
   GPIB_cmd_printf("CRSOR AVG;");        // Use average detection for noise (rather than peak)

   //
   // Log beginning of sweep series
   //

   S64 start = elapsed_uS();

   //
   // For each decade...
   //

   bool reference_level_set = FALSE;

   for (S32 d=min_decade; d <= max_decade; d++)
      {
      S32 decade_start = (S32) (pow(10.0, d) + 0.5F);
      S32 next_decade  = decade_start * 10;
      S32 total_span   = decade_start * 10;

      //
      // Select RBW and span
      //

      S32 RBW = decade_start / 10;

      if ((RBW == 10) && (!RFATT_supported))
         {
         RBW = 30;    // Allow use of older instruments' 30-Hz crystal filter in 100-1000 Hz decade
         }

      GPIB_cmd_printf("RESBW %dHZ;",RBW);                       // Set RBW
      GPIB_cmd_printf("SPAN %dHZ;",total_span / 10);            // Set span/division

      //
      // If reference level not already determined, establish it
      //

      if (!reference_level_set)
         {
         reference_level_set = TRUE;

         if (ref != 10000.0F)
            {
            //
            // Reference level is known -- use it to position the carrier at the
            // top of the screen
            //

            GPIB_cmd_printf("REFLVL %fDBM;",ref);                     
            }
         else
            {
            //
            // Start measurement by declaring a high reference level that is guaranteed to
            // place the signal peak onscreen (it's already been centered horizontally)
            //

            GPIB_cmd_printf("REFLVL 30DBM;");                         // Set +30 dBm reference level
            GPIB_cmd_printf("SIGSWP;WAIT;");                          // Take sweep
            GPIB_cmd_printf("FIBIG;");                                // Find peak signal
            GPIB_cmd_printf("TOPSIG;");                               // Set reference level = peak

            //
            // Query reference level
            //

            while (ref == 10000.0F)
               {
               C8 *RL_text = GPIB_query("REFLVL?");
               sscanf(RL_text,"REFLVL %f",&ref); 
               }
            }

         //
         // Apply reference-level clipping if requested
         //

         for (S32 cp=0; cp < clip; cp += 10)
            {
            GPIB_cmd_printf("REFLVL DEC;");
            }
         }

      //
      // Calculate initial CF to position the carrier at the
      // leftmost graticule line
      //

      S64 tune_freq = carrier + (decade_start * 5);

      //
      // Log RF attenuation and carrier amplitude used for this sweep
      //

      SINGLE att = -10000.0F;

      if (RFATT_supported)
         {
         while (att == -10000.0F)
            {
            C8 *AT_text = GPIB_query("RFATT?");      // Not supported pre-1984
            sscanf(AT_text,"RFATT %f",&att);
            }

         SAL_debug_printf("Carrier level = %.02f dBm, RF atten = %d dB\n",ref,S32(att + 0.5F));
         }

      fprintf(out,"CAR %I64d Hz, %.02f dBm\n",
         logged_carrier_Hz,
         ref);

      fprintf(out,"ATT %d dB\n",
         S32(att + 0.5F));

      static SINGLE trace [2][500];
      static DOUBLE freq  [2][500];
      SINGLE        correction[2];

      for (S32 half=0; half < 2; half++)
         {
         SAL_serve_message_queue();

         //
         // Take a reading at each half-decade
         //

         SAL_debug_printf("Analyzing decade 10^%d, sweep %d of 2...\n",d,half+1);

         if (key_hit)
            {
            key_hit = FALSE;
            EXIT_TEK490(clip, carrier-cur_Hz);

            SAL_alert_box("Error","Aborted by keypress");
            return FALSE;
            }

         GPIB_cmd_printf("TUNE %I64dHZ;",tune_freq-cur_Hz);             // Set center freq incrementally 
         cur_Hz = tune_freq;                                            

         GPIB_cmd_printf("SIGSWP;WAIT;");                               // Take a sweep

         //
         // Fetch and parse waveform preamble for our desired
         // format (trace A, binary encoded)
         //

         GPIB_cmd_printf("WFMPRE WFID:A,ENC:BIN;");
         GPIB_printf("WFMPRE?");

         C8 *preamble = GPIB_read_ASC(-1,TRUE);

         S32    nr_pt  = 0;
         S32    pt_off = 0;
         DOUBLE xincr  = 0.0F;
         DOUBLE xzero  = 0.0F;
         SINGLE yoff   = 0.0F;
         SINGLE ymult  = 0.0F;
         SINGLE yzero  = 0.0F;

         sscanf(preamble,"WFMPRE WFID:A,ENCDG:BIN,NR.PT:%d,PT.FMT:Y,PT.OFF:%d,\
XINCR:%lf,XZERO:%lf,XUNIT:HZ,YOFF:%f,YMULT:%f,YZERO:%f",
            &nr_pt,
            &pt_off,
            &xincr,
            &xzero,
            &yoff,
            &ymult,
            &yzero);

         assert(nr_pt == 500);

         //
         // Fetch binary data from display traces A+B and 
         // verify its checksum
         //

         GPIB_printf("CURVE?");

         C8 *expected     = "CURVE CRVID:A,%";
         C8 *curve_result = GPIB_read_ASC(strlen(expected), FALSE);

         assert(!_stricmp(expected,curve_result));

         C8 *cnt = GPIB_read_BIN(2, TRUE);

         U8 n_MSD = ((U8 *) cnt)[0];
         U8 n_LSD = ((U8 *) cnt)[1];

         S32 n_binary_nums = (256*S32(n_MSD)) + S32(n_LSD);
         assert(n_binary_nums == 501);          
         
         static U8 curve[500];

         memset(curve, 0, 500);
         memcpy(curve, GPIB_read_BIN(500,TRUE,FALSE), 500);

         U8 chksum = ((U8 *) GPIB_read_BIN(1,TRUE,FALSE))[0];

         chksum += n_MSD;
         chksum += n_LSD;

         for (i=0; i < 500; i++)
            {
            chksum += curve[i];
            }

         assert(chksum == 0);
 
         GPIB_read_ASC();   // swallow trailing CR/LF
      
         //
         // Fill in trace array with actual dBm/frequency values
         // (in original user carrier space)
         //

         DOUBLE fc = (DOUBLE) carrier;
         DOUBLE fo = (DOUBLE) (tune_freq - carrier);

         DOUBLE df_dp = ((DOUBLE) total_span) / 500.0;
         DOUBLE F     = (fc / ext_mult) + fo - (250.0 * df_dp);

         for (i=0; i < 500; i++)
            {
            freq [half][i] = F;
            trace[half][i] = yzero + ymult * (S32(curve[i]) - yoff);

            F += df_dp;
            }

         //
         // Calculate the effective noise-normalization factor
         // for this RBW
         //
         // Filter values obtained from CAL? query on 492AP B010115
         // (which has a 495P IF strip and 9.7 ROMs)
         //

         SINGLE RB_factor = -10.0F * (SINGLE) log10((DOUBLE) RBW);
         SINGLE F_factor;

         switch (RBW)
            {
            case 3000000: F_factor = 5.1F; break;
            case 1000000: F_factor = 4.2F; break;
            case 300000:  F_factor = 3.9F; break;
            case 100000:  F_factor = 3.7F; break;
            case 10000:   F_factor = 3.7F; break;
            case 1000:    F_factor = 4.4F; break;
            case 100:     F_factor = 3.6F; break;
            case 30:      F_factor = 3.6F; break;
            case 10:      F_factor = 3.7F; break;
            default:      F_factor = 3.7F; break;
            }

         correction[half] = RB_factor + F_factor + NF_characteristic;

         tune_freq += next_decade;
         }

      //
      // Display results
      //

      SAL_debug_printf("\n%8d Hz: %.02f dBc/Hz\n",
         decade_start,
         trace[0][50] + correction[0] + ext_mult_dB - ref);

      SAL_debug_printf("%8d Hz: %.02f dBc/Hz / %.02f dBc/Hz\n\n",
         next_decade,
         trace[0][499] + correction[0] + ext_mult_dB - ref,
         trace[1][50] + correction[1] + ext_mult_dB - ref);

      //
      // Send decade and overlap data to output file
      //

      fprintf(out,"DEC %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[0]);

      for (i=50; i < 500; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i - 50,
            freq[0][i],
            trace[0][i] + ext_mult_dB - ref);
         }

      fprintf(out,"OVL %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[1]);

      for (i=0; i < 500; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i,
            freq[1][i],
            trace[1][i] + ext_mult_dB - ref);
         }
      }

   //
   // Write cumulative sweep time and EOF timestamp
   //

   S64 end = elapsed_uS();

   fprintf(out,"ELP %I64ds\n",((end - start) + 500000) / 1000000);
   fprintf(out,"EOF %s\n",timestamp());

   EXIT_TEK490(clip, carrier-cur_Hz);

   return TRUE;
}

//****************************************************************************
//*                                                                          *
//* Acquire data from Tektronix 2782/2784                                    *
//*                                                                          *
//****************************************************************************

void EXIT_TEK2780(S32 clip, //)
                  S64 carrier_Hz)
{
   if (out != NULL)
      {
      fclose(out);
      out = NULL;
      }

   //
   // Put clipped peak back onscreen
   //

   for (S32 cp=0; cp < clip; cp += 10)
      {
      GPIB_printf("RL UP;");
      }

   GPIB_printf("MKOFF ALL;");             // No marker
   GPIB_printf("CONTS;");                 // Return to continuous-sweep mode
   GPIB_printf("CR;");                    // Autoselect RBW
   GPIB_printf("CV;");                    // Autoselect VBW
   GPIB_printf("ROFFSET 0DM;");           // No reference offset
   GPIB_printf("DET NRM;");               // Normal detection
   GPIB_printf("CF %I64dHZ;",carrier_Hz); // Center at carrier frequency

   GPIB_disconnect();
}

BOOL ACQ_TEK2780(C8    *filename,
                 S32    GPIB_address,
                 C8    *caption,
                 S64    carrier,           
                 S64    ext_IF,
                 S64    ext_LO,
                 SINGLE VBW_factor,
                 DOUBLE ext_mult,          
                 SINGLE NF_characteristic,
                 SINGLE min_offset,        
                 SINGLE max_offset,
                 S32    clip,
                 SINGLE ref)
{
   S32    i;
   S32    min_decade = S32(log10(min_offset)+0.5);
   S32    max_decade = S32(log10(max_offset)+0.5) - 1;
   SINGLE ext_mult_dB = 0.0F;

   SAL_debug_printf("Minimum offset = %d Hz\n",S32(pow(10.0,min_decade)+0.5));
   SAL_debug_printf("Maximum offset = %d Hz\n",S32(pow(10.0,max_decade+1)+0.5));

   ext_mult_dB = -20.0F * (SINGLE) log10(ext_mult);

   SAL_debug_printf("External frequency multiplier = %.06lfx (%.02f dB)\n",
                     ext_mult,ext_mult_dB);

   //
   // Start up GPIB
   //
   // If binary transfers are enabled, the analyzer needs to assert EOI
   //

   GPIB_connect (GPIB_address,
                 GPIB_error,
                 0,
                 1000000);

   GPIB_set_EOS_mode(10);
   GPIB_set_serial_read_dropout(500);

   //
   // Open output file
   //

   out = fopen(filename,"wt");

   if (out == NULL)
      {
      GPIB_disconnect();

      SAL_alert_box("Error","Could not open %s\n",filename);
      return FALSE;
      }

   fprintf(out,";\n");
   fprintf(out,";Composite-noise plot file %s\n",filename);
   fprintf(out,";\n");
   fprintf(out,";Generated by PN.EXE V"VERSION" of "__DATE__"\n");
   fprintf(out,";by John Miles, KE5FX (john@miles.io)\n");
   fprintf(out,";\n\n");

   //
   // Log caption, if present
   //

   if (caption[0])
      {
      fprintf(out,"CAP %s\n",caption);
      }

   //
   // Log timestamp and instrument ID
   //

   static C8 ID[1024];
   static C8 REV[1024];

   strcpy(ID,GPIB_query("ID?"));
   strcpy(REV,GPIB_query("REV"));

   kill_trailing_whitespace(ID);
   kill_trailing_whitespace(REV);

   fprintf(out,"TIM %s\n",timestamp());
   fprintf(out,"IMO %s\n", ID);
   fprintf(out,"IRV %s\n\n", REV);

   //
   // Log decade statistics
   //

   fprintf(out,"DRG %d,%d\n", min_decade,max_decade);
   fprintf(out,"DCC 900\n");    // Decade point count=900 points
   fprintf(out,"OVC 1000\n\n"); // Overlap point count=1000 points into next decade

   fprintf(out,"EIF %I64d Hz\n",ext_IF);
   fprintf(out,"ELO %I64d Hz\n",ext_LO);
   fprintf(out,"MUL %lf\n\n",ext_mult);

   fprintf(out,"VBF %.02f\n",VBW_factor);
   fprintf(out,"CLP %d\n\n",clip);

   //
   // Set basic analyzer state needed for operation of program
   //

   GPIB_printf("SNGLS;");     // Select single-sweep mode
   GPIB_printf("MKOFF ALL;"); // No markers by default
   GPIB_printf("LG 10DB;");   // Need 10 dB/division log scale
   GPIB_printf("CR;");        // Autoselect RBW
   GPIB_printf("CV;");        // Autoselect VBW
   GPIB_printf("CT;");        // Autoselect sweep time
   GPIB_printf("O3;");        // ASCII queries

   //
   // If no carrier specified, find the strongest signal in the 
   // currently-selected view
   //
   // We round the result to the nearest minimum-decade multiple (x100 Hz for 
   // the 8566 and similar instruments)
   //

   if (carrier < 0)
      {
      SAL_debug_printf("Measuring carrier frequency... ");
      assert(ext_LO == -1);

      //
      // Zero in on signal, dividing span by 10 at each step until span width is <= 5 kHz
      //

      while (1)
         {
         GPIB_printf("TS;");        // Take sweep
         GPIB_printf("MKPK HI;");   // Put marker on highest signal
         GPIB_printf("MKCF;");      // Marker -> center frequency

         DOUBLE span = -10000.0;

         while (span == -10000.0)   
            {
            C8 *span_text = GPIB_query("SP?");
            sscanf(span_text,"%lf",&span); 
            }

         if (span <= 5000.0)
            {
            break;
            }

         span /= 10.0;              // Select next-narrower span and repeat measurement  
         GPIB_printf("SP %lfHZ;",span);  
         }

      GPIB_flush_receive_buffers();

      DOUBLE C = -10000.0;

      while (C == -10000.0)         // Query final marker frequency
         {
         C8 *CF_text = GPIB_query("MKF?");
         sscanf(CF_text,"%lf",&C); 
         }

      if (C < 0.0F)
         {
         C = -C;
         }

      GPIB_printf("MKOFF ALL;");    // Kill marker

      carrier = S64(C + 0.5);

      S64 decade_start = (S64) (pow(10.0, min_decade) + 0.5F);

      carrier = ((carrier + (decade_start / 2)) / decade_start) * decade_start;

      carrier = S64(((DOUBLE) carrier / ext_mult) + 0.5);  

      SAL_debug_printf("Result = %I64d Hz\n",carrier);
      }

   //
   // Perform actual measurement at external IF, if one is specified
   //

   S64 source_carrier = carrier;

   if (ext_IF >= 0)
      {
      carrier = ext_IF;
      }

   //
   // Log beginning of sweep series
   //

   S64 start = elapsed_uS();

   //
   // For each decade...
   //

   bool reference_level_set = FALSE;

   S64 mult_delta_Hz = S64(((DOUBLE) carrier * ext_mult) + 0.5) - carrier;  

   for (S32 d=min_decade; d <= max_decade; d++)
      {
      static SINGLE trace_A   [2][1001];
      static DOUBLE freq      [2][1001];
      SINGLE        correction[2];

      S32 decade_start = (S32) (pow(10.0, d) + 0.5F);
      S32 next_decade  = decade_start * 10;

      GPIB_printf("MKOFF ALL;");                // Kill marker

      //
      // If reference level not already determined, establish it
      // using default Rosenfell detector
      //

      if (!reference_level_set)
         {
         reference_level_set = TRUE;

         if (ref == 10000.0F)
            {
            //
            // Program a reasonable span/RBW combination for peak-level measurement, and 
            // declare a high reference level that is guaranteed to place the signal 
            // peak onscreen
            //

            GPIB_printf("SP 100KZ;");     
            GPIB_printf("RB 10KZ;");      
            GPIB_printf("VB 10KZ;");               

            GPIB_printf("AUNITS DBM;");               // Use dBm units
            GPIB_printf("DET POS;");                  // Positive-peak detection

            GPIB_printf("RL 30DM;");                  // +30 dBm reference level
            GPIB_printf("ROFFSET 0DM;");              // Clear reference offset

            //
            // Tune to specified center frequency and take sweep
            //

            GPIB_printf("CF %I64dHZ;",carrier + mult_delta_Hz);   // Set CF
            GPIB_printf("TS;");                                   // Take sweep
            GPIB_printf("MKPK HI;");                              // Put marker on highest signal
            GPIB_printf("MKRL;");                                 // Set reference level = marker amplitude

            //
            // Query reference level and use it as the offset 
            // for subsequent dBm/Hz measurements
            //

            while (ref == 10000.0F)
               {
               C8 *RL_text = GPIB_query("RL?");

               sscanf(RL_text,"%f",&ref); 
               }
            }

         GPIB_printf("ROFFSET %fDM;",-ref);        // Establish reference offset
         GPIB_printf("RL 0DM;");                   // Set relative reference level = 0 dBm

         //
         // Apply reference-level clipping if requested
         //

         for (S32 cp=0; cp < clip; cp += 10)
            {
            GPIB_printf("RL DN;");
            }
         }

      //
      // Enable sample detection for noise-like signals
      //

      GPIB_printf("DET SMP;");

      GPIB_printf("MKOFF ALL;");                // No marker needed

      //
      // Log RF attenuation used for this sweep
      //

      SINGLE att = 10000.0F;

      while (att == 10000.0F)
         {
         C8 *AT_text = GPIB_query("AT?");
         sscanf(AT_text,"%f",&att);
         }

      SAL_debug_printf("Carrier level = %.02f dBm, RF atten = %d dB\n",ref,S32(att + 0.5F));

      //
      // Log carrier amplitude used for this sweep
      //

      fprintf(out,"CAR %I64d Hz, %.02f dBm\n",
         source_carrier,
         ref);

      fprintf(out,"ATT %d dB\n",
         S32(att + 0.5F));

      S32 tune_chunks  = 0;
      S32 tune_span    = 0;
      S32 using_RBW    = 0;

      for (S32 half=0; half < 2; half++)
         {
         SAL_serve_message_queue();

         //
         // Take a reading at each half-decade, with the marker positioned
         // at the right edge of the display for automatic noise-characteristic
         // measurement (if enabled)
         //

         SAL_debug_printf("Analyzing decade 10^%d, sweep %d of 2...\n",d,half+1);

         if (key_hit)
            {
            EXIT_TEK2780(clip, carrier + mult_delta_Hz);
            key_hit = FALSE;

            SAL_alert_box("Error","Aborted by keypress");
            return FALSE;
            }

         SINGLE *trace_dest = trace_A[half];
         DOUBLE *freq_dest  = freq   [half];

         tune_span = next_decade;
         using_RBW = decade_start / 10;

         if (using_RBW == 1)
            {
            using_RBW = 3;    // Allow use of 3 Hz crystal filter in 10-100 Hz decade
            }

         GPIB_printf("SP %dHZ;",tune_span);                                // Set total span
         GPIB_printf("RB %dHZ;",using_RBW);                                // Set RBW
         GPIB_printf("VB %dHZ;",(S32) ((using_RBW * VBW_factor) + 0.5F));  // Set VBW

         SAL_debug_printf("Span %d Hz, RBW %d Hz\n",tune_span,using_RBW);

         S64 tune_freq = carrier + S64(tune_span / 2) + S64(next_decade * half);

         GPIB_printf("CF %I64dHZ;",tune_freq + mult_delta_Hz);    // Set center freq
         GPIB_printf("TS;");                                      // Take a sweep

         //
         // Fetch data from normal display trace
         //
         // Binary acquisition is faster than ASCII and appears more reliable 
         // (see note below), so it's enabled by default
         //

         DOUBLE df_dp = ((DOUBLE) tune_span) / 1000.0;
         DOUBLE F     = ((DOUBLE) tune_freq) - (500.0 * df_dp);

#if BINARY_ACQ
         GPIB_printf("O2;TROUT TRNOR");       // Request 16-bit binary display values
                                      
         static U16 curve[1001];     

         memset(curve, 0, 1001 * sizeof(S16));
         memcpy(curve, GPIB_read_BIN(1001 * sizeof(S16),TRUE,FALSE), 1001 * sizeof(S16));

         GPIB_read_ASC();   // swallow trailing CR/LF

         SINGLE offset      = ref - 100.0F;
         SINGLE scale       = 100.0F / 1000;
         SINGLE grat_offset = 0.0F;

         for (i=0; i < 1001; i++)
            {
            U16 scrn_Y = ((curve[i] & 0xff00) >> 8) + ((curve[i] & 0x00ff) << 8);

            *trace_dest++ = (((SINGLE(scrn_Y) + grat_offset) * scale) + offset) - (ref + (SINGLE) clip);
            *freq_dest++  = F;

            F += df_dp;
            }

         GPIB_printf("O3;");
#else
         GPIB_printf("O3;TROUT TRNOR;");      // Request ASCII dBm values

         SINGLE last_valid_point;

         for (i=0; i < 1001; i++)
            {
            SINGLE trace_point;

            do
               {
               C8 *read_result = GPIB_read_ASC(16, FALSE);

               if ((read_result == NULL) || (read_result[0] == 0))
                  {
                  EXIT_TEK2780(clip, carrier + mult_delta_Hz);

                  SAL_alert_box("Error","Sweep read terminated prematurely at column %d\n",i);
                  return FALSE;
                  }

               trace_point = 10000.0F; 
               sscanf(read_result,"%f",&trace_point);
               }
            while (trace_point == 10000.0F);

            if (i == 0)
               {
               last_valid_point = trace_point;
               }
            else
               {
               //
               // Try to work around 278x firmware bug that occasionally returns invalid 
               // ASCII amplitude points, by rejecting any new points that vary from the
               // last valid point by more than half its magnitude.  (This obviously fails
               // if the first point in the trace is invalid, and won't work well for 
               // spurs or legitimate trace values near 0 dBc...)
               //
               // Observed in 2784 FV2.11H,FPV1.9,DV2.10,NV2.8H with Prologix board (A. Rosati)
               //

               SINGLE mag = fabs(last_valid_point);
               SINGLE dt  = fabs(trace_point - last_valid_point);

               if ((dt < (mag/2.0F)) 
                    || 
                  ((half==0) && (i <= 100)))  // (Skip first division containing the steep USB carrier skirt)
                  {
                  last_valid_point = trace_point;
                  }
               else
                  {
                  trace_point = last_valid_point;
                  }
               }

            *trace_dest++ = trace_point;
            *freq_dest++  = F;

            F += df_dp;
            }
#endif

         //
         // Log the noise normalization factor for this RBW
         //

         SINGLE RB_factor = -10.0F * (SINGLE) log10((DOUBLE) using_RBW);

         correction[half] = RB_factor + NF_characteristic;
         }

      //
      // Display results
      //

      SAL_debug_printf("\n%8d Hz: %.02f dBc/Hz\n",
         decade_start,
         trace_A[0][100] + correction[0] + ext_mult_dB);

      SAL_debug_printf("%8d Hz: %.02f dBc/Hz / %.02f dBc/Hz\n\n",
         next_decade,
         trace_A[0][1000] + correction[0] + ext_mult_dB,
         trace_A[1][100]  + correction[1] + ext_mult_dB);

      //
      // Send decade and overlap data to output file
      //

      fprintf(out,"DEC %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[0]);

      for (i=100; i < 1000; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i - 100,
            freq[0][i],
            trace_A[0][i] + ext_mult_dB);
         }

      fprintf(out,"OVL %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[1]);

      for (i=0; i < 1000; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i,
            freq[1][i],
            trace_A[1][i] + ext_mult_dB);
         }
      }

   //
   // Write cumulative sweep time and EOF timestamp
   //

   S64 end = elapsed_uS();

   fprintf(out,"ELP %I64ds\n",((end - start) + 500000) / 1000000);
   fprintf(out,"EOF %s\n",timestamp());

   EXIT_TEK2780(clip, carrier + mult_delta_Hz);
   return TRUE;
}

//****************************************************************************
//*                                                                          *
//* Acquire data from HP 3585A/B spectrum analyzer                           *
//*                                                                          *
//****************************************************************************

void EXIT_HP3585(S32 clip, //)
                 S64 carrier_Hz)
{
   if (out != NULL)
      {
      fclose(out);
      out = NULL;
      }

   //
   // Put clipped peak back onscreen
   //

   for (S32 cp=0; cp < clip; cp += 10)
      {
      GPIB_printf("RL UP");
      GPIB_printf("RL UP");
      }

   GPIB_printf("CP 1");                      // Autoselect RBW
   GPIB_printf("CF %I64dHZ", carrier_Hz);    // Set CF
   GPIB_printf("TB0S1");                     // Hide trace 'B', continuous sweep
   GPIB_printf("RC4");                       // Re-enable autocal
   GPIB_printf("RC5");                       // Re-enable beeper

   GPIB_disconnect();
}

BOOL ACQ_HP3585 (C8    *filename,
                 S32    GPIB_address,
                 C8    *caption,
                 S64    carrier,           
                 S64    ext_IF,
                 S64    ext_LO,
                 SINGLE VBW_factor,
                 DOUBLE ext_mult,          
                 SINGLE NF_characteristic,
                 SINGLE min_offset,        
                 SINGLE max_offset,
                 S32    clip,
                 SINGLE ref = 10000.0F)
{
   if ((carrier < 0) || (ref == 10000.0F))
      {
      SAL_alert_box("Error","Must specify both carrier frequency and reference level");
      return FALSE;
      }

   S32    min_decade = S32(log10(min_offset)+0.5);
   S32    max_decade = S32(log10(max_offset)+0.5) - 1;
   SINGLE ext_mult_dB = 0.0F;

   SAL_debug_printf("Minimum offset = %d Hz\n",S32(pow(10.0,min_decade)+0.5));
   SAL_debug_printf("Maximum offset = %d Hz\n",S32(pow(10.0,max_decade+1)+0.5));

   ext_mult_dB = -20.0F * (SINGLE) log10(ext_mult);

   SAL_debug_printf("External frequency multiplier = %.06lfx (%.02f dB)\n",
                     ext_mult,ext_mult_dB);

   //
   // Start up GPIB
   //

   GPIB_connect (GPIB_address,
                 GPIB_error,
                 0,
                 1000000);

   GPIB_set_EOS_mode(10, FALSE);
   GPIB_set_serial_read_dropout(500);

   //
   // Open output file
   //

   out = fopen(filename,"wt");

   if (out == NULL)
      {
      GPIB_disconnect();

      SAL_alert_box("Error","Could not open %s",filename);
      return FALSE;
      }

   fprintf(out,";\n");
   fprintf(out,";Composite-noise plot file %s\n",filename);
   fprintf(out,";\n");
   fprintf(out,";Generated by PN.EXE V"VERSION" of "__DATE__"\n");
   fprintf(out,";by John Miles, KE5FX (john@miles.io)\n");
   fprintf(out,";\n\n");

   //
   // Log caption, if present
   //

   if (caption[0])
      {
      fprintf(out,"CAP %s\n",caption);
      }

   //
   // Log timestamp and instrument ID
   //

   fprintf(out,"TIM %s\n",timestamp());
   fprintf(out,"IMO HP3585A/B\n");
   fprintf(out,"IRV N/A\n\n");

   //
   // Record 1001 trace points for 3585
   //

   S32 record_n_points = 100 * (1001 / 100);
   S32 decile_n_points = record_n_points / 10;

   //
   // Log decade statistics
   //

   fprintf(out,"DRG %d,%d\n",min_decade,max_decade);
   fprintf(out,"DCC %d\n",record_n_points - decile_n_points);
   fprintf(out,"OVC %d\n\n",record_n_points);

   fprintf(out,"EIF %I64d Hz\n",ext_IF);
   fprintf(out,"ELO %I64d Hz\n",ext_LO);
   fprintf(out,"MUL %lf\n\n",ext_mult);

   fprintf(out,"VBF %.02f\n",VBW_factor);
   fprintf(out,"CLP %d\n\n",clip);

   //
   // Set basic analyzer state needed for operation of program
   //

   GPIB_printf("SV4");        // Disable auto-calibration (which prevents end-of-sweep bit from going high)  
   GPIB_printf("SV5");        // Suppress error beeps due to carrier clipping
   GPIB_printf("CN0NL0");     // Turn counter/noise markers off for faster sweeps
   GPIB_printf("DD 10DB");    // 10 dB/division
   GPIB_printf("S2");         // Single-sweep mode
   GPIB_printf("CP1BH0");     // Enable RBW/VBW/ST coupling, deactivate RBW hold

   //
   // Perform actual measurement at external IF, if one is specified
   //

   S64 source_carrier = carrier;

   if (ext_IF >= 0)
      {
      carrier = ext_IF;
      }

   //
   // Log beginning of sweep series
   //

   S64 start = elapsed_uS();

   //
   // For each decade...
   //

   bool reference_level_set = FALSE;

   S64 mult_delta_Hz = S64(((DOUBLE) carrier * ext_mult) + 0.5) - carrier;  

   for (S32 d=min_decade; d <= max_decade; d++)
      {
      static SINGLE trace_A   [2][1001];
      static DOUBLE freq      [2][1001];
      SINGLE        correction[2];

      S32 decade_start = (S32) (pow(10.0, d) + 0.5F);
      S32 next_decade  = decade_start * 10;
      S32 total_span   = decade_start * 10;

      //
      // Select RBW, VBW, and span
      //

      S32 RBW = decade_start / 10;

      if (RBW == 1)
         {
         RBW = 3;    // Allow use of 10-100 Hz decade with 3 Hz filter
         }

      if (RBW > 30000)
         {
         RBW = 30000;
         }

      S32 VBW = (S32) ((RBW * VBW_factor) + 0.5F);

      if (VBW == 0)
         {
         VBW = 1;
         }

      if (VBW > 30000)
         {
         VBW = 30000;
         }

      GPIB_printf("FS %dHZ",total_span);
      GPIB_printf("RB %dHZ",RBW);
      GPIB_printf("VB %dHZ",VBW);

      //
      // Set reference level, if it hasn't already been established
      //

      if (!reference_level_set)
         {
         reference_level_set = TRUE;

         GPIB_printf("RL %fDM",ref);

         //
         // Apply reference-level clipping if requested
         //
         // We will read the reference level prior to each sweep to make sure
         // that we got what we asked for, so it's OK if this loop runs out 
         // of RL range
         //

         for (S32 cp=0; cp < clip; cp += 10)
            {
            GPIB_printf("RL DN");
            GPIB_printf("RL DN");
            }
         }

      //
      // Perform "D7T4" query to obtain display annotation with immediate triggering
      // (EOS is set to 10 to separate the strings)
      //

      static C8 annotation[10][64];
      memset(annotation, 0, sizeof(annotation));

      GPIB_printf("D7T4");

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
            if (isspace(annotation[i][j]))
               {
               strcpy(&annotation[i][j], &annotation[i][j+1]);
               }
            }
         }

      //
      // Determine HP 3585 reference level (which may have changed due to autoranging)
      //

      SINGLE SOURCE_max_dBm = 10000.0F;
      sscanf(annotation[0], "REF%fDBM", &SOURCE_max_dBm);

      //
      // Log some statistics used for this sweep
      //

      SAL_debug_printf("Carrier level = %.02f dBm, RL = %.02f dBm\n",
         ref, SOURCE_max_dBm);

      fprintf(out,"CAR %I64d Hz, %.02f dBm\n",
         source_carrier,
         ref);

      fprintf(out,"ATT -1 dB\n");
      fprintf(out,"REF %f dBm\n", SOURCE_max_dBm);
      fprintf(out,"RNG %s\n",     annotation[3]);
      fprintf(out,"RBW %s\n",     annotation[7]);
      fprintf(out,"VBW %s\n",     annotation[8]);
      fprintf(out,"SWP %s\n",     annotation[9]);

      S64 tune_freq = carrier + (decade_start * 5);

      for (S32 half=0; half < 2; half++)
         {
         SAL_serve_message_queue();

         //
         // Take a reading at each half-decade
         //

         SAL_debug_printf("Analyzing decade 10^%d, sweep %d of 2...\n",d,half+1);

         if (key_hit)
            {
            key_hit = FALSE;
            EXIT_HP3585(clip, carrier + mult_delta_Hz);

            SAL_alert_box("Error","Aborted by keypress");
            return FALSE;
            }

         GPIB_printf("CF %I64dHZ", tune_freq + mult_delta_Hz);  // Set CF 

         GPIB_printf("TB0S2");      // Disable trace 'B' visibility from last sweep, and execute the next sweep

         for (;;)                   // Poll status word to detect end-of-sweep condition
            {
            if (interruptable_sleep(500)) 
               {
               EXIT_HP3585(clip, carrier + mult_delta_Hz);
               SAL_alert_box("Error","Aborted by keypress");
               return FALSE;
               }

            GPIB_printf("D6T4");

            U16 status = 0;
            memcpy(&status, GPIB_read_BIN(sizeof(status)), sizeof(status));

            if (!(status & 0x80))
               {
               break;
               }
            }

         GPIB_printf("SABO");       // Store trace A to trace B, then initiate binary dump of trace B 

         static U16 curve[1002];    // first 16-bit word ignored (all 1s) 

         memset(curve, 0, 1002 * sizeof(S16));
         memcpy(curve, GPIB_read_BIN(1002 * sizeof(S16),TRUE,FALSE), 1002 * sizeof(S16));

         //
         // Convert trace to floating-point format
         // Incoming data is first converted from screenspace to dBm; subtract specified carrier ref lvl 
         // to convert to dBc
         //
   
         DOUBLE df_dp = ((DOUBLE) total_span) / (DOUBLE) 1001;
         DOUBLE F     = ((DOUBLE) tune_freq) - ((DOUBLE) 1001 / 2.0 * df_dp);

         SINGLE range       = 100.0F;             // 100 dB display range
         SINGLE offset      = SOURCE_max_dBm - range;
         SINGLE scale       = range / 1000.0F;    // bottom=0, ref=1000, top=1023
         SINGLE grat_offset = 0.0F;

         for (i=0; i < 1001; i++)     
            {
            U32 swapped = ((curve[i+1] & 0xff00) >> 8) + ((curve[i+1] & 0x0003) << 8);

            SINGLE dBm = ((SINGLE(swapped) + grat_offset) * scale) + offset; 

            trace_A[half][i] = dBm - ref;
            freq   [half][i] = F;
            F += df_dp;
            }

         //
         // Log the noise normalization factor for this RBW
         //

         SINGLE RB_factor = -10.0F * (SINGLE) log10((DOUBLE) RBW);

         correction[half] = RB_factor + NF_characteristic;

         tune_freq += next_decade;
         }

      //
      // Display results
      //

      SAL_debug_printf("\n%8d Hz: %.02f dBc/Hz\n",
         decade_start,
         trace_A[0][decile_n_points] + correction[0] + ext_mult_dB);

      SAL_debug_printf("%8d Hz: %.02f dBc/Hz / %.02f dBc/Hz\n\n",
         next_decade,
         trace_A[0][record_n_points-1]  + correction[0] + ext_mult_dB,
         trace_A[1][decile_n_points  ]  + correction[1] + ext_mult_dB);

      //
      // Send decade and overlap data to output file
      //

      fprintf(out,"DEC %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[0]);

      for (i=decile_n_points; i < record_n_points; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i - decile_n_points,
            freq[0][i],
            trace_A[0][i] + ext_mult_dB);
         }

      fprintf(out,"OVL %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[1]);

      for (i=0; i < record_n_points; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i,
            freq[1][i],
            trace_A[1][i] + ext_mult_dB);
         }
      }

   //
   // Write cumulative sweep time and EOF timestamp
   //

   S64 end = elapsed_uS();

   fprintf(out,"ELP %I64ds\n",((end - start) + 500000) / 1000000);
   fprintf(out,"EOF %s\n",timestamp());

   EXIT_HP3585(clip, carrier + mult_delta_Hz);
   return TRUE;
}

//****************************************************************************
//*                                                                          *
//* Acquire data from HP 3588A/3589A spectrum analyzer                       *
//*                                                                          *
//****************************************************************************

void EXIT_HP358XA(S32 clip, //)
                  S64 carrier_Hz)
{
   if (out != NULL)
      {
      fclose(out);
      out = NULL;
      }

   //
   // Put clipped peak back onscreen
   //

   for (S32 cp=0; cp < clip; cp += 10)
      {
      GPIB_query(":DISP:Y:SCAL:MAX UP;*OPC?");
      }

   GPIB_query(":SENS:BAND:RES:AUTO ON;*OPC?");               // Autoselect RBW
   GPIB_query(":SENS:DET:FUNC POS;*OPC?");                   // Normal (positive-peak) detection
   GPIB_printf(":SENS:FREQ:CENT %I64d HZ;*OPC?",carrier_Hz); // Set CF
   GPIB_read_ASC();

   GPIB_disconnect();
}

BOOL ACQ_HP358XA(C8    *filename,
                 S32    GPIB_address,
                 C8    *caption,
                 S64    carrier,           
                 S64    ext_IF,
                 S64    ext_LO,
                 SINGLE VBW_factor,
                 DOUBLE ext_mult,          
                 SINGLE NF_characteristic,
                 SINGLE min_offset,        
                 SINGLE max_offset,
                 S32    clip,
                 SINGLE ref = 10000.0F)
{
   S32 i;

   if ((carrier < 0) || (ref == 10000.0F))
      {
      SAL_alert_box("Error","Must specify both carrier frequency and reference level");
      return FALSE;
      }

   S32    min_decade = S32(log10(min_offset)+0.5);
   S32    max_decade = S32(log10(max_offset)+0.5) - 1;
   SINGLE ext_mult_dB = 0.0F;

   SAL_debug_printf("Minimum offset = %d Hz\n",S32(pow(10.0,min_decade)+0.5));
   SAL_debug_printf("Maximum offset = %d Hz\n",S32(pow(10.0,max_decade+1)+0.5));

   ext_mult_dB = -20.0F * (SINGLE) log10(ext_mult);

   SAL_debug_printf("External frequency multiplier = %.06lfx (%.02f dB)\n",
                     ext_mult,ext_mult_dB);

   //
   // Start up GPIB
   //

   GPIB_connect (GPIB_address,
                 GPIB_error,
                 0,
                 1000000);

   GPIB_set_EOS_mode(10);
   GPIB_set_serial_read_dropout(500);

   //
   // Open output file
   //

   out = fopen(filename,"wt");

   if (out == NULL)
      {
      GPIB_disconnect();

      SAL_alert_box("Error","Could not open %s",filename);
      return FALSE;
      }

   fprintf(out,";\n");
   fprintf(out,";Composite-noise plot file %s\n",filename);
   fprintf(out,";\n");
   fprintf(out,";Generated by PN.EXE V"VERSION" of "__DATE__"\n");
   fprintf(out,";by John Miles, KE5FX (john@miles.io)\n");
   fprintf(out,";\n\n");

   //
   // Log caption, if present
   //

   if (caption[0])
      {
      fprintf(out,"CAP %s\n",caption);
      }

   //
   // Disallow SRQs, clear error register
   //

   GPIB_query("*SRE 0;*CLS;*OPC?");

   //
   // Log timestamp and instrument ID
   //

   static C8 ID[1024];
   strcpy(ID,GPIB_query("*IDN?"));
   kill_trailing_whitespace(ID);

   fprintf(out,"TIM %s\n",timestamp());
   fprintf(out,"IMO %s\n", ID);
   fprintf(out,"IRV N/A\n\n");

   //
   // Assume 401 trace points for 358xA
   //

   S32 record_n_points = 100 * (401 / 100);
   S32 decile_n_points = record_n_points / 10;

   //
   // Log decade statistics
   //

   fprintf(out,"DRG %d,%d\n",min_decade,max_decade);
   fprintf(out,"DCC %d\n",record_n_points - decile_n_points);
   fprintf(out,"OVC %d\n\n",record_n_points);

   fprintf(out,"EIF %I64d Hz\n",ext_IF);
   fprintf(out,"ELO %I64d Hz\n",ext_LO);
   fprintf(out,"MUL %lf\n\n",ext_mult);

   fprintf(out,"CLP %d\n\n",clip);

   //
   // Set basic analyzer state needed for operation of program
   //

   GPIB_query(":MARK1:STAT OFF;*OPC?");              // Turn off any marker-based functionality
   GPIB_query(":DISP:Y:SCAL:PDIV 10 DB;*OPC?");      // Need 10 dB/division log scale
   GPIB_query(":SENS:BAND:RES:AUTO ON;*OPC?");       // Autoselect RBW

   //
   // Request 32-bit binary IEEE754 trace format
   //

   GPIB_query(":CALC1:FORM MLOG;:FORM:DATA REAL,32;*OPC?");

   //
   // Perform actual measurement at external IF, if one is specified
   //

   S64 source_carrier = carrier;

   if (ext_IF >= 0)
      {
      carrier = ext_IF;
      }

   //
   // Log beginning of sweep series
   //

   S64 start = elapsed_uS();

   //
   // For each decade...
   //

   bool reference_level_set = FALSE;

   S64 mult_delta_Hz = S64(((DOUBLE) carrier * ext_mult) + 0.5) - carrier;  

   for (S32 d=min_decade; d <= max_decade; d++)
      {
      static SINGLE trace_A   [2][401];
      static DOUBLE freq      [2][401];
      SINGLE        correction[2];

      S32 decade_start = (S32) (pow(10.0, d) + 0.5F);
      S32 next_decade  = decade_start * 10;
      S32 total_span   = decade_start * 10;

      //
      // Select RBW and span
      //

      SINGLE RBW = (SINGLE) (decade_start / 10);

      if (RBW < 1.1F)
         {
         RBW = 1.1F;       // 3588A does not have a 1 Hz filter, and will generate an error
         }                 // if we try to request it

      GPIB_printf(":SENS:FREQ:SPAN %d HZ;*OPC?",total_span);    // Set total span
      GPIB_read_ASC();
      GPIB_printf(":SENS:BAND:RES %f HZ;*OPC?",RBW);            // Set RBW
      GPIB_read_ASC();

      //
      // Set reference level, if it hasn't already been established
      //

      if (!reference_level_set)
         {
         reference_level_set = TRUE;

         GPIB_printf(":DISP:Y:SCAL:MAX %f DBM;*OPC?",ref);      // Set reference level
         GPIB_read_ASC();

         //
         // Apply reference-level clipping if requested
         //

         for (S32 cp=0; cp < clip; cp += 10)
            {
            GPIB_query(":DISP:Y:SCAL:MAX DOWN;*OPC?");
            }
         }

      //
      // Enable video filtering
      //

      GPIB_query(":SENS:DET:FUNC SAMPLE;*OPC?");       // We use sampled detection for noise-like signals... 

      S64 tune_freq = carrier + (decade_start * 5);

      //
      // Log RF attenuation used for this sweep
      // (not used on 3588A/3589A)
      //

      SAL_debug_printf("Carrier level = %.02f dBm\n",ref);

      //
      // Log carrier amplitude used for this sweep
      //

      fprintf(out,"CAR %I64d Hz, %.02f dBm\n",
         source_carrier,
         ref);

      fprintf(out,"ATT -1 dB\n");

      for (S32 half=0; half < 2; half++)
         {
         SAL_serve_message_queue();

         //
         // Take a reading at each half-decade
         //

         SAL_debug_printf("Analyzing decade 10^%d, sweep %d of 2...\n",d,half+1);

         if (key_hit)
            {
            key_hit = FALSE;
            EXIT_HP358XA(clip, carrier + mult_delta_Hz);

            SAL_alert_box("Error","Aborted by keypress");
            return FALSE;
            }

         GPIB_printf(":SENS:FREQ:CENT %I64d HZ;*OPC?",tune_freq + mult_delta_Hz);  // Set CF
         GPIB_read_ASC();

         //
         // Abort current sweep in progress and trigger the next one, then wait
         // for it to complete and request a trace dump
         //

         GPIB_puts(":ABOR;:INIT:IMM;*WAI;:CALC1:DATA?");

         //
         // Read preamble, and verify that the data size
         // is what we expect
         //

         S8 *preamble = (S8 *) GPIB_read_BIN(2);
         assert(preamble[0] == '#');
         S32 n_preamble_bytes = preamble[1] - '0';

         C8 *size_text = GPIB_read_ASC(n_preamble_bytes);
         S32 received_points = atoi(size_text) / sizeof(SINGLE);

         if (received_points != 401)
            {
            SAL_alert_box("Fatal error", "Expected %d sweep points but received %d.\n\nPlease ensure that analyzer is configured for %d-point sweeps.",
               401, received_points, 401);

            exit(1);
            }

         //
         // Acquire 32-bit IEEE754 float data from display trace A
         //
         // Trace data arrives in big-endian format, so we'll store it in a 32-bit int and
         // byte-swap it
         //

         U32 curve[401];

         memset(curve, 
                0, 
                401 * sizeof(U32));

         memcpy(curve, 
                GPIB_read_BIN(401 * sizeof(U32),
                              TRUE,
                              FALSE), 
                401 * sizeof(U32));

         //
         // Swallow trailing CR/LF
         //

         GPIB_read_ASC();
   
         //
         // Convert trace to floating-point format (often much faster on host PC
         // than on target analyzer)
         //
         // Incoming data is dBm; subtract ref to convert to dBc
         //
   
         DOUBLE df_dp = ((DOUBLE) total_span) / (DOUBLE) 401;
         DOUBLE F     = ((DOUBLE) tune_freq) - ((DOUBLE) 401 / 2.0 * df_dp);

         for (i=0; i < 401; i++)     
            {
            U32 swapped = (((curve[i] & 0x000000ff) << 24) |
                           ((curve[i] & 0x0000ff00) << 8)  |
                           ((curve[i] & 0x00ff0000) >> 8)  |
                           ((curve[i] & 0xff000000) >> 24));

            trace_A[half][i] = (*(SINGLE *) &swapped) - ref;
            freq   [half][i] = F;
            F += df_dp;
            }

         //
         // Apply noise-equivalent bandwidth correction
         //

         SINGLE NFC = 10000.0F;

         while (NFC == 10000.0F)
            {
            C8 *AT_text = GPIB_query(":SENS:BAND:NOIS:CORR?");   
            sscanf(AT_text,"%f",&NFC);
            }

         correction[half] = -NFC + NF_characteristic;

         tune_freq += next_decade;
         }

      //
      // Display results
      //

      SAL_debug_printf("\n%8d Hz: %.02f dBc/Hz\n",
         decade_start,
         trace_A[0][decile_n_points] + correction[0] + ext_mult_dB);

      SAL_debug_printf("%8d Hz: %.02f dBc/Hz / %.02f dBc/Hz\n\n",
         next_decade,
         trace_A[0][record_n_points-1]  + correction[0] + ext_mult_dB,
         trace_A[1][decile_n_points  ]  + correction[1] + ext_mult_dB);

      //
      // Send decade and overlap data to output file
      //

      fprintf(out,"DEC %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[0]);

      for (i=decile_n_points; i < record_n_points; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i - decile_n_points,
            freq[0][i],
            trace_A[0][i] + ext_mult_dB);
         }

      fprintf(out,"OVL %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[1]);

      for (i=0; i < record_n_points; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i,
            freq[1][i],
            trace_A[1][i] + ext_mult_dB);
         }
      }

   //
   // Write cumulative sweep time and EOF timestamp
   //

   S64 end = elapsed_uS();

   fprintf(out,"ELP %I64ds\n",((end - start) + 500000) / 1000000);
   fprintf(out,"EOF %s\n",timestamp());

   EXIT_HP358XA(clip, carrier + mult_delta_Hz);
   return TRUE;
}

//****************************************************************************
//*                                                                          *
//* Acquire data from HP4396A/B in spectrum analyzer mode                    *
//*                                                                          *
//****************************************************************************

void EXIT_HP4396A(S32 clip, //)
                  S64 carrier_Hz)
{
   if (out != NULL)
      {
      fclose(out);
      out = NULL;
      }

   GPIB_query("FMT SPECT;*OPC?");
   GPIB_query("ATTAUTO ON;*OPC?");
   
   C8 *text = GPIB_query(":DISP:WIND:TRAC:Y:RLEV?");
   SINGLE val;
   sscanf(text, "%f", &val);
   val += clip;
   GPIB_printf(":DISP:WIND:TRAC:Y:RLEV %f;*OPC?", val);          // Set to nominal level
   GPIB_read_ASC();

   GPIB_query(":INIT:CONT 1;*OPC?");                             // Return to continuous-sweep mode 
   GPIB_query(":SENS:BAND:RES:AUTO ON;*OPC?");                   // Autoselect RBW
   text = GPIB_query("BW?");
   sscanf(text, "%f", &val);
   GPIB_printf(":SENS:BAND:VID %f;*OPC?", val);                  // Set VBW = RBW
   GPIB_read_ASC();
   GPIB_query(":SENS:DET:FUNC POS;*OPC?");                       // Normal detection
   GPIB_printf(":SENS:FREQ:CENT %I64d HZ;*OPC?",carrier_Hz);     // Set CF
   GPIB_read_ASC();

   GPIB_disconnect();
}

BOOL ACQ_HP4396A(C8    *filename,
                 S32    GPIB_address,
                 C8    *caption,
                 S64    carrier,           
                 S64    ext_IF,
                 S64    ext_LO,
                 SINGLE VBW_factor,
                 DOUBLE ext_mult,          
                 SINGLE NF_characteristic,
                 SINGLE min_offset,        
                 SINGLE max_offset,
                 S32    clip,
                 SINGLE ref = 10000.0F)
{
   S32 i;

   if ((carrier < 0) || (ref == 10000.0F))
      {
      SAL_alert_box("Error","Must specify both carrier frequency and reference level");
      return FALSE;
      }

   S32    min_decade = S32(log10(min_offset)+0.5);
   S32    max_decade = S32(log10(max_offset)+0.5) - 1;
   SINGLE ext_mult_dB = 0.0F;

   SAL_debug_printf("Minimum offset = %d Hz\n",S32(pow(10.0,min_decade)+0.5));
   SAL_debug_printf("Maximum offset = %d Hz\n",S32(pow(10.0,max_decade+1)+0.5));

   ext_mult_dB = -20.0F * (SINGLE) log10(ext_mult);

   SAL_debug_printf("External frequency multiplier = %.06lfx (%.02f dB)\n",
                     ext_mult,ext_mult_dB);

   //
   // Start up GPIB
   //

   GPIB_connect (GPIB_address,
                 GPIB_error,
                 0,
                 1000000);

   GPIB_set_EOS_mode(10);
   GPIB_set_serial_read_dropout(500);

   //
   // Open output file
   //

   out = fopen(filename,"wt");

   if (out == NULL)
      {
      GPIB_disconnect();

      SAL_alert_box("Error","Could not open %s",filename);
      return FALSE;
      }

   fprintf(out,";\n");
   fprintf(out,";Composite-noise plot file %s\n",filename);
   fprintf(out,";\n");
   fprintf(out,";Generated by PN.EXE V"VERSION" of "__DATE__"\n");
   fprintf(out,";by John Miles, KE5FX (john@miles.io)\n");
   fprintf(out,";\n\n");

   //
   // Log caption, if present
   //

   if (caption[0])
      {
      fprintf(out,"CAP %s\n",caption);
      }

   //
   // Disallow SRQs, clear error register
   //

   GPIB_query("*SRE 0;*CLS;*OPC?");

   //
   // Log timestamp and instrument ID
   //

   static C8 ID[1024];
   strcpy(ID,GPIB_query("*IDN?"));
   kill_trailing_whitespace(ID);

   fprintf(out,"TIM %s\n",timestamp());
   fprintf(out,"IMO %s\n", ID);
   fprintf(out,"IRV N/A\n\n");

   //
   // Get # of trace points (rounding down to nearest 100 for logging
   // purposes)
   //

   const S32 HP4396A_N_POINTS = 801;

   S32 record_n_points = 100 * (HP4396A_N_POINTS / 100);
   S32 decile_n_points = record_n_points / 10;

   //
   // Log decade statistics
   //

   fprintf(out,"DRG %d,%d\n",min_decade,max_decade);
   fprintf(out,"DCC %d\n",record_n_points - decile_n_points);
   fprintf(out,"OVC %d\n\n",record_n_points);

   fprintf(out,"EIF %I64d Hz\n",ext_IF);
   fprintf(out,"ELO %I64d Hz\n",ext_LO);
   fprintf(out,"MUL %lf\n\n",ext_mult);

   fprintf(out,"VBF %.02f\n",VBW_factor);
   fprintf(out,"CLP %d\n\n",clip);

   //
   // Set basic analyzer state needed for operation of program
   //

   GPIB_query(":INIT:CONT 0;:ABOR;*OPC?");                   // Select single-sweep mode, abort sweep in progress
   GPIB_query("ATTAUTO ON;*OPC?");                           // Set the attenuation
   GPIB_printf(":DISP:WIND:TRAC:Y:RLEV %f;*OPC?", ref-clip); // Must be done before switching to NOISE format
   GPIB_read_ASC();
   
   //
   // Read out the attenuation
   //

   SINGLE att = 10000.0F;

   while (att == 10000.0F)
      {
      C8 *AT_text = GPIB_query(":SENS:POW:AC:ATT?");
      sscanf(AT_text,"%f",&att);
      }

   //
   // Fix attenuation value to prevent HP4396A from erroneously changing it
   // in the NOISE mode!
   //

   GPIB_printf("ATT %f;*OPC?", att); GPIB_read_ASC();
   GPIB_query("FMT NOISE;*OPC?");
   GPIB_query(":DISP:WIND:TRAC:Y:PDIV 10 DB;*OPC?"); // Need 10 dB/division log scale
   GPIB_query(":SENS:AVER:STAT OFF; *OPC?");         // Turn off the averaging
   GPIB_query(":SENS:BAND:RES:AUTO ON;*OPC?");       // Autoselect RBW

   //
   // Set video bandwidth to the requested ratio. Through the sweeps the analyzer
   // adjusts RBW and VBW automatically.
   //

   SINGLE RBW, VBW;
   C8 *rdbk = GPIB_query("BW?");
   sscanf(rdbk, "%f", &RBW);
   VBW = RBW * VBW_factor;
   GPIB_printf("VBW %f Hz;*OPC?", VBW);
   GPIB_read_ASC();

   GPIB_query(":SENS:SWEEP:TIME:AUTO ON;*OPC?");     // Autoselect sweep time

   //
   // Request 32-bit floating point trace format
   //

   GPIB_query(":FORM:DATA REAL,32;*OPC?");

   //
   // Perform actual measurement at external IF, if one is specified
   //

   S64 source_carrier = carrier;

   if (ext_IF >= 0)
      {
      carrier = ext_IF;
      }

   //
   // Log beginning of sweep series
   //

   S64 start = elapsed_uS();

   //
   // For each decade...
   //

   bool reference_level_set = FALSE;

   S64 mult_delta_Hz = S64(((DOUBLE) carrier * ext_mult) + 0.5) - carrier;  

   for (S32 d=min_decade; d <= max_decade; d++)
      {
      static SINGLE trace_A   [2][MAX_SCPI_POINTS];
      static DOUBLE freq      [2][MAX_SCPI_POINTS];
      SINGLE        correction[2];

      S32 decade_start = (S32) (pow(10.0, d) + 0.5F);
      S32 next_decade  = decade_start * 10;
      S32 total_span   = decade_start * 10;

      //
      // Select RBW and span
      //

      GPIB_printf(":SENS:FREQ:SPAN %d HZ;*OPC?",total_span);  // Set total span
      GPIB_read_ASC();

      //
      // Update the resolution bandwidth
      //

      rdbk = GPIB_query("BW?");
      sscanf(rdbk, "%f", &RBW);

      //
      // Update the reference level to keep the peak at the top of the screen
      //

      GPIB_printf(":DISP:WIND:TRAC:Y:RLEV %f;*OPC?", ref-clip-10.0*log10(RBW));
      GPIB_read_ASC();

      GPIB_query(":SENS:DET:FUNC SAMPLE;*OPC?");        // We use sampled detection for noise-like signals...

      S64 tune_freq = carrier + (decade_start * 5);

      SAL_debug_printf("Carrier level = %.02f dBm, RF atten = %d dB\n",ref,S32(att + 0.5F));

      //
      // Log carrier amplitude used for this sweep
      //

      fprintf(out,"CAR %I64d Hz, %.02f dBm\n",
         source_carrier,
         ref);

      fprintf(out,"ATT %d dB\n",
         S32(att + 0.5F));

      for (S32 half=0; half < 2; half++)
         {
         SAL_serve_message_queue();

         //
         // Take a reading at each half-decade
         //

         SAL_debug_printf("Analyzing decade 10^%d, sweep %d of 2...\n",d,half+1);

         if (key_hit)
            {
            key_hit = FALSE;
            EXIT_HP4396A(clip, carrier + mult_delta_Hz);

            SAL_alert_box("Error","Aborted by keypress");
            return FALSE;
            }

         GPIB_printf(":SENS:FREQ:CENT %I64d HZ;*OPC?",tune_freq + mult_delta_Hz);  // Set CF
         GPIB_read_ASC();

         //
         // Request trace dump
         // Read the status register first just in case
         //

         C8 *response = GPIB_query("ESB?");
       
         GPIB_query(":INIT:IMM;*OPC?");
       
         //
         // Poll status register, waiting for the sweep to complete
         //

         S32 mask = 0;

         do
            {
            response = GPIB_query("ESB?");
            sscanf(response, "%d", &mask); mask &= 1;

            if (mask)
               break;
            else
               Sleep(500);
            } 
         while (!mask);

         //
         // Data is ready - request it
         //

         GPIB_puts(":TRAC:DATA? DTR");

         //
         // Read preamble, and verify that the data size
         // is what we expect
         //
   
         S8 *preamble = (S8 *) GPIB_read_BIN(2);
         assert(preamble[0] == '#');
         S32 n_preamble_bytes = preamble[1] - '0';
   
         C8 *size_text = GPIB_read_ASC(n_preamble_bytes);

         if (atoi(size_text) != HP4396A_N_POINTS * sizeof(S32))   
            {
            SAL_alert_box("Error","Ensure HP 4396A/B is in spectrum analyzer mode with %d-point trace width selected", HP4396A_N_POINTS);
            exit(1);
            }
   
         //
         // Acquire 32-bit floating-point data from display trace A
         // 
   
         static S32 curve[MAX_SCPI_POINTS];

         memset(curve, 
                0, 
                HP4396A_N_POINTS * sizeof(S32));
   
         memcpy(curve, 
                GPIB_read_BIN(HP4396A_N_POINTS * sizeof(SINGLE),
                              TRUE,
                              FALSE), 
                HP4396A_N_POINTS * sizeof(S32));
   
         //
         // Swallow trailing newline
         //
   
         GPIB_read_ASC();
   
         //
         // Get the byte order right
         //
   
         DOUBLE df_dp = ((DOUBLE) total_span) / (DOUBLE) HP4396A_N_POINTS;
         DOUBLE F     = ((DOUBLE) tune_freq) - ((DOUBLE) HP4396A_N_POINTS / 2.0 * df_dp);

         for (i=0; i < HP4396A_N_POINTS; i++)     
            {
            U32 swapped = (((curve[i] & 0x000000ff) << 24) |
                           ((curve[i] & 0x0000ff00) << 8)  |
                           ((curve[i] & 0x00ff0000) >> 8)  |
                           ((curve[i] & 0xff000000) >> 24));

            trace_A[half][i] = (*(SINGLE *) &swapped) - ref;
            freq   [half][i] = F;
            F += df_dp;
            }
         
         //
         // Apply noise-normalization factor
         //
         // Since we set the display mode to dBm/Hz no internal normalization is needed
         //

         correction[half] = NF_characteristic;

         tune_freq += next_decade;
         }

      //
      // Display results
      //

      SAL_debug_printf("\n%8d Hz: %.02f dBc/Hz\n",
         decade_start,
         trace_A[0][decile_n_points] + correction[0] + ext_mult_dB);

      SAL_debug_printf("%8d Hz: %.02f dBc/Hz / %.02f dBc/Hz\n\n",
         next_decade,
         trace_A[0][record_n_points-1]  + correction[0] + ext_mult_dB,
         trace_A[1][decile_n_points  ]  + correction[1] + ext_mult_dB);

      //
      // Send decade and overlap data to output file
      //

      fprintf(out,"DEC %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[0]);

      for (i=decile_n_points; i < record_n_points; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i - decile_n_points,
            freq[0][i],
            trace_A[0][i] + ext_mult_dB);
         }

      fprintf(out,"OVL %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[1]);

      for (i=0; i < record_n_points; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i,
            freq[1][i],
            trace_A[1][i] + ext_mult_dB);
         }
      }

   //
   // Write cumulative sweep time and EOF timestamp
   //

   S64 end = elapsed_uS();

   fprintf(out,"ELP %I64ds\n",((end - start) + 500000) / 1000000);
   fprintf(out,"EOF %s\n",timestamp());

   EXIT_HP4396A(clip, carrier + mult_delta_Hz);
   return TRUE;
}

//****************************************************************************
//*                                                                          *
//* Acquire data from R3267-family spectrum analyzer                         *
//* Tested with R3267                                                        *
//*                                                                          *
//****************************************************************************

void EXIT_R3267(S32 clip, //)
                S64 carrier_Hz)
{
   if (out != NULL)
      {
      fclose(out);
      out = NULL;
      }

   //
   // Put clipped peak back onscreen
   //

   for (S32 cp=0; cp < clip; cp += 10)
      {
      GPIB_printf("RL UP");
      }

   GPIB_printf("DL0");                                        // Restore default <CR><LF><EOI> response delimiter
   GPIB_printf("SN");                                         // Continuous sweep
   GPIB_printf("BA");                                         // Autoselect RBW
   GPIB_printf("VA");                                         // Autoselect VBW
   GPIB_printf("DET NRM");                                    // Normal (Rosenfell) detection
   GPIB_printf("CF %I64dHZ",carrier_Hz);                      // Set CF

   GPIB_disconnect();
}

BOOL ACQ_R3267(C8    *filename,
               S32    GPIB_address,
               C8    *caption,
               S64    carrier,           
               S64    ext_IF,
               S64    ext_LO,
               SINGLE VBW_factor,
               DOUBLE ext_mult,          
               SINGLE NF_characteristic,
               SINGLE min_offset,        
               SINGLE max_offset,
               S32    requested_clip,
               SINGLE ref = 10000.0F)
{            
   S32 i;

   if ((carrier < 0) || (ref == 10000.0F))
      {
      SAL_alert_box("Error","Must specify both carrier frequency and reference level");
      return FALSE;
      }

   //
   // Start up GPIB
   //

   GPIB_connect (GPIB_address,
                 GPIB_error,
                 0,
                 1000000);

   GPIB_set_EOS_mode(10);
   GPIB_set_serial_read_dropout(500);

   //
   // Open output file
   //

   out = fopen(filename,"wt");

   if (out == NULL)
      {
      GPIB_disconnect();

      SAL_alert_box("Error","Could not open %s",filename);
      return FALSE;
      }

   fprintf(out,";\n");
   fprintf(out,";Composite-noise plot file %s\n",filename);
   fprintf(out,";\n");
   fprintf(out,";Generated by PN.EXE V"VERSION" of "__DATE__"\n");
   fprintf(out,";by John Miles, KE5FX (john@miles.io)\n");
   fprintf(out,";\n\n");

   //
   // Log caption, if present
   //

   if (caption[0])
      {
      fprintf(out,"CAP %s\n",caption);
      }

   //
   // Disallow SRQs, clear error register
   // Query results terminated by CR+LF+EOI
   // 

   GPIB_printf("*SRE 0;*CLS;DL0");
   GPIB_flush_receive_buffers();

   //
   // Log timestamp and instrument ID
   //

   static C8 ID[1024];
   static C8 REV[1024];

   strcpy(ID,GPIB_query("*IDN?"));
   strcpy(REV,GPIB_query("REV?"));

   kill_trailing_whitespace(ID);
   kill_trailing_whitespace(REV);

   fprintf(out,"TIM %s\n",timestamp());
   fprintf(out,"IMO %s\n", ID);
   fprintf(out,"IRV %s\n\n", REV);

   //
   // Get # of trace points (rounding down to nearest 100 for logging
   // purposes)
   //

   S32 precision = -1;

   while (precision == -1)
      {
      C8 *text = GPIB_query("TP?");
      sscanf(text,"%d",&precision);
      }

   S32 R3267_n_points = 1001;

   if (precision == 0) 
      {
      R3267_n_points = 501;
      }

   S32 record_n_points = 100 * (R3267_n_points / 100);
   S32 decile_n_points = record_n_points / 10;

   //
   // See if digital RBW filter is enabled
   //
   // If so, we can't apply carrier clipping in decades < 10-100 kHz -- the analyzer will
   // reduce the amplitude at the ADC input without telling us!
   //

   S32 DRBW_enabled = -1;

   while (DRBW_enabled == -1)
      {
      C8 *text = GPIB_query("DRBW?");
      sscanf(text,"%d",&DRBW_enabled);
      }

   S32 clip = DRBW_enabled ? 0 : requested_clip;

   if ((min_offset < 100) && (!DRBW_enabled))
      {
      min_offset = 100;    // (1 Hz RBW is available only with digital filter)
      }

   //
   // Log decade statistics
   //

   S32    min_decade = S32(log10(min_offset)+0.5);
   S32    max_decade = S32(log10(max_offset)+0.5) - 1;
   SINGLE ext_mult_dB = 0.0F;

   SAL_debug_printf("Minimum offset = %d Hz\n",S32(pow(10.0,min_decade)+0.5));
   SAL_debug_printf("Maximum offset = %d Hz\n",S32(pow(10.0,max_decade+1)+0.5));

   ext_mult_dB = -20.0F * (SINGLE) log10(ext_mult);

   SAL_debug_printf("External frequency multiplier = %.06lfx (%.02f dB)\n",
                     ext_mult,ext_mult_dB);

   fprintf(out,"DRG %d,%d\n",min_decade,max_decade);
   fprintf(out,"DCC %d\n",record_n_points - decile_n_points);
   fprintf(out,"OVC %d\n\n",record_n_points);

   fprintf(out,"EIF %I64d Hz\n",ext_IF);
   fprintf(out,"ELO %I64d Hz\n",ext_LO);
   fprintf(out,"MUL %lf\n\n",ext_mult);

   fprintf(out,"VBF %.02f\n",VBW_factor);
   fprintf(out,"CLP %d\n\n",requested_clip);

   //
   // Set basic analyzer state needed for operation of program
   //

   GPIB_printf("SI");                                 // Single-sweep mode
   GPIB_printf("MKOFF");                              // Turn off any marker-based functionality
   GPIB_printf("AUNITS DBM;DD 10DB");                 // Need 10 dB/division log scale
   GPIB_printf("BA");                                 // Autoselect RBW
   GPIB_printf("VA");                                 // Autoselect VBW
   GPIB_printf("AS");                                 // Autoselect sweep time

   //
   // Perform actual measurement at external IF, if one is specified
   //

   S64 source_carrier = carrier;

   if (ext_IF >= 0)
      {
      carrier = ext_IF;
      }

   //
   // Log beginning of sweep series
   //

   S64 start = elapsed_uS();

   //
   // For each decade...
   //

   bool reference_level_set = FALSE;

   S64 mult_delta_Hz = S64(((DOUBLE) carrier * ext_mult) + 0.5) - carrier;  

   for (S32 d=min_decade; d <= max_decade; d++)
      {
      static SINGLE trace_A   [2][1001];
      static DOUBLE freq      [2][1001];
      SINGLE        correction[2];

      S32 decade_start = (S32) (pow(10.0, d) + 0.5F);
      S32 next_decade  = decade_start * 10;
      S32 total_span   = decade_start * 10;

      //
      // Select RBW and span width
      //
      // To avoid error messages, we need to set a wide RBW before we 
      // increase the span width from the previous decade.  (If we set the desired RBW
      // first, the analyzer would complain about the initial span being too wide,
      // and if we set the span width first, the analyer would complain about 
      // the RBW from the previous decade being too narrow)
      //

      S32 RBW = decade_start / 10;
      S32 VBW = (S32) ((RBW * VBW_factor) + 0.5F);

      S32 using_digital_filter = DRBW_enabled && (RBW <= 100);

      GPIB_printf("RB 1000000HZ");
      GPIB_printf("SP %dHZ",total_span); 
      GPIB_printf("RB %dHZ",RBW);        

      //
      // Set reference level, if it hasn't already been established, and
      // apply reference-level clipping as soon as we can
      //

      if ((RBW > 100) && (clip != requested_clip))
         {
         reference_level_set = FALSE;
         }

      if (!reference_level_set)
         {
         reference_level_set = TRUE;
         GPIB_printf("RL %fDB",ref);       
   
         if (!using_digital_filter)
            {
            clip = requested_clip;

            for (S32 cp=0; cp < clip; cp += 10)
               {
               GPIB_printf("RL DN");
               }
            }
         }

      //
      // Query actual reference level being used for this sweep
      //

      GPIB_flush_receive_buffers();

      SINGLE SOURCE_max_dBm = 10000.0F;

      while (SOURCE_max_dBm == 10000.0F)
         {
         C8 *RL_text = GPIB_query("RL?");
         sscanf(RL_text,"%f",&SOURCE_max_dBm);
         }

      //
      // Enable video filtering, and select sample detection
      // for noise-like signals
      //
      // Both of these features are inaccessible when digital RBW filtering 
      // is in use
      //

      if (!using_digital_filter)
         {
         GPIB_printf("DET SMP");   
         GPIB_printf("VB %dHZ",VBW);
         }

      S64 tune_freq = carrier + (decade_start * 5);

      //
      // Log RF attenuation and carrier amplitude used for this sweep
      //

      GPIB_flush_receive_buffers();

      SINGLE att = 10000.0F;

      while (att == 10000.0F)
         {
         C8 *AT_text = GPIB_query("AT?");
         sscanf(AT_text,"%f",&att);
         }

      SAL_debug_printf("Carrier level = %.02f dBm, RF atten = %d dB\n",ref,S32(att + 0.5F));

      fprintf(out,"CAR %I64d Hz, %.02f dBm\n",
         source_carrier,
         ref);

      fprintf(out,"ATT %d dB\n",
         S32(att + 0.5F));

      for (S32 half=0; half < 2; half++)
         {
         SAL_serve_message_queue();

         //
         // Take a reading at each half-decade
         //

         SAL_debug_printf("Analyzing decade 10^%d, sweep %d of 2...\n",d,half+1);

         if (key_hit)
            {
            key_hit = FALSE;
            EXIT_R3267(clip, carrier + mult_delta_Hz);

            SAL_alert_box("Error","Aborted by keypress");
            return FALSE;
            }

         //
         // Trigger sweep at specified center frequency, and request binary trace dump 
         // (Use EOI termination for binary dump, not CR/LF)
         //

         GPIB_printf("CF %I64dHZ",tune_freq + mult_delta_Hz);

         GPIB_printf("DL2");
         GPIB_flush_receive_buffers();

         GPIB_printf("TS;TBA?");

         static U16 curve[1001];     

         memset(curve, 0, R3267_n_points * sizeof(S16));
         memcpy(curve, GPIB_read_BIN(R3267_n_points * sizeof(S16),TRUE,FALSE), R3267_n_points * sizeof(S16));

         GPIB_printf("DL0");
         GPIB_flush_receive_buffers();

         SINGLE range       = 100.0F;             // 100 dB display range
         SINGLE offset      = SOURCE_max_dBm - range;
         SINGLE scale       = range / 12800.0F;   // bottom=1792, ref=14592, top=14592+ 
         SINGLE grat_offset = -1792.0F;

         DOUBLE df_dp = ((DOUBLE) total_span) / (DOUBLE) R3267_n_points;
         DOUBLE F     = ((DOUBLE) tune_freq) - ((DOUBLE) R3267_n_points / 2.0 * df_dp);

         for (i=0; i < R3267_n_points; i++)     
            {
            curve[i] = ((curve[i] & 0xff00) >> 8) + ((curve[i] & 0x00ff) << 8);

            SINGLE dBm = ((SINGLE(curve[i]) + grat_offset) * scale) + offset; 

            trace_A[half][i] = dBm - ref;
            freq   [half][i] = F;

            F += df_dp;
            }

         //
         // Apply noise-normalization factor
         //
         // This is typically 2 dB greater than the baseline
         // 10*log(RBW) normalization factor due to filter
         // and detector noise-response characteristics
         // 

         SINGLE RB_factor = -10.0F * (SINGLE) log10((DOUBLE) RBW);

         correction[half] = RB_factor + NF_characteristic;

         tune_freq += next_decade;
         }

      //
      // Display results
      //

      SAL_debug_printf("\n%8d Hz: %.02f dBc/Hz\n",
         decade_start,
         trace_A[0][decile_n_points] + correction[0] + ext_mult_dB);

      SAL_debug_printf("%8d Hz: %.02f dBc/Hz / %.02f dBc/Hz\n\n",
         next_decade,
         trace_A[0][record_n_points-1]  + correction[0] + ext_mult_dB,
         trace_A[1][decile_n_points  ]  + correction[1] + ext_mult_dB);

      //
      // Send decade and overlap data to output file
      //

      fprintf(out,"DEC %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[0]);

      for (i=decile_n_points; i < record_n_points; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i - decile_n_points,
            freq[0][i],
            trace_A[0][i] + ext_mult_dB);
         }

      fprintf(out,"OVL %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[1]);

      for (i=0; i < record_n_points; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i,
            freq[1][i],
            trace_A[1][i] + ext_mult_dB);
         }
      }

   //
   // Write cumulative sweep time and EOF timestamp
   //

   S64 end = elapsed_uS();

   fprintf(out,"ELP %I64ds\n",((end - start) + 500000) / 1000000);
   fprintf(out,"EOF %s\n",timestamp());

   EXIT_R3267(clip, carrier + mult_delta_Hz);
   return TRUE;
}

//****************************************************************************
//*                                                                          *
//* Acquire data from R3465-family spectrum analyzer                         *
//* Tested with R3263                                                        *
//*                                                                          *
//****************************************************************************

void EXIT_R3465(S32 clip, //)
                S64 carrier_Hz)
{
   if (out != NULL)
      {
      fclose(out);
      out = NULL;
      }

   //
   // Put clipped peak back onscreen
   //

   for (S32 cp=0; cp < clip; cp += 10)
      {
      GPIB_printf("RL UP");
      }

   GPIB_printf("DL0");                                        // Restore default <CR><LF><EOI> response delimiter
   GPIB_printf("SN");                                         // Continuous sweep
   GPIB_printf("BA");                                         // Autoselect RBW
   GPIB_printf("VA");                                         // Autoselect VBW
   GPIB_printf("DET NRM");                                    // Normal (Rosenfell) detection
   GPIB_printf("CF %I64dHZ",carrier_Hz);                      // Set CF

   GPIB_disconnect();
}

BOOL ACQ_R3465(C8    *filename,
               S32    GPIB_address,
               C8    *caption,
               S64    carrier,           
               S64    ext_IF,
               S64    ext_LO,
               SINGLE VBW_factor,
               DOUBLE ext_mult,          
               SINGLE NF_characteristic,
               SINGLE min_offset,        
               SINGLE max_offset,
               S32    clip,
               SINGLE ref = 10000.0F)
{            
   S32 i;

   if ((carrier < 0) || (ref == 10000.0F))
      {
      SAL_alert_box("Error","Must specify both carrier frequency and reference level");
      return FALSE;
      }

   //
   // Start up GPIB
   //

   GPIB_connect (GPIB_address,
                 GPIB_error,
                 0,
                 1000000);

   GPIB_set_EOS_mode(10);
   GPIB_set_serial_read_dropout(500);

   //
   // Open output file
   //

   out = fopen(filename,"wt");

   if (out == NULL)
      {
      GPIB_disconnect();

      SAL_alert_box("Error","Could not open %s",filename);
      return FALSE;
      }

   fprintf(out,";\n");
   fprintf(out,";Composite-noise plot file %s\n",filename);
   fprintf(out,";\n");
   fprintf(out,";Generated by PN.EXE V"VERSION" of "__DATE__"\n");
   fprintf(out,";by John Miles, KE5FX (john@miles.io)\n");
   fprintf(out,";\n\n");

   //
   // Log caption, if present
   //

   if (caption[0])
      {
      fprintf(out,"CAP %s\n",caption);
      }

   //
   // Disallow SRQs, clear error register
   // Query results terminated by CR+LF+EOI
   // 

   GPIB_printf("*SRE 0;*CLS;DL0");
   GPIB_flush_receive_buffers();    // if we don't do this, the queries below return one string behind

   //
   // Log timestamp and instrument ID
   //

   static C8 ID[1024];
   static C8 REV[1024];

   strcpy(ID,GPIB_query("*IDN?"));
   strcpy(REV,GPIB_query("REV?"));

   kill_trailing_whitespace(ID);
   kill_trailing_whitespace(REV);

   fprintf(out,"TIM %s\n",timestamp());
   fprintf(out,"IMO %s\n", ID);
   fprintf(out,"IRV %s\n\n", REV);

   //
   // Use 1001 trace points on R3465 family
   //

   GPIB_printf("TPL");
   S32 R3465_n_points = 1001;

   S32 record_n_points = 100 * (R3465_n_points / 100);
   S32 decile_n_points = record_n_points / 10;

   //
   // Log decade statistics
   //

   S32    min_decade = S32(log10(min_offset)+0.5);
   S32    max_decade = S32(log10(max_offset)+0.5) - 1;
   SINGLE ext_mult_dB = 0.0F;

   SAL_debug_printf("Minimum offset = %d Hz\n",S32(pow(10.0,min_decade)+0.5));
   SAL_debug_printf("Maximum offset = %d Hz\n",S32(pow(10.0,max_decade+1)+0.5));

   ext_mult_dB = -20.0F * (SINGLE) log10(ext_mult);

   SAL_debug_printf("External frequency multiplier = %.06lfx (%.02f dB)\n",
                     ext_mult,ext_mult_dB);

   fprintf(out,"DRG %d,%d\n",min_decade,max_decade);
   fprintf(out,"DCC %d\n",record_n_points - decile_n_points);
   fprintf(out,"OVC %d\n\n",record_n_points);

   fprintf(out,"EIF %I64d Hz\n",ext_IF);
   fprintf(out,"ELO %I64d Hz\n",ext_LO);
   fprintf(out,"MUL %lf\n\n",ext_mult);

   fprintf(out,"VBF %.02f\n",VBW_factor);
   fprintf(out,"CLP %d\n\n",clip);

   //
   // Set basic analyzer state needed for operation of program
   //

   GPIB_printf("SI");                                 // Single-sweep mode
   GPIB_printf("MKOFF");                              // Turn off any marker-based functionality
   GPIB_printf("AUNITS DBM;DD 10DB");                 // Need 10 dB/division log scale
   GPIB_printf("BA");                                 // Autoselect RBW
   GPIB_printf("VA");                                 // Autoselect VBW
   GPIB_printf("AS");                                 // Autoselect sweep time

   //
   // Perform actual measurement at external IF, if one is specified
   //

   S64 source_carrier = carrier;

   if (ext_IF >= 0)
      {
      carrier = ext_IF;
      }

   //
   // Log beginning of sweep series
   //

   S64 start = elapsed_uS();

   //
   // For each decade...
   //

   bool reference_level_set = FALSE;

   S64 mult_delta_Hz = S64(((DOUBLE) carrier * ext_mult) + 0.5) - carrier;  

   for (S32 d=min_decade; d <= max_decade; d++)
      {
      static SINGLE trace_A   [2][1001];
      static DOUBLE freq      [2][1001];
      SINGLE        correction[2];

      S32 decade_start = (S32) (pow(10.0, d) + 0.5F);
      S32 next_decade  = decade_start * 10;
      S32 total_span   = decade_start * 10;

      //
      // Select RBW and span width
      //
      // To avoid error messages, we need to set a wide RBW before we 
      // increase the span width from the previous decade.  (If we set the desired RBW
      // first, the analyzer would complain about the initial span being too wide,
      // and if we set the span width first, the analyer would complain about 
      // the RBW from the previous decade being too narrow)
      //

      S32 RBW = decade_start / 10;

      if (RBW == 100)
         {
         RBW = 300;        // Allow use of 1000-10000 Hz decade with 300 Hz filter
         }

      if (RBW >= 100000)   
         {
         RBW = 10000;      // 100 kHz RBW filter has excess noise 
         }

      S32 VBW = (S32) ((RBW * VBW_factor) + 0.5F);

      GPIB_printf("RB 1000000HZ");
      GPIB_printf("SP %dHZ",total_span); 
      GPIB_printf("RB %dHZ",RBW);        

      //
      // Set reference level, if it hasn't already been established, and
      // apply reference-level clipping as soon as we can
      //

      if (!reference_level_set)
         {
         reference_level_set = TRUE;
         GPIB_printf("RL %fDB",ref);       
   
         for (S32 cp=0; cp < clip; cp += 10)
            {
            GPIB_printf("RL DN");
            }
         }

      //
      // Query actual reference level being used for this sweep
      //

      GPIB_flush_receive_buffers();

      SINGLE SOURCE_max_dBm = 10000.0F;

      while (SOURCE_max_dBm == 10000.0F)
         {
         C8 *RL_text = GPIB_query("RL?");
         sscanf(RL_text,"%f",&SOURCE_max_dBm);
         }

      //
      // Enable video filtering, and select sample detection
      // for noise-like signals
      //

      GPIB_printf("DET SMP");   
      GPIB_printf("VB %dHZ",VBW);

      S64 tune_freq = carrier + (decade_start * 5);

      //
      // Log some statistics used for this sweep
      //

      GPIB_flush_receive_buffers();    // AT? query is invalid under Prologix if we don't do this

      SINGLE att = 10000.0F;

      while (att == 10000.0F)
         {
         C8 *AT_text = GPIB_query("AT?");
         sscanf(AT_text,"%f",&att);
         }

      SAL_debug_printf("Carrier level = %.02f dBm, RF atten = %d dB\n",ref,S32(att + 0.5F));

      fprintf(out,"CAR %I64d Hz, %.02f dBm\n",
         source_carrier,
         ref);

      fprintf(out,"ATT %d dB\n",
         S32(att + 0.5F));

      fprintf(out,"REF %f dBm\n", SOURCE_max_dBm);

      SINGLE SOURCE_RBW = -10000.0F;

      while (SOURCE_RBW == -10000.0F)
         {
         C8 *RL_text = GPIB_query("RB?");
         sscanf(RL_text,"%f",&SOURCE_RBW);
         }

      fprintf(out,"RBW %f Hz\n", SOURCE_RBW);

      SINGLE SOURCE_VBW = -10000.0F;

      while (SOURCE_VBW == -10000.0F)
         {
         C8 *RL_text = GPIB_query("VB?");
         sscanf(RL_text,"%f",&SOURCE_VBW);
         }

      fprintf(out,"VBW %f Hz\n", SOURCE_VBW);

      for (S32 half=0; half < 2; half++)
         {
         SAL_serve_message_queue();

         //
         // Take a reading at each half-decade
         //

         SAL_debug_printf("Analyzing decade 10^%d, sweep %d of 2...\n",d,half+1);

         if (key_hit)
            {
            key_hit = FALSE;
            EXIT_R3465(clip, carrier + mult_delta_Hz);

            SAL_alert_box("Error","Aborted by keypress");
            return FALSE;
            }

         //
         // Trigger sweep at specified center frequency, and request binary trace dump 
         // (Use EOI termination for binary dump, not CR/LF)
         //
         // Add 2-second delay to work around R3463 firmware bug that allows subsequent 
         // commands to execute before CF has finished 
         //
         // (In tests with R3463, 100 ms was not enough, and 500 ms was reliable)
         //

         GPIB_printf("CF %I64dHZ",tune_freq + mult_delta_Hz);
         Sleep(2000);

         GPIB_printf("DL2");
         GPIB_flush_receive_buffers();

         GPIB_printf("TS;TBA?");

         static U16 curve[1001];     

         memset(curve, 0, R3465_n_points * sizeof(S16));
         memcpy(curve, GPIB_read_BIN(R3465_n_points * sizeof(S16),TRUE,FALSE), R3465_n_points * sizeof(S16));

         GPIB_printf("DL0");
         GPIB_flush_receive_buffers();

         SINGLE range       = 100.0F;             // 100 dB display range
         SINGLE offset      = SOURCE_max_dBm - range;
         SINGLE scale       = range / 12800.0F;   // bottom=1792, ref=14592, top=14592+ 
         SINGLE grat_offset = -1792.0F;

         DOUBLE df_dp = ((DOUBLE) total_span) / (DOUBLE) R3465_n_points;
         DOUBLE F     = ((DOUBLE) tune_freq) - ((DOUBLE) R3465_n_points / 2.0 * df_dp);

         for (i=0; i < R3465_n_points; i++)     
            {
            curve[i] = ((curve[i] & 0xff00) >> 8) + ((curve[i] & 0x00ff) << 8);

            SINGLE dBm = ((SINGLE(curve[i]) + grat_offset) * scale) + offset; 

            trace_A[half][i] = dBm - ref;
            freq   [half][i] = F;

            F += df_dp;
            }

         //
         // Apply noise-normalization factor
         //
         // This is typically 2 dB greater than the baseline
         // 10*log(RBW) normalization factor due to filter
         // and detector noise-response characteristics
         // 

         SINGLE RB_factor = -10.0F * (SINGLE) log10((DOUBLE) SOURCE_RBW);

         correction[half] = RB_factor + NF_characteristic;

         tune_freq += next_decade;
         }

      //
      // Display results
      //

      SAL_debug_printf("\n%8d Hz: %.02f dBc/Hz\n",
         decade_start,
         trace_A[0][decile_n_points] + correction[0] + ext_mult_dB);

      SAL_debug_printf("%8d Hz: %.02f dBc/Hz / %.02f dBc/Hz\n\n",
         next_decade,
         trace_A[0][record_n_points-1]  + correction[0] + ext_mult_dB,
         trace_A[1][decile_n_points  ]  + correction[1] + ext_mult_dB);

      //
      // Send decade and overlap data to output file
      //

      fprintf(out,"DEC %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[0]);

      for (i=decile_n_points; i < record_n_points; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i - decile_n_points,
            freq[0][i],
            trace_A[0][i] + ext_mult_dB);
         }

      fprintf(out,"OVL %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[1]);

      for (i=0; i < record_n_points; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i,
            freq[1][i],
            trace_A[1][i] + ext_mult_dB);
         }
      }

   //
   // Write cumulative sweep time and EOF timestamp
   //

   S64 end = elapsed_uS();

   fprintf(out,"ELP %I64ds\n",((end - start) + 500000) / 1000000);
   fprintf(out,"EOF %s\n",timestamp());

   EXIT_R3465(clip, carrier + mult_delta_Hz);
   return TRUE;
}

//****************************************************************************
//*                                                                          *
//* Acquire data from R3131-family spectrum analyzer                         *
//* Tested with R3131A                                                       *
//*                                                                          *
//****************************************************************************

void EXIT_R3131(S32 clip, //)
                S64 carrier_Hz)
{
   if (out != NULL)
      {
      fclose(out);
      out = NULL;
      }

   //
   // Put clipped peak back onscreen
   //

   for (S32 cp=0; cp < clip; cp += 10)
      {
      GPIB_printf("RL UP");
      }

   GPIB_printf("DL0");                                        // Restore default <CR><LF><EOI> response delimiter
   GPIB_printf("SN");                                         // Continuous sweep
   GPIB_printf("BA");                                         // Autoselect RBW
   GPIB_printf("VA");                                         // Autoselect VBW
   GPIB_printf("DET NRM");                                    // Normal (Rosenfell) detection
   GPIB_printf("CF %I64dHZ",carrier_Hz);                      // Set CF

   GPIB_disconnect();
}

BOOL ACQ_R3131(C8    *filename,
               S32    GPIB_address,
               C8    *caption,
               S64    carrier,           
               S64    ext_IF,
               S64    ext_LO,
               SINGLE VBW_factor,
               DOUBLE ext_mult,          
               SINGLE NF_characteristic,
               SINGLE min_offset,        
               SINGLE max_offset,
               S32    clip,
               SINGLE ref = 10000.0F)
{            
   S32 i;

   if ((carrier < 0) || (ref == 10000.0F))
      {
      SAL_alert_box("Error","Must specify both carrier frequency and reference level");
      return FALSE;
      }

   //
   // Start up GPIB
   //

   GPIB_connect (GPIB_address,
                 GPIB_error,
                 0,
                 1000000);

   GPIB_set_EOS_mode(10);
   GPIB_set_serial_read_dropout(500);

   //
   // Open output file
   //

   out = fopen(filename,"wt");

   if (out == NULL)
      {
      GPIB_disconnect();

      SAL_alert_box("Error","Could not open %s",filename);
      return FALSE;
      }

   fprintf(out,";\n");
   fprintf(out,";Composite-noise plot file %s\n",filename);
   fprintf(out,";\n");
   fprintf(out,";Generated by PN.EXE V"VERSION" of "__DATE__"\n");
   fprintf(out,";by John Miles, KE5FX (john@miles.io)\n");
   fprintf(out,";\n\n");

   //
   // Log caption, if present
   //

   if (caption[0])
      {
      fprintf(out,"CAP %s\n",caption);
      }

   //
   // Disallow SRQs, clear error register
   // Query results terminated by CR+LF+EOI
   // 

   GPIB_printf("*SRE 0;*CLS;DL0");
   GPIB_flush_receive_buffers();    // if we don't do this, the queries below return one string behind

   //
   // Log timestamp and instrument ID
   //

   static C8 ID[1024];
   strcpy(ID,GPIB_query("*IDN?"));
   kill_trailing_whitespace(ID);

   fprintf(out,"TIM %s\n",timestamp());
   fprintf(out,"IMO %s\n", ID);
   fprintf(out,"IRV N/A\n\n");

   //
   // Use 501 trace points on R3131A
   //

   GPIB_printf("TPL");     // TODO: TPL not actually supported?
   S32 R3131_n_points = 501;

   S32 record_n_points = 100 * (R3131_n_points / 100);
   S32 decile_n_points = record_n_points / 10;

   //
   // Log decade statistics
   //

   S32    min_decade = S32(log10(min_offset)+0.5);
   S32    max_decade = S32(log10(max_offset)+0.5) - 1;
   SINGLE ext_mult_dB = 0.0F;

   SAL_debug_printf("Minimum offset = %d Hz\n",S32(pow(10.0,min_decade)+0.5));
   SAL_debug_printf("Maximum offset = %d Hz\n",S32(pow(10.0,max_decade+1)+0.5));

   ext_mult_dB = -20.0F * (SINGLE) log10(ext_mult);

   SAL_debug_printf("External frequency multiplier = %.06lfx (%.02f dB)\n",
                     ext_mult,ext_mult_dB);

   fprintf(out,"DRG %d,%d\n",min_decade,max_decade);
   fprintf(out,"DCC %d\n",record_n_points - decile_n_points);
   fprintf(out,"OVC %d\n\n",record_n_points);

   fprintf(out,"EIF %I64d Hz\n",ext_IF);
   fprintf(out,"ELO %I64d Hz\n",ext_LO);
   fprintf(out,"MUL %lf\n\n",ext_mult);

   fprintf(out,"VBF %.02f\n",VBW_factor);
   fprintf(out,"CLP %d\n\n",clip);

   //
   // Set basic analyzer state needed for operation of program
   //

   GPIB_printf("SI");                                 // Single-sweep mode
   GPIB_printf("MKOFF");                              // Turn off any marker-based functionality
   GPIB_printf("AUNITS DBM;DD 10DB");                 // Need 10 dB/division log scale
   GPIB_printf("BA");                                 // Autoselect RBW
   GPIB_printf("VA");                                 // Autoselect VBW
   GPIB_printf("AS");                                 // Autoselect sweep time

   //
   // Perform actual measurement at external IF, if one is specified
   //

   S64 source_carrier = carrier;

   if (ext_IF >= 0)
      {
      carrier = ext_IF;
      }

   //
   // Log beginning of sweep series
   //

   S64 start = elapsed_uS();

   //
   // For each decade...
   //

   bool reference_level_set = FALSE;

   S64 mult_delta_Hz = S64(((DOUBLE) carrier * ext_mult) + 0.5) - carrier;  

   for (S32 d=min_decade; d <= max_decade; d++)
      {
      static SINGLE trace_A   [2][1001];
      static DOUBLE freq      [2][1001];
      SINGLE        correction[2];

      S32 decade_start = (S32) (pow(10.0, d) + 0.5F);
      S32 next_decade  = decade_start * 10;
      S32 total_span   = decade_start * 10;

      //
      // Select RBW and span width
      //
      // To avoid error messages, we need to set a wide RBW before we 
      // increase the span width from the previous decade.  (If we set the desired RBW
      // first, the analyzer would complain about the initial span being too wide,
      // and if we set the span width first, the analyer would complain about 
      // the RBW from the previous decade being too narrow)
      //

      S32 RBW = decade_start / 10;

      if (RBW == 10)
         {
         RBW = 30;         // Allow use of 100-1000 Hz decade with optional 30 Hz filter
         }

      if (RBW >= 10000)    // Use narrower RBW filters for better accuracy on R3131A
         {
         RBW /= 10;  
         }

      S32 VBW = (S32) ((RBW * VBW_factor) + 0.5F);

      GPIB_printf("RB 1000000HZ");
      GPIB_printf("SP %dHZ",total_span); 
      GPIB_printf("RB %dHZ",RBW);        

      //
      // Set reference level, if it hasn't already been established, and
      // apply reference-level clipping as soon as we can
      //

      if (!reference_level_set)
         {
         reference_level_set = TRUE;
         GPIB_printf("RL %fDB",ref);       
   
         for (S32 cp=0; cp < clip; cp += 10)
            {
            GPIB_printf("RL DN");
            }
         }

      //
      // Query actual reference level being used for this sweep
      //

      GPIB_flush_receive_buffers();

      SINGLE SOURCE_max_dBm = 10000.0F;

      while (SOURCE_max_dBm == 10000.0F)
         {
         C8 *RL_text = GPIB_query("RL?");
         sscanf(RL_text,"%f",&SOURCE_max_dBm);
         }

      //
      // Enable video filtering, and select sample detection
      // for noise-like signals
      //

      GPIB_printf("DET SMP");   
      GPIB_printf("VB %dHZ",VBW);

      S64 tune_freq = carrier + (decade_start * 5);

      //
      // Log some statistics used for this sweep
      //

      GPIB_flush_receive_buffers();    // AT? query is invalid under Prologix if we don't do this

      SINGLE att = 10000.0F;

      while (att == 10000.0F)
         {
         C8 *AT_text = GPIB_query("AT?");
         sscanf(AT_text,"%f",&att);
         }

      SAL_debug_printf("Carrier level = %.02f dBm, RF atten = %d dB\n",ref,S32(att + 0.5F));

      fprintf(out,"CAR %I64d Hz, %.02f dBm\n",
         source_carrier,
         ref);

      fprintf(out,"ATT %d dB\n",
         S32(att + 0.5F));

      fprintf(out,"REF %f dBm\n", SOURCE_max_dBm);

      SINGLE SOURCE_RBW = -10000.0F;

      while (SOURCE_RBW == -10000.0F)
         {
         C8 *RL_text = GPIB_query("RB?");
         sscanf(RL_text,"%f",&SOURCE_RBW);
         }

      fprintf(out,"RBW %f Hz\n", SOURCE_RBW);

      SINGLE SOURCE_VBW = -10000.0F;

      while (SOURCE_VBW == -10000.0F)
         {
         C8 *RL_text = GPIB_query("VB?");
         sscanf(RL_text,"%f",&SOURCE_VBW);
         }

      fprintf(out,"VBW %f Hz\n", SOURCE_VBW);

      for (S32 half=0; half < 2; half++)
         {
         SAL_serve_message_queue();

         //
         // Take a reading at each half-decade
         //

         SAL_debug_printf("Analyzing decade 10^%d, sweep %d of 2...\n",d,half+1);

         if (key_hit)
            {
            key_hit = FALSE;
            EXIT_R3131(clip, carrier + mult_delta_Hz);

            SAL_alert_box("Error","Aborted by keypress");
            return FALSE;
            }

         //
         // Trigger sweep at specified center frequency, and request binary trace dump 
         // (Use EOI termination for binary dump, not CR/LF)
         //
         // Add 2-second delay to work around R3463 firmware bug that allows subsequent 
         // commands to execute before CF has finished 
         //
         // (In tests with R3463, 100 ms was not enough, and 500 ms was reliable)
         //

         GPIB_printf("CF %I64dHZ",tune_freq + mult_delta_Hz);
         Sleep(2000);

         GPIB_printf("DL2");
         GPIB_flush_receive_buffers();

         GPIB_printf("TS;TBA?");

         static U16 curve[1001];     

         memset(curve, 0, R3131_n_points * sizeof(S16));
         memcpy(curve, GPIB_read_BIN(R3131_n_points * sizeof(S16),TRUE,FALSE), R3131_n_points * sizeof(S16));

         GPIB_printf("DL0");
         GPIB_flush_receive_buffers();

         SINGLE range       = 80.0F;              // 80 dB display range
         SINGLE offset      = SOURCE_max_dBm - range;
         SINGLE scale       = range / 2880.0F;  
         SINGLE grat_offset = -512.0F;

         DOUBLE df_dp = ((DOUBLE) total_span) / (DOUBLE) R3131_n_points;
         DOUBLE F     = ((DOUBLE) tune_freq) - ((DOUBLE) R3131_n_points / 2.0 * df_dp);

         for (i=0; i < R3131_n_points; i++)     
            {
            curve[i] = ((curve[i] & 0xff00) >> 8) + ((curve[i] & 0x00ff) << 8);

            SINGLE dBm = ((SINGLE(curve[i]) + grat_offset) * scale) + offset; 

            trace_A[half][i] = dBm - ref;
            freq   [half][i] = F;

            F += df_dp;
            }

         //
         // Apply noise-normalization factor
         //
         // This is typically 2 dB greater than the baseline
         // 10*log(RBW) normalization factor due to filter
         // and detector noise-response characteristics
         // 

         SINGLE RB_factor = -10.0F * (SINGLE) log10((DOUBLE) SOURCE_RBW);

         correction[half] = RB_factor + NF_characteristic;

         tune_freq += next_decade;
         }

      //
      // Display results
      //

      SAL_debug_printf("\n%8d Hz: %.02f dBc/Hz\n",
         decade_start,
         trace_A[0][decile_n_points] + correction[0] + ext_mult_dB);

      SAL_debug_printf("%8d Hz: %.02f dBc/Hz / %.02f dBc/Hz\n\n",
         next_decade,
         trace_A[0][record_n_points-1]  + correction[0] + ext_mult_dB,
         trace_A[1][decile_n_points  ]  + correction[1] + ext_mult_dB);

      //
      // Send decade and overlap data to output file
      //

      fprintf(out,"DEC %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[0]);

      for (i=decile_n_points; i < record_n_points; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i - decile_n_points,
            freq[0][i],
            trace_A[0][i] + ext_mult_dB);
         }

      fprintf(out,"OVL %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[1]);

      for (i=0; i < record_n_points; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i,
            freq[1][i],
            trace_A[1][i] + ext_mult_dB);
         }
      }

   //
   // Write cumulative sweep time and EOF timestamp
   //

   S64 end = elapsed_uS();

   fprintf(out,"ELP %I64ds\n",((end - start) + 500000) / 1000000);
   fprintf(out,"EOF %s\n",timestamp());

   EXIT_R3131(clip, carrier + mult_delta_Hz);
   return TRUE;
}


//****************************************************************************
//*                                                                          *
//* Acquire data from R3132-family spectrum analyzer                         *
//* Tested with R3132                                                        *
//*                                                                          *
//****************************************************************************

void EXIT_R3132(S32 clip, //)
                S64 carrier_Hz)
{
   if (out != NULL)
      {
      fclose(out);
      out = NULL;
      }

   //
   // Put clipped peak back onscreen
   //

   for (S32 cp=0; cp < clip; cp += 10)
      {
      GPIB_printf("RL UP");
      }

   GPIB_printf("DL0");                                        // Restore default <CR><LF><EOI> response delimiter
   GPIB_printf("SN");                                         // Continuous sweep
   GPIB_printf("BA");                                         // Autoselect RBW
   GPIB_printf("VA");                                         // Autoselect VBW
   GPIB_printf("DET NRM");                                    // Normal (Rosenfell) detection
   GPIB_printf("CF %I64dHZ",carrier_Hz);                      // Set CF

   GPIB_disconnect();
}

BOOL ACQ_R3132(C8    *filename,
               S32    GPIB_address,
               C8    *caption,
               S64    carrier,           
               S64    ext_IF,
               S64    ext_LO,
               SINGLE VBW_factor,
               DOUBLE ext_mult,          
               SINGLE NF_characteristic,
               SINGLE min_offset,        
               SINGLE max_offset,
               S32    clip,
               SINGLE ref = 10000.0F)
{            
   S32 i;

   if ((carrier < 0) || (ref == 10000.0F))
      {
      SAL_alert_box("Error","Must specify both carrier frequency and reference level");
      return FALSE;
      }

   //
   // Start up GPIB
   //

   GPIB_connect (GPIB_address,
                 GPIB_error,
                 0,
                 1000000);

   GPIB_set_EOS_mode(10);
   GPIB_set_serial_read_dropout(500);

   //
   // Open output file
   //

   out = fopen(filename,"wt");

   if (out == NULL)
      {
      GPIB_disconnect();

      SAL_alert_box("Error","Could not open %s",filename);
      return FALSE;
      }

   fprintf(out,";\n");
   fprintf(out,";Composite-noise plot file %s\n",filename);
   fprintf(out,";\n");
   fprintf(out,";Generated by PN.EXE V"VERSION" of "__DATE__"\n");
   fprintf(out,";by John Miles, KE5FX (john@miles.io)\n");
   fprintf(out,";\n\n");

   //
   // Log caption, if present
   //

   if (caption[0])
      {
      fprintf(out,"CAP %s\n",caption);
      }

   //
   // Disallow SRQs, clear error register
   // Query results terminated by CR+LF+EOI
   // 

   GPIB_printf("*SRE 0;*CLS;DL0");
   GPIB_flush_receive_buffers();    // if we don't do this, the queries below return one string behind

   //
   // Log timestamp and instrument ID
   //

   static C8 ID[1024];
   strcpy(ID,GPIB_query("*IDN?"));
   kill_trailing_whitespace(ID);

   fprintf(out,"TIM %s\n",timestamp());
   fprintf(out,"IMO %s\n", ID);
   fprintf(out,"IRV N/A\n\n");

   //
   // Use 1001 trace points on R3132 family
   //

   GPIB_printf("TPL");
   S32 R3132_n_points = 1001;

   S32 record_n_points = 100 * (R3132_n_points / 100);
   S32 decile_n_points = record_n_points / 10;

   //
   // Log decade statistics
   //

   S32    min_decade = S32(log10(min_offset)+0.5);
   S32    max_decade = S32(log10(max_offset)+0.5) - 1;
   SINGLE ext_mult_dB = 0.0F;

   SAL_debug_printf("Minimum offset = %d Hz\n",S32(pow(10.0,min_decade)+0.5));
   SAL_debug_printf("Maximum offset = %d Hz\n",S32(pow(10.0,max_decade+1)+0.5));

   ext_mult_dB = -20.0F * (SINGLE) log10(ext_mult);

   SAL_debug_printf("External frequency multiplier = %.06lfx (%.02f dB)\n",
                     ext_mult,ext_mult_dB);

   fprintf(out,"DRG %d,%d\n",min_decade,max_decade);
   fprintf(out,"DCC %d\n",record_n_points - decile_n_points);
   fprintf(out,"OVC %d\n\n",record_n_points);

   fprintf(out,"EIF %I64d Hz\n",ext_IF);
   fprintf(out,"ELO %I64d Hz\n",ext_LO);
   fprintf(out,"MUL %lf\n\n",ext_mult);

   fprintf(out,"VBF %.02f\n",VBW_factor);
   fprintf(out,"CLP %d\n\n",clip);

   //
   // Set basic analyzer state needed for operation of program
   //

   GPIB_printf("SI");                                 // Single-sweep mode
   GPIB_printf("MKOFF");                              // Turn off any marker-based functionality
   GPIB_printf("AUNITS DBM;DD 10DB");                 // Need 10 dB/division log scale
   GPIB_printf("BA");                                 // Autoselect RBW
   GPIB_printf("VA");                                 // Autoselect VBW
   GPIB_printf("AS");                                 // Autoselect sweep time

   //
   // Perform actual measurement at external IF, if one is specified
   //

   S64 source_carrier = carrier;

   if (ext_IF >= 0)
      {
      carrier = ext_IF;
      }

   //
   // Log beginning of sweep series
   //

   S64 start = elapsed_uS();

   //
   // For each decade...
   //

   bool reference_level_set = FALSE;

   S64 mult_delta_Hz = S64(((DOUBLE) carrier * ext_mult) + 0.5) - carrier;  

   for (S32 d=min_decade; d <= max_decade; d++)
      {
      static SINGLE trace_A   [2][1001];
      static DOUBLE freq      [2][1001];
      SINGLE        correction[2];

      S32 decade_start = (S32) (pow(10.0, d) + 0.5F);
      S32 next_decade  = decade_start * 10;
      S32 total_span   = decade_start * 10;

      //
      // Select RBW and span width
      //
      // To avoid error messages, we need to set a wide RBW before we 
      // increase the span width from the previous decade.  (If we set the desired RBW
      // first, the analyzer would complain about the initial span being too wide,
      // and if we set the span width first, the analyer would complain about 
      // the RBW from the previous decade being too narrow)
      //

      S32 RBW = decade_start / 10;

      if (RBW == 10)
         {
         RBW = 30;         // Allow use of 100-1000 Hz decade with optional 30 Hz filter
         }

      if (RBW >= 100000)   
         {
         RBW = 10000;      // (100 kHz RBW filter has excess noise on R3465, might not be necessary
         }                 // on R3132)

      S32 VBW = (S32) ((RBW * VBW_factor) + 0.5F);

      GPIB_printf("RB 1000000HZ");
      GPIB_printf("SP %dHZ",total_span); 
      GPIB_printf("RB %dHZ",RBW);        

      //
      // Set reference level, if it hasn't already been established, and
      // apply reference-level clipping as soon as we can
      //

      if (!reference_level_set)
         {
         reference_level_set = TRUE;
         GPIB_printf("RL %fDB",ref);       
   
         for (S32 cp=0; cp < clip; cp += 10)
            {
            GPIB_printf("RL DN");
            }
         }

      //
      // Query actual reference level being used for this sweep
      //

      GPIB_flush_receive_buffers();

      SINGLE SOURCE_max_dBm = 10000.0F;

      while (SOURCE_max_dBm == 10000.0F)
         {
         C8 *RL_text = GPIB_query("RL?");
         sscanf(RL_text,"%f",&SOURCE_max_dBm);
         }

      //
      // Enable video filtering, and select sample detection
      // for noise-like signals
      //

      GPIB_printf("DET SMP");   
      GPIB_printf("VB %dHZ",VBW);

      S64 tune_freq = carrier + (decade_start * 5);

      //
      // Log some statistics used for this sweep
      //

      GPIB_flush_receive_buffers();    // AT? query is invalid under Prologix if we don't do this

      SINGLE att = 10000.0F;

      while (att == 10000.0F)
         {
         C8 *AT_text = GPIB_query("AT?");
         sscanf(AT_text,"%f",&att);
         }

      SAL_debug_printf("Carrier level = %.02f dBm, RF atten = %d dB\n",ref,S32(att + 0.5F));

      fprintf(out,"CAR %I64d Hz, %.02f dBm\n",
         source_carrier,
         ref);

      fprintf(out,"ATT %d dB\n",
         S32(att + 0.5F));

      fprintf(out,"REF %f dBm\n", SOURCE_max_dBm);

      SINGLE SOURCE_RBW = -10000.0F;

      while (SOURCE_RBW == -10000.0F)
         {
         C8 *RL_text = GPIB_query("RB?");
         sscanf(RL_text,"%f",&SOURCE_RBW);
         }

      fprintf(out,"RBW %f Hz\n", SOURCE_RBW);

      SINGLE SOURCE_VBW = -10000.0F;

      while (SOURCE_VBW == -10000.0F)
         {
         C8 *RL_text = GPIB_query("VB?");
         sscanf(RL_text,"%f",&SOURCE_VBW);
         }

      fprintf(out,"VBW %f Hz\n", SOURCE_VBW);

      for (S32 half=0; half < 2; half++)
         {
         SAL_serve_message_queue();

         //
         // Take a reading at each half-decade
         //

         SAL_debug_printf("Analyzing decade 10^%d, sweep %d of 2...\n",d,half+1);

         if (key_hit)
            {
            key_hit = FALSE;
            EXIT_R3132(clip, carrier + mult_delta_Hz);

            SAL_alert_box("Error","Aborted by keypress");
            return FALSE;
            }

         //
         // Trigger sweep at specified center frequency, and request binary trace dump 
         // (Use EOI termination for binary dump, not CR/LF)
         //
         // Add 2-second delay to work around R3463 firmware bug that allows subsequent 
         // commands to execute before CF has finished 
         //
         // (In tests with R3463, 100 ms was not enough, and 500 ms was reliable)
         //

         GPIB_printf("CF %I64dHZ",tune_freq + mult_delta_Hz);
         Sleep(2000);

         GPIB_printf("DL2");
         GPIB_flush_receive_buffers();

         GPIB_printf("TS;TBA?");

         static U16 curve[1001];     

         memset(curve, 0, R3132_n_points * sizeof(S16));
         memcpy(curve, GPIB_read_BIN(R3132_n_points * sizeof(S16),TRUE,FALSE), R3132_n_points * sizeof(S16));

         GPIB_printf("DL0");
         GPIB_flush_receive_buffers();

         SINGLE range       = 100.0F;             // 100 dB display range
         SINGLE offset      = SOURCE_max_dBm - range;
         SINGLE scale       = range / 12800.0F;   // bottom=1792, ref=14592, top=14592+ 
         SINGLE grat_offset = -1792.0F;

         DOUBLE df_dp = ((DOUBLE) total_span) / (DOUBLE) R3132_n_points;
         DOUBLE F     = ((DOUBLE) tune_freq) - ((DOUBLE) R3132_n_points / 2.0 * df_dp);

         for (i=0; i < R3132_n_points; i++)     
            {
            curve[i] = ((curve[i] & 0xff00) >> 8) + ((curve[i] & 0x00ff) << 8);

            SINGLE dBm = ((SINGLE(curve[i]) + grat_offset) * scale) + offset; 

            trace_A[half][i] = dBm - ref;
            freq   [half][i] = F;

            F += df_dp;
            }

         //
         // Apply noise-normalization factor
         //
         // This is typically 2 dB greater than the baseline
         // 10*log(RBW) normalization factor due to filter
         // and detector noise-response characteristics
         // 

         SINGLE RB_factor = -10.0F * (SINGLE) log10((DOUBLE) SOURCE_RBW);

         correction[half] = RB_factor + NF_characteristic;

         tune_freq += next_decade;
         }

      //
      // Display results
      //

      SAL_debug_printf("\n%8d Hz: %.02f dBc/Hz\n",
         decade_start,
         trace_A[0][decile_n_points] + correction[0] + ext_mult_dB);

      SAL_debug_printf("%8d Hz: %.02f dBc/Hz / %.02f dBc/Hz\n\n",
         next_decade,
         trace_A[0][record_n_points-1]  + correction[0] + ext_mult_dB,
         trace_A[1][decile_n_points  ]  + correction[1] + ext_mult_dB);

      //
      // Send decade and overlap data to output file
      //

      fprintf(out,"DEC %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[0]);

      for (i=decile_n_points; i < record_n_points; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i - decile_n_points,
            freq[0][i],
            trace_A[0][i] + ext_mult_dB);
         }

      fprintf(out,"OVL %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[1]);

      for (i=0; i < record_n_points; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i,
            freq[1][i],
            trace_A[1][i] + ext_mult_dB);
         }
      }

   //
   // Write cumulative sweep time and EOF timestamp
   //

   S64 end = elapsed_uS();

   fprintf(out,"ELP %I64ds\n",((end - start) + 500000) / 1000000);
   fprintf(out,"EOF %s\n",timestamp());

   EXIT_R3132(clip, carrier + mult_delta_Hz);
   return TRUE;
}

//****************************************************************************
//*                                                                          *
//* Acquire data from R3261/R3361-family spectrum analyzer                   *
//* Tested with R3361                                                        *
//*                                                                          *
//****************************************************************************

void EXIT_R3261(S32 clip, //)
                S64 carrier_Hz)
{
   if (out != NULL)
      {
      fclose(out);
      out = NULL;
      }

   //
   // Put clipped peak back onscreen
   //

   for (S32 cp=0; cp < clip; cp += 10)
      {
      GPIB_printf("RL UP");
      }

   GPIB_printf("SN");                                         // Continuous sweep
   GPIB_printf("FR");                                         // Free-run trigger
   GPIB_printf("BA");                                         // Autoselect RBW
   GPIB_printf("VA");                                         // Autoselect VBW
   GPIB_printf("DTN");                                        // Normal detection
   GPIB_printf("CF %I64dHZ",carrier_Hz);                      // Set CF

   GPIB_disconnect();
}

BOOL ACQ_R3261(C8    *filename,
               S32    GPIB_address,
               C8    *caption,
               S64    carrier,           
               S64    ext_IF,
               S64    ext_LO,
               SINGLE VBW_factor,
               DOUBLE ext_mult,          
               SINGLE NF_characteristic,
               SINGLE min_offset,        
               SINGLE max_offset,
               S32    clip,
               SINGLE ref = 10000.0F)
{            
   S32 i;

   if ((carrier < 0) || (ref == 10000.0F))
      {
      SAL_alert_box("Error","Must specify both carrier frequency and reference level");
      return FALSE;
      }

   //
   // Start up GPIB
   //

   GPIB_connect (GPIB_address,
                 GPIB_error,
                 0,
                 1000000);

   GPIB_set_EOS_mode(10);
   GPIB_set_serial_read_dropout(2000);

   //
   // Open output file
   //

   out = fopen(filename,"wt");

   if (out == NULL)
      {
      GPIB_disconnect();

      SAL_alert_box("Error","Could not open %s",filename);
      return FALSE;
      }

   fprintf(out,";\n");
   fprintf(out,";Composite-noise plot file %s\n",filename);
   fprintf(out,";\n");
   fprintf(out,";Generated by PN.EXE V"VERSION" of "__DATE__"\n");
   fprintf(out,";by John Miles, KE5FX (john@miles.io)\n");
   fprintf(out,";\n\n");

   //
   // Log caption, if present
   //

   if (caption[0])
      {
      fprintf(out,"CAP %s\n",caption);
      }

   //
   // Expect (and return) CR+LF|EOI delimiters, use single-sweep mode, disable SRQs, and clear status byte
   //
   // Log timestamp and instrument ID, and whether the instrument is in the R3261/R3361 or
   // R3265/R3271 family.  
   //

   GPIB_puts("DL0 SI S1 S2 HD0");
   GPIB_flush_receive_buffers();

   static C8 ID[1024];
   strcpy(ID,GPIB_query("TYP?")); 
   kill_trailing_whitespace(ID);

   if (ID[0] == '0')
      {
      //
      // TYP? returns '0' on at least some R3271As instead of a valid model number (Khoa Dang, 5/11) so fall
      // back to VER? as an alternative means of identifying the model
      //

      strcpy(ID,GPIB_query("VER?"));
      kill_trailing_whitespace(ID);

      if (ID[0] == '1')
         strcpy(ID,"Advantest R3271 family");
      else
         strcpy(ID,"Advantest R3265 family");
      }

   BOOL is_R3261_R3361 = (strstr(ID,"3261") != NULL) || 
                         (strstr(ID,"3361") != NULL);

   if (!is_R3261_R3361)
      {
      GPIB_puts("TPC");          // If R3265/R3271, select 401-point vertical resolution for compatibility with R3261/R3361
      }

   fprintf(out,"TIM %s\n",timestamp());
   fprintf(out,"IMO %s\n", ID);
   fprintf(out,"IRV N/A\n\n");

   //
   // Use 701 trace points on R3261 family
   //

   S32 R3261_n_points = 701;

   S32 record_n_points = 100 * (R3261_n_points / 100);
   S32 decile_n_points = record_n_points / 10;

   //
   // Log decade statistics
   //

   S32    min_decade = S32(log10(min_offset)+0.5);
   S32    max_decade = S32(log10(max_offset)+0.5) - 1;
   SINGLE ext_mult_dB = 0.0F;

   SAL_debug_printf("Minimum offset = %d Hz\n",S32(pow(10.0,min_decade)+0.5));
   SAL_debug_printf("Maximum offset = %d Hz\n",S32(pow(10.0,max_decade+1)+0.5));

   ext_mult_dB = -20.0F * (SINGLE) log10(ext_mult);

   SAL_debug_printf("External frequency multiplier = %.06lfx (%.02f dB)\n",
                     ext_mult,ext_mult_dB);

   fprintf(out,"DRG %d,%d\n",min_decade,max_decade);
   fprintf(out,"DCC %d\n",record_n_points - decile_n_points);
   fprintf(out,"OVC %d\n\n",record_n_points);

   fprintf(out,"EIF %I64d Hz\n",ext_IF);
   fprintf(out,"ELO %I64d Hz\n",ext_LO);
   fprintf(out,"MUL %lf\n\n",ext_mult);

   fprintf(out,"VBF %.02f\n",VBW_factor);
   fprintf(out,"CLP %d\n\n",clip);

   //
   // Set basic analyzer state needed for operation of program
   //

   GPIB_printf("MO");                      // Turn off any marker-based functionality
   GPIB_printf("UB");                      // Need log scale
   GPIB_printf("DD 10DB");                 // Need 10 dB/division
   GPIB_printf("BA");                      // Autoselect RBW
   GPIB_printf("VA");                      // Autoselect VBW
   GPIB_printf("AS");                      // Autoselect sweep time

   //
   // R3261/R3361 support 8/12 division displays at 10 dB/division -- find out
   // which is active
   //
   // R3265/R3271 is always 10 divisions
   //

   SINGLE range = 100.0F;

   if (is_R3261_R3361)
      {
      if (GPIB_query("DV?")[0] == '1')
         range = 120.0F;
      else
         range = 80.0F;
      }

   //
   // Perform actual measurement at external IF, if one is specified
   //

   S64 source_carrier = carrier;

   if (ext_IF >= 0)
      {
      carrier = ext_IF;
      }

   //
   // Log beginning of sweep series
   //

   S64 start = elapsed_uS();

   //
   // For each decade...
   //

   bool reference_level_set = FALSE;

   S64 mult_delta_Hz = S64(((DOUBLE) carrier * ext_mult) + 0.5) - carrier;  

   for (S32 d=min_decade; d <= max_decade; d++)
      {
      static SINGLE trace_A   [2][701];
      static DOUBLE freq      [2][701];
      SINGLE        correction[2];

      S32 decade_start = (S32) (pow(10.0, d) + 0.5F);
      S32 next_decade  = decade_start * 10;
      S32 total_span   = decade_start * 10;

      //
      // Select RBW and span width
      //
      // To avoid error messages, we need to set a wide RBW before we 
      // increase the span width from the previous decade.  (If we set the desired RBW
      // first, some analyzers would complain about the initial span being too wide,
      // and if we set the span width first, they would complain about 
      // the RBW from the previous decade being too narrow)
      //

      S32 RBW = decade_start / 10;

      if (RBW == 10)
         {
         RBW = 30;         // Allow use of 30 Hz filter
         }

      S32 VBW = (S32) ((RBW * VBW_factor) + 0.5F);

      GPIB_printf("RB 1000000HZ");
      GPIB_printf("LS %dHZ",total_span); 
      GPIB_printf("RB %dHZ",RBW);        

      //
      // Set reference level, if it hasn't already been established, and
      // apply reference-level clipping as soon as we can
      //

      if (!reference_level_set)
         {
         reference_level_set = TRUE;
         GPIB_printf("RL %fDB",ref);       
   
         for (S32 cp=0; cp < clip; cp += 10)
            {
            GPIB_printf("RL DN");
            }
         }

      //
      // Query actual reference level being used for this sweep
      //

      SINGLE SOURCE_max_dBm = 10000.0F;

      while (SOURCE_max_dBm == 10000.0F)
         {
         C8 *RL_text = GPIB_query("RL?");
         sscanf(RL_text,"%f",&SOURCE_max_dBm);
         }

      //
      // Enable video filtering, and select sample detection
      // for noise-like signals
      //

      GPIB_printf("DTS");   
      GPIB_printf("VB %dHZ",VBW);

      S64 tune_freq = carrier + (decade_start * 5);

      //
      // Log some statistics used for this sweep
      //

      SINGLE att = 10000.0F;

      while (att == 10000.0F)
         {
         C8 *AT_text = GPIB_query("AT?");
         sscanf(AT_text,"%f",&att);
         }

      SAL_debug_printf("Carrier level = %.02f dBm, RF atten = %d dB\n",ref,S32(att + 0.5F));

      fprintf(out,"CAR %I64d Hz, %.02f dBm\n",
         source_carrier,
         ref);

      fprintf(out,"ATT %d dB\n",
         S32(att + 0.5F));

      fprintf(out,"REF %f dBm\n", SOURCE_max_dBm);

      SINGLE SOURCE_RBW = -10000.0F;

      while (SOURCE_RBW == -10000.0F)
         {
         C8 *RB_text = GPIB_query("RB?");
         sscanf(RB_text,"%f",&SOURCE_RBW);
         }

      fprintf(out,"RBW %f Hz\n", SOURCE_RBW);

      SINGLE SOURCE_VBW = -10000.0F;

      while (SOURCE_VBW == -10000.0F)
         {
         C8 *VB_text = GPIB_query("VB?");
         sscanf(VB_text,"%f",&SOURCE_VBW);
         }

      fprintf(out,"VBW %f Hz\n", SOURCE_VBW);

      for (S32 half=0; half < 2; half++)
         {
         SAL_serve_message_queue();

         //
         // Take a reading at each half-decade
         //

         SAL_debug_printf("Analyzing decade 10^%d, sweep %d of 2...\n",d,half+1);

         if (key_hit)
            {
            key_hit = FALSE;
            EXIT_R3261(clip, carrier + mult_delta_Hz);

            SAL_alert_box("Error","Aborted by keypress");
            return FALSE;
            }

         //
         // Trigger sweep at specified center frequency, and request binary trace dump 
         // (Use EOI termination for binary dump, not CR/LF)
         //

         GPIB_printf("CF %I64dHZ",tune_freq + mult_delta_Hz);

         //
         // Take sweep, and acquire 16-bit binary data from display trace A
         //

         static U16 curve[701];     

         S32 result = 0;

         GPIB_printf("S2");     // Clear status byte and start sweep  
         GPIB_printf("SR");     // (must be on separate lines)

         GPIB_auto_read_mode(FALSE);

         do                     // Wait for end of sweep
            {
            result = GPIB_serial_poll();

            if (interruptable_sleep(30))
               {
               SAL_alert_box("Error","Sweep aborted by keypress");

               GPIB_auto_read_mode(TRUE);
               EXIT_R3261(clip, carrier + mult_delta_Hz);
               return FALSE;
               }
            }
         while (!result);

         GPIB_auto_read_mode(TRUE);

         GPIB_printf("DL2");    // No CR/LF termination, use EOI only
         GPIB_printf("TBA?");   // Request binary trace data transfer

         memset(curve, 0, 701 * sizeof(S16));
         memcpy(curve, GPIB_read_BIN(701 * sizeof(S16),TRUE,FALSE), 701 * sizeof(S16));

         GPIB_printf("DL0");    // Switch back to CR/LF termination

         SINGLE offset      = SOURCE_max_dBm - range;
         SINGLE scale       = range / 400.0F;   // bottom=0, ref=400
         SINGLE grat_offset = 0.0F;

         DOUBLE df_dp = ((DOUBLE) total_span) / (DOUBLE) R3261_n_points;
         DOUBLE F     = ((DOUBLE) tune_freq) - ((DOUBLE) R3261_n_points / 2.0 * df_dp);

         for (i=0; i < 701; i++)     
            {
            curve[i] = ((curve[i] & 0xff00) >> 8) + ((curve[i] & 0x00ff) << 8);

            SINGLE dBm = ((SINGLE(curve[i]) + grat_offset) * scale) + offset; 

            trace_A[half][i] = dBm - ref;
            freq   [half][i] = F;

            F += df_dp;
            }

         //
         // Apply noise-normalization factor
         //
         // This is typically 2 dB greater than the baseline
         // 10*log(RBW) normalization factor due to filter
         // and detector noise-response characteristics
         // 

         SINGLE RB_factor = -10.0F * (SINGLE) log10((DOUBLE) SOURCE_RBW);

         correction[half] = RB_factor + NF_characteristic;

         tune_freq += next_decade;
         }

      //
      // Display results
      //

      SAL_debug_printf("\n%8d Hz: %.02f dBc/Hz\n",
         decade_start,
         trace_A[0][decile_n_points] + correction[0] + ext_mult_dB);

      SAL_debug_printf("%8d Hz: %.02f dBc/Hz / %.02f dBc/Hz\n\n",
         next_decade,
         trace_A[0][record_n_points-1]  + correction[0] + ext_mult_dB,
         trace_A[1][decile_n_points  ]  + correction[1] + ext_mult_dB);

      //
      // Send decade and overlap data to output file
      //

      fprintf(out,"DEC %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[0]);

      for (i=decile_n_points; i < record_n_points; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i - decile_n_points,
            freq[0][i],
            trace_A[0][i] + ext_mult_dB);
         }

      fprintf(out,"OVL %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[1]);

      for (i=0; i < record_n_points; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i,
            freq[1][i],
            trace_A[1][i] + ext_mult_dB);
         }
      }

   //
   // Write cumulative sweep time and EOF timestamp
   //

   S64 end = elapsed_uS();

   fprintf(out,"ELP %I64ds\n",((end - start) + 500000) / 1000000);
   fprintf(out,"EOF %s\n",timestamp());

   EXIT_R3261(clip, carrier + mult_delta_Hz);
   return TRUE;
}

//****************************************************************************
//*                                                                          *
//* Acquire data from TR417X-family spectrum analyzer                        *
//* Tested with TR4172                                                       *
//*                                                                          *
//****************************************************************************

void EXIT_TR417X(S32 clip, //)
                 S64 carrier_Hz)
{
   if (out != NULL)
      {
      fclose(out);
      out = NULL;
      }

   //
   // Put clipped peak back onscreen
   //

   for (S32 cp=0; cp < clip; cp += 10)
      {
      GPIB_printf("RL UP");
      }

   GPIB_printf("SRDR");                                       // Restore default <CR><LF><EOI> response delimiter
   GPIB_printf("IN");                                         // Continuous sweep
   GPIB_printf("BA");                                         // Autoselect RBW
   GPIB_printf("VA");                                         // Autoselect VBW
   GPIB_printf("SHAS");                                       // Normal (Rosenfell) detection
   GPIB_printf("CF %I64dHZ",carrier_Hz);                      // Set CF

   GPIB_disconnect();
}

BOOL ACQ_TR417X(C8    *filename,
                S32    GPIB_address,
                C8    *caption,
                S64    carrier,           
                S64    ext_IF,
                S64    ext_LO,
                SINGLE VBW_factor,
                DOUBLE ext_mult,          
                SINGLE NF_characteristic,
                SINGLE min_offset,        
                SINGLE max_offset,
                S32    clip,
                SINGLE ref = 10000.0F)
{            
   S32 i;

   if ((carrier < 0) || (ref == 10000.0F))
      {
      SAL_alert_box("Error","Must specify both carrier frequency and reference level");
      return FALSE;
      }

   //
   // Start up GPIB
   //

   GPIB_connect (GPIB_address,
                 GPIB_error,
                 0,
                 1000000);

   GPIB_set_EOS_mode(10);
   GPIB_set_serial_read_dropout(500);

   //
   // Open output file
   //

   out = fopen(filename,"wt");

   if (out == NULL)
      {
      GPIB_disconnect();

      SAL_alert_box("Error","Could not open %s",filename);
      return FALSE;
      }

   fprintf(out,";\n");
   fprintf(out,";Composite-noise plot file %s\n",filename);
   fprintf(out,";\n");
   fprintf(out,";Generated by PN.EXE V"VERSION" of "__DATE__"\n");
   fprintf(out,";by John Miles, KE5FX (john@miles.io)\n");
   fprintf(out,";\n\n");

   //
   // Log caption, if present
   //

   if (caption[0])
      {
      fprintf(out,"CAP %s\n",caption);
      }

   //
   // Disallow SRQs, clear status register
   // 

   GPIB_printf("SRDR");
   GPIB_flush_receive_buffers();    // if we don't do this, the queries below return one string behind

   //
   // Log timestamp and instrument ID
   //

   static C8 ID[1024] = "TR417X";
   static C8 REV[1024] = "N/A";

   fprintf(out,"TIM %s\n",timestamp());
   fprintf(out,"IMO %s\n", ID);
   fprintf(out,"IRV %s\n\n", REV);

   //
   // Use 1001 trace points
   //

   GPIB_printf("AW");
   S32 TR417X_n_points = 1001;

   S32 record_n_points = 100 * (TR417X_n_points / 100);
   S32 decile_n_points = record_n_points / 10;

   //
   // Log decade statistics
   //

   S32    min_decade = S32(log10(min_offset)+0.5);
   S32    max_decade = S32(log10(max_offset)+0.5) - 1;
   SINGLE ext_mult_dB = 0.0F;

   SAL_debug_printf("Minimum offset = %d Hz\n",S32(pow(10.0,min_decade)+0.5));
   SAL_debug_printf("Maximum offset = %d Hz\n",S32(pow(10.0,max_decade+1)+0.5));

   ext_mult_dB = -20.0F * (SINGLE) log10(ext_mult);

   SAL_debug_printf("External frequency multiplier = %.06lfx (%.02f dB)\n",
                     ext_mult,ext_mult_dB);

   fprintf(out,"DRG %d,%d\n",min_decade,max_decade);
   fprintf(out,"DCC %d\n",record_n_points - decile_n_points);
   fprintf(out,"OVC %d\n\n",record_n_points);

   fprintf(out,"EIF %I64d Hz\n",ext_IF);
   fprintf(out,"ELO %I64d Hz\n",ext_LO);
   fprintf(out,"MUL %lf\n\n",ext_mult);

   fprintf(out,"VBF %.02f\n",VBW_factor);
   fprintf(out,"CLP %d\n\n",clip);

   //
   // Set basic analyzer state needed for operation of program
   //

   GPIB_printf("NO");               // Normal measurement mode
   GPIB_printf("SI");               // Single-sweep mode
   GPIB_printf("MO");               // Turn off any marker-based functionality
   GPIB_printf("SH7");              // Need 10 dB/division log scale
   GPIB_printf("BA");               // Autoselect RBW
   GPIB_printf("VA");               // Autoselect VBW
   GPIB_printf("AS");               // Autoselect sweep time

   //
   // Perform actual measurement at external IF, if one is specified
   //

   S64 source_carrier = carrier;

   if (ext_IF >= 0)
      {
      carrier = ext_IF;
      }

   //
   // Log beginning of sweep series
   //

   S64 start = elapsed_uS();

   //
   // For each decade...
   //

   bool reference_level_set = FALSE;

   S64 mult_delta_Hz = S64(((DOUBLE) carrier * ext_mult) + 0.5) - carrier;  

   for (S32 d=min_decade; d <= max_decade; d++)
      {
      static SINGLE trace_A   [2][1001];
      static DOUBLE freq      [2][1001];
      SINGLE        correction[2];

      S32 decade_start = (S32) (pow(10.0, d) + 0.5F);
      S32 next_decade  = decade_start * 10;
      S32 total_span   = decade_start * 10;

      //
      // Select RBW and span width
      //
      // To avoid error messages, we need to set a wide RBW before we 
      // increase the span width from the previous decade.  (If we set the desired RBW
      // first, the analyzer would complain about the initial span being too wide,
      // and if we set the span width first, the analyer would complain about 
      // the RBW from the previous decade being too narrow)
      //

      S32 RBW = decade_start / 10;

      if (RBW >= 100000)
         RBW = 10000;      // 100 kHz RBW filter has excess noise (at least on R3465)

      S32 VBW = (S32) ((RBW * VBW_factor) + 0.5F);

      GPIB_printf("RB 1MZ");
      GPIB_printf("SP %dHZ",total_span); 

      if (RBW <= 7)
          GPIB_printf("SHLABA");    // select 7Hz mode
      else
          GPIB_printf("RB %dHZ",RBW);

      //
      // Set reference level, if it hasn't already been established, and
      // apply reference-level clipping as soon as we can
      //

      if (!reference_level_set)
         {
         reference_level_set = TRUE;

         if (ref-clip < 0)
            GPIB_printf("RE %fDM",clip-ref);
         else
            GPIB_printf("RE %fDP",ref-clip);
         }

      //
      // Enable video filtering, and select sample detection
      // for noise-like signals
      //

      GPIB_printf("SHCA");   // select sample detector
      GPIB_printf("VB %dHZ",VBW);

      S64 tune_freq = carrier + (decade_start * 5);

      //
      // Query actual reference level being used for this sweep
      //

      GPIB_flush_receive_buffers();

      SINGLE SOURCE_max_dBm = 10000.0F;

      while (SOURCE_max_dBm == 10000.0F)
         {
         C8 *RL_text = GPIB_query("REOA");
         sscanf(RL_text,"%f",&SOURCE_max_dBm);
         }

      //
      // Log some statistics used for this sweep
      //

      GPIB_flush_receive_buffers();    // AT? query is invalid under Prologix if we don't do this

      SINGLE att = 10000.0F;

      while (att == 10000.0F)
         {
         C8 *AT_text = GPIB_query("ATOA");
         sscanf(AT_text,"%f",&att);
         }

      SAL_debug_printf("Carrier level = %.02f dBm, RF atten = %d dB\n",ref,S32(att + 0.5F));

      fprintf(out,"CAR %I64d Hz, %.02f dBm\n",
         source_carrier,
         ref);

      fprintf(out,"ATT %d dB\n",
         S32(att + 0.5F));

      fprintf(out,"REF %f dBm\n", SOURCE_max_dBm);

      SINGLE SOURCE_RBW = -10000.0F;
      if(RBW <= 7)
          SOURCE_RBW = 7;     //FIXME

      while (SOURCE_RBW == -10000.0F)
         {
         C8 *RL_text = GPIB_query("RBOA");
         sscanf(RL_text,"%f",&SOURCE_RBW);
         }

      fprintf(out,"RBW %f Hz\n", SOURCE_RBW);

      SINGLE SOURCE_VBW = -10000.0F;

      while (SOURCE_VBW == -10000.0F)
         {
         C8 *RL_text = GPIB_query("VBOA");
         sscanf(RL_text,"%f",&SOURCE_VBW);
         }

      fprintf(out,"VBW %f Hz\n", SOURCE_VBW);

      for (S32 half=0; half < 2; half++)
         {
         SAL_serve_message_queue();

         //
         // Take a reading at each half-decade
         //

         SAL_debug_printf("Analyzing decade 10^%d, sweep %d of 2...\n",d,half+1);

         if (key_hit)
            {
            key_hit = FALSE;
            EXIT_TR417X(clip, carrier + mult_delta_Hz);

            SAL_alert_box("Error","Aborted by keypress");
            return FALSE;
            }

         //
         // Trigger sweep at specified center frequency, and request binary trace dump 
         // 

         GPIB_printf("CF %I64dHZ",tune_freq + mult_delta_Hz);
         //Sleep(2000);

         GPIB_printf("SQSIDR");
         GPIB_flush_receive_buffers();
         for(;;)    // wait for trace complete
            {
            C8 stat = GPIB_serial_poll();
            if(stat & 0x04)
               break;  // trace complete
            else
               Sleep(100);
            }
         GPIB_printf("RDC01807D2");

         static U32 curve[1001];     

         memset(curve, 0, TR417X_n_points * sizeof(U32));
         memcpy(curve, GPIB_read_BIN(TR417X_n_points * sizeof(U32),TRUE,FALSE), TR417X_n_points * sizeof(U32));

         GPIB_read_ASC();
         GPIB_printf("SRDR");
         GPIB_flush_receive_buffers();

         SINGLE range       = 100.0F;             // 100 dB display range
         SINGLE offset      = SOURCE_max_dBm - range;
         SINGLE scale       = range / 0x400;
         SINGLE grat_offset = 0.0F;

         DOUBLE df_dp = ((DOUBLE) total_span) / (DOUBLE) TR417X_n_points;
         DOUBLE F     = ((DOUBLE) tune_freq) - ((DOUBLE) TR417X_n_points / 2.0 * df_dp);

         for (i=0; i < TR417X_n_points; i++)     
            {
            C8 *pos = (C8*)(curve+i);
            int lo,hi;
            sscanf(pos, "%2x%2x", &lo, &hi);
            U16 point = lo+256*hi & 0x3FF;

            SINGLE dBm = ((SINGLE(point) + grat_offset) * scale) + offset; 

            trace_A[half][i] = dBm - ref;
            freq   [half][i] = F;

            F += df_dp;
            }

         //
         // Apply noise-normalization factor
         //
         // This is typically 2 dB greater than the baseline
         // 10*log(RBW) normalization factor due to filter
         // and detector noise-response characteristics
         // 

         SINGLE RB_factor = -10.0F * (SINGLE) log10((DOUBLE) SOURCE_RBW);

         correction[half] = RB_factor + NF_characteristic;

         tune_freq += next_decade;
         }

      //
      // Display results
      //

      SAL_debug_printf("\n%8d Hz: %.02f dBc/Hz\n",
         decade_start,
         trace_A[0][decile_n_points] + correction[0] + ext_mult_dB);

      SAL_debug_printf("%8d Hz: %.02f dBc/Hz / %.02f dBc/Hz\n\n",
         next_decade,
         trace_A[0][record_n_points-1]  + correction[0] + ext_mult_dB,
         trace_A[1][decile_n_points  ]  + correction[1] + ext_mult_dB);

      //
      // Send decade and overlap data to output file
      //

      fprintf(out,"DEC %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[0]);

      for (i=decile_n_points; i < record_n_points; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i - decile_n_points,
            freq[0][i],
            trace_A[0][i] + ext_mult_dB);
         }

      fprintf(out,"OVL %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[1]);

      for (i=0; i < record_n_points; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i,
            freq[1][i],
            trace_A[1][i] + ext_mult_dB);
         }
      }

   //
   // Write cumulative sweep time and EOF timestamp
   //

   S64 end = elapsed_uS();

   fprintf(out,"ELP %I64ds\n",((end - start) + 500000) / 1000000);
   fprintf(out,"EOF %s\n",timestamp());

   EXIT_TR417X(clip, carrier + mult_delta_Hz);
   return TRUE;
}

//****************************************************************************
//*                                                                          *
//* Acquire data from Anritsu MS8604A or MS8608A, may work with others       *
//*                                                                          *
//* Command set similar to HP 70000 series, but with IEEE 488.2 commands     *
//* and no reference-offset feature                                          *
//*                                                                          *
//****************************************************************************

void EXIT_MS8604A(S32 clip, //)
                  S64 carrier_Hz)
{
   if (out != NULL)
      {
      fclose(out);
      out = NULL;
      }

   //
   // Put clipped peak back onscreen
   //

   for (S32 cp=0; cp < clip; cp += 10)
      {
      GPIB_cmd_printf("RL UP;*OPC?");
      }

   GPIB_cmd_printf("MKOFF ALL;*OPC?");               // No marker
   GPIB_cmd_printf("CONTS;*OPC?");                   // Return to continuous-sweep mode
   GPIB_cmd_printf("RB AUTO;*OPC?");                 // Autoselect RBW
   GPIB_cmd_printf("VB AUTO;*OPC?");                 // Autoselect VBW
   GPIB_cmd_printf("DET 0;*OPC?");                   // Positive-peak detection (power-up default)
   GPIB_cmd_printf("CF %I64dHZ;*OPC?",carrier_Hz);   // Restore CF

   GPIB_disconnect();
}

BOOL ACQ_MS8604A(C8    *filename,
                 S32    GPIB_address,
                 C8    *caption,
                 S64    carrier,           
                 S64    ext_IF,
                 S64    ext_LO,
                 SINGLE VBW_factor,
                 DOUBLE ext_mult,          
                 SINGLE NF_characteristic,
                 SINGLE min_offset,        
                 SINGLE max_offset,
                 S32    clip,
                 SINGLE ref = 10000.0F)
{
   S32 i;

   S32    min_decade = S32(log10(min_offset)+0.5);
   S32    max_decade = S32(log10(max_offset)+0.5) - 1;
   SINGLE ext_mult_dB = 0.0F;

   SAL_debug_printf("Minimum offset = %d Hz\n",S32(pow(10.0,min_decade)+0.5));
   SAL_debug_printf("Maximum offset = %d Hz\n",S32(pow(10.0,max_decade+1)+0.5));

   ext_mult_dB = -20.0F * (SINGLE) log10(ext_mult);

   SAL_debug_printf("External frequency multiplier = %.06lfx (%.02f dB)\n",
                     ext_mult,ext_mult_dB);

   //
   // Start up GPIB
   //

   GPIB_connect (GPIB_address,
                 GPIB_error,
                 0,
                 1000000);

   GPIB_set_EOS_mode(10);
   GPIB_set_serial_read_dropout(500);

   //
   // Open output file
   //

   out = fopen(filename,"wt");

   if (out == NULL)
      {
      GPIB_disconnect();

      SAL_alert_box("Error","Could not open %s\n",filename);
      return FALSE;
      }

   fprintf(out,";\n");
   fprintf(out,";Composite-noise plot file %s\n",filename);
   fprintf(out,";\n");
   fprintf(out,";Generated by PN.EXE V"VERSION" of "__DATE__"\n");
   fprintf(out,";by John Miles, KE5FX (john@miles.io)\n");
   fprintf(out,";\n\n");

   //
   // Log caption, if present
   //

   if (caption[0])
      {
      fprintf(out,"CAP %s\n",caption);
      }

   //
   // Log timestamp and instrument ID
   //

   static C8 ID[1024];
   static C8 REV[1024];

   strcpy(ID,GPIB_query("*IDN?"));
   strcpy(REV,"N/A");

   kill_trailing_whitespace(ID);
   kill_trailing_whitespace(REV);

   fprintf(out,"TIM %s\n",timestamp());
   fprintf(out,"IMO %s\n", ID);
   fprintf(out,"IRV %s\n\n", REV);

   //
   // Get # of trace points (rounding down to nearest 100 for logging
   // purposes)
   //

   S32 record_n_points;

   if (strstr(GPIB_query("DPOINT?"),"DOUBLE") != NULL)
      record_n_points = 1000;
   else
      record_n_points = 500;

   S32 decile_n_points = record_n_points / 10;

   //
   // Log decade statistics
   //

   fprintf(out,"DRG %d,%d\n",min_decade,max_decade);
   fprintf(out,"DCC %d\n",record_n_points - decile_n_points);
   fprintf(out,"OVC %d\n\n",record_n_points);

   fprintf(out,"EIF %I64d Hz\n",ext_IF);
   fprintf(out,"ELO %I64d Hz\n",ext_LO);
   fprintf(out,"MUL %lf\n\n",ext_mult);

   fprintf(out,"VBF %.02f\n",VBW_factor);
   fprintf(out,"CLP %d\n\n",clip);

   //
   // Set basic analyzer state needed for operation of program
   //

   GPIB_cmd_printf("SNGLS;*OPC?");     // Select single-sweep mode
   GPIB_cmd_printf("MKOFF ALL;*OPC?"); // No markers by default
   GPIB_cmd_printf("LG 10DB;*OPC?");   // Need 10 dB/division log scale
   GPIB_cmd_printf("RB AUTO;*OPC?");   // Autoselect RBW
   GPIB_cmd_printf("VB AUTO;*OPC?");   // Autoselect VBW
   GPIB_cmd_printf("ST AUTO;*OPC?");   // Autoselect sweep time
   GPIB_cmd_printf("LSS 10;*OPC?");    // Reference level up/down increment = 10 dB
   GPIB_cmd_printf("BIN 0;*OPC?");     // ASCII queries

   //
   // If no carrier specified, find the strongest signal in the 
   // currently-selected view
   //
   // This routine will generally land within 10-20 Hz.  We round
   // the result to the nearest minimum-decade multiple
   //

   if (carrier < 0)
      {
      SAL_debug_printf("Measuring carrier frequency... ");
      assert(ext_LO == -1);

      //
      // Zero in on signal, dividing span by 10 at each step until span width is <= 5 kHz
      //

      while (1)
         {
         GPIB_cmd_printf("TS;*OPC?");        // Take sweep
         GPIB_cmd_printf("MKPK HI;*OPC?");   // Put marker on highest signal
         GPIB_cmd_printf("MKCF;*OPC?");      // Marker -> center frequency

         DOUBLE span = -10000.0;

         while (span == -10000.0)
            {
            C8 *span_text = GPIB_query("SP?");
            sscanf(span_text,"%lf",&span); 
            }

         if (span <= 5000.0)
            {
            break;
            }

         span /= 10.0;              // Select next-narrower span and repeat measurement  
         GPIB_cmd_printf("SP %lfHZ;*OPC?",span);
         }

      GPIB_flush_receive_buffers();

      DOUBLE C = -10000.0;

      while (C == -10000.0)         // Query final marker frequency
         {
         C8 *CF_text = GPIB_query("MKF?");
         sscanf(CF_text,"%lf",&C); 
         }

      if (C < 0.0F)
         {
         C = -C;
         }

      GPIB_cmd_printf("MKOFF ALL;*OPC?");    // Kill marker

      carrier = S64(C + 0.5);

      S64 decade_start = (S64) (pow(10.0, min_decade) + 0.5F);

      carrier = ((carrier + (decade_start / 2)) / decade_start) * decade_start;

      carrier = S64(((DOUBLE) carrier / ext_mult) + 0.5);  
      
      SAL_debug_printf("Result = %I64d Hz\n",carrier);
      }

   //
   // Perform actual measurement at external IF, if one is specified
   //

   S64 source_carrier = carrier;

   if (ext_IF >= 0)
      {
      carrier = ext_IF;
      }

   //
   // Log beginning of sweep series
   //

   S64 start = elapsed_uS();

   //
   // For each decade...
   //

   bool reference_level_set = FALSE;

   S64 mult_delta_Hz = S64(((DOUBLE) carrier * ext_mult) + 0.5) - carrier;  

   for (S32 d=min_decade; d <= max_decade; d++)
      {
      static SINGLE trace_A   [2][1024];
      static DOUBLE freq      [2][1024];
      SINGLE        correction[2];

      S32 decade_start = (S32) (pow(10.0, d) + 0.5F);
      S32 next_decade  = decade_start * 10;
      S32 total_span   = decade_start * 10;

      //
      // Select RBW and span
      //

      S32 RBW = decade_start / 10;
      S32 VBW = (S32) ((RBW * VBW_factor) + 0.5F);

      GPIB_cmd_printf("MKOFF ALL;*OPC?");                // Kill marker

      GPIB_cmd_printf("SP %dHZ;*OPC?",total_span);       // Set total span
      GPIB_cmd_printf("RB %dHZ;*OPC?",RBW);              // Set RBW

      //
      // If reference level not explicitly specified, establish fullscreen level 
      // using positive-peak detector and no additional video filtering
      //

      if (!reference_level_set)
         {
         reference_level_set = TRUE;

         if (ref == 10000.0F)
            {
            //
            // Tune to specified center frequency and establish fullscreen 
            // reference level using positive-peak detector and no 
            // additional video filtering
            //
            // Start by declaring a high reference level that is guaranteed to
            // place the signal peak onscreen
            //

            if (VBW > RBW)
               {
               GPIB_cmd_printf("VB %dHZ;*OPC?",VBW);  // Set specified video BW
               }
            else
               {
               GPIB_cmd_printf("VBR 1;*OPC?");        // Set 1:1 VBW/RBW ratio
               }

            GPIB_cmd_printf("AUNITS DBM;*OPC?");      // Use dBm units
            GPIB_cmd_printf("DET 0;*OPC?");           // Positive-peak detection

            GPIB_cmd_printf("RL 30DBM;*OPC?");        // +30 dBm reference level

            //
            // Tune to specified center frequency and take sweep
            //

            GPIB_cmd_printf("CF %I64dHZ;*OPC?",carrier + mult_delta_Hz);   // Set CF
            GPIB_cmd_printf("TS;*OPC?");                                   // Take sweep
            GPIB_cmd_printf("MKPK HI;*OPC?");                              // Put marker on highest signal
            GPIB_cmd_printf("MKRL;*OPC?");                                 // Set reference level = marker amplitude

            //
            // Query reference level and use it as the offset 
            // for subsequent dBm/Hz measurements
            //

            while (ref == 10000.0F)
               {
               C8 *RL_text = GPIB_query("RL?");
               sscanf(RL_text,"%f",&ref); 
               }
            }

         //
         // Set reference level and apply clipping if requested
         //
         // MS8604A returns readings directly in dBm, so we don't need to read 
         // the RL back for verification
         //

         GPIB_cmd_printf("RL %fDM;*OPC?",ref);

         for (S32 cp=0; cp < clip; cp += 10)
            {
            GPIB_cmd_printf("RL DN;*OPC?");
            }
         }

      //
      // Enable video filtering
      //

      GPIB_cmd_printf("MKOFF ALL;*OPC?");       // No markers
      GPIB_cmd_printf("DET 1;*OPC?");           // We use sampled detection for noise-like signals...

      GPIB_cmd_printf("VB %dHZ;*OPC?",VBW);     // Set video bandwidth

      S64 tune_freq = carrier + (decade_start * 5);

      //
      // Log RF attenuation used for this sweep
      //

      SINGLE att = 10000.0F;

      while (att == 10000.0F)
         {
         C8 *AT_text = GPIB_query("AT?");
         sscanf(AT_text,"%f",&att);
         }

      SAL_debug_printf("Carrier level = %.02f dBm, RF atten = %d dB\n",ref,S32(att + 0.5F));

      //
      // Log carrier amplitude used for this sweep
      //

      fprintf(out,"CAR %I64d Hz, %.02f dBm\n",
         source_carrier,
         ref);

      fprintf(out,"ATT %d dB\n",
         S32(att + 0.5F));

      for (S32 half=0; half < 2; half++)
         {
         SAL_serve_message_queue();

         //
         // Take a reading at each half-decade
         //

         SAL_debug_printf("Analyzing decade 10^%d, sweep %d of 2...\n",d,half+1);

         if (key_hit)
            {
            key_hit = FALSE;
            EXIT_MS8604A(clip, carrier + mult_delta_Hz);

            SAL_alert_box("Error","Aborted by keypress");
            return FALSE;
            }

         GPIB_cmd_printf("CF %I64dHZ;*OPC?",tune_freq + mult_delta_Hz);    // Set center freq

         //
         // Fetch data from display trace A
         // MS8604A data is comma-separated rather than CR/LF separated
         //

         GPIB_set_serial_read_dropout(2000);

         GPIB_printf("TS;XMA? 0,%d",record_n_points);     // Request ASCII dBm values from trace A

         static C8 input_line[32768];
         input_line[0] = 0;

         while (1)
            {
            C8 *read_result = GPIB_read_ASC(1024, TRUE);

            if ((read_result == NULL) || (read_result[0] == 0))
               {
               EXIT_MS8604A(clip, carrier + mult_delta_Hz);

               SAL_alert_box("Error","Sweep read terminated prematurely\n");
               return FALSE;
               }

            strcat(input_line, read_result);

            if ((strchr(read_result, 10) != NULL) ||
                (strlen(read_result) != 1024)     ||
                (strlen(input_line) >= 32767-1024))
               {
               break;
               }
            }

         GPIB_set_serial_read_dropout(500);

         //
         // Parse input line
         // 

         DOUBLE df_dp = ((DOUBLE) total_span) / record_n_points;
         DOUBLE F     = ((DOUBLE) tune_freq) - ((DOUBLE) (record_n_points / 2) * df_dp);

         C8 *ptr = input_line;
         i = 0;

         while (1)
            {
            S32 val = 0;
            sscanf(ptr,"%d",&val);

            trace_A[half][i] = (SINGLE) val / 100.0F;

            freq[half][i] = F;
            F += df_dp;

            i++;
            if (i == record_n_points)
               {
               break;
               }

            ptr = strchr(ptr,',');

            if (ptr == NULL)
               {
               EXIT_MS8604A(clip, carrier + mult_delta_Hz);

               SAL_alert_box("Error","Input line terminated prematurely at column %d, offset %d\n",i,ptr-input_line);
               return FALSE;
               }

            ptr++;
            }

         //
         // Apply noise-normalization factor
         //
         // This is typically 2 dB greater than the baseline
         // 10*log(RBW) normalization factor due to filter
         // and detector noise-response characteristics
         // 

         SINGLE RB_factor = -10.0F * (SINGLE) log10((DOUBLE) RBW);

         correction[half] = RB_factor + NF_characteristic;

         tune_freq += next_decade;
         }

      //
      // Display results
      //

      SAL_debug_printf("\n%8d Hz: %.02f dBc/Hz\n",
         decade_start,
         trace_A[0][decile_n_points] + correction[0] + ext_mult_dB);

      SAL_debug_printf("%8d Hz: %.02f dBc/Hz / %.02f dBc/Hz\n\n",
         next_decade,
         trace_A[0][record_n_points-1]  + correction[0] + ext_mult_dB,
         trace_A[1][decile_n_points  ]  + correction[1] + ext_mult_dB);

      //
      // Send decade and overlap data to output file
      //

      fprintf(out,"DEC %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[0]);

      for (i=decile_n_points; i < record_n_points; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i - decile_n_points,
            freq[0][i],
            trace_A[0][i] + ext_mult_dB);
         }

      fprintf(out,"OVL %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[1]);

      for (i=0; i < record_n_points; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i,
            freq[1][i],
            trace_A[1][i] + ext_mult_dB);
         }
      }

   //
   // Write cumulative sweep time and EOF timestamp
   //

   S64 end = elapsed_uS();

   fprintf(out,"ELP %I64ds\n",((end - start) + 500000) / 1000000);
   fprintf(out,"EOF %s\n",timestamp());

   EXIT_MS8604A(clip, carrier + mult_delta_Hz);
   return TRUE;
}

//****************************************************************************
//*                                                                          *
//* Acquire data from Anritsu MS2650/MS2660 series                           *
//*                                                                          *
//* Command set similar to HP 70000 series, but with IEEE 488.2 commands     *
//*                                                                          *
//****************************************************************************

void EXIT_MS2650(S32 clip, //)
                 S64 carrier_Hz)
{
   if (out != NULL)
      {
      fclose(out);
      out = NULL;
      }

   //
   // Put clipped peak back onscreen
   //

   for (S32 cp=0; cp < clip; cp += 10)
      {
      GPIB_cmd_printf("RL UP;*OPC?");
      }

   GPIB_cmd_printf("MKOFF ALL;*OPC?");               // No marker
   GPIB_cmd_printf("CONTS;*OPC?");                   // Return to continuous-sweep mode
   GPIB_cmd_printf("RB AUTO;*OPC?");                 // Autoselect RBW
   GPIB_cmd_printf("VB AUTO;*OPC?");                 // Autoselect VBW
   GPIB_cmd_printf("DET 0;*OPC?");                   // Positive-peak detection (power-up default)
   GPIB_cmd_printf("CF %I64dHZ;*OPC?",carrier_Hz);   // Restore CF

   GPIB_disconnect();
}

BOOL ACQ_MS2650(C8    *filename,
                S32    GPIB_address,
                C8    *caption,
                S64    carrier,           
                S64    ext_IF,
                S64    ext_LO,
                SINGLE VBW_factor,
                DOUBLE ext_mult,          
                SINGLE NF_characteristic,
                SINGLE min_offset,        
                SINGLE max_offset,
                S32    clip,
                SINGLE ref = 10000.0F)
{
   S32 i;

   S32    min_decade = S32(log10(min_offset)+0.5);
   S32    max_decade = S32(log10(max_offset)+0.5) - 1;
   SINGLE ext_mult_dB = 0.0F;

   SAL_debug_printf("Minimum offset = %d Hz\n",S32(pow(10.0,min_decade)+0.5));
   SAL_debug_printf("Maximum offset = %d Hz\n",S32(pow(10.0,max_decade+1)+0.5));

   ext_mult_dB = -20.0F * (SINGLE) log10(ext_mult);

   SAL_debug_printf("External frequency multiplier = %.06lfx (%.02f dB)\n",
                     ext_mult,ext_mult_dB);

   //
   // Start up GPIB
   //

   GPIB_connect (GPIB_address,
                 GPIB_error,
                 0,
                 1000000);

   GPIB_set_EOS_mode(10);
   GPIB_set_serial_read_dropout(500);

   //
   // Open output file
   //

   out = fopen(filename,"wt");

   if (out == NULL)
      {
      GPIB_disconnect();

      SAL_alert_box("Error","Could not open %s\n",filename);
      return FALSE;
      }

   fprintf(out,";\n");
   fprintf(out,";Composite-noise plot file %s\n",filename);
   fprintf(out,";\n");
   fprintf(out,";Generated by PN.EXE V"VERSION" of "__DATE__"\n");
   fprintf(out,";by John Miles, KE5FX (john@miles.io)\n");
   fprintf(out,";\n\n");

   //
   // Log caption, if present
   //

   if (caption[0])
      {
      fprintf(out,"CAP %s\n",caption);
      }

   //
   // Log timestamp and instrument ID
   //

   static C8 ID[1024];
   static C8 REV[1024];

   strcpy(ID,GPIB_query("*IDN?"));
   strcpy(REV,"N/A");

   kill_trailing_whitespace(ID);
   kill_trailing_whitespace(REV);

   fprintf(out,"TIM %s\n",timestamp());
   fprintf(out,"IMO %s\n", ID);
   fprintf(out,"IRV %s\n\n", REV);

   //
   // Get # of trace points (rounding down to nearest 100 for logging
   // purposes)
   //

   S32 record_n_points = 500;
   S32 decile_n_points = record_n_points / 10;

   //
   // Log decade statistics
   //

   fprintf(out,"DRG %d,%d\n",min_decade,max_decade);
   fprintf(out,"DCC %d\n",record_n_points - decile_n_points);
   fprintf(out,"OVC %d\n\n",record_n_points);

   fprintf(out,"EIF %I64d Hz\n",ext_IF);
   fprintf(out,"ELO %I64d Hz\n",ext_LO);
   fprintf(out,"MUL %lf\n\n",ext_mult);

   fprintf(out,"VBF %.02f\n",VBW_factor);
   fprintf(out,"CLP %d\n\n",clip);

   //
   // Set basic analyzer state needed for operation of program
   //

   GPIB_cmd_printf("SNGLS;*OPC?");     // Select single-sweep mode
   GPIB_cmd_printf("MKOFF ALL;*OPC?"); // No markers by default
   GPIB_cmd_printf("LG 10DB;*OPC?");   // Need 10 dB/division log scale
   GPIB_cmd_printf("RB AUTO;*OPC?");   // Autoselect RBW
   GPIB_cmd_printf("VB AUTO;*OPC?");   // Autoselect VBW
   GPIB_cmd_printf("ST AUTO;*OPC?");   // Autoselect sweep time
   GPIB_cmd_printf("LSS 10;*OPC?");    // Reference level up/down increment = 10 dB
   GPIB_cmd_printf("BIN 0;*OPC?");     // ASCII queries

   //
   // If no carrier specified, find the strongest signal in the 
   // currently-selected view
   //
   // This routine will generally land within 10-20 Hz.  We round
   // the result to the nearest minimum-decade multiple
   //

   if (carrier < 0)
      {
      SAL_debug_printf("Measuring carrier frequency... ");
      assert(ext_LO == -1);

      //
      // Zero in on signal, dividing span by 10 at each step until span width is <= 5 kHz
      //

      while (1)
         {
         GPIB_cmd_printf("TS;*OPC?");        // Take sweep
         GPIB_cmd_printf("MKPK HI;*OPC?");   // Put marker on highest signal
         GPIB_cmd_printf("MKCF;*OPC?");      // Marker -> center frequency

         DOUBLE span = -10000.0;

         while (span == -10000.0)
            {
            C8 *span_text = GPIB_query("SP?");
            sscanf(span_text,"%lf",&span); 
            }

         if (span <= 5000.0)
            {
            break;
            }

         span /= 10.0;              // Select next-narrower span and repeat measurement  
         GPIB_cmd_printf("SP %lfHZ;*OPC?",span);
         }

      GPIB_flush_receive_buffers();

      DOUBLE C = -10000.0;

      while (C == -10000.0)         // Query final marker frequency
         {
         C8 *CF_text = GPIB_query("MKF?");
         sscanf(CF_text,"%lf",&C); 
         }

      if (C < 0.0F)
         {
         C = -C;
         }

      GPIB_cmd_printf("MKOFF ALL;*OPC?");    // Kill marker

      carrier = S64(C + 0.5);

      S64 decade_start = (S64) (pow(10.0, min_decade) + 0.5F);

      carrier = ((carrier + (decade_start / 2)) / decade_start) * decade_start;

      carrier = S64(((DOUBLE) carrier / ext_mult) + 0.5);  
      
      SAL_debug_printf("Result = %I64d Hz\n",carrier);
      }

   //
   // Perform actual measurement at external IF, if one is specified
   //

   S64 source_carrier = carrier;

   if (ext_IF >= 0)
      {
      carrier = ext_IF;
      }

   //
   // Log beginning of sweep series
   //

   S64 start = elapsed_uS();

   //
   // For each decade...
   //

   bool reference_level_set = FALSE;

   S64 mult_delta_Hz = S64(((DOUBLE) carrier * ext_mult) + 0.5) - carrier;  

   for (S32 d=min_decade; d <= max_decade; d++)
      {
      static SINGLE trace_A   [2][1024];
      static DOUBLE freq      [2][1024];
      SINGLE        correction[2];

      S32 decade_start = (S32) (pow(10.0, d) + 0.5F);
      S32 next_decade  = decade_start * 10;
      S32 total_span   = decade_start * 10;

      //
      // Select RBW and span
      //

      S32 RBW = decade_start / 10;

      if (RBW == 10)
         {
         RBW = 30;         // Allow use of 100-1000 Hz decade with optional 30 Hz filter
         }

      S32 VBW = (S32) ((RBW * VBW_factor) + 0.5F);

      GPIB_cmd_printf("MKOFF ALL;*OPC?");                // Kill marker

      GPIB_cmd_printf("SP %dHZ;*OPC?",total_span);       // Set total span
      GPIB_cmd_printf("RB %dHZ;*OPC?",RBW);              // Set RBW

      //
      // If reference level not explicitly specified, establish fullscreen level 
      // using positive-peak detector and no additional video filtering
      //

      if (!reference_level_set)
         {
         reference_level_set = TRUE;

         if (ref == 10000.0F)
            {
            //
            // Tune to specified center frequency and establish fullscreen 
            // reference level using positive-peak detector and no 
            // additional video filtering
            //
            // Start by declaring a high reference level that is guaranteed to
            // place the signal peak onscreen
            //

            if (VBW > RBW)
               {
               GPIB_cmd_printf("VB %dHZ;*OPC?",VBW);  // Set specified video BW
               }
            else
               {
               GPIB_cmd_printf("VBR 1;*OPC?");        // Set 1:1 VBW/RBW ratio
               }

            GPIB_cmd_printf("AUNITS DBM;*OPC?");      // Use dBm units
            GPIB_cmd_printf("DET 0;*OPC?");           // Positive-peak detection

            GPIB_cmd_printf("RL 30DBM;*OPC?");        // +30 dBm reference level

            //
            // Tune to specified center frequency and take sweep
            //

            GPIB_cmd_printf("CF %I64dHZ;*OPC?",carrier + mult_delta_Hz);   // Set CF
            GPIB_cmd_printf("TS;*OPC?");                                   // Take sweep
            GPIB_cmd_printf("MKPK HI;*OPC?");                              // Put marker on highest signal
            GPIB_cmd_printf("MKRL;*OPC?");                                 // Set reference level = marker amplitude

            //
            // Query reference level and use it as the offset 
            // for subsequent dBm/Hz measurements
            //

            while (ref == 10000.0F)
               {
               C8 *RL_text = GPIB_query("RL?");
               sscanf(RL_text,"%f",&ref); 
               }
            }

         //
         // Set reference level and apply clipping if requested
         //
         // MS2650 returns readings directly in dBm, so we don't need to read 
         // the RL back for verification
         //

         GPIB_cmd_printf("RL %fDM;*OPC?",ref);

         for (S32 cp=0; cp < clip; cp += 10)
            {
            GPIB_cmd_printf("RL DN;*OPC?");
            }
         }

      //
      // Enable video filtering
      //

      GPIB_cmd_printf("MKOFF ALL;*OPC?");       // No markers
      GPIB_cmd_printf("DET 1;*OPC?");           // We use sampled detection for noise-like signals...

      GPIB_cmd_printf("VB %dHZ;*OPC?",VBW);     // Set video bandwidth

      S64 tune_freq = carrier + (decade_start * 5);

      //
      // Log RF attenuation used for this sweep
      //

      SINGLE att = 10000.0F;

      while (att == 10000.0F)
         {
         C8 *AT_text = GPIB_query("AT?");
         sscanf(AT_text,"%f",&att);
         }

      SAL_debug_printf("Carrier level = %.02f dBm, RF atten = %d dB\n",ref,S32(att + 0.5F));

      //
      // Log carrier amplitude used for this sweep
      //

      fprintf(out,"CAR %I64d Hz, %.02f dBm\n",
         source_carrier,
         ref);

      fprintf(out,"ATT %d dB\n",
         S32(att + 0.5F));

      for (S32 half=0; half < 2; half++)
         {
         SAL_serve_message_queue();

         //
         // Take a reading at each half-decade
         //

         SAL_debug_printf("Analyzing decade 10^%d, sweep %d of 2...\n",d,half+1);

         if (key_hit)
            {
            key_hit = FALSE;
            EXIT_MS2650(clip, carrier + mult_delta_Hz);

            SAL_alert_box("Error","Aborted by keypress");
            return FALSE;
            }

         GPIB_cmd_printf("CF %I64dHZ;*OPC?",tune_freq + mult_delta_Hz);    // Set center freq

         //
         // Fetch data from display trace A
         // MS2650 data is comma-separated rather than CR/LF separated
         //

         GPIB_set_serial_read_dropout(2000);

         GPIB_printf("TS;XMA? 0,%d",record_n_points);     // Request ASCII dBm values from trace A

         static C8 input_line[32768];
         input_line[0] = 0;

         while (1)
            {
            C8 *read_result = GPIB_read_ASC(1024, TRUE);

            if ((read_result == NULL) || (read_result[0] == 0))
               {
               EXIT_MS2650(clip, carrier + mult_delta_Hz);

               SAL_alert_box("Error","Sweep read terminated prematurely\n");
               return FALSE;
               }

            strcat(input_line, read_result);

            if ((strchr(read_result, 10) != NULL) ||
                (strlen(read_result) != 1024)     ||
                (strlen(input_line) >= 32767-1024))
               {
               break;
               }
            }

         GPIB_set_serial_read_dropout(500);

         //
         // Parse input line
         // 

         DOUBLE df_dp = ((DOUBLE) total_span) / record_n_points;
         DOUBLE F     = ((DOUBLE) tune_freq) - ((DOUBLE) (record_n_points / 2) * df_dp);

         C8 *ptr = input_line;
         i = 0;

         while (1)
            {
            S32 val = 0;
            sscanf(ptr,"%d",&val);

            trace_A[half][i] = (SINGLE) val / 100.0F;

            freq[half][i] = F;
            F += df_dp;

            i++;
            if (i == record_n_points)
               {
               break;
               }

            ptr = strchr(ptr,',');

            if (ptr == NULL)
               {
               EXIT_MS2650(clip, carrier + mult_delta_Hz);

               SAL_alert_box("Error","Input line terminated prematurely at column %d, offset %d\n",i,ptr-input_line);
               return FALSE;
               }

            ptr++;
            }

         //
         // Apply noise-normalization factor
         //
         // This is typically 2 dB greater than the baseline
         // 10*log(RBW) normalization factor due to filter
         // and detector noise-response characteristics
         // 

         SINGLE RB_factor = -10.0F * (SINGLE) log10((DOUBLE) RBW);

         correction[half] = RB_factor + NF_characteristic;

         tune_freq += next_decade;
         }

      //
      // Display results
      //

      SAL_debug_printf("\n%8d Hz: %.02f dBc/Hz\n",
         decade_start,
         trace_A[0][decile_n_points] + correction[0] + ext_mult_dB);

      SAL_debug_printf("%8d Hz: %.02f dBc/Hz / %.02f dBc/Hz\n\n",
         next_decade,
         trace_A[0][record_n_points-1]  + correction[0] + ext_mult_dB,
         trace_A[1][decile_n_points  ]  + correction[1] + ext_mult_dB);

      //
      // Send decade and overlap data to output file
      //

      fprintf(out,"DEC %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[0]);

      for (i=decile_n_points; i < record_n_points; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i - decile_n_points,
            freq[0][i],
            trace_A[0][i] + ext_mult_dB);
         }

      fprintf(out,"OVL %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[1]);

      for (i=0; i < record_n_points; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i,
            freq[1][i],
            trace_A[1][i] + ext_mult_dB);
         }
      }

   //
   // Write cumulative sweep time and EOF timestamp
   //

   S64 end = elapsed_uS();

   fprintf(out,"ELP %I64ds\n",((end - start) + 500000) / 1000000);
   fprintf(out,"EOF %s\n",timestamp());

   EXIT_MS2650(clip, carrier + mult_delta_Hz);
   return TRUE;
}

//****************************************************************************
//*                                                                          *
//* Acquire data from FSIQ/FSP/FSQ/FSU-compatible spectrum analyzer          *
//* Tested with FSIQ 3, FSEA, FSQ/8, FSP, and FSU                            *
//*                                                                          *
//****************************************************************************

void EXIT_FSIQ(S32 clip, //)
               S64 carrier_Hz,
               S32 dig_1kHz,
               S32 FFT_filters)
{
   if (out != NULL)
      {
      fclose(out);
      out = NULL;
      }

   //
   // Put clipped peak back onscreen
   //

   for (S32 cp=0; cp < clip; cp += 10)
      {
      GPIB_query(":DISP:WIND:TRAC:Y:RLEV UP;*OPC?");
      }

   GPIB_query(":INIT:CONT 1;*OPC?");                                    // Return to continuous-sweep mode 

   if (FFT_filters != -1)
      {
      GPIB_printf(":SENS:BAND:MODE:FFT %d;*OPC?", FFT_filters);         // Restore FFT/swept filter mode
      GPIB_read_ASC();
      }

   GPIB_printf(":SENS:BAND:MODE %s;*OPC?", dig_1kHz ? "DIG" : "ANAL");  // Restore 1-kHz bandwidth filter mode
   GPIB_read_ASC();

   GPIB_query(":SENS:BAND:RES:AUTO ON;*OPC?");                          // Autoselect RBW
   GPIB_query(":SENS:BAND:VID:AUTO ON;*OPC?");                          // Autoselect VBW
   GPIB_query(":DISP:WIND:TRAC:Y:RLEV:OFFS 0;*OPC?");                   // No reference offset
   GPIB_query(":SENS:DET:AUTO ON;*OPC?");                               // Couple detector to trace setting

   GPIB_printf(":SENS:FREQ:CENT %I64d HZ;*OPC?",carrier_Hz);            // Set CF
   GPIB_read_ASC();

   GPIB_disconnect();
}

BOOL ACQ_FSIQ(C8    *filename,
              S32    GPIB_address,
              C8    *caption,
              S64    carrier,           
              S64    ext_IF,
              S64    ext_LO,
              SINGLE VBW_factor,
              DOUBLE ext_mult,          
              SINGLE NF_characteristic,
              SINGLE min_offset,        
              SINGLE max_offset,
              S32    clip,
              SINGLE ref,
              S32    use_FFT,
              S32    n_points,
              S32    force_0dB)
{
   S32 i;

   if ((carrier < 0) || (ref == 10000.0F))
      {
      SAL_alert_box("Error","Must specify both carrier frequency and reference level");
      return FALSE;
      }

   S32    min_decade = S32(log10(min_offset)+0.5);
   S32    max_decade = S32(log10(max_offset)+0.5) - 1;
   SINGLE ext_mult_dB = 0.0F;

   SAL_debug_printf("Minimum offset = %d Hz\n",S32(pow(10.0,min_decade)+0.5));
   SAL_debug_printf("Maximum offset = %d Hz\n",S32(pow(10.0,max_decade+1)+0.5));

   ext_mult_dB = -20.0F * (SINGLE) log10(ext_mult);

   SAL_debug_printf("External frequency multiplier = %.06lfx (%.02f dB)\n",
                     ext_mult,ext_mult_dB);

   //
   // Start up GPIB
   //

   GPIB_connect (GPIB_address,
                 GPIB_error,
                 0,
                 1000000);

   GPIB_set_EOS_mode(10);
   GPIB_set_serial_read_dropout(500);

   //
   // Open output file
   //

   out = fopen(filename,"wt");

   if (out == NULL)
      {
      GPIB_disconnect();

      SAL_alert_box("Error","Could not open %s",filename);
      return FALSE;
      }

   fprintf(out,";\n");
   fprintf(out,";Composite-noise plot file %s\n",filename);
   fprintf(out,";\n");
   fprintf(out,";Generated by PN.EXE V"VERSION" of "__DATE__"\n");
   fprintf(out,";by John Miles, KE5FX (john@miles.io)\n");
   fprintf(out,";\n\n");

   //
   // Log caption, if present
   //

   if (caption[0])
      {
      fprintf(out,"CAP %s\n",caption);
      }

   //
   // Disallow SRQs, clear error register
   //

   GPIB_query("*SRE 0;*CLS;*OPC?");

   //
   // Log timestamp and instrument ID
   //

   static C8 ID[1024];
   strcpy(ID,GPIB_query("*IDN?"));
   kill_trailing_whitespace(ID);

   C8 *mfg = strtok(ID,",");
   C8 *mod = strtok(NULL,",");
   C8 *rev = strtok(NULL,",");

   fprintf(out,"TIM %s\n",timestamp());

   if ((mfg == NULL) || (mod == NULL))
      fprintf(out,"IMO %s\n", (mfg == NULL) ? "N/A" : mfg);
   else
      fprintf(out,"IMO %s %s\n",mfg,mod);
      
   fprintf(out,"IRV %s\n\n", (rev == NULL) ? "N/A" : rev);

   GPIB_query(":SYST:DISP:UPD ON;*OPC?");             // Leave display active for diagnostic purposes

   if (strstr(mod,"FSP") != NULL)
      {
      GPIB_query(":SENS:SWE:POIN 501;*OPC?");         // FSP point count is configurable, but we only support the default
      }

   //
   // Get # of trace points (rounding down to nearest 100 for logging
   // purposes)
   //

   S32 record_n_points = 100 * (n_points / 100);
   S32 decile_n_points = record_n_points / 10;

   //
   // Log decade statistics
   //

   fprintf(out,"DRG %d,%d\n",min_decade,max_decade);
   fprintf(out,"DCC %d\n",record_n_points - decile_n_points);
   fprintf(out,"OVC %d\n\n",record_n_points);

   fprintf(out,"EIF %I64d Hz\n",ext_IF);
   fprintf(out,"ELO %I64d Hz\n",ext_LO);
   fprintf(out,"MUL %lf\n\n",ext_mult);

   fprintf(out,"VBF %.02f\n",VBW_factor);
   fprintf(out,"CLP %d\n\n",clip);

   //
   // Set basic analyzer state needed for operation of program
   //

   GPIB_query(":INIT:CONT 0;*OPC?");                  // Select single-sweep mode
   GPIB_query(":CALC:MARK:AOFF;*OPC?");               // Turn off any marker-based functionality
   GPIB_query(":DISP:WIND:TRAC:Y:SCAL 120 DB;*OPC?"); // Need 10 dB/division log scale
   GPIB_query(":SENS:BAND:RES:AUTO ON;*OPC?");        // Autoselect RBW
   GPIB_query(":SENS:BAND:VID:AUTO ON;*OPC?");        // Autoselect RBW
   GPIB_query(":SENS:SWEEP:TIME:AUTO ON;*OPC?");      // Autoselect sweep time

   //
   // Record FFT and digital/analog filter selections so they can be restored
   //

   S32 dig_1kHz    = FALSE;
   S32 FFT_filters = FALSE;

   C8 *text = GPIB_query(":SENS:BAND:MODE?");

   if (!_strnicmp(text,"DIG",3))
      {
      dig_1kHz = TRUE;
      }

   if (!use_FFT)
      {
      FFT_filters = -1;
      }
   else
      {
      text = GPIB_query(":SENS:BAND:MODE:FFT?");

      if (text[0] == '1')
         {
         FFT_filters = TRUE;
         }
      }

   //
   // If FFT filters are requested, enable them
   //
   // Otherwise, force use of analog filter at RBW 1 kHz (i.e., in 10 kHz-100 kHz span),
   // and swept digital filters below that
   //

   if (use_FFT)
      {
      GPIB_query(":SENS:BAND:MODE DIG;*OPC?");
      GPIB_query(":SENS:BAND:MODE:FFT 1;*OPC?");
      }
   else
      {
      GPIB_query(":SENS:BAND:MODE ANAL;*OPC?");
      }

   //
   // Request 32-bit little-endian IEEE754 real trace format
   //

   GPIB_query(":FORM:DATA REAL,32;*OPC?");

   //
   // Perform actual measurement at external IF, if one is specified
   //

   S64 source_carrier = carrier;

   if (ext_IF >= 0)
      {
      carrier = ext_IF;
      }

   //
   // Log beginning of sweep series
   //

   S64 start = elapsed_uS();

   //
   // For each decade...
   //

   bool reference_level_set = FALSE;

   S64 mult_delta_Hz = S64(((DOUBLE) carrier * ext_mult) + 0.5) - carrier;  

   for (S32 d=min_decade; d <= max_decade; d++)
      {
      static SINGLE trace_A   [2][MAX_SCPI_POINTS];
      static DOUBLE freq      [2][MAX_SCPI_POINTS];
      SINGLE        correction[2];

      S32 decade_start = (S32) (pow(10.0, d) + 0.5F);
      S32 next_decade  = decade_start * 10;
      S32 total_span   = decade_start * 10;

      //
      // Select RBW and span
      //

      S32 RBW = decade_start / 10;
      if (RBW < 1) RBW = 1;

      if (decade_start >= 100000) 
         {
         RBW = decade_start / 100;                            // Use 1/10 min offset for decades up to 100K-1M, 1/100 min offset afterward
         }

      S32 VBW = (S32) ((RBW * VBW_factor) + 0.5F);
      if (VBW < 1) VBW = 1;

      GPIB_printf(":SENS:FREQ:SPAN %d HZ;*OPC?",total_span);  // Set total span
      GPIB_read_ASC();

      GPIB_printf(":SENS:BAND:RES %d HZ;*OPC?",RBW);          // Set RBW
      GPIB_read_ASC();

      //
      // Set reference level, if it hasn't already been established
      //

      if (!reference_level_set)
         {
         reference_level_set = TRUE;

         GPIB_printf(":DISP:WIND:TRAC:Y:RLEV:OFFS %f;*OPC?",-ref);  // Establish reference offset
         GPIB_read_ASC();

         GPIB_query(":DISP:WIND:TRAC:Y:RLEV 0 DBM;*OPC?");          // Set relative reference level = 0 dBm 

         //
         // Apply reference-level clipping if requested
         //

         for (S32 cp=0; cp < clip; cp += 10)
            {
            GPIB_query(":DISP:WIND:TRAC:Y:RLEV DOWN;*OPC?");
            }

         if (force_0dB)
            {
            GPIB_printf(":INP:ATT 0DB;");
            }
         }

      //
      // Enable video filtering
      //

      GPIB_query(":SENS:DET:FUNC SAMP;*OPC?");          // We use sampled detection for noise-like signals... 

      GPIB_printf(":SENS:BAND:VID %d HZ;*OPC?",VBW);    // Set VBW
      GPIB_read_ASC();

      S64 tune_freq = carrier + (decade_start * 5);

      //
      // Log RF attenuation used for this sweep
      //

      SINGLE att = 10000.0F;

      while (att == 10000.0F)
         {
         C8 *AT_text = GPIB_query(":INP:ATT?");
         sscanf(AT_text,"%f",&att);
         }

      SAL_debug_printf("Carrier level = %.02f dBm, RF atten = %d dB\n",ref,S32(att + 0.5F));

      //
      // Log carrier amplitude used for this sweep
      //

      fprintf(out,"CAR %I64d Hz, %.02f dBm\n",
         source_carrier,
         ref);

      fprintf(out,"ATT %d dB\n",
         S32(att + 0.5F));

      for (S32 half=0; half < 2; half++)
         {
         SAL_serve_message_queue();

         //
         // Take a reading at each half-decade
         //

         SAL_debug_printf("Analyzing decade 10^%d, sweep %d of 2...\n",d,half+1);

         if (key_hit)
            {
            key_hit = FALSE;
            EXIT_FSIQ(clip, carrier + mult_delta_Hz, dig_1kHz, FFT_filters);

            SAL_alert_box("Error","Aborted by keypress");
            return FALSE;
            }

         GPIB_printf(":SENS:FREQ:CENT %I64d HZ;*OPC?",tune_freq + mult_delta_Hz);  // Set CF
         GPIB_read_ASC();

         //
         // Request trace dump
         //

         GPIB_printf(":ABOR;:INIT:IMM;*WAI;:TRAC:DATA? TRACE1");

         //
         // Read preamble, and verify that the data size
         // is what we expect
         //
   
         S8 *preamble = (S8 *) GPIB_read_BIN(2);
         assert(preamble[0] == '#');
         S32 n_preamble_bytes = preamble[1] - '0';
   
         C8 *size_text = GPIB_read_ASC(n_preamble_bytes);
         S32 received_points = atoi(size_text) / sizeof(S32);

         if (received_points != n_points)
            {
            SAL_alert_box("Fatal error", "Expected %d sweep points but received %d.\n\nPlease ensure that analyzer is configured for %d-point sweeps.",
               n_points, received_points, n_points);

            exit(1);
            }
   
         //
         // Acquire 32-bit real data from display trace A
         // 
         // Because we applied a reference-level offset equal to -ref dBm, 
         // these are dBc values, not dBm
         //
   
         static SINGLE curve[MAX_SCPI_POINTS];

         memset(curve, 
                0, 
                n_points * sizeof(SINGLE));
   
         memcpy(curve, 
                GPIB_read_BIN(n_points * sizeof(SINGLE),
                              TRUE,
                              FALSE), 
                n_points * sizeof(SINGLE));
   
         DOUBLE df_dp = ((DOUBLE) total_span) / (DOUBLE) n_points;
         DOUBLE F     = ((DOUBLE) tune_freq) - ((DOUBLE) n_points / 2.0 * df_dp);

         for (i=0; i < n_points; i++)     
            {
            trace_A[half][i] = curve[i];
            freq   [half][i] = F;
            F += df_dp;
            }

         //
         // Swallow trailing newline
         //
   
         GPIB_read_ASC();

         //
         // Apply noise-normalization factor
         //
         // This is typically 2 dB greater than the baseline
         // 10*log(RBW) normalization factor due to filter
         // and detector noise-response characteristics
         // 

         SINGLE RB_factor = -10.0F * (SINGLE) log10((DOUBLE) RBW);

         correction[half] = RB_factor + NF_characteristic;

         tune_freq += next_decade;
         }

      //
      // Display results
      //

      SAL_debug_printf("\n%8d Hz: %.02f dBc/Hz\n",
         decade_start,
         trace_A[0][decile_n_points] + correction[0] + ext_mult_dB);

      SAL_debug_printf("%8d Hz: %.02f dBc/Hz / %.02f dBc/Hz\n\n",
         next_decade,
         trace_A[0][record_n_points-1]  + correction[0] + ext_mult_dB,
         trace_A[1][decile_n_points  ]  + correction[1] + ext_mult_dB);

      //
      // Send decade and overlap data to output file
      //

      fprintf(out,"DEC %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[0]);

      for (i=decile_n_points; i < record_n_points; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i - decile_n_points,
            freq[0][i],
            trace_A[0][i] + ext_mult_dB);
         }

      fprintf(out,"OVL %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[1]);

      for (i=0; i < record_n_points; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i,
            freq[1][i],
            trace_A[1][i] + ext_mult_dB);
         }
      }

   //
   // Write cumulative sweep time and EOF timestamp
   //

   S64 end = elapsed_uS();

   fprintf(out,"ELP %I64ds\n",((end - start) + 500000) / 1000000);
   fprintf(out,"EOF %s\n",timestamp());

   EXIT_FSIQ(clip, carrier + mult_delta_Hz, dig_1kHz, FFT_filters);
   return TRUE;
}

//****************************************************************************
//*                                                                          *
//* Acquire data from SCPI-compatible E4400-series spectrum analyzer         *
//* Tested with Agilent E4402B                                               *
//*                                                                          *
//****************************************************************************

void EXIT_E4400(S32 clip, //)
                S64 carrier_Hz)
{
   if (out != NULL)
      {
      fclose(out);
      out = NULL;
      }

   //
   // Put clipped peak back onscreen
   //

   for (S32 cp=0; cp < clip; cp += 10)
      {
      GPIB_query(":DISP:WIND:TRAC:Y:RLEV UP;*OPC?");
      }

   GPIB_query(":INIT:CONT 1;*OPC?");                          // Return to continuous-sweep mode 
   GPIB_query(":SENS:BAND:RES:AUTO ON;*OPC?");                // Autoselect RBW
   GPIB_query(":SENS:BAND:VID:AUTO ON;*OPC?");                // Autoselect VBW
   GPIB_query(":DISP:WIND:TRAC:Y:RLEV:OFFS 0;*OPC?");         // No reference offset
   GPIB_query(":SENS:DET:FUNC NORM;*OPC?");                   // Normal detection
   GPIB_printf(":SENS:FREQ:CENT %I64d HZ;*OPC?",carrier_Hz);  // Set CF
   GPIB_read_ASC();

   GPIB_disconnect();
}

BOOL ACQ_E4400(C8    *filename,
               S32    GPIB_address,
               C8    *caption,
               S64    carrier,           
               S64    ext_IF,
               S64    ext_LO,
               SINGLE VBW_factor,
               DOUBLE ext_mult,          
               SINGLE NF_characteristic,
               SINGLE min_offset,        
               SINGLE max_offset,
               S32    clip,
               SINGLE ref = 10000.0F)
{
   S32 i;

   if ((carrier < 0) || (ref == 10000.0F))
      {
      SAL_alert_box("Error","Must specify both carrier frequency and reference level");
      return FALSE;
      }

   S32    min_decade = S32(log10(min_offset)+0.5);
   S32    max_decade = S32(log10(max_offset)+0.5) - 1;
   SINGLE ext_mult_dB = 0.0F;

   SAL_debug_printf("Minimum offset = %d Hz\n",S32(pow(10.0,min_decade)+0.5));
   SAL_debug_printf("Maximum offset = %d Hz\n",S32(pow(10.0,max_decade+1)+0.5));

   ext_mult_dB = -20.0F * (SINGLE) log10(ext_mult);

   SAL_debug_printf("External frequency multiplier = %.06lfx (%.02f dB)\n",
                     ext_mult,ext_mult_dB);

   //
   // Start up GPIB
   //

   GPIB_connect (GPIB_address,
                 GPIB_error,
                 0,
                 1000000);

   GPIB_set_EOS_mode(10);
   GPIB_set_serial_read_dropout(500);

   //
   // Open output file
   //

   out = fopen(filename,"wt");

   if (out == NULL)
      {
      GPIB_disconnect();

      SAL_alert_box("Error","Could not open %s",filename);
      return FALSE;
      }

   fprintf(out,";\n");
   fprintf(out,";Composite-noise plot file %s\n",filename);
   fprintf(out,";\n");
   fprintf(out,";Generated by PN.EXE V"VERSION" of "__DATE__"\n");
   fprintf(out,";by John Miles, KE5FX (john@miles.io)\n");
   fprintf(out,";\n\n");

   //
   // Log caption, if present
   //

   if (caption[0])
      {
      fprintf(out,"CAP %s\n",caption);
      }

   //
   // Disallow SRQs, clear error register
   //

   GPIB_query("*SRE 0;*CLS;*OPC?");

   //
   // Log timestamp and instrument ID
   //

   static C8 ID[1024];
   strcpy(ID,GPIB_query("*IDN?"));
   kill_trailing_whitespace(ID);

   fprintf(out,"TIM %s\n",timestamp());
   fprintf(out,"IMO %s\n", ID);
   fprintf(out,"IRV N/A\n\n");

   //
   // Get # of trace points (rounding down to nearest 100 for logging
   // purposes)
   //

   S32 E4400_n_points = 0;

   while (E4400_n_points == 0)
      {
      C8 *text = GPIB_query(":SENS:SWEEP:POINTS?");
      sscanf(text,"%d",&E4400_n_points);
      }

   if (E4400_n_points > MAX_SCPI_POINTS)
      {
      GPIB_disconnect();

      SAL_alert_box("Error","Sweep contains more than %d points -- reduce sweep point count",MAX_SCPI_POINTS);
      return FALSE;
      }

   S32 record_n_points = 100 * (E4400_n_points / 100);
   S32 decile_n_points = record_n_points / 10;

   //
   // Log decade statistics
   //

   fprintf(out,"DRG %d,%d\n",min_decade,max_decade);
   fprintf(out,"DCC %d\n",record_n_points - decile_n_points);
   fprintf(out,"OVC %d\n\n",record_n_points);

   fprintf(out,"EIF %I64d Hz\n",ext_IF);
   fprintf(out,"ELO %I64d Hz\n",ext_LO);
   fprintf(out,"MUL %lf\n\n",ext_mult);

   fprintf(out,"VBF %.02f\n",VBW_factor);
   fprintf(out,"CLP %d\n\n",clip);

   //
   // Set basic analyzer state needed for operation of program
   //

   GPIB_query(":INIT:CONT 0;*OPC?");                 // Select single-sweep mode
   GPIB_query(":CALC:MARKER1:STAT OFF;*OPC?");       // Turn off any marker-based functionality
   GPIB_query(":DISP:WIND:TRAC:Y:PDIV 10 DB;*OPC?"); // Need 10 dB/division log scale
   GPIB_query(":SENS:BAND:RES:AUTO ON;*OPC?");       // Autoselect RBW
   GPIB_query(":SENS:BAND:VID:AUTO ON;*OPC?");       // Autoselect VBW
   GPIB_query(":SENS:SWEEP:TIME:AUTO ON;*OPC?");     // Autoselect sweep time

   //
   // Request 32-bit little-endian integer trace format
   //

   GPIB_query(":FORM:TRACE:DATA INT,32;:FORM:BORD SWAP;*OPC?");

   //
   // Perform actual measurement at external IF, if one is specified
   //

   S64 source_carrier = carrier;

   if (ext_IF >= 0)
      {
      carrier = ext_IF;
      }

   //
   // Log beginning of sweep series
   //

   S64 start = elapsed_uS();

   //
   // For each decade...
   //

   bool reference_level_set = FALSE;

   S64 mult_delta_Hz = S64(((DOUBLE) carrier * ext_mult) + 0.5) - carrier;  

   for (S32 d=min_decade; d <= max_decade; d++)
      {
      static SINGLE trace_A   [2][MAX_SCPI_POINTS];
      static DOUBLE freq      [2][MAX_SCPI_POINTS];
      SINGLE        correction[2];

      S32 decade_start = (S32) (pow(10.0, d) + 0.5F);
      S32 next_decade  = decade_start * 10;
      S32 total_span   = decade_start * 10;

      //
      // Select RBW and span
      //

      S32 RBW = decade_start / 10;
      S32 VBW = (S32) ((RBW * VBW_factor) + 0.5F);

      GPIB_printf(":SENS:FREQ:SPAN %d HZ;*OPC?",total_span);  // Set total span
      GPIB_read_ASC();

      GPIB_printf(":SENS:BAND:RES %d HZ;*OPC?",RBW);          // Set RBW
      GPIB_read_ASC();

      //
      // Set reference level, if it hasn't already been established
      //

      if (!reference_level_set)
         {
         reference_level_set = TRUE;

         GPIB_printf(":DISP:WIND:TRAC:Y:RLEV:OFFS %f;*OPC?",-ref);  // Establish reference offset
         GPIB_read_ASC();

         GPIB_query(":DISP:WIND:TRAC:Y:RLEV 0 DBM;*OPC?");          // Set relative reference level = 0 dBm 

         //
         // Apply reference-level clipping if requested
         //

         for (S32 cp=0; cp < clip; cp += 10)
            {
            GPIB_query(":DISP:WIND:TRAC:Y:RLEV DOWN;*OPC?");
            }
         }

      //
      // Enable video filtering
      //

      GPIB_query(":SENS:DET:FUNC SAMPLE;*OPC?");        // We use sampled detection for noise-like signals... 

      GPIB_printf(":SENS:BAND:VID %d HZ;*OPC?",VBW);    // Set VBW
      GPIB_read_ASC();

      S64 tune_freq = carrier + (decade_start * 5);

      //
      // Log RF attenuation used for this sweep
      //

      SINGLE att = 10000.0F;

      while (att == 10000.0F)
         {
         C8 *AT_text = GPIB_query(":SENS:POW:RF:ATT?");
         sscanf(AT_text,"%f",&att);
         }

      SAL_debug_printf("Carrier level = %.02f dBm, RF atten = %d dB\n",ref,S32(att + 0.5F));

      //
      // Log carrier amplitude used for this sweep
      //

      fprintf(out,"CAR %I64d Hz, %.02f dBm\n",
         source_carrier,
         ref);

      fprintf(out,"ATT %d dB\n",
         S32(att + 0.5F));

      for (S32 half=0; half < 2; half++)
         {
         SAL_serve_message_queue();

         //
         // Take a reading at each half-decade
         //

         SAL_debug_printf("Analyzing decade 10^%d, sweep %d of 2...\n",d,half+1);

         if (key_hit)
            {
            key_hit = FALSE;
            EXIT_E4400(clip, carrier + mult_delta_Hz);

            SAL_alert_box("Error","Aborted by keypress");
            return FALSE;
            }

         GPIB_printf(":SENS:FREQ:CENT %I64d HZ;*OPC?",tune_freq + mult_delta_Hz);  // Set CF
         GPIB_read_ASC();

         //
         // Request trace dump
         //

         GPIB_printf(":INIT:IMM;*WAI;:TRAC:DATA? TRACE1");

         //
         // Read preamble, and verify that the data size
         // is what we expect
         //
   
         S8 *preamble = (S8 *) GPIB_read_BIN(2);
         assert(preamble[0] == '#');
         S32 n_preamble_bytes = preamble[1] - '0';
   
         C8 *size_text = GPIB_read_ASC(n_preamble_bytes);

         if (atoi(size_text) != (S32) (E4400_n_points * sizeof(S32)))
            { // TODO: First run on E4440A PSA with 82357B+Agilent I/O layer yields 404Ò instead of 2404; subsequent runs OK, per R. Griffiths 2/2010
            SAL_alert_box("Notice (please report to john@miles.io)","size_text = [%s], n_preamble_bytes = %d\n\n(Expected %d-point sweep)",
               size_text, n_preamble_bytes, E4400_n_points);
            }

         assert(atoi(size_text) == (S32) (E4400_n_points * sizeof(S32)));
   
         //
         // Acquire 32-bit integer data from display trace A
         // 
   
         static S32 curve[MAX_SCPI_POINTS];

         memset(curve, 
                0, 
                E4400_n_points * sizeof(S32));
   
         memcpy(curve, 
                GPIB_read_BIN(E4400_n_points * sizeof(S32),
                              TRUE,
                              FALSE), 
                E4400_n_points * sizeof(S32));
   
         //
         // Swallow trailing newline
         //
   
         GPIB_read_ASC();
   
         //
         // Convert trace to floating-point format (often much faster on host PC
         // than on target analyzer)
         //
         // Because we applied a reference-level offset equal to -ref dBm, 
         // these are dBc values, not dBm
         //
   
         DOUBLE df_dp = ((DOUBLE) total_span) / (DOUBLE) E4400_n_points;
         DOUBLE F     = ((DOUBLE) tune_freq) - ((DOUBLE) E4400_n_points / 2.0 * df_dp);

         for (i=0; i < E4400_n_points; i++)     
            {
            trace_A[half][i] = (SINGLE(curve[i]) / 1000.0F);
            freq   [half][i] = F;
            F += df_dp;
            }

         //
         // Apply noise-normalization factor
         //
         // This is typically 2 dB greater than the baseline
         // 10*log(RBW) normalization factor due to filter
         // and detector noise-response characteristics
         // 

         SINGLE RB_factor = -10.0F * (SINGLE) log10((DOUBLE) RBW);

         correction[half] = RB_factor + NF_characteristic;

         tune_freq += next_decade;
         }

      //
      // Display results
      //

      SAL_debug_printf("\n%8d Hz: %.02f dBc/Hz\n",
         decade_start,
         trace_A[0][decile_n_points] + correction[0] + ext_mult_dB);

      SAL_debug_printf("%8d Hz: %.02f dBc/Hz / %.02f dBc/Hz\n\n",
         next_decade,
         trace_A[0][record_n_points-1]  + correction[0] + ext_mult_dB,
         trace_A[1][decile_n_points  ]  + correction[1] + ext_mult_dB);

      //
      // Send decade and overlap data to output file
      //

      fprintf(out,"DEC %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[0]);

      for (i=decile_n_points; i < record_n_points; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i - decile_n_points,
            freq[0][i],
            trace_A[0][i] + ext_mult_dB);
         }

      fprintf(out,"OVL %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[1]);

      for (i=0; i < record_n_points; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i,
            freq[1][i],
            trace_A[1][i] + ext_mult_dB);
         }
      }

   //
   // Write cumulative sweep time and EOF timestamp
   //

   S64 end = elapsed_uS();

   fprintf(out,"ELP %I64ds\n",((end - start) + 500000) / 1000000);
   fprintf(out,"EOF %s\n",timestamp());

   EXIT_E4400(clip, carrier + mult_delta_Hz);
   return TRUE;
}

//****************************************************************************
//*                                                                          *
//* Acquire data from Agilent E4406A VSA                                     *
//*                                                                          *
//****************************************************************************

void EXIT_E4406A(S32 clip, //)
                 S64 carrier_Hz,
                 S32 orig_averaging_status)
{
   if (out != NULL)
      {
      fclose(out);
      out = NULL;
      }

   if (orig_averaging_status)                                         // Restore averaging status
      GPIB_query(":SENS:SPEC:AVER:STAT 1;*OPC?");
   else
      GPIB_query(":SENS:SPEC:AVER:STAT 0;*OPC?");

   C8 *text = GPIB_query(":DISP:SPEC1:WIND1:TRAC:Y:SCAL:RLEV?");      // Put clipped peak back onscreen 
   SINGLE val;
   sscanf(text, "%f", &val);
   val += clip;
   GPIB_printf(":DISP:SPEC1:WIND1:TRAC:Y:SCAL:RLEV %f;*OPC?", val);   
   GPIB_read_ASC();

   GPIB_query(":INIT:CONT 1;*OPC?");                                  // Return to continuous-sweep mode 
   GPIB_query(":DISP:SPEC1:WIND1:TRAC:Y:SCAL:PDIV 10;*OPC?");         // Switch to default 10 dB/division log scale
   GPIB_query(":SENS:SPEC:BAND:RES:AUTO ON;*OPC?");                   // Autoselect RBW
   GPIB_printf(":SENS:FREQ:CENT %I64d HZ;*OPC?",carrier_Hz);          // Set CF
   GPIB_read_ASC();

   GPIB_disconnect();
}

BOOL ACQ_E4406A(C8    *filename,
               S32    GPIB_address,
               C8    *caption,
               S64    carrier,           
               S64    ext_IF,
               S64    ext_LO,
               SINGLE VBW_factor,
               DOUBLE ext_mult,          
               SINGLE NF_characteristic,
               SINGLE min_offset,        
               SINGLE max_offset,
               S32    clip,
               SINGLE ref = 10000.0F)
{
   S32 i;

   if ((carrier < 0) || (ref == 10000.0F))
      {
      SAL_alert_box("Error","Must specify both carrier frequency and reference level");
      return FALSE;
      }

   S32    min_decade = S32(log10(min_offset)+0.5);
   S32    max_decade = S32(log10(max_offset)+0.5) - 1;
   SINGLE ext_mult_dB = 0.0F;

   SAL_debug_printf("Minimum offset = %d Hz\n",S32(pow(10.0,min_decade)+0.5));
   SAL_debug_printf("Maximum offset = %d Hz\n",S32(pow(10.0,max_decade+1)+0.5));

   ext_mult_dB = -20.0F * (SINGLE) log10(ext_mult);

   SAL_debug_printf("External frequency multiplier = %.06lfx (%.02f dB)\n",
                     ext_mult,ext_mult_dB);

   //
   // Start up GPIB
   //

   GPIB_connect (GPIB_address,
                 GPIB_error,
                 0,
                 1000000);

   GPIB_set_EOS_mode(10);
   GPIB_set_serial_read_dropout(500);

   //
   // Open output file
   //

   out = fopen(filename,"wt");

   if (out == NULL)
      {
      GPIB_disconnect();

      SAL_alert_box("Error","Could not open %s",filename);
      return FALSE;
      }

   fprintf(out,";\n");
   fprintf(out,";Composite-noise plot file %s\n",filename);
   fprintf(out,";\n");
   fprintf(out,";Generated by PN.EXE V"VERSION" of "__DATE__"\n");
   fprintf(out,";by John Miles, KE5FX (john@miles.io)\n");
   fprintf(out,";\n\n");

   //
   // Log caption, if present
   //

   if (caption[0])
      {
      fprintf(out,"CAP %s\n",caption);
      }

   //
   // Disallow SRQs, clear error register, and ensure we're in the right mode
   //

   GPIB_query(":INST:SEL BASIC;*OPC?");
   GPIB_query("*SRE 0;*CLS;*OPC?");

   //
   // Log timestamp and instrument ID
   //
   // Example instrument ID string:
   //  
   //   Hewlett-Packard,E4406A,MY44021638,A.08.10   20041215  12:30:18
   //

   static C8 ID[1024];
   strcpy(ID,GPIB_query("*IDN?"));
   kill_trailing_whitespace(ID);

   C8 *rev = strstr(ID, "E44");

   if (rev != NULL)
      {
      rev = strchr(rev,',');

      if (rev != NULL)
         {
         *rev++ = 0;

         C8 *kill_first_comma = strchr(ID,',');

         if (kill_first_comma != NULL)
            {
            *kill_first_comma = ' ';
            }
         }
      }

   fprintf(out,"TIM %s\n",timestamp());
   fprintf(out,"IMO %s\n", ID);

   if (rev != NULL)
      fprintf(out,"IRV %s\n\n", rev);
   else
      fprintf(out,"IRV N/A\n\n");

   //
   // Because the E4406A's trace point count is arbitary, variable, and unobtainable prior to actually
   // fetching a trace, we'll resample the number we actually get to 500 points
   //

   S32 record_n_points = 500;
   S32 decile_n_points = 50;

   //
   // Log decade statistics
   //

   fprintf(out,"DRG %d,%d\n",min_decade,max_decade);
   fprintf(out,"DCC %d\n",record_n_points - decile_n_points);
   fprintf(out,"OVC %d\n\n",record_n_points);

   fprintf(out,"EIF %I64d Hz\n",ext_IF);
   fprintf(out,"ELO %I64d Hz\n",ext_LO);
   fprintf(out,"MUL %lf\n\n",ext_mult);

   fprintf(out,"VBF %.02f\n",VBW_factor);
   fprintf(out,"CLP %d\n\n",clip);

   //
   // Set basic analyzer state needed for operation of program
   //

   S32 avg = (GPIB_query(":SENS:SPEC:AVER:STAT?")[0] != '0');

   GPIB_query(":SENS:SPEC:AVER:STAT 0;*OPC?");                 // Disable averaging after recording original setting for later restoration
   GPIB_query(":DISP:FORM:ZOOM1;*OPC?");                       // Zoom to spectrum analyzer display window
   GPIB_query(":INIT:CONT 0;*OPC?");                           // Select single-sweep mode
   GPIB_query(":CALC:SPEC:MARK:AOFF;*OPC?");                   // Kill any markers
   GPIB_query(":DISP:SPEC1:WIND1:TRAC:Y:SCAL:PDIV 15;*OPC?");  // Use 15 dB/division log scale
   GPIB_query(":SENS:SPEC:BAND:RES:AUTO ON;*OPC?");            // Autoselect RBW
   GPIB_query(":SENS:SPEC:SWE:TIME:AUTO ON;*OPC?");            // Autoselect sweep time

   //
   // Request 32-bit little-endian real trace format
   //

   GPIB_query(":FORM:DATA REAL,32;:FORM:BORD SWAP;*OPC?");

   //
   // Perform actual measurement at external IF, if one is specified
   //

   S64 source_carrier = carrier;

   if (ext_IF >= 0)
      {
      carrier = ext_IF;
      }

   //
   // Log beginning of sweep series
   //

   S64 start = elapsed_uS();

   //
   // For each decade...
   //

   bool reference_level_set = FALSE;

   S64 mult_delta_Hz = S64(((DOUBLE) carrier * ext_mult) + 0.5) - carrier;  

   for (S32 d=min_decade; d <= max_decade; d++)
      {
      static SINGLE trace_A   [2][MAX_SCPI_POINTS];
      static DOUBLE freq      [2][MAX_SCPI_POINTS];
      SINGLE        correction[2];

      S32 decade_start = (S32) (pow(10.0, d) + 0.5F);
      S32 next_decade  = decade_start * 10;
      S32 total_span   = decade_start * 10;

      //
      // Select RBW and span
      //

      DOUBLE RBW = (DOUBLE) decade_start / 10.0;

      GPIB_printf(":SENS:SPEC:FREQ:SPAN %d HZ;*OPC?",total_span);  // Set total span
      GPIB_read_ASC();

      GPIB_printf(":SENS:SPEC:BAND:RES %lf HZ;*OPC?",RBW);         // Set RBW
      GPIB_read_ASC();

      //
      // Set reference level, if it hasn't already been established
      //

      if (!reference_level_set)
         {
         reference_level_set = TRUE;
         GPIB_printf(":DISP:SPEC1:WIND1:TRAC:Y:RLEV %f;*OPC?",ref-clip);     
         GPIB_read_ASC();
         }

      S64 tune_freq = carrier + (decade_start * 5);

      //
      // Log RF attenuation used for this sweep
      //
      // (Contrary to E4406A manual, :SENS:POW:RF:ATT:AUTO isn't supported)
      //

      SINGLE att = 10000.0F;

      while (att == 10000.0F)
         {
         C8 *AT_text = GPIB_query(":SENS:POW:RF:ATT?");
         sscanf(AT_text,"%f",&att);
         }

      SAL_debug_printf("Carrier level = %.02f dBm, RF atten = %d dB\n",ref,S32(att + 0.5F));

      //
      // Log carrier amplitude used for this sweep
      //

      fprintf(out,"CAR %I64d Hz, %.02f dBm\n",
         source_carrier,
         ref);

      fprintf(out,"ATT %d dB\n",
         S32(att + 0.5F));

      for (S32 half=0; half < 2; half++)
         {
         SAL_serve_message_queue();

         //
         // Take a reading at each half-decade
         //

         SAL_debug_printf("Analyzing decade 10^%d, sweep %d of 2...\n",d,half+1);

         if (key_hit)
            {
            key_hit = FALSE;
            EXIT_E4406A(clip, carrier + mult_delta_Hz, avg);

            SAL_alert_box("Error","Aborted by keypress");
            return FALSE;
            }

         GPIB_printf(":SENS:FREQ:CENT %I64d HZ;*OPC?",tune_freq + mult_delta_Hz);  // Set CF
         GPIB_read_ASC();

         //
         // Request trace dump
         //

         static SINGLE curve[MAX_SCPI_POINTS];

         GPIB_puts(":INIT:IMM;*WAI;:FETCH:SPEC4?");

         S8 *preamble = (S8 *) GPIB_read_BIN(2);
         assert(preamble[0] == '#');
         S32 n_preamble_bytes = preamble[1] - '0';

         C8 *size_text = GPIB_read_ASC(n_preamble_bytes);
         S32 E4406A_n_points = atoi(size_text) / sizeof(SINGLE);

         if ((E4406A_n_points < 2) || (E4406A_n_points > MAX_SCPI_POINTS))
            {
            SAL_alert_box("Notice (please report to john@miles.io)","size_text = [%s], n_preamble_bytes = %d\n",
               size_text, n_preamble_bytes);

            EXIT_E4406A(clip, carrier + mult_delta_Hz, avg);
            return FALSE;
            }
         
         memset(curve, 
                0, 
                E4406A_n_points * sizeof(SINGLE));

         memcpy(curve, 
                GPIB_read_BIN(E4406A_n_points * sizeof(SINGLE),
                              TRUE,
                              FALSE), 
                E4406A_n_points * sizeof(SINGLE));

         //
         // Swallow trailing newline
         //
   
         GPIB_read_ASC();

         //
         // Point-sample trace (up or down) to 500 points
         //
         // Because the E4406A doesn't support reference-level offsets, we need to subtract the
         // carrier level here as well
         //
   
         DOUBLE dd_dp = ((DOUBLE) E4406A_n_points / (DOUBLE) record_n_points);      // > 1 if there are more src than dest points
         DOUBLE index = 0.5;                

         DOUBLE df_dp = ((DOUBLE) total_span) / (DOUBLE) record_n_points;
         DOUBLE F     = ((DOUBLE) tune_freq) - ((DOUBLE) record_n_points / 2.0 * df_dp);

         for (i=0; i < record_n_points; i++)
            {
            S32 int_index = (S32) index;
            index += dd_dp;

            trace_A[half][i] = curve[int_index] - ref;
            freq   [half][i] = F;
            F += df_dp;
            }

         //
         // Apply noise-normalization factor
         //
         // This is typically 2 dB greater than the baseline
         // 10*log(RBW) normalization factor due to filter
         // and detector noise-response characteristics
         // 

         SINGLE RB_factor = -10.0F * (SINGLE) log10(RBW);

         correction[half] = RB_factor + NF_characteristic;

         tune_freq += next_decade;
         }

      //
      // Display results
      //

      SAL_debug_printf("\n%8d Hz: %.02f dBc/Hz\n",
         decade_start,
         trace_A[0][decile_n_points] + correction[0] + ext_mult_dB);

      SAL_debug_printf("%8d Hz: %.02f dBc/Hz / %.02f dBc/Hz\n\n",
         next_decade,
         trace_A[0][record_n_points-1]  + correction[0] + ext_mult_dB,
         trace_A[1][decile_n_points  ]  + correction[1] + ext_mult_dB);

      //
      // Send decade and overlap data to output file
      //

      fprintf(out,"DEC %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[0]);

      for (i=decile_n_points; i < record_n_points; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i - decile_n_points,
            freq[0][i],
            trace_A[0][i] + ext_mult_dB);
         }

      fprintf(out,"OVL %d\n",d);
      fprintf(out,"NCF %.02f dB\n", correction[1]);

      for (i=0; i < record_n_points; i++)
         {
         fprintf(out,"   TRA %d: %lf Hz, %.02f dBc\n",
            i,
            freq[1][i],
            trace_A[1][i] + ext_mult_dB);
         }
      }

   //
   // Write cumulative sweep time and EOF timestamp
   //

   S64 end = elapsed_uS();

   fprintf(out,"ELP %I64ds\n",((end - start) + 500000) / 1000000);
   fprintf(out,"EOF %s\n",timestamp());

   EXIT_E4406A(clip, carrier + mult_delta_Hz, avg);
   return TRUE;
}

//****************************************************************************
//*                                                                          *
//*  Read all or part of a file into memory, returning memory location       *
//*  or NULL on error                                                        *
//*                                                                          *
//*  Memory will be allocated if dest==NULL                                  *
//*                                                                          *
//****************************************************************************

void * FILE_read (C8     *filename, //)     
                  S32    *len_dest     = NULL,
                  void   *dest         = NULL,
                  S32     len          = -1,
                  S32     start_offset = 0)

{
   HANDLE handle;
   U32    n_bytes;
   U32    nbytes_read;
   S32    result;
   void  *buf;

   //
   // Open file
   //

   handle = CreateFile(filename,
                       GENERIC_READ,
                       FILE_SHARE_READ,
                       NULL,
                       OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                       NULL);

   if (handle == INVALID_HANDLE_VALUE)
      {
      return NULL;
      }

   //
   // Get its size
   //

   S32 actual_len = GetFileSize(handle, NULL);

   if (len_dest != NULL)
      {
      *len_dest = actual_len;
      }

   //
   // Set pointer to beginning of range
   //

   if (SetFilePointer(handle,
                      start_offset,
                      NULL,
                      FILE_BEGIN) == 0xffffffff)
      {
      CloseHandle(handle);
      return NULL;
      }

   //
   // Allocate memory for file range
   //

   n_bytes = len;

   if (n_bytes == 0xffffffff)
      {
      n_bytes = actual_len - start_offset;
      }

   buf = (dest == NULL) ? malloc(n_bytes) : dest;

   if (buf == NULL)
      {
      CloseHandle(handle);
      return NULL;
      }

   //
   // Read range
   //

   result = ReadFile(handle,
                     buf,
                     n_bytes,
          (LPDWORD) &nbytes_read,
                     NULL);

   CloseHandle(handle);

   if ((!result) || (nbytes_read != n_bytes))
      {
      if (dest != buf)
         {
         free(buf);
         }

      return NULL;
      }   

   return buf;
}

//****************************************************************************
//*                                                                          *
//*  Update checked items, etc. in main menu                                 *
//*                                                                          *
//****************************************************************************

void main_menu_update(void)
{
   for (S32 i=0; i < MAX_LEGEND_FIELDS; i++)
      {
      if (INI_show_legend_field[i])
         {
         CheckMenuItem(hmenu, IDM_S_CAPTION+i, MF_CHECKED);
         }
      else
         {
         CheckMenuItem(hmenu, IDM_S_CAPTION+i, MF_UNCHECKED);
         }
      }

   CheckMenuItem(hmenu, IDM_LOW_CONTRAST,   (contrast==0)    ? MF_CHECKED : MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_MED_CONTRAST,   (contrast==1)    ? MF_CHECKED : MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_HIGH_CONTRAST,  (contrast==2)    ? MF_CHECKED : MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_THICK_TRACE,    thick_trace      ? MF_CHECKED : MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_GHOST,          ghost_sample     ? MF_CHECKED : MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_ALLOW_CLIPPING, allow_clipping   ? MF_CHECKED : MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_SORT_MODIFY,    sort_modify_date ? MF_CHECKED : MF_UNCHECKED);

   CheckMenuItem(hmenu, IDM_S_NONE, (smooth_samples == 0)   ? MF_CHECKED : MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_S_2,    (smooth_samples == 2)   ? MF_CHECKED : MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_S_4,    (smooth_samples == 4)   ? MF_CHECKED : MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_S_8,    (smooth_samples == 8)   ? MF_CHECKED : MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_S_16,   (smooth_samples == 16)  ? MF_CHECKED : MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_S_24,   (smooth_samples == 24)  ? MF_CHECKED : MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_S_32,   (smooth_samples == 32)  ? MF_CHECKED : MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_S_48,   (smooth_samples == 48)  ? MF_CHECKED : MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_S_64,   (smooth_samples == 64)  ? MF_CHECKED : MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_S_96,   (smooth_samples == 96)  ? MF_CHECKED : MF_UNCHECKED);

   CheckMenuItem(hmenu, IDM_S_ALT,  alt_smoothing  ? MF_CHECKED : MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_SPUR,   spur_reduction ? MF_CHECKED : MF_UNCHECKED);

   CheckMenuItem(hmenu, IDM_384,  (RES_Y == 384)  ? MF_CHECKED : MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_400,  (RES_Y == 400)  ? MF_CHECKED : MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_480,  (RES_Y == 480)  ? MF_CHECKED : MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_600,  (RES_Y == 600)  ? MF_CHECKED : MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_768,  (RES_Y == 768)  ? MF_CHECKED : MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_864,  (RES_Y == 864)  ? MF_CHECKED : MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_1024, (RES_Y == 1024) ? MF_CHECKED : MF_UNCHECKED);
}

//****************************************************************************
//*                                                                          *
//*  Initialize main menu                                                    *
//*                                                                          *
//****************************************************************************

void main_menu_init(void)
{
   if (hmenu != NULL)
      {
      SetMenu(hWnd,NULL);
      DestroyMenu(hmenu);
      }

   hmenu = CreateMenu();

   HMENU pop = CreateMenu();

   AppendMenu(pop, MF_STRING, IDM_LOAD_PNP,     "&Load PNP data ... \t l");
   AppendMenu(pop, MF_STRING, IDM_SAVE,         "&Save image or PNP data ... \t s");
   AppendMenu(pop, MF_STRING, IDM_EXPORT,       "&Export smoothed trace to .TXT or .CSV file ... \t x");
   AppendMenu(pop, MF_SEPARATOR, 0, NULL);
   AppendMenu(pop, MF_STRING, IDM_PRINT_IMAGE,  "&Print image \t p");
   AppendMenu(pop, MF_SEPARATOR, 0, NULL);
   AppendMenu(pop, MF_STRING, IDM_SORT_MODIFY,  "Sort file dialogs by modification date");
   AppendMenu(pop, MF_SEPARATOR, 0, NULL);
   AppendMenu(pop, MF_STRING, IDM_CLOSE,        "Close current plot \t Del");
   AppendMenu(pop, MF_STRING, IDM_CLOSE_ALL,    "Close all visible plots \t Home");
   AppendMenu(pop, MF_STRING, IDM_DELETE,       "Delete current .PNP file\t Ctrl-Del");
   AppendMenu(pop, MF_SEPARATOR, 0, NULL);
   AppendMenu(pop, MF_STRING, IDM_QUIT,         "Quit \t q");

   AppendMenu(hmenu, MF_POPUP, (UINT) pop,      "&File");

   pop = CreateMenu();

   AppendMenu(pop, MF_STRING, IDM_BROWSE,       "Browse sources one a time\t b");
   AppendMenu(pop, MF_STRING, IDM_OVERLAY,      "Overlay all sources\t o");
   AppendMenu(pop, MF_SEPARATOR, 0, NULL);
   AppendMenu(pop, MF_STRING, IDM_NEXT,         "Next source \t + or up-arrow");
   AppendMenu(pop, MF_STRING, IDM_PREV,         "Previous source \t - or down-arrow");
   AppendMenu(pop, MF_SEPARATOR, 0, NULL);
   AppendMenu(pop, MF_STRING, IDM_MOVE_UP,      "Move selected source up \t Ctrl-up-arrow");
   AppendMenu(pop, MF_STRING, IDM_MOVE_DOWN,    "Move selected source down \t Ctrl-down-arrow");
   AppendMenu(pop, MF_SEPARATOR, 0, NULL);
   AppendMenu(pop, MF_STRING, IDM_VIS,          "Toggle visibility of selected source \t v");

   AppendMenu(hmenu, MF_POPUP, (UINT) pop,      "&View");

   pop = CreateMenu();

   AppendMenu(pop, MF_STRING, IDM_384,          "512 x 384 \t 5");
   AppendMenu(pop, MF_STRING, IDM_400,          "640 x 400");
   AppendMenu(pop, MF_STRING, IDM_480,          "640 x 480 \t 6");
   AppendMenu(pop, MF_STRING, IDM_600,          "800 x 600 \t 7");
   AppendMenu(pop, MF_STRING, IDM_768,          "1024 x 768 \t 8");
   AppendMenu(pop, MF_STRING, IDM_864,          "1152 x 864 \t 9");
   AppendMenu(pop, MF_STRING, IDM_1024,         "1280 x 1024 \t 0");
   AppendMenu(pop, MF_SEPARATOR, 0, NULL);
   AppendMenu(pop, MF_STRING, IDM_LOW_CONTRAST, "Low contrast");
   AppendMenu(pop, MF_STRING, IDM_MED_CONTRAST, "Medium contrast");
   AppendMenu(pop, MF_STRING, IDM_HIGH_CONTRAST,"High contrast \t C");

   AppendMenu(hmenu, MF_POPUP, (UINT) pop,      "&Display");

   pop = CreateMenu();

   AppendMenu(pop, MF_STRING, IDM_S_NONE,        "No smoothing\tCtrl-0");
   AppendMenu(pop, MF_STRING, IDM_S_2,           "2-point smoothing\tCtrl-1");
   AppendMenu(pop, MF_STRING, IDM_S_4,           "4-point smoothing\tCtrl-2");
   AppendMenu(pop, MF_STRING, IDM_S_8,           "8-point smoothing\tCtrl-3");
   AppendMenu(pop, MF_STRING, IDM_S_16,          "16-point smoothing\tCtrl-4");
   AppendMenu(pop, MF_STRING, IDM_S_24,          "24-point smoothing\tCtrl-5");
   AppendMenu(pop, MF_STRING, IDM_S_32,          "32-point smoothing\tCtrl-6");
   AppendMenu(pop, MF_STRING, IDM_S_48,          "48-point smoothing\tCtrl-7");
   AppendMenu(pop, MF_STRING, IDM_S_64,          "64-point smoothing\tCtrl-8");
   AppendMenu(pop, MF_STRING, IDM_S_96,          "96-point smoothing\tCtrl-9");
   AppendMenu(pop, MF_SEPARATOR, 0, NULL);
   AppendMenu(pop, MF_STRING, IDM_S_ALT,         "Use alternate smoothing algorithm");
   AppendMenu(pop, MF_SEPARATOR, 0, NULL);
   AppendMenu(pop, MF_STRING, IDM_SPUR,          "Enable spur reduction \t Ctrl-s");
   AppendMenu(pop, MF_STRING, IDM_EDIT_SPUR,     "Set spur threshold in dB ... \t t");
   AppendMenu(pop, MF_SEPARATOR, 0, NULL);
   AppendMenu(pop, MF_STRING, IDM_THICK_TRACE,    "Thick traces \t T");
   AppendMenu(pop, MF_STRING, IDM_GHOST,          "Enable ghosting \t g");
   AppendMenu(pop, MF_STRING, IDM_ALLOW_CLIPPING, "Allow vertical clipping\t c");
   AppendMenu(pop, MF_SEPARATOR, 0, NULL);
   AppendMenu(pop, MF_STRING, IDM_EDIT_CAPTION,   "Edit caption for selected trace ... \t e");

   AppendMenu(hmenu, MF_POPUP, (UINT) pop,      "&Trace");

   pop = CreateMenu();

   S32 i;
   for (i=0; i < MAX_LEGEND_FIELDS; i++)
      {
      AppendMenu(pop, MF_STRING, IDM_S_CAPTION+i, legend_captions[i]);
      }

   AppendMenu(pop, MF_SEPARATOR, 0, NULL);
   AppendMenu(pop, MF_STRING, IDM_ALL,          "All");
   AppendMenu(pop, MF_STRING, IDM_NONE,         "None");

   AppendMenu(hmenu, MF_POPUP, (UINT) pop,      "&Legend");

   pop = CreateMenu();

   for (i=0; i < MAX_ACQUIRE_OPTIONS; i++)
      {
      S32 id = FIRST_ACQ_SOURCE+i;

      AppendMenu(pop, MF_STRING, id, acquire_options[i]);

      if ((id == IDM_TEK2780) || 
          (id == IDM_HP8590)  || 
          (id == IDM_HP4195A) ||
          (id == IDM_HP8568B) || 
          (id == IDM_HP70000) || 
          (id == IDM_HP4396A) || 
          (id == IDM_TR417X)  ||
          (id == IDM_MS2650)  ||
          (id == IDM_E4406A)  ||
          (id == IDM_HP358XA))
         {
         AppendMenu(pop, MF_SEPARATOR, 0, NULL);
         }
      }

   AppendMenu(pop, MF_SEPARATOR, 0, NULL);
   AppendMenu(pop, MF_STRING, IDM_REPEAT_LAST,  "&Repeat last acquisition ... \t Space");

   AppendMenu(hmenu, MF_POPUP, (UINT) pop,      "&Acquire");

   pop = CreateMenu();

   AppendMenu(pop, MF_STRING, IDM_USER_GUIDE,   "&User guide \t F1");
   AppendMenu(pop, MF_SEPARATOR, 0, NULL);
   AppendMenu(pop, MF_STRING, IDM_ABOUT,        "&About PN");

   AppendMenu(hmenu, MF_POPUP, (UINT) pop,      "&Help");

   SetMenu(hWnd,hmenu);

   main_menu_update();
   DrawMenuBar(hWnd);
}

//****************************************************************************
//
// Establish display resolution
//
//****************************************************************************

void set_display_mode(void)
{
   S32 width  = RES_X;
   S32 height = RES_Y;

   if (!VFX_set_display_mode(width,
                             height,
                             16,
                             VFX_WINDOW_MODE,
                             FALSE))
      {
      exit(1);
      }

   //
   // Store font metrics for use by renderer
   //

   font_height = VFX_font_height(VFX_default_system_font());
   em_width    = string_width("m");

   transparent_font_CLUT[0] = (U16) RGB_TRANSPARENT;
   transparent_font_CLUT[1] = (U16) RGB_NATIVE(0,0,0);

   //
   // Configure output window and clipping regions
   //

   if (stage_window)
      {
      VFX_window_destroy(stage_window);
      }

   stage_window = VFX_window_construct(width, height);

   if (stage)
      {
      VFX_pane_destroy(stage);
      stage = NULL;
      }

   if (trace_area)
      {
      VFX_pane_destroy(trace_area);
      trace_area = NULL;
      }

   stage = VFX_pane_construct(stage_window,
                              0,
                              0,
                              width-1, 
                              height-1);

   trace_area = VFX_pane_construct(stage_window,
                                   0,
                                   0,
                                   width-1, 
                                   height-1);

   if (image)
      {
      VFX_pane_destroy(image);
      image = NULL;
      }

   image = VFX_pane_construct(stage_window,
                              0,
                              GetSystemMetrics(SM_CYMENU),
                              width-1, 
                              height-1);

   VFX_assign_window_buffer(stage_window,
                            NULL,
                           -1);

   if (screen_window)
      {
      VFX_window_destroy(screen_window);
      }

   screen_window = VFX_window_construct(width, height);

   if (screen)
      {
      VFX_pane_destroy(screen);
      }

   screen = VFX_pane_construct(screen_window,
                               0,
                               0,
                               width-1, 
                               height-1);

   background_invalid = TRUE;

   main_menu_init();

   //
   // Set pen colors as native RGB words for compatibility with
   // font renderer
   // 

   for (S32 s=0; s < N_PEN_LEVELS; s++)
      {
      for (S32 c=0; c < N_PEN_COLORS; c++)
         {
         VFX_RGB color = *VFX_color_to_RGB(pen_colors[s][c]);
         pen_colors[s][c] = RGB_NATIVE(color.r, color.g, color.b);
         }
      }
}

//****************************************************************************
//
// Center with respect to another window (from KB140722)
//
// Specifying NULL for hwndParent centers hwndChild relative to the
// screen
//
//****************************************************************************

BOOL CenterWindow(HWND hwndChild, HWND hwndParent)
{
   RECT rcChild, rcParent;
   int  cxChild, cyChild, cxParent, cyParent;
   int  cxScreen, cyScreen, xNew, yNew;
   HDC  hdc;

   GetWindowRect(hwndChild, &rcChild);
   cxChild = rcChild.right - rcChild.left;
   cyChild = rcChild.bottom - rcChild.top;

   if (hwndParent)
      {
      GetWindowRect(hwndParent, &rcParent);
      cxParent = rcParent.right - rcParent.left;
      cyParent = rcParent.bottom - rcParent.top;
      }
   else
      {
      cxParent = GetSystemMetrics (SM_CXSCREEN);
      cyParent = GetSystemMetrics (SM_CYSCREEN);
      rcParent.left = 0;
      rcParent.top  = 0;
      rcParent.right = cxParent;
      rcParent.bottom= cyParent;
      }

   hdc = GetDC(hwndChild);
   cxScreen = GetDeviceCaps(hdc, HORZRES);
   cyScreen = GetDeviceCaps(hdc, VERTRES);
   ReleaseDC(hwndChild, hdc);

   xNew = rcParent.left + ((cxParent - cxChild) / 2);

   if (xNew < 0)
      {
      xNew = 0;
      }
   else if ((xNew + cxChild) > cxScreen)
      {
      xNew = cxScreen - cxChild;
      }

   yNew = rcParent.top  + ((cyParent - cyChild) / 2);

   if (yNew < 0)
      {
      yNew = 0;
      }
   else if ((yNew + cyChild) > cyScreen)
      {
      yNew = cyScreen - cyChild;
      }

   return SetWindowPos(hwndChild,
                       NULL,
                       xNew, yNew,
                       0, 0,
                       SWP_NOSIZE | SWP_NOZORDER);
}

//****************************************************************************
//*                                                                          *
//*  Utility hook procedure to center save-filename dialogs                  *
//*                                                                          *
//****************************************************************************

S32 already_sent = FALSE;

UINT CALLBACK SFNHookProc(HWND hdlg,      // handle to the dialog box window //)
                          UINT uiMsg,     // message identifier
                          WPARAM wParam,  // message parameter
                          LPARAM lParam)  // message parameter
{
   if (uiMsg == WM_NOTIFY)
      {
      LPNMHDR pnmh = (LPNMHDR) lParam;
      HWND hwndParent = GetParent(hdlg);

      if (pnmh->code == CDN_SELCHANGE)
         {
         HWND hwndListView = FindWindowEx(hwndParent, 0, "SHELLDLL_DefView", 0);

         if ((hwndListView > 0) && sort_modify_date && (!already_sent))
            {
            const S32 SORTATTRIBUTES = 0x7608;
            const S32 SORTMODIFIED   = 0x7605;

            PostMessage(hwndListView, WM_COMMAND, SORTATTRIBUTES, 0);
            PostMessage(hwndListView, WM_COMMAND, SORTMODIFIED, 0);

            already_sent = TRUE;
            }
         }
      else if (pnmh->code == CDN_INITDONE)
         {
         if (SAL_window_status() == SAL_WINDOW)
            {
            CenterWindow(hwndParent, hWnd);                      
            PostMessage(hWnd, WM_USER, (WPARAM) hwndParent, 0);    // can't resize the box from here, must delegate it 
            }
         }
      }

   return 0;
}

//****************************************************************************
//*                                                                          *
//*  Utility hook procedure to center filename dialogs and force             *
//*  sort-by-modified date mode                                              *
//*                                                                          *
//****************************************************************************

UINT CALLBACK OFNHookProc(HWND hdlg,      // handle to the dialog box window //)
                          UINT uiMsg,     // message identifier
                          WPARAM wParam,  // message parameter
                          LPARAM lParam)  // message parameter
{
   if (uiMsg == WM_NOTIFY)
      {
      LPNMHDR pnmh = (LPNMHDR) lParam;
      HWND hwndParent = GetParent(hdlg);

      if (pnmh->code == CDN_SELCHANGE)
         {
         HWND hwndListView = FindWindowEx(hwndParent, 0, "SHELLDLL_DefView", 0);

         if ((hwndListView > 0) && sort_modify_date && (!already_sent))
            {
            const S32 SORTATTRIBUTES = 0x7608;
            const S32 SORTMODIFIED   = 0x7605;

            PostMessage(hwndListView, WM_COMMAND, SORTATTRIBUTES, 0);
            PostMessage(hwndListView, WM_COMMAND, SORTMODIFIED, 0);

            already_sent = TRUE;
            }
         }
      else if (pnmh->code == CDN_INITDONE)
         {
         if (SAL_window_status() == SAL_WINDOW)
            {
            CenterWindow(hwndParent, hWnd);                      
            PostMessage(hWnd, WM_USER, (WPARAM) hwndParent, 0);    // can't resize the box from here, must delegate it 
            }
         }
      }

   return 0;
}

//****************************************************************************
//*                                                                          *
//*  Return rectangle containing client-area boundaries in screenspace       *
//*                                                                          *
//****************************************************************************

RECT *client_screen_rect(HWND hWnd)
{
   static RECT rect;
   POINT       ul,lr;

   GetClientRect(hWnd, &rect);

   ul.x = rect.left; 
   ul.y = rect.top; 
   lr.x = rect.right; 
   lr.y = rect.bottom; 

   ClientToScreen(hWnd, &ul); 
   ClientToScreen(hWnd, &lr); 

   SetRect(&rect, ul.x, ul.y, 
                  lr.x-1, lr.y-1); 

   return &rect;
}

//****************************************************************************
//
// Get list of all files in directory with given name specification
//
//****************************************************************************

void add_files_to_list(C8 *dirspec, //)   
                       C8 *filespec,
                       S32 maxfiles)
{
   C8 dir_buffer[MAX_PATH];

   if (dirspec != NULL)
      {
      strcpy(dir_buffer,dirspec);
      }
   else
      {
      GetCurrentDirectory(sizeof(dir_buffer),
                          dir_buffer);   
      }

   if (strlen(dir_buffer)) 
      {
      if (dir_buffer[strlen(dir_buffer)-1] != '\\')
         {
         strcat(dir_buffer,"\\");
         }
      }

   C8 name_buffer[MAX_PATH+4];

   strcpy(name_buffer,dir_buffer);
   strcat(name_buffer,filespec);

   HANDLE          search_handle;
   WIN32_FIND_DATA found;

   search_handle = FindFirstFile(name_buffer, &found);

   if ((search_handle != NULL) && (search_handle != INVALID_HANDLE_VALUE))
      {
      do
         {
         if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
            continue;
            }

         if (n_data_sources >= maxfiles)
            {
            break;
            }

         strcpy(name_buffer, dir_buffer);
         strcat(name_buffer, found.cFileName);

         if (data_source_list[n_data_sources].load(name_buffer, FALSE))
            {
            n_data_sources++;
            }
         }
      while (FindNextFile(search_handle, &found));

      FindClose(search_handle);
      }
}

//****************************************************************************
//*                                                                          *
//* Get filename to save                                                     *
//*                                                                          *
//****************************************************************************

BOOL get_save_filename(C8 *string)
{
   OPENFILENAME fn;
   C8           fn_buff[MAX_PATH];

   fn_buff[0] = 0;

   if (strlen(string))     // (Force use of INI_save_directory by removing path portion of
      {                    // supplied default filename)
      C8 *f = strrchr(string,'\\');

      if (f != NULL)
         f = &f[1];
      else
         f = string;

      strcpy(fn_buff, f);
      }

   memset(&fn, 0, sizeof(fn));
   fn.lStructSize       = sizeof(fn);
   fn.hwndOwner         = hWnd;
   fn.hInstance         = NULL;

   fn.lpstrFilter       = "\
Noise plot files (*.PNP)\0*.PNP\0\
Image files (*.GIF)\0*.GIF\0\
Image files (*.TGA)\0*.TGA\0\
Image files (*.BMP)\0*.BMP\0\
Image files (*.PCX)\0*.PCX\0\
";

   fn.lpstrCustomFilter = NULL;
   fn.nMaxCustFilter    = 0;
   fn.nFilterIndex      = 1;
   fn.lpstrFile         = fn_buff;
   fn.nMaxFile          = sizeof(fn_buff);
   fn.lpstrFileTitle    = NULL;
   fn.nMaxFileTitle     = 0;

   if (INI_save_directory[0])
      {
      fn.lpstrInitialDir = INI_save_directory;
      }
   else
      {
      fn.lpstrInitialDir = NULL;
      }

   fn.lpstrTitle        = "Save";
   fn.Flags             = OFN_EXPLORER |
                          OFN_LONGNAMES |
                          OFN_ENABLESIZING |
                          OFN_NOCHANGEDIR |
                          OFN_ENABLEHOOK |
                          OFN_PATHMUSTEXIST |
                          OFN_OVERWRITEPROMPT |
                          OFN_HIDEREADONLY;
   fn.nFileOffset       = 0;
   fn.nFileExtension    = 0;
   fn.lpstrDefExt       = NULL;
   fn.lCustData         = NULL;
   fn.lpfnHook          = SFNHookProc;
   fn.lpTemplateName    = NULL;

   already_sent = FALSE;

   if (!GetSaveFileName(&fn))
      {
      return FALSE;
      }
   
   strcpy(string, fn_buff);

   //
   // Extract directory portion of filename and save it to .INI record
   //

   C8 *last_slash = strrchr(string,'\\');

   if (last_slash != NULL)
      {
      C8 *src  = string;
      C8 *dest = INI_save_directory;

      while (1)
         {
         *dest++ = *src;

         if (src == last_slash)
            {
            *dest++ = 0;
            break;
            }

         src++;
         }
      }

   //
   // Force string to end in a valid suffix
   //

   C8 *suffixes[] = 
      {
      ".pnp",
      ".gif",
      ".tga",
      ".bmp",
      ".pcx"
      };

   S32 l = strlen(string);

   if (fn.nFilterIndex-1 < ARY_CNT(suffixes))
      {
      BOOL valid = FALSE;

      for (S32 i=0; i < ARY_CNT(suffixes); i++)
         {
         if (l >= 4)
            {
            if (!_stricmp(&string[l-4],suffixes[i]))
               {
               valid = TRUE;
               }
            }
         }

      if (!valid)
         {
         strcat(string, suffixes[fn.nFilterIndex-1]);   // (use first suffix by default)

         FILE *test = fopen(string,"rb");

         if (test != NULL)
            {
            fclose(test);

            if (!option_box("Save","%s already exists.\nDo you want to replace it?", string))
               {
               return FALSE;
               }
            }
         }
      }

   return TRUE;
}

//****************************************************************************
//*                                                                          *
//* Get filename to export                                                   *
//*                                                                          *
//****************************************************************************

BOOL get_export_filename(C8 *string)
{
   OPENFILENAME fn;
   C8           fn_buff[MAX_PATH];

   fn_buff[0] = 0;

   if (strlen(string))     // (Force use of INI_save_directory by removing path portion of
      {                    // supplied default filename)
      C8 *f = strrchr(string,'\\');

      if (f != NULL)
         f = &f[1];
      else
         f = string;

      strcpy(fn_buff, f);
      }

   memset(&fn, 0, sizeof(fn));
   fn.lStructSize       = sizeof(fn);
   fn.hwndOwner         = hWnd;
   fn.hInstance         = NULL;

   fn.lpstrFilter       = "\
Comma-separated offset,amplitude list (*.CSV)\0*.CSV\0\
MS-DOS text with line breaks (*.TXT)\0*.TXT\0\
";

   fn.lpstrCustomFilter = NULL;
   fn.nMaxCustFilter    = 0;
   fn.nFilterIndex      = 1;
   fn.lpstrFile         = fn_buff;
   fn.nMaxFile          = sizeof(fn_buff);
   fn.lpstrFileTitle    = NULL;
   fn.nMaxFileTitle     = 0;

   if (INI_save_directory[0])
      {
      fn.lpstrInitialDir = INI_save_directory;
      }
   else
      {
      fn.lpstrInitialDir = NULL;
      }

   fn.lpstrTitle        = "Export";
   fn.Flags             = OFN_EXPLORER |
                          OFN_LONGNAMES |
                          OFN_ENABLESIZING |
                          OFN_NOCHANGEDIR |
                          OFN_ENABLEHOOK |
                          OFN_PATHMUSTEXIST |
                          OFN_OVERWRITEPROMPT |
                          OFN_HIDEREADONLY;
   fn.nFileOffset       = 0;
   fn.nFileExtension    = 0;
   fn.lpstrDefExt       = NULL;
   fn.lCustData         = NULL;
   fn.lpfnHook          = SFNHookProc;
   fn.lpTemplateName    = NULL;

   already_sent = FALSE;

   if (!GetSaveFileName(&fn))
      {
      return FALSE;
      }
   
   strcpy(string, fn_buff);

   //
   // Extract directory portion of filename and save it to .INI record
   //

   C8 *last_slash = strrchr(string,'\\');

   if (last_slash != NULL)
      {
      C8 *src  = string;
      C8 *dest = INI_save_directory;

      while (1)
         {
         *dest++ = *src;

         if (src == last_slash)
            {
            *dest++ = 0;
            break;
            }

         src++;
         }
      }

   //
   // Force string to end in a valid suffix
   //
   // OFN_OVERWRITEPROMPT isn't effective if the suffix is applied here, so we
   // have to emulate it
   //

   C8 *suffixes[] = 
      {
      ".csv",
      ".txt"
      };

   S32 l = strlen(string);

   if (fn.nFilterIndex-1 < ARY_CNT(suffixes))
      {
      BOOL valid = FALSE;

      for (S32 i=0; i < ARY_CNT(suffixes); i++)
         {
         if (l >= 4)
            {
            if (!_stricmp(&string[l-4],suffixes[i]))
               {
               valid = TRUE;
               }
            }
         }

      if (!valid)
         {
         strcat(string, suffixes[fn.nFilterIndex-1]);   // (use first suffix by default)

         FILE *test = fopen(string,"rb");

         if (test != NULL)
            {
            fclose(test);

            if (!option_box("Save","%s already exists.\nDo you want to replace it?", string))
               {
               return FALSE;
               }
            }
         }
      }

   return TRUE;
}

//****************************************************************************
//*                                                                          *
//* Get filename of plotter file to load                                     *
//*                                                                          *
//****************************************************************************

BOOL get_load_filename(C8 *string)
{
   OPENFILENAME fn;
   C8           fn_buff[MAX_PATH];

   fn_buff[0] = 0;

   if (strlen(string))
      {
      strcpy(fn_buff, string);
      }

   memset(&fn, 0, sizeof(fn));
   fn.lStructSize       = sizeof(fn);
   fn.hwndOwner         = hWnd;
   fn.hInstance         = NULL;
   fn.lpstrFilter       = "Noise plot files (*.PNP)\0*.PNP\0All files (*.*)\0*.*\0";
   fn.lpstrCustomFilter = NULL;
   fn.nMaxCustFilter    = 0;
   fn.nFilterIndex      = 1;
   fn.lpstrFile         = fn_buff;
   fn.nMaxFile          = sizeof(fn_buff);
   fn.lpstrFileTitle    = NULL;
   fn.nMaxFileTitle     = 0;

   if (INI_save_directory[0])
      {
      fn.lpstrInitialDir = INI_save_directory;
      }
   else
      {
      fn.lpstrInitialDir = NULL;
      }

   fn.lpstrTitle        = "Load Noise Plot File";
   fn.Flags             = OFN_EXPLORER |
                          OFN_LONGNAMES |
                          OFN_ENABLESIZING |
                          OFN_NOCHANGEDIR |
                          OFN_ENABLEHOOK |
                          OFN_PATHMUSTEXIST |
                          OFN_HIDEREADONLY;
   fn.nFileOffset       = 0;
   fn.nFileExtension    = 0;
   fn.lpstrDefExt       = NULL;
   fn.lCustData         = NULL;
   fn.lpfnHook          = OFNHookProc;
   fn.lpTemplateName    = NULL;

   already_sent = FALSE;

   if (!GetOpenFileName(&fn))
      {
      return FALSE;
      }
   
   strcpy(string, fn_buff);

   //
   // Extract directory portion of filename and save it to .INI record
   //

   C8 *last_slash = strrchr(string,'\\');

   if (last_slash != NULL)
      {
      C8 *src  = string;
      C8 *dest = INI_save_directory;

      while (1)
         {
         *dest++ = *src;

         if (src == last_slash)
            {
            *dest++ = 0;
            break;
            }

         src++;
         }
      }

   //
   // Force string to end in a valid suffix
   //

   C8 *suffixes[] = 
      {
      ".pnp",
      };

   if (fn.nFilterIndex-1 < ARY_CNT(suffixes))
      {
      S32 l = strlen(string);

      for (S32 i=0; i < ARY_CNT(suffixes); i++)
         {
         if (l >= 4)
            {
            if (!_stricmp(&string[l-4],suffixes[i]))
               {
               return TRUE;
               }
            }
         }

      strcat(string, suffixes[fn.nFilterIndex-1]);
      }

   return TRUE;
}

//****************************************************************************
//*                                                                          
//* Support routines to dump bitmap to printer
//*
//* Source: http://support.microsoft.com/default.aspx?scid=kb;EN-US;q186736
//*                                                                          
//****************************************************************************

// Return a HDC for the default printer.
HDC GetPrinterDC(void)
{
   PRINTDLG pdlg;
   memset( &pdlg, 0, sizeof( PRINTDLG ) );
   pdlg.lStructSize = sizeof( PRINTDLG );
   pdlg.Flags = PD_RETURNDEFAULT | PD_RETURNDC;
   PrintDlg( &pdlg );
   return pdlg.hDC;
}

// Create a copy of the current system palette.
HPALETTE GetSystemPalette()
{
    HDC hDC;
    HPALETTE hPal;
    LPLOGPALETTE lpLogPal;

    // Get a DC for the desktop.
    hDC = GetDC(NULL);

    // Check to see if you are a running in a palette-based video mode.
    if (!(GetDeviceCaps(hDC, RASTERCAPS) & RC_PALETTE)) {
        ReleaseDC(NULL, hDC);
        return NULL;
    }

    // Allocate memory for the palette.
    lpLogPal = (tagLOGPALETTE *) GlobalAlloc(GPTR, sizeof(LOGPALETTE) + 256 *
                           sizeof(PALETTEENTRY));
    if (!lpLogPal)
        return NULL;

    // Initialize.
    lpLogPal->palVersion = 0x300;
    lpLogPal->palNumEntries = 256;

    // Copy the current system palette into the logical palette.
    GetSystemPaletteEntries(hDC, 0, 256,
        (LPPALETTEENTRY)(lpLogPal->palPalEntry));

    // Create the palette.
    hPal = CreatePalette(lpLogPal);

    // Clean up.
    GlobalFree(lpLogPal);
    ReleaseDC(NULL, hDC);

    return hPal;
}

// Create a 16-bit-per-pixel surface.
HBITMAP Create16BPPDIBSection(HDC hDC, int iWidth, int iHeight, U16 **ptr)
{
    BITMAPINFO bmi;
    HBITMAP hbm;

    // Initialize to 0s.
    ZeroMemory(&bmi, sizeof(bmi));

    // Initialize the header.
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = iWidth;
    bmi.bmiHeader.biHeight = iHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 16;
    bmi.bmiHeader.biCompression = BI_RGB;

    // Create the surface.
    hbm = CreateDIBSection(hDC, &bmi, DIB_RGB_COLORS, (void **) ptr, NULL, 0);

    return(hbm);
}

// Print the entire contents of the specified window to the default printer

BOOL PrintWindowToDC(HWND hWnd)
{
    HBITMAP hbm;
    HDC     hdcPrinter;
    HDC     hdcMemory;
    HDC     hdcWindow;
    int     iWidth;
    int     iHeight;
    DOCINFO di;
    RECT    rc,wrc,crc;
    DIBSECTION ds;
    HPALETTE   hPal;

    // Do you have a valid window?
    if (!IsWindow(hWnd))
        return FALSE;

    // Get a HDC for the default printer.
    hdcPrinter = GetPrinterDC();
    if (!hdcPrinter)
        return FALSE;

    // Get the HDC for the entire window.
    hdcWindow  = GetWindowDC(hWnd);

    // Get the rectangles bounding the window and client area.
    GetWindowRect(hWnd, &wrc);

    crc = *client_screen_rect(hWnd);

    int cw = crc.right-crc.left+1;
    int ch = crc.bottom-crc.top+1;

    rc.left   = crc.left - wrc.left;
    rc.top    = crc.top  - wrc.top;
    rc.right  = rc.left + cw - 1;
    rc.bottom = rc.top + ch - 1;

    //
    // Get the resolution of the printer device in square pixels
    //

    iWidth  = GetDeviceCaps(hdcPrinter, HORZRES);
    iHeight = GetDeviceCaps(hdcPrinter, VERTRES);

    //
    // Maintain aspect ratio of printed image
    //

    if (iHeight > iWidth)
      {
      //
      // Page is taller than it is wide (portrait mode)
      //
      // Scale the height to conform to the client area's size
      //

      iHeight = iWidth * ch / cw;
      }
   else
      {
      //
      // Page is wider than it is tall (landscape mode)
      //
      // Leave the width and height alone for a full-page printout
      //
      }

    U16 *ptr;

    // Create the intermediate drawing surface at client resolution.

    S32 width  = rc.right-rc.left+1;
    S32 height = rc.bottom-rc.top+1;

    hbm = Create16BPPDIBSection(hdcWindow, width, height, &ptr);

    if (!hbm) {
        DeleteDC(hdcPrinter);
        ReleaseDC(hWnd, hdcWindow);
        return FALSE;
    }

    // Prepare the surface for drawing.
    hdcMemory = CreateCompatibleDC(hdcWindow);
    SelectObject(hdcMemory, hbm);

    // Get the current system palette.
    hPal = GetSystemPalette();

    // If a palette was returned.
    if (hPal) {
        // Apply the palette to the source DC.
        SelectPalette(hdcWindow, hPal, FALSE);
        RealizePalette(hdcWindow);

        // Apply the palette to the destination DC.
        SelectPalette(hdcMemory, hPal, FALSE);
        RealizePalette(hdcMemory);
    }

    // Copy the window contents to the memory surface.
    BitBlt(hdcMemory, 0, 0, rc.right-rc.left+1, rc.bottom-rc.top+1,
        hdcWindow, rc.left, rc.top, SRCCOPY);

    // Prepare the DOCINFO.
    ZeroMemory(&di, sizeof(di));
    di.cbSize = sizeof(di);
    di.lpszDocName = "Window Contents";

    // Initialize the print job.
    if (StartDoc(hdcPrinter, &di) > 0) {

        // Prepare to send a page.
        if (StartPage(hdcPrinter) > 0) {

            // Retrieve the information describing the surface.
            GetObject(hbm, sizeof(DIBSECTION), &ds);

            // Print the contents of the surface.
            StretchDIBits(hdcPrinter,
                0, 0, iWidth, iHeight,
                0, 0, rc.right-rc.left+1, rc.bottom-rc.top+1,
                ds.dsBm.bmBits,
                (LPBITMAPINFO)&ds.dsBmih,
                DIB_RGB_COLORS,
                SRCCOPY);

            // Let the driver know the page is done.
            EndPage(hdcPrinter);
        }

        // Let the driver know the document is done.
        EndDoc(hdcPrinter);
    }

    // Clean up the objects you created.
    DeleteDC(hdcPrinter);
    DeleteDC(hdcMemory);
    ReleaseDC(hWnd, hdcWindow);
    DeleteObject(hbm);
    if (hPal)
        DeleteObject(hPal);

    return TRUE;
}

// Print the entire contents of the SAL back buffer to the default printer

BOOL PrintBackBufferToDC(void)
{
    HBITMAP    hbm;
    HDC        hdcPrinter;
    HDC        hdcMemory;
    HDC        hdcWindow;
    int        iWidth;
    int        iHeight;
    S32        cw;
    S32        ch;
    DOCINFO    di;
    DIBSECTION ds;
    HPALETTE   hPal;

    // Get a HDC for the default printer.
    hdcPrinter = GetPrinterDC();
    if (!hdcPrinter)
        return FALSE;

    // Get back buffer dimensions and DC
    SAL_get_back_buffer_DC(&hdcWindow);

    SAL_client_resolution(&cw, &ch);

    //
    // Get the resolution of the printer device in square pixels
    //

    iWidth  = GetDeviceCaps(hdcPrinter, HORZRES);
    iHeight = GetDeviceCaps(hdcPrinter, VERTRES);

    //
    // Maintain aspect ratio of printed image
    //

    if (iHeight > iWidth)
      {
      //
      // Page is taller than it is wide (portrait mode)
      //
      // Scale the height to conform to the client area's size
      //

      iHeight = iWidth * ch / cw;
      }
   else
      {
      //
      // Page is wider than it is tall (landscape mode)
      //
      // Leave the width and height alone for a full-page printout
      //
      }

    U16 *ptr;

    // Create the intermediate drawing surface at client resolution.

    hbm = Create16BPPDIBSection(hdcWindow, cw, ch, &ptr);

    if (!hbm) {
        DeleteDC(hdcPrinter);
        SAL_release_back_buffer_DC(hdcWindow);
        return FALSE;
    }

    // Prepare the surface for drawing.
    hdcMemory = CreateCompatibleDC(hdcWindow);
    SelectObject(hdcMemory, hbm);

    // Get the current system palette.
    hPal = GetSystemPalette();

    // If a palette was returned.
    if (hPal) {
        // Apply the palette to the source DC.
        SelectPalette(hdcWindow, hPal, FALSE);
        RealizePalette(hdcWindow);

        // Apply the palette to the destination DC.
        SelectPalette(hdcMemory, hPal, FALSE);
        RealizePalette(hdcMemory);
    }

    // Copy the window contents to the memory surface.
    BitBlt(hdcMemory, 0, 0, cw, ch,
        hdcWindow, 0, 0, SRCCOPY);

    // Prepare the DOCINFO.
    ZeroMemory(&di, sizeof(di));
    di.cbSize = sizeof(di);
    di.lpszDocName = "Window Contents";

    // Initialize the print job.
    if (StartDoc(hdcPrinter, &di) > 0) {

        // Prepare to send a page.
        if (StartPage(hdcPrinter) > 0) {

            // Retrieve the information describing the surface.
            GetObject(hbm, sizeof(DIBSECTION), &ds);

            // Print the contents of the surface.
            StretchDIBits(hdcPrinter,
                0, 0, iWidth, iHeight,
                0, 0, cw,     ch,
                ds.dsBm.bmBits,
                (LPBITMAPINFO)&ds.dsBmih,
                DIB_RGB_COLORS,
                SRCCOPY);

            // Let the driver know the page is done.
            EndPage(hdcPrinter);
        }

        // Let the driver know the document is done.
        EndDoc(hdcPrinter);
    }

    // Clean up the objects you created.
    DeleteDC(hdcPrinter);
    DeleteDC(hdcMemory);
    SAL_release_back_buffer_DC(hdcWindow);
    DeleteObject(hbm);
    if (hPal)
        DeleteObject(hPal);

    return TRUE;
}

//****************************************************************************
//*                                                                          
//*  Refresh screen_window                                                   
//*                                                                          
//****************************************************************************

void refresh(void)
{
   //
   // Lock the buffer and validate the VFX_WINDOW
   //

   VFX_lock_window_surface(screen_window,VFX_BACK_SURFACE);

   //
   // Copy entire staging pane to screen_window
   // 

   VFX_pane_copy(stage,0,0,screen,0,0,NO_COLOR);

   //
   // Release surface and perform page flip
   //

   VFX_unlock_window_surface(screen_window, TRUE);
}

//****************************************************************************
//
// Render data from files specified on command line
//
// Optionally, the plot-selection cursor can be disabled, e.g. for saving or
// printing the image
//
//****************************************************************************

SOURCE *global_source = NULL;
PANE   *global_pane   = NULL;

void __cdecl trace_style_CB(S32 x, S32 y)
{
   if (!((x >> 1) & 0x03))
      {
      VFX_pixel_write(global_pane, x, y, global_source->color);
      VFX_pixel_write(global_pane, x, y+1, global_source->color);
      }
}

void clip_spurs(SOURCE *S, S32 width)
{
   DOUBLE *A = S->VD;

   S32 window = width / 25;

   for (S32 c=0; c < width; c++)
      {
      if (!S->VV[c])
         {
         continue;
         }

      DOUBLE left_min  = 100000.0;
      DOUBLE right_min = 100000.0;

      DOUBLE avg = 0.0;
      S32    cnt = 0;

      for (S32 j=1; j < window; j++)
         {
         S32 left  = c-j; if (left < 0)       left  = 0;
         S32 right = c+j; if (right >= width) right = width-1;

         if (S->VV[left]) 
            {
            avg += A[left];
            ++cnt;

            if (A[left] < left_min)
               {
               left_min = A[left];
               }
            }
            
         if (S->VV[right])
            {
            avg += A[right];
            ++cnt;

            if (A[right] < right_min) 
               {
               right_min = A[right];
               }
            }
         }                    

      if (((A[c] - left_min)  >= spur_dB) && 
          ((A[c] - right_min) >= spur_dB))
         {
         A[c] = avg / cnt;
         }
      }
}

void render_source_list(BOOL show_selection_cursor)
{
   //
   // Legend entry (derived from data source)
   //

   struct LEGEND_ENTRY
      {
      S32 source_index;

      C8  text  [MAX_LEGEND_FIELDS][1024];
      S32 width [MAX_LEGEND_FIELDS];
      };

   static LEGEND_ENTRY legend_entry_list[MAX_SOURCES];
   S32 n_legend_entries;

   S32 i,s,col,display_top = GetSystemMetrics(SM_CYMENU);

   // ===============================
   // Clear background to white
   // ===============================

   VFX_pane_wipe(stage, RGB_TRIPLET(255,255,255));

   if (n_data_sources == 0)
      {
      return;
      }

   // ===============================
   // Establish graph X properties
   // ===============================

   S32 x0 = 60;
   S32 x1 = RES_X - 60 - 1;

   if (x1 > x0 + MAX_GRAPH_WIDTH - 1)
      {
      x1 = x0 + MAX_GRAPH_WIDTH - 1;
      }

   S32 width  = (x1 - x0) + 1;

   spot_range_x0 = x0 - 8;
   spot_range_x1 = x1 + 8;

   trace_area->x0 = x0;
   trace_area->x1 = x1;

   // ===============================
   // Establish graph frequency scale
   // ===============================

   S32 min_decade_exp = INT_MAX;
   S32 max_decade_exp = INT_MIN;

   for (i=0; i < n_data_sources; i++)
      {
      SOURCE *S = &data_source_list[i];

      if (S->max_decade_exp > max_decade_exp) max_decade_exp = S->max_decade_exp;
      if (S->min_decade_exp < min_decade_exp) min_decade_exp = S->min_decade_exp;
      }

   S32 n_decades = (max_decade_exp + 1) - min_decade_exp;   // (Add 1 to force labelling of beginning of last decade) 

   static SINGLE frequency[MAX_GRAPH_WIDTH];

   SINGLE last_decade_exp = -100;

   for (col=0; col < width; col++)
      {
      SINGLE x_fract = SINGLE(col) / (x1 - x0);
      SINGLE decade_exp = (x_fract * n_decades) + min_decade_exp;

      if ((SINGLE) floor(decade_exp) != (SINGLE) floor(last_decade_exp))
         {
         decade_exp = (SINGLE) round_to_nearest(decade_exp);
         }

      frequency[col] = (SINGLE) pow(10.0F, decade_exp);
      last_decade_exp = decade_exp;
      }

   // ===============================
   // Compose trace cache contents, if needed
   // ===============================

   if (invalid_trace_cache)
      {
      for (s=0; s < n_data_sources; s++)
         {
         SOURCE *S = &data_source_list[s];

         for (col=0; col < width; col++)
            {
            S->VV[col] = S->sampled_value(frequency[col], &S->VD[col]);
            }

         if (spur_reduction)
            {
            clip_spurs(S, width);
            }

         S->min_dBc_Hz = 300;
         S->max_dBc_Hz = -300;

         for (col=0; col < width; col++)
            {
            if (S->VV[col])
               {
               if (S->VD[col] > S->max_dBc_Hz) S->max_dBc_Hz = S->VD[col];
               if (S->VD[col] < S->min_dBc_Hz) S->min_dBc_Hz = S->VD[col];

               S->VW[col] = pow(10.0, S->VD[col] / 10.0);
               }
            }
         }

      invalid_trace_cache = FALSE;
      }

   // ===============================
   // Establish graph Y properties, adjusting the height as needed
   // to display all sources
   // ===============================

   S32 table_height = 1 + 2 + font_height + 2 + 1 + 2 
                    + 2 + 1;

   S32 G_bottom   = INT_MAX;
   S32 G_top      = INT_MIN;

   DOUBLE min_dBc_Hz = 300;
   DOUBLE max_dBc_Hz = -300;

   S32 clip_enabled = TRUE;

   for (i=0; i < n_data_sources; i++)
      {
      SOURCE *S = &data_source_list[i];

      if (S->max_dBc_Hz > max_dBc_Hz) max_dBc_Hz = S->max_dBc_Hz;
      if (S->min_dBc_Hz < min_dBc_Hz) min_dBc_Hz = S->min_dBc_Hz;

      S->last_sampled_y  = -1;
      S->last_smoothed_y = -1;

      if ((S->VBW_factor > 0.00001F) && (S->VBW_factor < 1.0F)) clip_enabled = FALSE;
      }

   if ((allow_clipping) && (clip_enabled) && ((max_dBc_Hz - min_dBc_Hz) > 30.0))
      {
      min_dBc_Hz += 10.0;         // Try to avoid wasting space at top and bottom of the graph, even
      max_dBc_Hz -= 5.0;          // if we have to clip a bit of ghosted noise to do it
      }                           // (Don't enable clipping in plots with lots of video filtering, or the whole trace may disappear)

   G_bottom = S32(min_dBc_Hz - 0.5F);
   G_top    = S32(max_dBc_Hz - 0.5F);

   G_bottom -= (10 - ((((G_bottom / 10) * 10) - G_bottom)));
   G_top    += ((((G_top / 10) * 10) - G_top));

   S32 y0 = 40 + display_top;
   S32 y1 = RES_Y - ((table_height + (n_data_sources * (font_height + 2))) + 60);

   S32 n_Y_divisions = (G_top - G_bottom) / 10;

   if (n_Y_divisions <= 0) 
      {
      n_Y_divisions = 1;      // This is an invalid plot; try to give it a valid scale...
      G_top         = 0;
      G_bottom      = -10;
      }

   S32 Y_step = ((y1 - y0) + 1) / n_Y_divisions;

   y1 = y0 + (n_Y_divisions * Y_step);

   S32 height = (y1 - y0) + 1;

   spot_range_y0 = y0 - 8;
   spot_range_y1 = y1 + 8;

   trace_area->y0 = y0;
   trace_area->y1 = y1;

   // ===============================
   // Draw decade scale and record
   // locations of spot-frequency columns
   // ===============================

   S32    cur_int_decade_exp        = -100;
   SINGLE decade_label_Hz           = -100.0F;
   SINGLE next_light_line_frequency = 0.0F;

   n_spot_cols = 0;     // (# of columns eligible for hit testing)

   S32 spot_in_col = -1;
   S32 L_column    = 0;
   S32 U_column    = width-1;
   S32 L_spot_col  = -1;
   S32 U_spot_col  = -1;

   //
   // Draw ghosted trace behind graticule
   //

   for (col=0; col < width; col++)
      {
      S32 x = col + x0;

      SINGLE x_fract = SINGLE(col) / (x1 - x0);
      SINGLE decade_exp = (x_fract * n_decades) + min_decade_exp;

      BOOL at_RMS_bounds = FALSE;
      BOOL at_spot = FALSE;

      //
      // Draw ghost-sampled traces behind graticule (unless turned off)
      //

      if (ghost_sample)
         {
         for (s=0; s < n_data_sources; s++)
            {
            if ((mode == M_BROWSE) && (s != current_data_source))
               {
               continue;
               }

            SOURCE *S = &data_source_list[s];

            if ((mode == M_OVERLAY) && (!S->visible))
               {
               continue;
               }

            if (S->VV[col])
               {
               DOUBLE Y_percent = (S->VD[col] - G_bottom) / (G_top - G_bottom);

               S32 y = y1 - S32(Y_percent * DOUBLE(height));

               if (S->last_sampled_y != -1)
                  {
                  VFX_line_draw(trace_area,
                                x                 - x0,
                                S->last_sampled_y - y0,
                                x + 1             - x0,
                                y                 - y0,
                                LD_DRAW,
                                pen_colors[contrast][s % N_PEN_COLORS]);
                  }

               S->last_sampled_y = y;
               }
            }
         }

      //
      // Check for decade crossings
      //

      S32 int_decade_exp = round_to_nearest(floor(decade_exp));

      if (int_decade_exp != cur_int_decade_exp)
         {
         static C8 *decade_labels[] = { "1 uHz",
                                        "10 uHz",
                                        "100 uHz",
                                        "0.001 Hz",
                                        "0.01 Hz",
                                        "0.1 Hz",
                                        "1 Hz",
                                        "10 Hz",
                                        "100 Hz",
                                        "1 kHz",
                                        "10 kHz",
                                        "100 kHz",
                                        "1 MHz",
                                        "10 MHz",
                                        "100 MHz" };


         decade_label_Hz = pow(10.0F,int_decade_exp);

         next_light_line_frequency = frequency[col] + decade_label_Hz;

         spot_col_freq[n_spot_cols] = decade_label_Hz;
         spot_col_x   [n_spot_cols] = x;

         at_RMS_bounds = ((epsilon_match(spot_col_freq[n_spot_cols],L_offset_Hz)) || (epsilon_match(spot_col_freq[n_spot_cols],U_offset_Hz))) 
                           && RMS_needed;

         at_spot = epsilon_match(spot_col_freq[n_spot_cols], spot_offset_Hz) && INI_show_legend_field[T_SPOT_AMP];

         if (!(at_spot || at_RMS_bounds))
            {
            VFX_line_draw(stage,
                          x,
                          y0,
                          x,
                          y1,
                          LD_DRAW,
                          RGB_TRIPLET(0,0,0));

            if (contrast==2)
               {
               VFX_line_draw(stage,
                             x-1,
                             y0,
                             x-1,
                             y1,
                             LD_DRAW,
                             RGB_TRIPLET(0,0,0));
               }
            }
         else
            {
            if (at_spot)
               {
               spot_in_col = col;

               S32 step = RES_Y / 100;

               for (S32 y=y0; y < y1; y += (step * 2))
                  {
                  VFX_line_draw(stage,
                                x,
                                y,
                                x,
                                min(y + step, y1),
                                LD_DRAW,
                                RGB_TRIPLET(255,0,0));

                  if (contrast==2)
                     {
                     VFX_line_draw(stage,
                                   x-1,
                                   y,
                                   x-1,
                                   min (y + step, y1),
                                   LD_DRAW,
                                   RGB_TRIPLET(255,0,0));
                     }
                  }
               }

            if (at_RMS_bounds)
               {
               SINGLE col_freq = spot_col_freq[n_spot_cols];

               if (epsilon_match(col_freq, L_offset_Hz)) { L_column = col; L_spot_col = n_spot_cols; }
               if (epsilon_match(col_freq, U_offset_Hz)) { U_column = col; U_spot_col = n_spot_cols; } 

               S32 step = RES_Y / 100;

               for (S32 y=y0; y < y1; y += (step * 2))
                  {
                  VFX_line_draw(stage,
                                x,
                                min(y + step, y1),
                                x,
                                min(y + (step * 2), y1),
                                LD_DRAW,
                                RGB_TRIPLET(0,0,255));

                  if (contrast==2)
                     {
                     VFX_line_draw(stage,
                                   x-1,
                                   min(y + step, y1),
                                   x-1,
                                   min(y + (step * 2), y1),
                                   LD_DRAW,
                                   RGB_TRIPLET(0,0,255));
                     }
                  }
               }
            }

         C8 *T = decade_labels[int_decade_exp - MIN_DECADE_EXP];

         S32 sw = string_width(T);
             
         VFX_string_draw(stage,
                         x - (sw / 2),
                         y1 + font_height,
                         VFX_default_system_font(),
                         T,
                         transparent_font_CLUT);

         n_spot_cols++;
         }

      cur_int_decade_exp = int_decade_exp;

      //
      // Time to draw a new intra-decade line?
      //

      if (frequency[col] >= next_light_line_frequency)
         {
         frequency[col] = next_light_line_frequency;

         spot_col_freq[n_spot_cols] = frequency[col]; 
         spot_col_x   [n_spot_cols] = x;

         at_RMS_bounds = (epsilon_match(spot_col_freq[n_spot_cols], L_offset_Hz) || epsilon_match(spot_col_freq[n_spot_cols], U_offset_Hz)) 
                           && RMS_needed;

         at_spot = epsilon_match(spot_col_freq[n_spot_cols], spot_offset_Hz) && INI_show_legend_field[T_SPOT_AMP]; 

         next_light_line_frequency = frequency[col] + decade_label_Hz;

         if (!(at_spot || at_RMS_bounds))
            {
            S32 lvl=230;
            if (contrast==1) lvl=180;
            if (contrast==2) lvl=0;

            VFX_line_draw(stage,
                          x,
                          y0,
                          x,
                          y1,
                          LD_DRAW,
                          RGB_TRIPLET(lvl,lvl,lvl));
            }
         else
            {
            if (at_spot)
               {
               spot_in_col = col;

               S32 step = RES_Y / 100;

               for (S32 y=y0; y < y1; y += (step * 2))
                  {
                  VFX_line_draw(stage,
                                x,
                                y,
                                x,
                                min(y + step, y1),
                                LD_DRAW,
                                RGB_TRIPLET(255,150,150));
                  }
               }

            if (at_RMS_bounds)
               {
               if (epsilon_match(spot_col_freq[n_spot_cols], L_offset_Hz)) { L_column = col; L_spot_col = n_spot_cols; }  
               if (epsilon_match(spot_col_freq[n_spot_cols], U_offset_Hz)) { U_column = col; U_spot_col = n_spot_cols; }  

               S32 step = RES_Y / 100;

               for (S32 y=y0; y < y1; y += (step * 2))
                  {
                  VFX_line_draw(stage,
                                x,
                                min(y + step, y1),
                                x,
                                min(y + (step * 2), y1),
                                LD_DRAW,
                                RGB_TRIPLET(150,150,255));
                  }
               }
            }

         n_spot_cols++;
         }
      }

   //
   // Draw horizontal lines
   //

   S32 dB_label = G_top;

   for (S32 y=y0; y <= y1; y += Y_step)
      {
      VFX_line_draw(stage,
                    x0,
                    y,
                    x1,
                    y,
                    LD_DRAW,
                    RGB_TRIPLET(0,0,0));

      sprintf(text,"%d",dB_label);

      S32 sw = string_width(text);
             
      VFX_string_draw(stage,
                      x0 - (sw + em_width),
                      y - 4,
                      VFX_default_system_font(),
                      text,
                      transparent_font_CLUT);

      dB_label -= 10;
      }

   // ===============================
   // For each visible data source...
   // ===============================

   for (s=0; s < n_data_sources; s++)
      {
      SOURCE *S = &data_source_list[s];

      S->color     = pen_colors[2]             [s % N_PEN_COLORS];
      S->sat_color = pen_colors[(contrast+1)/2][s % N_PEN_COLORS];

      if ((mode == M_BROWSE) && (s != current_data_source))
         {     
         continue;
         }

      if ((mode == M_OVERLAY) && (!S->visible))
         {
         continue;
         }

      // -------------------------------
      // Create temporary array of sampled values at each screen column
      // -------------------------------
   
      memset(S->smoothed, 0, sizeof(S->smoothed));

      assert(width <= MAX_GRAPH_WIDTH);

      for (col=0; col < width; col++)
         {
         S->smoothed[col] = S->VD[col];
         S->offset  [col] = frequency[col];
         }

      // -------------------------------
      // Draw smoothed trace on top of graticule
      // -------------------------------

      for (col=0; col < width; col++)
         {
         S32 n_smooth = smooth_samples;
         if (n_smooth > S->smoothing_limit) n_smooth = S->smoothing_limit;

         S32 min_required = 0;
         S32 missing      = 0;

         if (!alt_smoothing)
            {                                      // Prone to endpoint droop when a
            min_required = n_smooth;               // steep slope is present at the 
            missing      = 0;                      // left boundary
            }                                      
         else
            {
            min_required = 0;

            S32 start = col - n_smooth;
            S32 end   = col + n_smooth;

            S32 missing_from_left = 0;
            S32 missing_from_right = 0;

            S32 max_x = width-1;
#if 1
            if (end > max_x) end = max_x;          // (don't enforce symmetry at right end -- it's not helpful there)
#else
            if (end > max_x) { missing_from_right = end-max_x; end = max_x; }
#endif
            if (start < 0)   { missing_from_left  = -start;    start = 0;     }

            for (i=start; i <= end; i++)           // Alternative algorithm ensures that moving average is 
               {                                   // applied symmetrically, to avoid endpoint droop                              
               if (!S->VV[i])                      // Noisier, since less smoothing is applied at the endpoints
                  {
                  if (i < col) ++missing_from_left;
                  if (i > col) ++missing_from_right;
                  }
               }

            missing = max(missing_from_left,missing_from_right);
            }

         S32 start  = col - (n_smooth-missing); 
         S32 end    = col + (n_smooth-missing); 

         if (start < 0)       start = 0;        
         if (end   > width-1) end   = width-1;

         DOUBLE acc = 0.0;                   
         S32    cnt = 0;                     

         for (i=start; i <= end; i++)
            {
            if (S->VV[i]) 
               {
               acc += S->VW[i];
               cnt++;
               }
            }

         if (cnt > min_required)
            {
            acc /= cnt;
         
            acc = 10.0 * log10(acc);

            S->smoothed[col] = acc;

            if (spot_in_col == col)
               {
               S->spot_dBc_Hz = acc;
               }

            DOUBLE Y_percent = (acc - G_bottom) / (G_top - G_bottom);

            S32 y = y1 - S32(Y_percent * DOUBLE(height));

            if (S->last_smoothed_y != -1)
               {
               S32 x = col + x0;

               global_source = S;
               global_pane   = trace_area;

               if (S->style == SLS_SOLID)
                  {
                  VFX_line_draw(trace_area,
                                x                  - x0,
                                S->last_smoothed_y - y0,
                                x + 1              - x0,
                                y                  - y0,
                                LD_DRAW,
                                S->color);

                  if (thick_trace)
                     {
                     VFX_line_draw(trace_area,
                                   x                      - x0,
                                   S->last_smoothed_y + 1 - y0,
                                   x + 1                  - x0,
                                   y+1                    - y0,
                                   LD_DRAW,
                                   S->color);
                     }
                  }
               else
                  {
                  VFX_line_draw(trace_area,
                                x                  - x0,
                                S->last_smoothed_y - y0,
                                x + 1              - x0,
                                y                  - y0,
                                LD_EXECUTE,
                                (U32) trace_style_CB);

                  if (thick_trace)
                     {
                     VFX_line_draw(trace_area,
                                   x                      - x0,
                                   S->last_smoothed_y + 1 - y0,
                                   x + 1                  - x0,
                                   y + 1                  - y0,
                                   LD_EXECUTE,
                                   (U32) trace_style_CB);
                     }
                  }
               }

            S->last_smoothed_y = y;
            }
         }

      // -------------------------------
      // Perform RMS integration if requested
      // -------------------------------

      if (!RMS_needed)
         {
         S->RMS_rads = 0.0;
         S->CNR      = 0.0;
         S->resid_FM = 0.0;
         S->jitter_s = 0.0;
         }
      else
         {
         // 
         // According to duncan.smith@pacbell.net, email of 30-Dec-05, integrated phase
         // noise in radians rms = 
         //
         //    sqrt(2 * sum(10 ^ ((VALUE - (VALUE  - VALUE   ) / 2) / 10) * (F    - F )))
         //                             i        i        i+1                 i+1    i
         //
         // ... where VALUE is in dBc/Hz, F is in Hz, and i is the ith measurement point
         //
         // It's not clear whether the interval-midpoint calculation should be performed before
         // or after the dBc/Hz values are converted to spectral density with the 10^(n/10) 
         // operation, so both techniques are implemented.  Values in the former case
         // are around 10% lower when evaluating unsmoothed noise traces
         //
         // Other references:
         //  
         //  HP AN270-2, "Automated Noise Sideband Measurements Using the HP 8568A Spectrum Analyzer"
         //  Maxim AN3359, "Clock (CLK) Jitter and Phase Noise Conversion"
         //  Zarlink Semiconductor, "Phase Noise and Jitter - A Primer for Digital Designers"
         //

         DOUBLE PM_sum = 0.0;
         DOUBLE FM_sum = 0.0;

         DOUBLE *src = S->smoothed;                // Can use either "val" or "smoothed" here
                                                   // (Choice of interpolation method doesn't
         if (L_column == U_column)                 // matter much if smoothed data used) 
            {
            if (S->VV[L_column])
               {
               PM_sum = pow(10.0, src[L_column] / 10.0);
               FM_sum = PM_sum * frequency[L_column] * frequency[L_column];
               }
            }
         else
            {
            for (i=L_column; i < U_column-1; i++)
               {
               if (S->VV[i] && S->VV[i+1])  
                  {
                  DOUBLE V,V1,V2;

                  DOUBLE F1 = frequency[i];
                  DOUBLE F2 = frequency[i+1];

                  DOUBLE F = F1 + ((F2 - F1) / 2.0);
#if 0
                  V1 = pow(10.0, src[i  ] / 10.0);    // Interpolate midpoint in linear density space,
                  V2 = pow(10.0, src[i+1] / 10.0);    // then integrate

                  V = V1 + ((V2 - V1) / 2.0);
#else
                  V1 = src[i  ];                      // Interpolate midpoint in dB space, then
                  V2 = src[i+1];                      // convert to linear space and integrate

                  V = pow(10.0, (V1 + ((V2 - V1) / 2.0)) / 10.0);
#endif
                  FM_sum += (V * (F * F) * (F2 - F1));
                  PM_sum += (V           * (F2 - F1));
                  }        
               }
            }

         S->CNR      = (PM_sum == 0.0) ? 0.0 : (10.0 * log10(PM_sum));
         S->RMS_rads = sqrt(PM_sum * 2.0);
         S->resid_FM = sqrt(FM_sum * 2.0);
         S->jitter_s = S->RMS_rads / (S->carrier_Hz * PI * 2.0);
         }
      }

   // ===============================
   // Draw integration cursor limits
   // ===============================

   if (RMS_needed)
      {
      transparent_font_CLUT[0] = (U16) RGB_NATIVE(255,255,255);
      transparent_font_CLUT[1] = (U16) RGB_NATIVE(0,0,255);
    
      sprintf(text,"%s Hz",log_Hz_string(L_offset_Hz));
      S32 lsw = string_width(text);
      S32 lx = spot_col_x[L_spot_col] - lsw;
    
      VFX_string_draw(stage,
                      lx,
                      y1-(2*font_height),
                      VFX_default_system_font(),
                      text,
                      transparent_font_CLUT);
    
    
      sprintf(text,"%s Hz",log_Hz_string(U_offset_Hz));
      S32 usw = string_width(text);
      S32 ux = spot_col_x[U_spot_col] - usw;
    
      S32 uy;
    
      if (ux - em_width <= spot_col_x[L_spot_col]) 
         uy = y1-(4*font_height);
      else
         uy = y1-(2*font_height);
    
      VFX_string_draw(stage,
                      ux,
                      uy,
                      VFX_default_system_font(),
                      text,
                      transparent_font_CLUT);
    
      transparent_font_CLUT[0] = (U16) RGB_TRANSPARENT;
      transparent_font_CLUT[1] = (U16) RGB_NATIVE(0,0,0);
      }

   // ===============================
   // Draw graticule frame
   // ===============================

   transparent_font_CLUT[0] = (U16) RGB_TRANSPARENT;
   transparent_font_CLUT[1] = (U16) RGB_NATIVE(0,0,255);

   C8 *text = "Phase Noise in dBc/Hz";

   S32 sw = string_width(text);
       
   VFX_string_draw(stage,
                   ((x1+x0)/2) - (sw / 2),
                   y0 - 18,
                   VFX_default_system_font(),
                   text,
                   transparent_font_CLUT);

   VFX_rectangle_draw(stage,
                      x0,y0,x1,y1,
                      LD_DRAW,
                      RGB_TRIPLET(0,0,0));

   VFX_rectangle_draw(stage,
                      x0-1,y0-1,x1+1,y1+1,
                      LD_DRAW,
                      RGB_TRIPLET(0,0,0));

   // ===============================
   // Clear legend entry list, and set up to record
   // maximum field widths among all visible sources
   // ===============================

   memset(legend_entry_list, 0, sizeof(legend_entry_list));
   n_legend_entries = 0;

   //
   // Minimum string width of each field is that of its caption
   // plus three leading and trailing spaces
   //

   memcpy(working_captions, legend_captions, sizeof(working_captions));

   sprintf(working_captions[T_SPOT_AMP  ], "dBc/Hz at %s Hz", log_Hz_string(spot_offset_Hz));
   sprintf(working_captions[T_JITTER_RADS],"RMS Noise Rads"); 
   sprintf(working_captions[T_JITTER_DEGS],"RMS Noise Degs"); 
   sprintf(working_captions[T_JITTER_SECS],"RMS Jitter"); 
   sprintf(working_captions[T_CNR],        "SSB C/N dB"); 
   sprintf(working_captions[T_RESID_FM],   "Residual FM Hz"); 

   S32 padding_width = string_width("      ");

   S32 max_width[MAX_LEGEND_FIELDS];

   for (i=0; i < MAX_LEGEND_FIELDS; i++)   
      {
      max_width[i] = string_width(working_captions[i]) + padding_width;
      }

   // ===============================
   // For all sources in list...
   // ===============================

   for (s=0; s < n_data_sources; s++)
      {
      //
      // If we're in 'browse mode', process only the currently-browsed 
      // source
      //

      if ((mode == M_BROWSE) && (s != current_data_source))
         {
         continue;
         }

      SOURCE *S = &data_source_list[s];

      //
      // Create legend entry for this source
      //

      LEGEND_ENTRY *L = &legend_entry_list[n_legend_entries++];

      L->source_index = s;

      sprintf(L->text[T_CAPTION],"   %s   ",S->caption);
      L->width[T_CAPTION] = string_width(L->text[T_CAPTION]) ;
      if (L->width[T_CAPTION] > max_width[T_CAPTION]) max_width[T_CAPTION] = L->width[T_CAPTION];
      
      sprintf(L->text[T_FILENAME],"   %s   ",S->filename);
      L->width[T_FILENAME] = string_width(L->text[T_FILENAME]) ;
      if (L->width[T_FILENAME] > max_width[T_FILENAME]) max_width[T_FILENAME] = L->width[T_FILENAME];

      sprintf(L->text[T_CARRIER_FREQ],"   %s   ",Hz_string(S->carrier_Hz));
      L->width[T_CARRIER_FREQ] = string_width(L->text[T_CARRIER_FREQ]) ;
      if (L->width[T_CARRIER_FREQ] > max_width[T_CARRIER_FREQ]) max_width[T_CARRIER_FREQ] = L->width[T_CARRIER_FREQ];

      sprintf(L->text[T_CARRIER_AMP],"   %.02f   ",S->carrier_dBm);
      L->width[T_CARRIER_AMP] = string_width(L->text[T_CARRIER_AMP]) ;
      if (L->width[T_CARRIER_AMP] > max_width[T_CARRIER_AMP]) max_width[T_CARRIER_AMP] = L->width[T_CARRIER_AMP];

      if (INI_show_legend_field[T_SPOT_AMP])
         {
         sprintf(L->text[T_SPOT_AMP],"   %.01f   ",S->spot_dBc_Hz);
         }
      else
         {
         L->text[T_SPOT_AMP][0] = 0;
         }

      L->width[T_SPOT_AMP] = string_width(L->text[T_SPOT_AMP]) ;
      if (L->width[T_SPOT_AMP] > max_width[T_SPOT_AMP]) max_width[T_SPOT_AMP] = L->width[T_SPOT_AMP];

      if (INI_show_legend_field[T_JITTER_DEGS])
         {
         sprintf(L->text[T_JITTER_DEGS],"   %.02E   ",S->RMS_rads * 57.29578F);
         }
      else
         {
         L->text[T_JITTER_DEGS][0] = 0;
         }

      L->width[T_JITTER_DEGS] = string_width(L->text[T_JITTER_DEGS]) ;
      if (L->width[T_JITTER_DEGS] > max_width[T_JITTER_DEGS]) max_width[T_JITTER_DEGS] = L->width[T_JITTER_DEGS];

      if (INI_show_legend_field[T_JITTER_RADS])
         {
         sprintf(L->text[T_JITTER_RADS],"   %.02E   ",S->RMS_rads);
         }
      else
         {
         L->text[T_JITTER_RADS][0] = 0;
         }

      L->width[T_JITTER_RADS] = string_width(L->text[T_JITTER_RADS]) ;
      if (L->width[T_JITTER_RADS] > max_width[T_JITTER_RADS]) max_width[T_JITTER_RADS] = L->width[T_JITTER_RADS];

      if (INI_show_legend_field[T_JITTER_SECS])
         {
         sprintf(L->text[T_JITTER_SECS],"   %.01E s   ",S->jitter_s);
         }
      else
         {
         L->text[T_JITTER_SECS][0] = 0;
         }

      L->width[T_JITTER_SECS] = string_width(L->text[T_JITTER_SECS]) ;
      if (L->width[T_JITTER_SECS] > max_width[T_JITTER_SECS]) max_width[T_JITTER_SECS] = L->width[T_JITTER_SECS];


      if (INI_show_legend_field[T_RESID_FM])
         {
         sprintf(L->text[T_RESID_FM],"   %.02E   ",S->resid_FM);
         }
      else
         {
         L->text[T_RESID_FM][0] = 0;
         }

      L->width[T_RESID_FM] = string_width(L->text[T_RESID_FM]) ;
      if (L->width[T_RESID_FM] > max_width[T_RESID_FM]) max_width[T_RESID_FM] = L->width[T_RESID_FM];

      if (INI_show_legend_field[T_CNR])
         {
         sprintf(L->text[T_CNR],"   %.01f   ",S->CNR);
         }
      else
         {
         L->text[T_CNR][0] = 0;
         }

      L->width[T_CNR] = string_width(L->text[T_CNR]) ;
      if (L->width[T_CNR] > max_width[T_CNR]) max_width[T_CNR] = L->width[T_CNR];

      if (fabs(S->ext_mult - 1.0) < 0.00001)
         sprintf(L->text[T_EXT_MULT],"   None   ");
      else
         if (fabs(S->ext_mult - S32(S->ext_mult + 0.5)) < 0.00001)
            sprintf(L->text[T_EXT_MULT],"   %dx   ",S32(S->ext_mult + 0.5));
         else
            sprintf(L->text[T_EXT_MULT],"   %.04lfx   ",S->ext_mult);

      L->width[T_EXT_MULT] = string_width(L->text[T_EXT_MULT]) ;
      if (L->width[T_EXT_MULT] > max_width[T_EXT_MULT]) max_width[T_EXT_MULT] = L->width[T_EXT_MULT];

      if (INI_show_legend_field[T_EXT_IF])
         if (S->ext_IF_Hz >= 0)
            sprintf(L->text[T_EXT_IF],"   %s   ",Hz_string(S->ext_IF_Hz));
         else
            sprintf(L->text[T_EXT_IF],"   None   ");
      else
         L->text[T_EXT_IF][0] = 0;

      L->width[T_EXT_IF] = string_width(L->text[T_EXT_IF]) ;
      if (L->width[T_EXT_IF] > max_width[T_EXT_IF]) max_width[T_EXT_IF] = L->width[T_EXT_IF];

      if (INI_show_legend_field[T_EXT_LO])
         if (S->ext_LO_Hz >= 0)
            sprintf(L->text[T_EXT_LO],"   %s   ",Hz_string(S->ext_LO_Hz));
         else
            sprintf(L->text[T_EXT_LO],"   None   ");
      else
         L->text[T_EXT_LO][0] = 0;

      L->width[T_EXT_LO] = string_width(L->text[T_EXT_LO]) ;
      if (L->width[T_EXT_LO] > max_width[T_EXT_LO]) max_width[T_EXT_LO] = L->width[T_EXT_LO];

      if (S->RF_atten >= 0)
         {
         sprintf(L->text[T_RF_ATTEN],"   %d   ",S->RF_atten);
         }
      else
         {
         sprintf(L->text[T_RF_ATTEN],"   N/A   ");
         }

      L->width[T_RF_ATTEN] = string_width(L->text[T_RF_ATTEN]) ;
      if (L->width[T_RF_ATTEN] > max_width[T_RF_ATTEN]) max_width[T_RF_ATTEN] = L->width[T_RF_ATTEN];

      if (S->clip_dB >= 0)
         {
         sprintf(L->text[T_CLIP_DB],"   %d   ",S->clip_dB);
         }
      else
         {
         sprintf(L->text[T_CLIP_DB]," ");
         }

      L->width[T_CLIP_DB] = string_width(L->text[T_CLIP_DB]);
      if (L->width[T_CLIP_DB] > max_width[T_CLIP_DB]) max_width[T_CLIP_DB] = L->width[T_CLIP_DB];

      if (S->VBW_factor < 0.00001F)
         {
         sprintf(L->text[T_VB_FACTOR],"   None   ");
         }
      else
         {
         sprintf(L->text[T_VB_FACTOR],"   %.02f   ",S->VBW_factor);
         }

      L->width[T_VB_FACTOR] = string_width(L->text[T_VB_FACTOR]) ;
      if (L->width[T_VB_FACTOR] > max_width[T_VB_FACTOR]) max_width[T_VB_FACTOR] = L->width[T_VB_FACTOR];

      sprintf(L->text[T_SWEEP_TIME],"   %s   ",S->elapsed_time);
      L->width[T_SWEEP_TIME] = string_width(L->text[T_SWEEP_TIME]) ;
      if (L->width[T_SWEEP_TIME] > max_width[T_SWEEP_TIME]) max_width[T_SWEEP_TIME] = L->width[T_SWEEP_TIME];

      sprintf(L->text[T_TIMESTAMP],"   %s   ",S->timestamp);
      L->width[T_TIMESTAMP] = string_width(L->text[T_TIMESTAMP]) ;
      if (L->width[T_TIMESTAMP] > max_width[T_TIMESTAMP]) max_width[T_TIMESTAMP] = L->width[T_TIMESTAMP];

      sprintf(L->text[T_INSTRUMENT],"   %s   ",S->instrument);
      L->width[T_INSTRUMENT] = string_width(L->text[T_INSTRUMENT]) ;
      if (L->width[T_INSTRUMENT] > max_width[T_INSTRUMENT]) max_width[T_INSTRUMENT] = L->width[T_INSTRUMENT];

      sprintf(L->text[T_INSTRUMENT_REV],"   %s   ",S->rev);
      L->width[T_INSTRUMENT_REV] = string_width(L->text[T_INSTRUMENT_REV]) ;
      if (L->width[T_INSTRUMENT_REV] > max_width[T_INSTRUMENT_REV]) max_width[T_INSTRUMENT_REV] = L->width[T_INSTRUMENT_REV];

      //
      // Update window title
      //

      C8 buffer[MAX_PATH+128];
      C8 current[MAX_PATH+128];

      if ((mode == M_OVERLAY) && (n_data_sources > 1))
         {
         sprintf(buffer,"%s - %d sources",szAppName, n_data_sources);
         }
      else
         {
         if (n_data_sources == 0)
            {
            strcpy(buffer,szAppName);
            }
         else
            {
            sprintf(buffer,"%s - %s",szAppName, data_source_list[s].filename);
            }
         }

      GetWindowText(hWnd, current, MAX_PATH + 127);

      if (strcmp(current,buffer))
         {
         SetWindowText(hWnd, buffer);
         }
      }

   // ===============================
   // Render legend table using selected visible fields
   // ===============================

   S32 table_width = 1 + 2 + 
                     2 + 1;

   S32 field_x0[MAX_LEGEND_FIELDS];
   S32 field_x1[MAX_LEGEND_FIELDS];
   U32 field_label_color[MAX_LEGEND_FIELDS];

   S32 x_offset = 1 + 2;

   for (i=0; i < MAX_LEGEND_FIELDS; i++)   
      {
      if ((i == T_JITTER_DEGS) || 
          (i == T_JITTER_RADS) || 
          (i == T_JITTER_SECS) || 
          (i == T_RESID_FM)    || 
          (i == T_CNR))
         {
         field_label_color[i] = RGB_NATIVE(0,0,255);
         }
      else if (i == T_SPOT_AMP)
         {
         field_label_color[i] = RGB_NATIVE(0,0,0);
         }
      else
         {
         field_label_color[i] = RGB_NATIVE(0,0,0);
         }

      field_x0[i] = x_offset;

      if (INI_show_legend_field[i])
         {
         table_width += max_width[i];
         x_offset    += max_width[i];
         }

      field_x1[i] = x_offset-1;
      }

   table_height += (n_legend_entries * (font_height + 2));

   S32 bottom_of_graticule_area = (y1 + (font_height * 2));

   S32 space_height = RES_Y - bottom_of_graticule_area;

   S32 table_x0 = (RES_X - table_width) / 2;
   S32 table_x1 = table_x0 + table_width;

   S32 table_y0 = bottom_of_graticule_area + ((space_height - table_height) / 2);
   S32 table_y1 = table_y0 + table_height;

   for (i=0; i < MAX_LEGEND_FIELDS; i++)   
      {
      field_x0[i] += table_x0;
      field_x1[i] += table_x0;
      }

   if (table_width > 16)
      {
      VFX_rectangle_draw(stage,
                         table_x0,table_y0 + font_height + 5,table_x1,table_y1,
                         LD_DRAW,
                         RGB_TRIPLET(0,0,0));

      VFX_rectangle_draw(stage,
                         table_x0,table_y0,table_x1,table_y1,
                         LD_DRAW,
                         RGB_TRIPLET(0,0,0));

      VFX_rectangle_draw(stage,
                         table_x0-1,table_y0-1,table_x1+1,table_y1+1,
                         LD_DRAW,
                         RGB_TRIPLET(0,0,0));

      for (i=0; i < MAX_LEGEND_FIELDS; i++)   
         {
         if (!INI_show_legend_field[i])
            {
            continue;
            }

         C8 *text = working_captions[i];

         S32 sx0 = field_x0[i] + (((field_x1[i] - field_x0[i] + 1) - string_width(text)) / 2);

         transparent_font_CLUT[1] = (U16) field_label_color[i];

         VFX_string_draw(stage,
                         sx0,
                         table_y0 + 3,
                         VFX_default_system_font(),
                         text,
                         transparent_font_CLUT);

         transparent_font_CLUT[1] = RGB_NATIVE(0,0,0);

         if (field_x1[i] + em_width < table_x1) 
            {
            VFX_line_draw(stage,
                          field_x1[i],
                          table_y0,
                          field_x1[i],
                          table_y1,
                          LD_DRAW,
                          RGB_TRIPLET(0,0,0));
            }
         }
      
      for (s=0; s < n_legend_entries; s++)
         {
         LEGEND_ENTRY *L = &legend_entry_list[s];

         SOURCE *S = &data_source_list[L->source_index];
         
         S32 string_y = table_y0 + 3 + font_height + 7 + ((font_height + 2) * s);

         if ((L->source_index == current_data_source) && 
             (n_data_sources > 1)                     &&
             (mode == M_OVERLAY)                      && 
             (show_selection_cursor))
            {
            for (i=0; i < 8; i++)         // Draw triangle cursor at left edge of table
               {
               VFX_line_draw(stage,
                             table_x0,
                             string_y,
                             table_x0 + i,
                             string_y + (font_height / 2),
                             LD_DRAW,
                             RGB_TRIPLET(0,0,0));

               VFX_line_draw(stage,
                             table_x0 + i,
                             string_y + (font_height / 2),
                             table_x0,
                             string_y + font_height,
                             LD_DRAW,
                             RGB_TRIPLET(0,0,0));
               }
            }

         for (i=0; i < MAX_LEGEND_FIELDS; i++)   
            {
            if (!INI_show_legend_field[i])
               {
               continue;
               }

            if ((mode == M_OVERLAY) && (!S->visible))
               transparent_font_CLUT[1] = (U16) S->sat_color;
            else
               transparent_font_CLUT[1] = (U16) S->color;

            VFX_string_draw(stage,
                            field_x0[i],
                            string_y,
                            VFX_default_system_font(),
                            L->text[i],
                            transparent_font_CLUT);
            }
         }

      transparent_font_CLUT[1] = (U16) RGB_NATIVE(0,0,0);
      }
}

//****************************************************************************
//
// Remove specified plot from list of renderable sources
//
//****************************************************************************

void close_source(S32 data_source)
{
   if (n_data_sources == 0)
      {
      return;
      }

   S32 j;

   for (j=data_source+1; j < n_data_sources; j++)
      {
      data_source_list[j-1] = data_source_list[j];
      }

   n_data_sources--;

   if (current_data_source >= n_data_sources)
      {
      current_data_source = n_data_sources-1;
      }

   if (!n_data_sources)
      {
      SetWindowText(hWnd, szAppName);
      }

   force_redraw = TRUE;
   invalid_trace_cache = TRUE;
}

//****************************************************************************
//
// Delete source .PNP file, closing any plots that depend on it
//
//****************************************************************************

void delete_source_by_filename(C8 *name)
{
   if (n_data_sources == 0)
      {
      return;
      }

   C8 filename[MAX_PATH];
   strcpy(filename, name);

   _unlink(filename);

   //
   // Close all sources that refer to this filename
   //

   for (S32 j=0; j < n_data_sources; j++)
      {
      if (!_stricmp(filename, data_source_list[j].filename))
         {
         close_source(j);
         }
      }
}

//****************************************************************************
//
// Update .INI file with changed user options, where applicable
//
//****************************************************************************

void update_INI(void)
{
   FILE *read_options = fopen(INI_path,"rt");

   if (read_options == NULL)
      {
      return;
      }

   C8 *write_filename = tmpnam(NULL);

   if (!write_filename[0])
      {
      fclose(read_options);
      return;
      }

   FILE *write_options = fopen(write_filename,"wt");

   if (write_options == NULL)
      {
      fclose(read_options);
      return;
      }

   while (1)
      {
      //
      // Read input line and make a copy
      //

      C8 linbuf[512];

      memset(linbuf,
             0,
             sizeof(linbuf));

      C8 *result = fgets(linbuf, 
                         sizeof(linbuf) - 1, 
                         read_options);

      if ((result == NULL) || (feof(read_options)))
         {
         break;
         }

      C8 original_line[512];
      strcpy(original_line, linbuf);

      //
      // Skip blank lines, and kill trailing and leading spaces
      //

      S32 l = strlen(linbuf);

      if ((!l) || (linbuf[0] == ';'))
         {
         if (fputs(original_line, write_options) == EOF)
            {
            _fcloseall();
            return;
            }

         continue;
         }

      C8 *lexeme  = linbuf;
      C8 *end     = linbuf;
      S32 leading = 1;

      for (S32 i=0; i < l; i++)
         {
         if (!isspace(linbuf[i]))
            {
            if (leading)
               {
               lexeme = &linbuf[i];
               leading = 0;
               }

            end = &linbuf[i];
            }
         }

      end[1] = 0;

      if ((leading) || (!strlen(lexeme)))
         {
         if (fputs(original_line, write_options) == EOF)
            {
            _fcloseall();
            return;
            }

         continue;
         }

      //
      // Terminate key substring at first space
      //

      C8 *value = strchr(lexeme,' ');

      if (value == NULL)
         {
         value = strchr(lexeme,'\t');
         }

      if (value != NULL)
         {
         *value = 0;
         }

      //
      // Write modified values for recognized parameters, passing unrecognized 
      // or inapplicable ones to the new file unchanged
      //

      C8 modified_line[512];
      modified_line[0] = 0;

      S32 j = 0;

      for (j=0; j < MAX_LEGEND_FIELDS; j++)
         {
         if (!_stricmp(lexeme,legend_INI_names[j]))
            {
            sprintf(modified_line,"%s %d\n",legend_INI_names[j],INI_show_legend_field[j]);
            break;
            }
         }

      if (j == MAX_LEGEND_FIELDS)
         {
         if      (!_stricmp(lexeme,"res_x"))            sprintf(modified_line,"res_x %d\n",            requested_RES_X);
         else if (!_stricmp(lexeme,"res_y"))            sprintf(modified_line,"res_y %d\n",            requested_RES_Y); 
         else if (!_stricmp(lexeme,"smooth"))           sprintf(modified_line,"smooth %d\n",           smooth_samples);
         else if (!_stricmp(lexeme,"contrast"))         sprintf(modified_line,"contrast %d\n",         contrast);
         else if (!_stricmp(lexeme,"ghost_sample"))     sprintf(modified_line,"ghost_sample %d\n",     ghost_sample);
         else if (!_stricmp(lexeme,"thick_trace"))      sprintf(modified_line,"thick_trace %d\n",      thick_trace);
         else if (!_stricmp(lexeme,"allow_clipping"))   sprintf(modified_line,"allow_clipping %d\n",   allow_clipping);
         else if (!_stricmp(lexeme,"alt_smoothing"))    sprintf(modified_line,"alt_smoothing %d\n",    alt_smoothing);
         else if (!_stricmp(lexeme,"spur_reduction"))   sprintf(modified_line,"spur_reduction %d\n",   spur_reduction);
         else if (!_stricmp(lexeme,"spur_dB"))          sprintf(modified_line,"spur_dB %d\n",          spur_dB);
         else if (!_stricmp(lexeme,"spot_offset"))      sprintf(modified_line,"spot_offset %f\n",      spot_offset_Hz);
         else if (!_stricmp(lexeme,"sort_modify_date")) sprintf(modified_line,"sort_modify_date %d\n", sort_modify_date);
         else if (!_stricmp(lexeme,"L_offset"))         sprintf(modified_line,"L_offset %f\n",         L_offset_Hz);
         else if (!_stricmp(lexeme,"U_offset"))         sprintf(modified_line,"U_offset %f\n",         U_offset_Hz);
         else if (!_stricmp(lexeme,"DLG_source_ID"))    sprintf(modified_line,"DLG_source_ID %d\n",    INI_DLG_source_ID);
         else if (!_stricmp(lexeme,"DLG_caption"))      sprintf(modified_line,"DLG_caption %s\n",      INI_DLG_caption);
         else if (!_stricmp(lexeme,"DLG_carrier"))      sprintf(modified_line,"DLG_carrier %s\n",      INI_DLG_carrier);
         else if (!_stricmp(lexeme,"DLG_LO"))           sprintf(modified_line,"DLG_LO %s\n",           INI_DLG_LO);
         else if (!_stricmp(lexeme,"DLG_IF"))           sprintf(modified_line,"DLG_IF %s\n",           INI_DLG_IF);
         else if (!_stricmp(lexeme,"DLG_carrier_ampl")) sprintf(modified_line,"DLG_carrier_ampl %s\n", INI_DLG_carrier_ampl);
         else if (!_stricmp(lexeme,"DLG_VBF"))          sprintf(modified_line,"DLG_VBF %s\n",          INI_DLG_VBF);
         else if (!_stricmp(lexeme,"DLG_NF"))           sprintf(modified_line,"DLG_NF %s\n",           INI_DLG_NF);
         else if (!_stricmp(lexeme,"DLG_min"))          sprintf(modified_line,"DLG_min %s\n",          INI_DLG_min);
         else if (!_stricmp(lexeme,"DLG_max"))          sprintf(modified_line,"DLG_max %s\n",          INI_DLG_max);
         else if (!_stricmp(lexeme,"DLG_mult"))         sprintf(modified_line,"DLG_mult %s\n",         INI_DLG_mult);
         else if (!_stricmp(lexeme,"DLG_GPIB"))         sprintf(modified_line,"DLG_GPIB %d\n",         INI_DLG_GPIB);
         else if (!_stricmp(lexeme,"DLG_clip"))         sprintf(modified_line,"DLG_clip %d\n",         INI_DLG_clip);
         else if (!_stricmp(lexeme,"DLG_alt"))          sprintf(modified_line,"DLG_alt %d\n",          INI_DLG_alt);
         else if (!_stricmp(lexeme,"DLG_0dB"))          sprintf(modified_line,"DLG_0dB %d\n",          INI_DLG_0dB);
         else if (!_stricmp(lexeme,"default_save_dir")) sprintf(modified_line,"default_save_dir %s\n", INI_save_directory);
         else if (!_stricmp(lexeme,"PNP_source"))       continue;
         }

      if (modified_line[0])
         {
         if (fputs(modified_line, write_options) == EOF)
            {
            _fcloseall();
            return;
            }
         }
      else
         {
         if (fputs(original_line, write_options) == EOF)
            {
            _fcloseall();
            return;
            }
         }
      }

   //
   // Append filenames of all loaded sources to .INI file, so they'll be reloaded at startup time
   //

   for (S32 s=0; s < n_data_sources; s++)
      {
      SOURCE *S = &data_source_list[s];

      fprintf(write_options, "PNP_source %s\n", S->filename);
      }

   //
   // Overwrite original .INI file with our modified copy
   //

   _fcloseall();

   CopyFile(write_filename,
            INI_path,
            FALSE);

   DeleteFile(write_filename);
}

//****************************************************************************
//
// Dialog-box procedure for acquisition dialog, and global vars used to
// communicate with it
//
//****************************************************************************

C8  DLG_box_caption[256];

S32 DLG_source_ID;
C8  DLG_caption[256];
C8  DLG_carrier[256];
C8  DLG_carrier_ampl[256];
C8  DLG_VBF[256];
C8  DLG_NF[256];
C8  DLG_min[256];
C8  DLG_max[256];
C8  DLG_LO[256];
C8  DLG_IF[256];
C8  DLG_mult[256];
S32 DLG_GPIB;
S32 DLG_clip;
S32 DLG_alt;
S32 DLG_0dB;

BOOL CALLBACK AcqDlgProc (HWND   hDlg,  
                          UINT   message,
                          WPARAM wParam,
                          LPARAM lParam)
{
   switch (message)
      {
      case WM_INITDIALOG:
         {
         //
         // Center dialog on screen
         //

         S32 screen_w = GetSystemMetrics(SM_CXSCREEN); 
         S32 screen_h = GetSystemMetrics(SM_CYSCREEN); 

         LPNMHDR pnmh = (LPNMHDR) lParam;

         RECT r;

         GetWindowRect(hDlg, &r);

         r.right  -= r.left;
         r.bottom -= r.top;

         r.left = (screen_w - r.right)  / 2;
         r.top  = (screen_h - r.bottom) / 2;

         MoveWindow(hDlg, r.left, r.top, r.right, r.bottom, TRUE);

         //
         // Set caption for window
         //

         SetWindowText(hDlg, DLG_box_caption);

         //
         // Set initial control values, disabling any that aren't supported by 
         // the specified source
         //

         C8 buffer[32];

         SetWindowText(GetDlgItem(hDlg,IDC_CAPTION),     DLG_caption);
         SetWindowText(GetDlgItem(hDlg,IDC_CARRIER_HZ),  DLG_carrier);
         SetWindowText(GetDlgItem(hDlg,IDC_CARRIER_DBM), DLG_carrier_ampl);
         SetWindowText(GetDlgItem(hDlg,IDC_MIN),         DLG_min);
         SetWindowText(GetDlgItem(hDlg,IDC_MAX),         DLG_max);
         SetWindowText(GetDlgItem(hDlg,IDC_EXT_MULT),    DLG_mult);
         SetWindowText(GetDlgItem(hDlg,IDC_EXT_LO),      DLG_LO);
         SetWindowText(GetDlgItem(hDlg,IDC_EXT_IF),      DLG_IF);
         SetWindowText(GetDlgItem(hDlg,IDC_GPIB_ADDR),   _itoa(DLG_GPIB,buffer,10));
         SetWindowText(GetDlgItem(hDlg,IDC_NF),          DLG_NF);

         switch (DLG_clip)
            {
            case 10: Button_SetCheck(GetDlgItem(hDlg, IDC_CLIP_10), 1); break;
            case 20: Button_SetCheck(GetDlgItem(hDlg, IDC_CLIP_20), 1); break;
            case 40: Button_SetCheck(GetDlgItem(hDlg, IDC_CLIP_40), 1); break;
            default: Button_SetCheck(GetDlgItem(hDlg, IDC_CLIP_0),  1); break;
            }

         switch (DLG_alt)
            {
            case 0:  Button_SetCheck(GetDlgItem(hDlg, IDC_ALT_ACQ), 0); break;
            default: Button_SetCheck(GetDlgItem(hDlg, IDC_ALT_ACQ), 1); break;
            }

         switch (DLG_0dB)
            {
            case 0:  Button_SetCheck(GetDlgItem(hDlg, IDC_0DB), 0); break;
            default: Button_SetCheck(GetDlgItem(hDlg, IDC_0DB), 1); break;
            }

         if (DLG_source_ID == IDM_HP4396A)
            {
            EnableWindow(GetDlgItem(hDlg,IDC_STATIC_CLIP), FALSE);
            EnableWindow(GetDlgItem(hDlg,IDC_CLIP_0), FALSE);
            EnableWindow(GetDlgItem(hDlg,IDC_CLIP_10), FALSE);
            EnableWindow(GetDlgItem(hDlg,IDC_CLIP_20), FALSE);
            EnableWindow(GetDlgItem(hDlg,IDC_CLIP_40), FALSE);
            }

         if ((DLG_source_ID == IDM_TEK490) || (DLG_source_ID == IDM_HP358XA))
            {
            SetWindowText(GetDlgItem(hDlg,IDC_VBF), "");
            EnableWindow(GetDlgItem(hDlg,IDC_VBF), FALSE);
            EnableWindow(GetDlgItem(hDlg,IDC_STATIC_VBF), FALSE);
            }
         else
            {
            SetWindowText(GetDlgItem(hDlg,IDC_VBF), DLG_VBF);
            }

         if ((DLG_source_ID == IDM_FSIQ) || (DLG_source_ID == IDM_FSU) || (DLG_source_ID == IDM_FSQ) || (DLG_source_ID == IDM_FSP))
            {
            HWND alt = GetDlgItem(hDlg,IDC_ALT_ACQ);
            SetWindowText(alt,"Use FFT filters");
            EnableWindow(alt, TRUE);
            ShowWindow(alt, TRUE);
            }
         else if ((DLG_source_ID != IDM_HP8566A) && (DLG_source_ID != IDM_HP8566B))
            {
            EnableWindow(GetDlgItem(hDlg,IDC_ALT_ACQ), FALSE);
            ShowWindow(GetDlgItem(hDlg, IDC_ALT_ACQ), FALSE);
            }

         if ((DLG_source_ID != IDM_HP8566A) && (DLG_source_ID != IDM_HP8566B) &&
             (DLG_source_ID != IDM_HP8567A) && (DLG_source_ID != IDM_HP8567B) && 
             (DLG_source_ID != IDM_HP8568A) && (DLG_source_ID != IDM_HP8568B) &&
             (DLG_source_ID != IDM_FSU)     && (DLG_source_ID != IDM_FSIQ)    && (DLG_source_ID != IDM_FSP) && (DLG_source_ID != IDM_FSQ))
            {
            EnableWindow(GetDlgItem(hDlg,IDC_0DB), FALSE);
            ShowWindow(GetDlgItem(hDlg, IDC_0DB), FALSE);
            }

         return TRUE;
         }

      case WM_COMMAND:
         {
         //
         // Update context-sensitive help text
         //

         if ((HIWORD(wParam) == EN_SETFOCUS) || (HIWORD(wParam) == 0))
            {
            S32 ID = LOWORD(wParam);

            switch (ID)
               {
               case IDC_CAPTION:
                  {
                  SetDlgItemText(hDlg, IDC_HELPGROUP, "Caption");
                  SetDlgItemText(hDlg, IDC_HELPBOX,   "\
Specifies an optional caption for the trace being acquired.\n\nTo display trace captions beneath \
the graph in the main window, select the\nLegend -> Trace menu option.");
                  break;
                  }

               case IDC_CARRIER_HZ:
                  {
                  SetDlgItemText(hDlg, IDC_HELPGROUP, "Carrier Frequency");

                  if (DLG_source_ID == IDM_TEK490)
                     {
                     SetDlgItemText(hDlg, IDC_HELPBOX,   "\
Specifies the carrier frequency in hertz (e.g., 10000000 or 10.0E6 = 10 MHz).  This is the frequency of \
the source under test BEFORE any external frequency multiplication or IF conversion is applied.\n\n\
If this field is left blank, or if a Tektronix 492P/496P analyzer is in use, you will be prompted to \
position the carrier manually at the center of the screen prior to making the measurement.");
                     }
                  else if ((DLG_source_ID != IDM_E4400)   &&
                           (DLG_source_ID != IDM_E4406A)  &&
                           (DLG_source_ID != IDM_R3267)   &&
                           (DLG_source_ID != IDM_R3465)   &&
                           (DLG_source_ID != IDM_R3131)   &&
                           (DLG_source_ID != IDM_R3132)   &&
                           (DLG_source_ID != IDM_R3261)   &&
                           (DLG_source_ID != IDM_TR417X)  &&
                           (DLG_source_ID != IDM_HP4396A) &&
                           (DLG_source_ID != IDM_HP358XA) && 
                           (DLG_source_ID != IDM_FSIQ)    &&
                           (DLG_source_ID != IDM_FSP)     &&
                           (DLG_source_ID != IDM_FSQ)     &&
                           (DLG_source_ID != IDM_FSU)     &&
                           (DLG_source_ID != IDM_HP3585))
                     {
                     SetDlgItemText(hDlg, IDC_HELPBOX,   "\
Specifies the carrier frequency in hertz (e.g., 10000000 or 10.0E6 = 10 MHz).  This is the frequency of \
the source under test BEFORE any external frequency multiplication or IF conversion is applied.\n\n\
Leave this field blank to measure the highest onscreen signal and round its frequency to the nearest multiple \
of the minimum offset frequency.  (The desired signal must be higher than any other peak, including the 0 Hz spike.)");
                     }
                  else
                     {
                     SetDlgItemText(hDlg, IDC_HELPBOX,   "\
Specifies the carrier frequency in hertz (e.g., 10000000 or 10.0E6 = 10 MHz).\n\nThis is the frequency of \
the source under test BEFORE any external frequency multiplication or IF conversion is applied.");
                     }
                  break;
                  }

               case IDC_CARRIER_DBM:
                  {
                  SetDlgItemText(hDlg, IDC_HELPGROUP, "Carrier Amplitude");

                  if ((DLG_source_ID == IDM_E4400)   || 
                      (DLG_source_ID == IDM_HP4396A) ||
                      (DLG_source_ID == IDM_R3267)   ||
                      (DLG_source_ID == IDM_R3465)   ||
                      (DLG_source_ID == IDM_R3131)   ||
                      (DLG_source_ID == IDM_R3132)   ||
                      (DLG_source_ID == IDM_R3261)   ||
                      (DLG_source_ID == IDM_TR417X)  ||
                      (DLG_source_ID == IDM_FSIQ)    ||
                      (DLG_source_ID == IDM_FSP)     ||
                      (DLG_source_ID == IDM_FSQ)     ||
                      (DLG_source_ID == IDM_FSU)     ||
                      (DLG_source_ID == IDM_HP358XA) || 
                      (DLG_source_ID == IDM_HP3585))
                     {
                     SetDlgItemText(hDlg, IDC_HELPBOX,   "\
Specifies the amplitude of the carrier in dBm.  This is the amplitude of \
the source under test AFTER any external frequency multiplication and/or IF conversion is applied.");
                     }
                  else if (DLG_source_ID == IDM_E4406A)
                     {
                     SetDlgItemText(hDlg, IDC_HELPBOX,   "\
Specifies the amplitude of the carrier in dBm.  This is the amplitude of \
the source under test AFTER any external frequency multiplication and/or IF conversion is applied.\n\n\
Be sure to adjust the input attenuation (Input/Output->Input Atten) to avoid input overload errors -- this is not \
done automatically on the E4406A!");
                     }
                  else
                     {
                     SetDlgItemText(hDlg, IDC_HELPBOX,   "\
Specifies the amplitude of the carrier in dBm.  This is the amplitude of \
the source under test AFTER any external frequency multiplication and/or IF conversion is applied.\n\n\
You can leave this field blank to measure the carrier amplitude automatically.  If the \
amplitude is known, specifying it here will improve repeatability and reduce measurement time.");
                     }

                  break;
                  }

               case IDC_EXT_MULT:
                  {
                  SetDlgItemText(hDlg, IDC_HELPGROUP, "External Multiplier");
                  SetDlgItemText(hDlg, IDC_HELPBOX,   "\
Specifies that an external frequency multiplier (e.g., 2 for a doubler, 3 for a tripler...) is \
present between the source under test and the spectrum analyzer's RF input.\nUse fractional values to \
specify frequency division.  Enter 1 if no multiplier is present.\n\n\
The use of an external frequency multiplier improves the analyzer's equivalent phase-noise floor by \
20 x log10(n) dB.  Multiplication is assumed to take place after any specified IF conversion occurs.");

                  break;
                  }

               case IDC_EXT_LO:
               case IDC_EXT_IF:
                  {
                  SetDlgItemText(hDlg, IDC_HELPGROUP, "External Conversion");
                  SetDlgItemText(hDlg, IDC_HELPBOX,   "\
Use these fields when the carrier is mixed with an external LO signal to generate an IF within the spectrum analyzer's tuning \
range (or an optional frequency multiplier's input range).\n\n\
Leave both fields blank if no external mixer is present.  Otherwise, enter the LO frequency used to perform the conversion, \
followed by the IF at which the measurement (or multiplication) takes place.");
                  break;
                  }

               case IDC_MIN:
               case IDC_MAX:
                  {
                  SetDlgItemText(hDlg, IDC_HELPGROUP, "Measurement Offset Range");

                  if (DLG_source_ID == IDM_HP358XA)
                     {
                  SetDlgItemText(hDlg, IDC_HELPBOX,   "\
Specifies the minimum and maximum carrier offset frequencies in Hz.  Offset \
frequencies are rounded to the nearest power of 10 and used to determine the left \
and right boundaries of the log-log plot.\n\n\
The HP 3588A/3589A analyzers can support 10-Hz minimum offsets, but 100 Hz will provide much faster sweeps when close-in measurements are not needed.  The maximum offset frequency should be limited to 1 MHz for most measurements.");
                     }
                  else if (DLG_source_ID == IDM_HP3585)
                     {
                  SetDlgItemText(hDlg, IDC_HELPBOX,   "\
Specifies the minimum and maximum carrier offset frequencies in Hz.  Offset \
frequencies are rounded to the nearest power of 10 and used to determine the left \
and right boundaries of the log-log plot.\n\n\
Note that the default offset range (10 - 1000000 Hz) will take 25 minutes to acquire on the HP 3585A at the default VBW:RBW ratio of 0.01.  You can use VBW:RBW ratios up to 1.0 for faster sweeps at the expense of absolute accuracy.");
                     }
                  else if (DLG_source_ID == IDM_E4406A)
                     {
                  SetDlgItemText(hDlg, IDC_HELPBOX,   "\
Specifies the minimum and maximum carrier offset frequencies in Hz.  Offset \
frequencies are rounded to the nearest power of 10 and used to determine the left \
and right boundaries of the log-log plot.\n\n\
You can select minimum offsets down to 1 Hz on the E4406A, but acquisition will be much faster if the minimum offset is left at its default value of 10 Hz.");
                     }
                  else if (DLG_source_ID == IDM_HP8560A)
                     {
                  SetDlgItemText(hDlg, IDC_HELPBOX,   "\
Specifies the minimum and maximum carrier offset frequencies in Hz.  Offset \
frequencies are rounded to the nearest power of 10 and used to determine the left \
and right boundaries of the log-log plot.\n\n\
WARNING: HP 8560A analyzers with option 026 cannot be used at offsets below 1000 Hz due to their 2.5-kHz minimum span width limitation.");
                     }
                  else if (DLG_source_ID == IDM_R3267)
                     {
                  SetDlgItemText(hDlg, IDC_HELPBOX,   "\
Specifies the minimum and maximum carrier offset frequencies in Hz.  Offset \
frequencies are rounded to the nearest power of 10 and used to determine the left \
and right boundaries of the log-log plot.\n\n\
R3267-family analyzers can support 10-Hz minimum offsets, but only if their digital resolution-bandwidth filters are enabled.  Otherwise, the minimum offset will be limited to 100 Hz.");
                     }
                  else if (DLG_source_ID == IDM_R3261)
                     {
                  SetDlgItemText(hDlg, IDC_HELPBOX,   "\
Specifies the minimum and maximum carrier offset frequencies in Hz.  Offset \
frequencies are rounded to the nearest power of 10 and used to determine the left \
and right boundaries of the log-log plot.\n\n\
The lowest supported minimum offset is 100 Hz with the R3261/R3361 family's 30-Hz RBW filter.  For better skirt selectivity, use 1000 Hz as the minimum offset.");
                     }
                  else if (DLG_source_ID == IDM_R3132)
                     {
                  SetDlgItemText(hDlg, IDC_HELPBOX,   "\
Specifies the minimum and maximum carrier offset frequencies in Hz.  Offset \
frequencies are rounded to the nearest power of 10 and used to determine the left \
and right boundaries of the log-log plot.\n\n\
R3132-family analyzers with the optional 30-300 Hz RBW filters can support 100-Hz minimum offsets.  Otherwise, the minimum supported offset is limited to 10000 Hz.");
                     }
                  else if (DLG_source_ID == IDM_MS2650)
                     {
                  SetDlgItemText(hDlg, IDC_HELPBOX,   "\
Specifies the minimum and maximum carrier offset frequencies in Hz.  Offset \
frequencies are rounded to the nearest power of 10 and used to determine the left \
and right boundaries of the log-log plot.\n\n\
MS2650/MS2660-family analyzers equipped with option 02 (30-300 Hz RBW filters) can support 100-Hz minimum offsets.  Otherwise, the minimum supported offset is limited to 10000 Hz.");
                     }
                  else if ((DLG_source_ID == IDM_FSIQ) || (DLG_source_ID == IDM_FSU) || (DLG_source_ID == IDM_FSP) || (DLG_source_ID == IDM_FSQ))
                     {
                  SetDlgItemText(hDlg, IDC_HELPBOX,   "\
Specifies the minimum and maximum carrier offset frequencies in Hz.  Offset \
frequencies are rounded to the nearest power of 10 and used to determine the left \
and right boundaries of the log-log plot.\n\n\
The minimum offset frequency should be at least 100 Hz, or 10 Hz if FFT filters are enabled.\n\
The maximum offset frequency should be limited to 1 MHz for most measurements.");
                     }
                  else
                     {
                  SetDlgItemText(hDlg, IDC_HELPBOX,   "\
Specifies the minimum and maximum carrier offset frequencies in Hz.  Offset \
frequencies are rounded to the nearest power of 10 and used to determine the left \
and right boundaries of the log-log plot.\n\n\
The minimum offset frequency should be at least 3x the analyzer's minimum resolution \
bandwidth.  The maximum offset frequency should be limited to 1 MHz for most measurements.");
                     }
                  break;
                  }

               case IDC_NF:
                  {
                  SetDlgItemText(hDlg, IDC_HELPGROUP, "Noise Response Characteristic");
                  if ((DLG_source_ID == IDM_HP3585) || (DLG_source_ID == IDM_FSIQ) || (DLG_source_ID == IDM_FSU))
                     {
                  SetDlgItemText(hDlg, IDC_HELPBOX,   "\
Specifies the factor used to correct the measurement for the response of the analyzer's \
RBW filters and video detector to Gaussian noise, plus any other system-level factors.  This value \
will be added to all displayed data points.\n");
                     }
                  else if ((DLG_source_ID != IDM_TEK490)  &&
                           (DLG_source_ID != IDM_HP4396A) &&
                           (DLG_source_ID != IDM_HP358XA))
                     {
                  SetDlgItemText(hDlg, IDC_HELPBOX,   "\
Specifies the factor used to correct the measurement for the response of the analyzer's \
RBW filters and video detector to Gaussian noise, plus any other system-level factors.  This value \
will be added to all displayed data points.\n\nA reasonable default value for most analyzers is 2 dB.\n");
                     }
                  else
                     {
                  SetDlgItemText(hDlg, IDC_HELPBOX,   "\
Specifies the factor used to correct the measurement for the response of the analyzer's \
RBW filters and video detector to Gaussian noise, plus any other system-level factors.  This value \
will be added to all displayed data points.\n\nThe selected analyzer model \
maintains its noise-correction factors internally, so the default value for this field should be 0 dB.");
                     }
                  break;
                  }

               case IDC_ALT_ACQ:
                  {
                  if ((DLG_source_ID == IDM_FSIQ) || (DLG_source_ID == IDM_FSU) || (DLG_source_ID == IDM_FSP) || (DLG_source_ID == IDM_FSQ))
                     {
                  SetDlgItemText(hDlg, IDC_HELPGROUP, "Use FFT filters");
                  SetDlgItemText(hDlg, IDC_HELPBOX,   "\
If this option is selected, FFT-based filters will be used at offsets below 100 kHz.\n\n\
FFT filters yield much faster measurements, especially at minimum offsets of 1 kHz and below.   However, distortion and discontinuities may appear \
at high signal levels, especially when carrier clipping is used.");
                     }
                  else
                     {
                  SetDlgItemText(hDlg, IDC_HELPGROUP, "Alternative trace acquisition");
                  SetDlgItemText(hDlg, IDC_HELPBOX,   "\
If this option is selected, trace data for the 10 kHz-100 kHz decade will be acquired from a series of 25-kHz spans rather than \
a single 100-kHz span.  On some HP 8566A/B analyzers, this can improve the baseline \
noise response in the important\n10 kHz-20 kHz vicinity.  However, it also increases acquisition time, and can actually worsen the noise level \
in the rest of the decade.\n\nMost users should leave this option turned off.\n");
                     }

                  break;
                  }

               case IDC_0DB:
                  {
                  SetDlgItemText(hDlg, IDC_HELPGROUP, "Force 0 dB RF attenuation");
                  SetDlgItemText(hDlg, IDC_HELPBOX,   "\
If this option is selected, the RF attenuator will be forced to 0 dB. \
This can lower the analyzer's noise floor, at the risk of exposing its mixer to excessive RF energy. \n\
Use this option with care!\n");

                  break;
                  }

               case IDC_GPIB_ADDR:
                  {
                  SetDlgItemText(hDlg, IDC_HELPGROUP, "Instrument GPIB Address");

                  if ((DLG_source_ID == IDM_HP8566A) ||
                      (DLG_source_ID == IDM_HP8567A) ||
                      (DLG_source_ID == IDM_HP8568A) ||
                      (DLG_source_ID == IDM_HP8566B) ||
                      (DLG_source_ID == IDM_HP8567B) ||
                      (DLG_source_ID == IDM_HP8568B) ||
                      (DLG_source_ID == IDM_HP8560A) ||
                      (DLG_source_ID == IDM_HP8560E) ||
                      (DLG_source_ID == IDM_HP8590)  ||
                      (DLG_source_ID == IDM_HP4195A) ||
                      (DLG_source_ID == IDM_HP70000) ||
                      (DLG_source_ID == IDM_E4400)   ||
                      (DLG_source_ID == IDM_E4406A))
                     {
                     SetDlgItemText(hDlg, IDC_HELPBOX,   "\
Specifies the spectrum analyzer's GPIB address as a decimal value.\n\n\
The factory default GPIB address for many HP/Agilent analyzers is 18.");
                     }
                  else if (DLG_source_ID == IDM_TEK490)
                     {
                     SetDlgItemText(hDlg, IDC_HELPBOX,   "\
Specifies the spectrum analyzer's GPIB address as a decimal value.n\n\
The LF-or-EOI DIP switch on the instrument's rear panel must be set to 1 (up).");
                     }
                  else
                     {
                     SetDlgItemText(hDlg, IDC_HELPBOX,   "\
Specifies the spectrum analyzer's GPIB address as a decimal value.");
                     }
                  break;
                  }

               case IDC_CLIP_0:
               case IDC_CLIP_10:
               case IDC_CLIP_20:
               case IDC_CLIP_40:
                  {
                  SetDlgItemText(hDlg, IDC_HELPGROUP, "Carrier Clipping Level");

                  if ((DLG_source_ID == IDM_TEK490) || (DLG_source_ID == IDM_FSIQ) || (DLG_source_ID == IDM_FSU) || (DLG_source_ID == IDM_FSP) || (DLG_source_ID == IDM_FSQ))
                     {
                     SetDlgItemText(hDlg, IDC_HELPBOX,   "\
Specifies the amplitude offset above the analyzer's reference level (top graticule line) \
at which the carrier will be positioned during the noise measurement.  Clipping can improve the \
accuracy of measurements made near the analyzer's LO or RF noise floor, at the risk of \
driving its front-end mixer into compression.\n\n\
Use caution at high signal levels!");
                     }
                  else if (DLG_source_ID == IDM_E4406A)
                     {
                     SetDlgItemText(hDlg, IDC_HELPBOX,   "\
Specifies the amplitude offset above the analyzer's reference level (top graticule line) \
at which the carrier will be positioned during the noise measurement.\n\nThe carrier clipping \
selection has little or no effect on the E4406A.  It's more important to select an input attenuation \
value (Input/Output->Input Atten) that preserves the front-end noise figure without causing \
Input Overload errors.  This must be done manually prior to acquisition.");
                     }
                  else
                     {
                     SetDlgItemText(hDlg, IDC_HELPBOX,   "\
Specifies the amplitude offset above the analyzer's reference level (top graticule line) \
at which the carrier will be positioned during the noise measurement.  Clipping can improve the \
accuracy of measurements made near the analyzer's LO or RF noise floor, at the risk of \
driving its front-end mixer into compression.\n\n\
10 dB or 20 dB is recommended for most applications.  Use caution at high signal levels!");
                     }
                  break;
                  }

               case IDC_VBF:
                  {
                  SetDlgItemText(hDlg, IDC_HELPGROUP, "VBW:RBW Ratio");

                  if (DLG_source_ID == IDM_HP3585)
                     {
                     SetDlgItemText(hDlg, IDC_HELPBOX,   "\
Specifies the video:resolution bandwidth ratio.  This determines the compromise between \
measurement time and repeatability.\n\n\
Because the HP 3585A supports only positive-peak detection, acquisition of the default offset range (10 - 1000000 Hz) will take 25 minutes at the recommended \
VBW:RBW ratio of 0.01.  You can use VBW:RBW ratios up to 1.0 for faster sweeps at the expense of absolute accuracy.");
                     }
                  else if (DLG_source_ID == IDM_E4406A)
                     {
                     SetDlgItemText(hDlg, IDC_HELPBOX,   "\
Specifies the video:resolution bandwidth ratio.  This control has no effect on the E4406A.");
                     }
                  else
                     {
                     SetDlgItemText(hDlg, IDC_HELPBOX,   "\
Specifies the video:resolution bandwidth ratio.  This determines the compromise between \
measurement time and repeatability.\n\n\
Most applications should use 1.0.  Lower values can yield better repeatability \
(assuming the analyzer has warmed up!) but can take much longer to execute.");
                     }
                  break;
                  }

               default:
                  {
                  SetDlgItemText(hDlg, IDC_HELPGROUP, "Help");
                  SetDlgItemText(hDlg, IDC_HELPBOX,   "No help available");
                  break;
                  }
               }
            }

         //
         // Process input
         // 

         switch (LOWORD(wParam))
            {
            //
            // Measure/cancel buttons return to acquisition routine
            //

            case IDOK:
               {
               //
               // Copy values from control windows back to DLG_ variables
               // prior to returning
               //

               memset(DLG_caption,      0, sizeof(DLG_caption));      GetWindowText(GetDlgItem(hDlg, IDC_CAPTION),      DLG_caption,      sizeof(DLG_caption)-1);
               memset(DLG_carrier,      0, sizeof(DLG_carrier));      GetWindowText(GetDlgItem(hDlg, IDC_CARRIER_HZ),   DLG_carrier,      sizeof(DLG_carrier)-1);
               memset(DLG_LO,           0, sizeof(DLG_LO));           GetWindowText(GetDlgItem(hDlg, IDC_EXT_LO),       DLG_LO,           sizeof(DLG_LO)-1);
               memset(DLG_IF,           0, sizeof(DLG_IF));           GetWindowText(GetDlgItem(hDlg, IDC_EXT_IF),       DLG_IF,           sizeof(DLG_IF)-1);
               memset(DLG_carrier_ampl, 0, sizeof(DLG_carrier_ampl)); GetWindowText(GetDlgItem(hDlg, IDC_CARRIER_DBM),  DLG_carrier_ampl, sizeof(DLG_carrier_ampl)-1);
               memset(DLG_min,          0, sizeof(DLG_min));          GetWindowText(GetDlgItem(hDlg, IDC_MIN),          DLG_min,          sizeof(DLG_min)-1);
               memset(DLG_max,          0, sizeof(DLG_max));          GetWindowText(GetDlgItem(hDlg, IDC_MAX),          DLG_max,          sizeof(DLG_max)-1);
               memset(DLG_NF,           0, sizeof(DLG_NF));           GetWindowText(GetDlgItem(hDlg, IDC_NF),           DLG_NF,           sizeof(DLG_NF)-1);
               memset(DLG_mult,         0, sizeof(DLG_mult));         GetWindowText(GetDlgItem(hDlg, IDC_EXT_MULT),     DLG_mult,         sizeof(DLG_mult)-1);

               C8 buffer[32];

               memset(buffer, 0, sizeof(buffer)); 
               GetWindowText(GetDlgItem(hDlg, IDC_GPIB_ADDR), buffer, sizeof(buffer)-1);
               DLG_GPIB = atoi(buffer);
               
               if (Button_GetCheck(GetDlgItem(hDlg, IDC_CLIP_10))) DLG_clip = 10;
               else if (Button_GetCheck(GetDlgItem(hDlg, IDC_CLIP_20))) DLG_clip = 20; 
               else if (Button_GetCheck(GetDlgItem(hDlg, IDC_CLIP_40))) DLG_clip = 40; 
               else DLG_clip = 0;

               if (Button_GetCheck(GetDlgItem(hDlg, IDC_ALT_ACQ))) DLG_alt = 1;
               else DLG_alt = 0;

               if (Button_GetCheck(GetDlgItem(hDlg, IDC_0DB))) DLG_0dB = 1;
               else DLG_0dB = 0;

               if (DLG_source_ID != IDM_TEK490)
                  {
                  memset(DLG_VBF, 0, sizeof(DLG_VBF)); GetWindowText(GetDlgItem(hDlg, IDC_VBF), DLG_VBF, sizeof(DLG_VBF)-1);
                  }

               EndDialog(hDlg, 1);
               return TRUE;
               }

            case IDCANCEL:
               {
               EndDialog(hDlg, 0);
               return FALSE;
               }
            }
         break;
         }
      }

   return FALSE;
}

//****************************************************************************
//
// Dialog-box procedure for new-caption dialog, which shares some of the globals
// from AcqDlgProc() above
//
//****************************************************************************

BOOL CALLBACK CapDlgProc (HWND   hDlg,  
                          UINT   message,
                          WPARAM wParam,
                          LPARAM lParam)
{
   switch (message)
      {
      case WM_INITDIALOG:
         {
         //
         // Center dialog on screen
         //

         S32 screen_w = GetSystemMetrics(SM_CXSCREEN); 
         S32 screen_h = GetSystemMetrics(SM_CYSCREEN); 

         LPNMHDR pnmh = (LPNMHDR) lParam;

         RECT r;

         GetWindowRect(hDlg, &r);

         r.right  -= r.left;
         r.bottom -= r.top;

         r.left = (screen_w - r.right)  / 2;
         r.top  = (screen_h - r.bottom) / 2;

         MoveWindow(hDlg, r.left, r.top, r.right, r.bottom, TRUE);

         //
         // Set caption for window
         //

         SetWindowText(hDlg, DLG_box_caption);

         //
         // Set initial control values
         //

         SetWindowText(GetDlgItem(hDlg,IDC_CAPTION), DLG_caption);
         return TRUE;
         }

      case WM_COMMAND:
         {
         //
         // Update context-sensitive help text
         //

         if ((HIWORD(wParam) == EN_SETFOCUS) || (HIWORD(wParam) == 0))
            {
            S32 ID = LOWORD(wParam);

            switch (ID)
               {
               case IDC_CAPTION:
                  {
                  SetDlgItemText(hDlg, IDC_HELPGROUP, "Edit caption");
                  SetDlgItemText(hDlg, IDC_HELPBOX,   "\
Enter a new caption for the selected trace.\n\nTo display trace captions beneath \
the graph in the main window, select the\nLegend -> Trace menu option.");
                  break;
                  }
               }
            }

         //
         // Process input
         // 

         switch (LOWORD(wParam))
            {
            //
            // Accept/cancel buttons return to CMD_caption()
            //

            case IDOK:
               {
               //
               // Copy values from control windows back to DLG_ variables
               // prior to returning
               //

               memset(DLG_caption,  0, sizeof(DLG_caption));      
               GetWindowText(GetDlgItem(hDlg, IDC_CAPTION), DLG_caption, sizeof(DLG_caption)-1);

               EndDialog(hDlg, 1);
               return TRUE;
               }

            case IDCANCEL:
               {
               EndDialog(hDlg, 0);
               return FALSE;
               }
            }
         break;
         }
      }

   return FALSE;
}

/*********************************************************************/
//
// SpurDlgProc()
//
/*********************************************************************/

BOOL CALLBACK SpurDlgProc (HWND   hDlg,  
                           UINT   message,
                           WPARAM wParam,
                           LPARAM lParam)
{
   switch (message)
      {
      case WM_INITDIALOG:
         {
         //
         // Center dialog on screen
         //

         S32 screen_w = GetSystemMetrics(SM_CXSCREEN); 
         S32 screen_h = GetSystemMetrics(SM_CYSCREEN); 

         LPNMHDR pnmh = (LPNMHDR) lParam;

         RECT r;

         GetWindowRect(hDlg, &r);

         r.right  -= r.left;
         r.bottom -= r.top;

         r.left = (screen_w - r.right)  / 2;
         r.top  = (screen_h - r.bottom) / 2;

         MoveWindow(hDlg, r.left, r.top, r.right, r.bottom, TRUE);

         //
         // Set caption for window
         //

         SetWindowText(hDlg, DLG_box_caption);

         //
         // Set initial control values
         //

         C8 text[256];

         _snprintf(text,sizeof(text),"%d",spur_dB);
         SetWindowText(GetDlgItem(hDlg,IDC_SPUR_LEVEL), text);

         return TRUE;
         }

      case WM_COMMAND:
         {
         //
         // Update context-sensitive help text
         //

         if ((HIWORD(wParam) == EN_SETFOCUS) || (HIWORD(wParam) == 0))
            {
            S32 ID = LOWORD(wParam);

            switch (ID)
               {
               case IDC_SPUR_LEVEL:
                  {
                  SetDlgItemText(hDlg, IDC_HELPGROUP, "Spur clipping level");
                  SetDlgItemText(hDlg, IDC_HELPBOX,   "\
This field allows you to specify the level in dB above the local average trace value that will be considered a spurious response. \
Spurs are suppressed only when spur reduction is selected in the Trace menu, or by use of the ctrl-s keyboard shortcut.\n\n\
Enter a positive value in dB.  (You can also use ctrl-keypad+ and ctrl-keypad- to increment and decrement this value while watching the plot.)");
                  break;
                  }

               }
            }

         //
         // Process input
         // 

         switch (LOWORD(wParam))
            {
            //
            // Accept/cancel buttons return to CMD_edit_spur_level()
            //

            case IDOK:
               {
               //
               // Copy values from control windows back to globals 
               // prior to returning
               //

               C8 text[256];

               memset(text, 0, sizeof(text));      
               GetWindowText(GetDlgItem(hDlg, IDC_SPUR_LEVEL), text, sizeof(text)-1);
               sscanf(text,"%d",&spur_dB);

               if (spur_dB < MIN_SPUR_CLIP_DB) spur_dB = MIN_SPUR_CLIP_DB;
               if (spur_dB > MAX_SPUR_CLIP_DB) spur_dB = MAX_SPUR_CLIP_DB;

               EndDialog(hDlg, 1);
               return TRUE;
               }

            case IDCANCEL:
               {
               EndDialog(hDlg, 0);
               return FALSE;
               }
            }
         break;
         }
      }

   return FALSE;
}

//****************************************************************************
//
// Exit handlers must be present in every SAL application
//
// These routines handle exits under different conditions (exit() call, 
// user request via GUI, etc.)
//
//****************************************************************************

static int exit_handler_active = 0;

void WINAPI WinClean(void)
{
   if (exit_handler_active)
      {
      return;
      }

   exit_handler_active = 1;

   update_INI();

   SAL_shutdown();

   if (out != NULL)
      {
      fclose(out);
      }

   if (cleanup_filename[0])
      {
      _unlink(cleanup_filename);
      cleanup_filename[0] = 0;
      }

   GPIB_disconnect();

   OutputDebugString("Final exit OK\n");
}

void WINAPI WinExit(void)
{
   OutputDebugString("WinExit called\n");

   if (!exit_handler_active)
      {
      WinClean();
      }

   exit(0);
}

void AppExit(void)
{
   OutputDebugString("AppExit() called via atexit()\n");

   if (!exit_handler_active)
      {
      WinClean();
      }

   return;
}

//****************************************************************************
//
// CMD_load_PNP() 
//
//****************************************************************************

void CMD_load_PNP()
{
   if (n_data_sources >= MAX_SOURCES)
      {
      SAL_alert_box("Error","Too many sources -- must close or delete at least one first");
      }
   else
      {
      C8 buffer[MAX_PATH];
      buffer[0] = 0;

      if (get_load_filename(buffer))
         {
         if (data_source_list[n_data_sources].load(buffer, TRUE))
            {
            current_data_source = n_data_sources++;
            }

         force_redraw = TRUE;
         invalid_trace_cache = TRUE;
         }
      }
}

//****************************************************************************
//
// CMD_save()
//
//****************************************************************************

void CMD_save(C8 *explicit_save_filename)
{
   C8 buffer[MAX_PATH];

   if (current_data_source >= n_data_sources)
      {
      return;
      }

   C8 *src = data_source_list[current_data_source].filename;

   if (src[0] == '\"')
      {
      strcpy(buffer,&src[1]);
      }
   else
      {
      strcpy(buffer,src);
      }

   C8 *suffix = strrchr(buffer,'.');

   if (suffix)
      {
      *suffix = 0;
      }

   BOOL buffer_valid = FALSE;

   if (explicit_save_filename != NULL)
      {
      strcpy(buffer, explicit_save_filename);
      buffer_valid = TRUE;
      }

   if (!buffer_valid)
      {
      buffer_valid = get_save_filename(buffer);
      }

   if (buffer_valid)
      {
      render_source_list(FALSE);
      refresh();

      C8 *suffix = strrchr(buffer,'.');
      assert(suffix);

      if (!_stricmp(suffix,".tga"))
         {
         TGA_write_16bpp(image, buffer);
         }
      else if (!_stricmp(suffix,".gif"))
         {
         GIF_write_16bpp(image, buffer);
         }
      else if (!_stricmp(suffix,".bmp"))
         {
         BMP_write_16bpp(image, buffer);
         }
      else if (!_stricmp(suffix,".pcx"))
         {
         PCX_write_16bpp(image, buffer);
         }
      else if (!_stricmp(suffix,".pnp"))
         {
         C8 src[MAX_PATH];

         C8 *p = data_source_list[current_data_source].filename;

         sprintf(src,"%s",p);

         CopyFile(src,
                  buffer,
                  FALSE);

         strcpy(data_source_list[current_data_source].filename, buffer);
         }

      force_redraw = TRUE;
      }
}

//****************************************************************************
//
// CMD_export()
//
//****************************************************************************

void CMD_export(C8 *explicit_export_filename)
{
   C8 buffer[MAX_PATH];

   if (current_data_source >= n_data_sources)
      {
      return;
      }

   SOURCE *S = &data_source_list[current_data_source];

   C8 *src = S->filename;

   if (src[0] == '\"')
      {
      strcpy(buffer,&src[1]);
      }
   else
      {
      strcpy(buffer,src);
      }

   C8 *suffix = strrchr(buffer,'.');

   if (suffix)
      {
      *suffix = 0;
      }

   BOOL buffer_valid = FALSE;

   if (explicit_export_filename != NULL)
      {
      strcpy(buffer, explicit_export_filename);
      buffer_valid = TRUE;
      }

   if (!buffer_valid)
      {
      buffer_valid = get_export_filename(buffer);
      }

   if (buffer_valid)
      {
      C8 *suffix = strrchr(buffer,'.');
      assert(suffix);

      FILE *out = fopen(buffer,"wt");

      if (out == NULL)
         {
         SAL_alert_box("Error","Couldn't open %s for writing", buffer);
         return;
         }

      if (!_stricmp(suffix,".csv"))    // .csv pairs only
         {
         BOOL first = TRUE;

         for (S32 i=0; i < MAX_GRAPH_WIDTH; i++)
            {
            if (!S->VV[i])
               {
               continue;
               }

            if (first)
               first = FALSE;
            else
               fprintf(out,",");

            fprintf(out,"%.20lf,%.20lf", S->offset[i], S->smoothed[i]);
            }
         }
      else     // .txt with line breaks
         {
         for (S32 i=0; i < MAX_GRAPH_WIDTH; i++)
            {
            if (!S->VV[i])
               {
               continue;
               }

            fprintf(out,"%.20lf, %.20lf\n", S->offset[i], S->smoothed[i]);
            }
         }

      fclose(out);
      }
}

//****************************************************************************
//
// CMD_print_image() 
//
//****************************************************************************

void CMD_print_image()
{
   render_source_list(FALSE);
   refresh();

   PrintBackBufferToDC();

   force_redraw = TRUE;
}

//****************************************************************************
//
// CMD_quit() 
//
//****************************************************************************

void CMD_quit()
{
   exit(0);
}

//****************************************************************************
//
// CMD_browse(void) 
//
//****************************************************************************

void CMD_browse(void)
{
   mode = M_BROWSE;
   force_redraw = TRUE;
}

//****************************************************************************
//
// CMD_overlay(void) 
//
//****************************************************************************

void CMD_overlay(void)
{
   mode = M_OVERLAY;
   force_redraw = TRUE;
}

//****************************************************************************
//
// CMD_next(void) 
//
//****************************************************************************

void CMD_next(void)
{
   current_data_source++;
   
   if (current_data_source >= n_data_sources) 
      {
      current_data_source = n_data_sources-1;
      }

   force_redraw = TRUE;
}

//****************************************************************************
//
// CMD_prev(void) 
//
//****************************************************************************

void CMD_prev(void)
{
   --current_data_source;

   if (current_data_source < 0)
      {
      current_data_source = 0;
      }

   force_redraw = TRUE;
}

//****************************************************************************
//
// CMD_move_up()
//
//****************************************************************************

void CMD_move_up(void)
{
   S32 swap = current_data_source-1;
   if (swap < 0) swap = n_data_sources-1;

   static SOURCE temp;

   temp = data_source_list[current_data_source];
   data_source_list[current_data_source] = data_source_list[swap];
   data_source_list[swap] = temp;

   current_data_source = swap;
   force_redraw = TRUE;
}

//****************************************************************************
//
// CMD_move_down()
//
//****************************************************************************

void CMD_move_down(void)
{
   S32 swap = current_data_source+1;
   if (swap >= n_data_sources) swap = 0;

   static SOURCE temp;
   
   temp = data_source_list[current_data_source];
   data_source_list[current_data_source] = data_source_list[swap];
   data_source_list[swap] = temp;

   current_data_source = swap;
   force_redraw = TRUE;
}

//****************************************************************************
//
// CMD_set_smoothing(S32 samples)
//
//****************************************************************************

void CMD_set_smoothing(S32 samples)
{
   smooth_samples = samples;
   force_redraw   = TRUE;
   main_menu_update();
}

//****************************************************************************
//
// CMD_set_contrast()
//
//****************************************************************************

void CMD_set_contrast(S32 c)
{
   contrast=c;
   force_redraw  = TRUE;
   main_menu_update();
}

//****************************************************************************
//
// CMD_toggle_sort_modify_date()
//
//****************************************************************************

void CMD_toggle_sort_modify_date(void)
{
   sort_modify_date = !sort_modify_date;

   main_menu_update();
}

//****************************************************************************
//
// CMD_toggle_ghost_sample()
//
//****************************************************************************

void CMD_toggle_ghost_sample(void)
{
   ghost_sample  = !ghost_sample;
   force_redraw  = TRUE;
   main_menu_update();
}

//****************************************************************************
//
// CMD_toggle_thick_trace()
//
//****************************************************************************

void CMD_toggle_thick_trace(void)
{
   thick_trace  = !thick_trace;
   force_redraw  = TRUE;
   main_menu_update();
}

//****************************************************************************
//
// CMD_toggle_clipping()
//
//****************************************************************************

void CMD_toggle_clipping(void)
{
   allow_clipping = !allow_clipping;
   force_redraw = TRUE;
   main_menu_update();
}

//****************************************************************************
//
// CMD_toggle_visibility()
//
//****************************************************************************

void CMD_toggle_visibility(void)
{
   if (current_data_source >= n_data_sources)
      {
      return;
      }

   data_source_list[current_data_source].visible = !data_source_list[current_data_source].visible;
   force_redraw = TRUE;
   main_menu_update();
}

//****************************************************************************
//
// CMD_toggle_alt_smoothing()
//
//****************************************************************************

void CMD_toggle_alt_smoothing(void)
{
   alt_smoothing = !alt_smoothing;
   force_redraw = TRUE;
   main_menu_update();
}

//****************************************************************************
//
// CMD_toggle_spur_reduction()
//
//****************************************************************************

void CMD_toggle_spur_reduction(void)
{
   spur_reduction = !spur_reduction;
   force_redraw = TRUE;
   invalid_trace_cache = TRUE;
   main_menu_update();
}

//****************************************************************************
//
// CMD_set_resolution(S32 width, S32 height) 
//
//****************************************************************************

void CMD_set_resolution(S32 width, S32 height)
{
   requested_RES_X = width;
   requested_RES_Y = height;
}

//****************************************************************************
//
// CMD_close_source(S32 data_source)
//
//****************************************************************************

void CMD_close_source(S32 data_source)
{
   close_source(data_source);
}

//****************************************************************************
//
// CMD_close_all_source(void)
//
//****************************************************************************

void CMD_close_all_sources(void)
{
   while (n_data_sources > 0)
      {
      close_source(current_data_source);
      }
}

//****************************************************************************
//
// CMD_delete_source(void)
//
//****************************************************************************

void CMD_delete_source(S32 data_source)
{
   C8 confirm[MAX_PATH+256];

   wsprintf(confirm,"Are you sure you wish to delete %s?",
      data_source_list[data_source].filename);

   if (MessageBox(hWnd,
                  confirm,
                 "Confirm file deletion",
                  MB_OKCANCEL | MB_ICONQUESTION) == IDOK)
      {
      delete_source_by_filename(data_source_list[data_source].filename);
      }
}

//****************************************************************************
//
// CMD_acquire(S32 source_ID)
//
//****************************************************************************

void CMD_acquire(S32 source_ID)
{
   if (n_data_sources >= MAX_SOURCES)
      {
      SAL_alert_box("Error","Too many sources -- must close or delete at least one first");
      return;
      }

   DLG_source_ID = source_ID;

   strcpy(DLG_box_caption, "Acquire noise plot from ");

   if (DLG_source_ID == IDM_FSIQ)
      {
      strcat(DLG_box_caption, "Rohde & Schwarz FSEA/B/M/K or FSIQ");   // (hack to avoid printing two ampersands from acquire option string)
      }
   else if (DLG_source_ID == IDM_FSU)
      {
      strcat(DLG_box_caption, "Rohde & Schwarz FSU");                  // (hack to avoid printing two ampersands from acquire option string)
      }
   else if (DLG_source_ID == IDM_FSQ)
      {
      strcat(DLG_box_caption, "Rohde & Schwarz FSQ");                  // (hack to avoid printing two ampersands from acquire option string)
      }
   else if (DLG_source_ID == IDM_FSP)
      {
      strcat(DLG_box_caption, "Rohde & Schwarz FSP");                  // (hack to avoid printing two ampersands from acquire option string)
      }
   else
      {
      strcat(DLG_box_caption, acquire_options[DLG_source_ID - FIRST_ACQ_SOURCE]);
      DLG_box_caption[strlen(DLG_box_caption)-3] = 0;
      }

   //
   // Set initial measurement parameters for specified source, using values from 
   // PN.INI if the source hasn't changed
   //

   if (INI_DLG_source_ID == DLG_source_ID)
      {
      strcpy(DLG_caption,      INI_DLG_caption);
      strcpy(DLG_carrier,      INI_DLG_carrier);
      strcpy(DLG_LO,           INI_DLG_LO);
      strcpy(DLG_IF,           INI_DLG_IF);
      strcpy(DLG_carrier_ampl, INI_DLG_carrier_ampl);
      strcpy(DLG_VBF,          INI_DLG_VBF);
      strcpy(DLG_NF,           INI_DLG_NF);
      strcpy(DLG_min,          INI_DLG_min);
      strcpy(DLG_max,          INI_DLG_max);
      strcpy(DLG_mult,         INI_DLG_mult);

      DLG_GPIB = INI_DLG_GPIB;
      DLG_clip = INI_DLG_clip;
      DLG_alt  = INI_DLG_alt;
      DLG_0dB  = INI_DLG_0dB;
      }
   else
      {
      switch (DLG_source_ID)
         {
         case IDM_TEK490 :

            strcpy(DLG_caption, "");
            strcpy(DLG_carrier,"100E6");
            strcpy(DLG_LO,"");
            strcpy(DLG_IF,"");
            strcpy(DLG_carrier_ampl,"-20");
            strcpy(DLG_VBF,"");
            strcpy(DLG_NF,"0");
            strcpy(DLG_min,"1000");
            strcpy(DLG_max,"1000000");
            strcpy(DLG_mult,"1");
            DLG_GPIB = 3;
            DLG_clip = 20;
            DLG_alt  = 0;
            break;

         case IDM_TEK2780 :

            strcpy(DLG_caption, "");
            strcpy(DLG_carrier,"100E6");
            strcpy(DLG_LO,"");
            strcpy(DLG_IF,"");
            strcpy(DLG_carrier_ampl,"-20");
            strcpy(DLG_VBF,"1.0");
            strcpy(DLG_NF,"2.0");
            strcpy(DLG_min,"10");
            strcpy(DLG_max,"1000000");
            strcpy(DLG_mult,"1");
            DLG_GPIB = 1;
            DLG_clip = 10;
            DLG_alt  = 0;
            DLG_0dB  = 0;
            break;

         case IDM_HP8566A :

            strcpy(DLG_caption, "");
            strcpy(DLG_carrier,"100E6");
            strcpy(DLG_LO,"");
            strcpy(DLG_IF,"");
            strcpy(DLG_carrier_ampl,"-10");
            strcpy(DLG_VBF,"1.0");
            strcpy(DLG_NF,"2.0");
            strcpy(DLG_min,"100");
            strcpy(DLG_max,"1000000");
            strcpy(DLG_mult,"1");
            DLG_GPIB = 18;
            DLG_clip = 10;
            DLG_alt  = 0;
            DLG_0dB  = 0;
            break;

         case IDM_HP8567A :

            strcpy(DLG_caption, "");
            strcpy(DLG_carrier,"20E6");
            strcpy(DLG_LO,"");
            strcpy(DLG_IF,"");
            strcpy(DLG_carrier_ampl,"-10");
            strcpy(DLG_VBF,"1.0");
            strcpy(DLG_NF,"2.0");
            strcpy(DLG_min,"10000");
            strcpy(DLG_max,"1000000");
            strcpy(DLG_mult,"1");
            DLG_GPIB = 18;
            DLG_clip = 10;
            DLG_alt  = 0;
            DLG_0dB  = 0;
            break;

         case IDM_HP8568A :

            strcpy(DLG_caption, "");
            strcpy(DLG_carrier,"20E6");
            strcpy(DLG_LO,"");
            strcpy(DLG_IF,"");
            strcpy(DLG_carrier_ampl,"-10");
            strcpy(DLG_VBF,"1.0");
            strcpy(DLG_NF,"2.0");
            strcpy(DLG_min,"100");
            strcpy(DLG_max,"1000000");
            strcpy(DLG_mult,"1");
            DLG_GPIB = 18;
            DLG_clip = 10;
            DLG_alt  = 0;
            DLG_0dB  = 0;
            break;

         case IDM_HP8566B :

            strcpy(DLG_caption, "");
            strcpy(DLG_carrier,"100E6");
            strcpy(DLG_LO,"");
            strcpy(DLG_IF,"");
            strcpy(DLG_carrier_ampl,"-10");
            strcpy(DLG_VBF,"1.0");
            strcpy(DLG_NF,"2.0");
            strcpy(DLG_min,"100");
            strcpy(DLG_max,"1000000");
            strcpy(DLG_mult,"1");
            DLG_GPIB = 18;
            DLG_clip = 10;
            DLG_alt  = 0;
            DLG_0dB  = 0;
            break;

         case IDM_HP8567B :

            strcpy(DLG_caption, "");
            strcpy(DLG_carrier,"20E6");
            strcpy(DLG_LO,"");
            strcpy(DLG_IF,"");
            strcpy(DLG_carrier_ampl,"-10");
            strcpy(DLG_VBF,"1.0");
            strcpy(DLG_NF,"2.0");
            strcpy(DLG_min,"10000");
            strcpy(DLG_max,"1000000");
            strcpy(DLG_mult,"1");
            DLG_GPIB = 18;
            DLG_clip = 10;
            DLG_alt  = 0;
            DLG_0dB  = 0;
            break;

         case IDM_HP8568B :

            strcpy(DLG_caption, "");
            strcpy(DLG_carrier,"20E6");
            strcpy(DLG_LO,"");
            strcpy(DLG_IF,"");
            strcpy(DLG_carrier_ampl,"-10");
            strcpy(DLG_VBF,"1.0");
            strcpy(DLG_NF,"2.0");
            strcpy(DLG_min,"100");
            strcpy(DLG_max,"1000000");
            strcpy(DLG_mult,"1");
            DLG_GPIB = 18;
            DLG_clip = 10;
            DLG_alt  = 0;
            DLG_0dB  = 0;
            break;

         case IDM_HP8560A:

            strcpy(DLG_caption, "");
            strcpy(DLG_carrier,"300E6");
            strcpy(DLG_LO,"");
            strcpy(DLG_IF,"");
            strcpy(DLG_carrier_ampl,"-10");
            strcpy(DLG_VBF,"1.0");
            strcpy(DLG_NF,"2.0");
            strcpy(DLG_min,"1000");
            strcpy(DLG_max,"1000000");
            strcpy(DLG_mult,"1");
            DLG_GPIB = 18;
            DLG_clip = 10;
            DLG_alt  = 0;
            DLG_0dB  = 0;
            break;

         case IDM_HP8560E:

            strcpy(DLG_caption, "");
            strcpy(DLG_carrier,"300E6");
            strcpy(DLG_LO,"");
            strcpy(DLG_IF,"");
            strcpy(DLG_carrier_ampl,"-10");
            strcpy(DLG_VBF,"1.0");
            strcpy(DLG_NF,"2.0");
            strcpy(DLG_min,"10");
            strcpy(DLG_max,"1000000");
            strcpy(DLG_mult,"1");
            DLG_GPIB = 18;
            DLG_clip = 10;
            DLG_alt  = 0;
            DLG_0dB  = 0;
            break;

         case IDM_HP8590 :

            strcpy(DLG_caption, "");
            strcpy(DLG_carrier,"300E6");
            strcpy(DLG_LO,"");
            strcpy(DLG_IF,"");
            strcpy(DLG_carrier_ampl,"-20");
            strcpy(DLG_VBF,"1.0");
            strcpy(DLG_NF,"2.0");
            strcpy(DLG_min,"1000");
            strcpy(DLG_max,"1000000");
            strcpy(DLG_mult,"1");
            DLG_GPIB = 18;
            DLG_clip = 10;
            DLG_alt  = 0;
            DLG_0dB  = 0;
            break;

         case IDM_HP4195A :

            strcpy(DLG_caption, "");
            strcpy(DLG_carrier,"10E6");
            strcpy(DLG_LO,"");
            strcpy(DLG_IF,"");
            strcpy(DLG_carrier_ampl,"-20");
            strcpy(DLG_VBF,"1.0");
            strcpy(DLG_NF,"0.0");
            strcpy(DLG_min,"1000");
            strcpy(DLG_max,"1000000");
            strcpy(DLG_mult,"1");
            DLG_GPIB = 17;
            DLG_clip = 10;
            DLG_alt  = 0;
            DLG_0dB  = 0;
            break;

         case IDM_HP70000:

            strcpy(DLG_caption, "");
            strcpy(DLG_carrier,"300E6");
            strcpy(DLG_LO,"");
            strcpy(DLG_IF,"");
            strcpy(DLG_carrier_ampl,"-10");
            strcpy(DLG_VBF,"1.0");
            strcpy(DLG_NF,"2.0");
            strcpy(DLG_min,"100");
            strcpy(DLG_max,"1000000");
            strcpy(DLG_mult,"1");
            DLG_GPIB = 18;
            DLG_clip = 10;
            DLG_alt  = 0;
            DLG_0dB  = 0;
            break;

         case IDM_HP3585:

            strcpy(DLG_caption, "");
            strcpy(DLG_carrier,"10E6");
            strcpy(DLG_LO,"");
            strcpy(DLG_IF,"");
            strcpy(DLG_carrier_ampl,"-10");
            strcpy(DLG_VBF,"0.01");
            strcpy(DLG_NF,"2.5");
            strcpy(DLG_min,"10");
            strcpy(DLG_max,"1000000");
            strcpy(DLG_mult,"1");
            DLG_GPIB = 11;
            DLG_clip = 10;
            DLG_alt  = 0;
            DLG_0dB  = 0;
            break;

         case IDM_R3267:

            strcpy(DLG_caption, "");
            strcpy(DLG_carrier,"30E6");
            strcpy(DLG_LO,"");
            strcpy(DLG_IF,"");
            strcpy(DLG_carrier_ampl,"-10");
            strcpy(DLG_VBF,"1.0");
            strcpy(DLG_NF,"2.0");
            strcpy(DLG_min,"10");
            strcpy(DLG_max,"1000000");
            strcpy(DLG_mult,"1");
            DLG_GPIB = 7;
            DLG_clip = 10;
            DLG_alt  = 0;
            DLG_0dB  = 0;
            break;

         case IDM_R3465:

            strcpy(DLG_caption, "");
            strcpy(DLG_carrier,"30E6");
            strcpy(DLG_LO,"");
            strcpy(DLG_IF,"");
            strcpy(DLG_carrier_ampl,"-10");
            strcpy(DLG_VBF,"1.0");
            strcpy(DLG_NF,"2.0");
            strcpy(DLG_min,"1000");
            strcpy(DLG_max,"1000000");
            strcpy(DLG_mult,"1");
            DLG_GPIB = 17;
            DLG_clip = 10;
            DLG_alt  = 0;
            DLG_0dB  = 0;
            break;

         case IDM_R3132:

            strcpy(DLG_caption, "");
            strcpy(DLG_carrier,"30E6");
            strcpy(DLG_LO,"");
            strcpy(DLG_IF,"");
            strcpy(DLG_carrier_ampl,"-20");
            strcpy(DLG_VBF,"1.0");
            strcpy(DLG_NF,"2.0");
            strcpy(DLG_min,"10000");
            strcpy(DLG_max,"1000000");
            strcpy(DLG_mult,"1");
            DLG_GPIB = 8;
            DLG_clip = 10;
            DLG_alt  = 0;
            DLG_0dB  = 0;
            break;

         case IDM_R3131:

            strcpy(DLG_caption, "");
            strcpy(DLG_carrier,"30E6");
            strcpy(DLG_LO,"");
            strcpy(DLG_IF,"");
            strcpy(DLG_carrier_ampl,"-20");
            strcpy(DLG_VBF,"1.0");
            strcpy(DLG_NF,"2.0");
            strcpy(DLG_min,"10000");
            strcpy(DLG_max,"1000000");
            strcpy(DLG_mult,"1");
            DLG_GPIB = 8;
            DLG_clip = 10;
            DLG_alt  = 0;
            DLG_0dB  = 0;
            break;

         case IDM_R3261:

            strcpy(DLG_caption, "");
            strcpy(DLG_carrier,"30E6");
            strcpy(DLG_LO,"");
            strcpy(DLG_IF,"");
            strcpy(DLG_carrier_ampl,"-20");
            strcpy(DLG_VBF,"1.0");
            strcpy(DLG_NF,"2.0");
            strcpy(DLG_min,"100");
            strcpy(DLG_max,"1000000");
            strcpy(DLG_mult,"1");
            DLG_GPIB = 18;
            DLG_clip = 20;
            DLG_alt  = 0;
            DLG_0dB  = 0;
            break;

         case IDM_TR417X:

            strcpy(DLG_caption, "");
            strcpy(DLG_carrier,"50E6");
            strcpy(DLG_LO,"");
            strcpy(DLG_IF,"");
            strcpy(DLG_carrier_ampl,"-20");
            strcpy(DLG_VBF,"1.0");
            strcpy(DLG_NF,"2.0");
            strcpy(DLG_min,"100");
            strcpy(DLG_max,"1000000");
            strcpy(DLG_mult,"1");
            DLG_GPIB = 16;
            DLG_clip = 10;
            DLG_alt  = 0;
            DLG_0dB = 0;
            break;

         case IDM_MS8604A:

            strcpy(DLG_caption, "");
            strcpy(DLG_carrier,"50E6");
            strcpy(DLG_LO,"");
            strcpy(DLG_IF,"");
            strcpy(DLG_carrier_ampl,"0");
            strcpy(DLG_VBF,"1.0");
            strcpy(DLG_NF,"2.0");
            strcpy(DLG_min,"100");
            strcpy(DLG_max,"1000000");
            strcpy(DLG_mult,"1");
            DLG_GPIB = 1;
            DLG_clip = 10;
            DLG_alt  = 0;
            DLG_0dB = 0;
            break;

         case IDM_MS2650:

            strcpy(DLG_caption, "");
            strcpy(DLG_carrier,"50E6");
            strcpy(DLG_LO,"");
            strcpy(DLG_IF,"");
            strcpy(DLG_carrier_ampl,"0");
            strcpy(DLG_VBF,"1.0");
            strcpy(DLG_NF,"2.0");
            strcpy(DLG_min,"10000");
            strcpy(DLG_max,"1000000");
            strcpy(DLG_mult,"1");
            DLG_GPIB = 1;
            DLG_clip = 10;
            DLG_alt  = 0;
            DLG_0dB = 0;
            break;

         case IDM_E4400:

            strcpy(DLG_caption, "");
            strcpy(DLG_carrier,"50E6");
            strcpy(DLG_LO,"");
            strcpy(DLG_IF,"");
            strcpy(DLG_carrier_ampl,"-20");
            strcpy(DLG_VBF,"1.0");
            strcpy(DLG_NF,"2.0");
            strcpy(DLG_min,"100");
            strcpy(DLG_max,"1000000");
            strcpy(DLG_mult,"1");
            DLG_GPIB = 18;
            DLG_clip = 10;
            DLG_alt  = 0;
            DLG_0dB  = 0;
            break;

         case IDM_E4406A:

            strcpy(DLG_caption, "");
            strcpy(DLG_carrier,"50E6");
            strcpy(DLG_LO,"");
            strcpy(DLG_IF,"");
            strcpy(DLG_carrier_ampl,"10");
            strcpy(DLG_VBF,"1.0");
            strcpy(DLG_NF,"2.0");
            strcpy(DLG_min,"10");
            strcpy(DLG_max,"1000000");
            strcpy(DLG_mult,"1");
            DLG_GPIB = 18;
            DLG_clip = 0;
            DLG_alt  = 0;
            DLG_0dB  = 0;
            break;

         case IDM_HP4396A:

            strcpy(DLG_caption, "");
            strcpy(DLG_carrier,"100E6");
            strcpy(DLG_LO,"");
            strcpy(DLG_IF,"");
            strcpy(DLG_carrier_ampl,"-10");
            strcpy(DLG_VBF,"1.0");
            strcpy(DLG_NF,"0.0");
            strcpy(DLG_min,"100");
            strcpy(DLG_max,"1000000");
            strcpy(DLG_mult,"1");
            DLG_GPIB = 17;
            DLG_clip = 0;
            DLG_alt  = 0;
            DLG_0dB  = 0;
            break;

       case IDM_HP358XA:

            strcpy(DLG_caption, "");
            strcpy(DLG_carrier,"20E6");
            strcpy(DLG_LO,"");
            strcpy(DLG_IF,"");
            strcpy(DLG_carrier_ampl,"-10");
            strcpy(DLG_VBF,"1.0");
            strcpy(DLG_NF,"0");
            strcpy(DLG_min,"100");
            strcpy(DLG_max,"1000000");
            strcpy(DLG_mult,"1");
            DLG_GPIB = 7;
            DLG_clip = 10;
            DLG_alt  = 0;
            DLG_0dB  = 0;
            break;

       case IDM_FSIQ:

            strcpy(DLG_caption, "");
            strcpy(DLG_carrier,"100E6");
            strcpy(DLG_LO,"");
            strcpy(DLG_IF,"");
            strcpy(DLG_carrier_ampl,"0");
            strcpy(DLG_VBF,"1.0");
            strcpy(DLG_NF,"2.0");
            strcpy(DLG_min,"100");
            strcpy(DLG_max,"1000000");
            strcpy(DLG_mult,"1");
            DLG_GPIB = 18;
            DLG_clip = 0;
            DLG_alt  = 0;
            DLG_0dB  = 0;
            break;

       case IDM_FSU:

            strcpy(DLG_caption, "");
            strcpy(DLG_carrier,"100E6");
            strcpy(DLG_LO,"");
            strcpy(DLG_IF,"");
            strcpy(DLG_carrier_ampl,"0");
            strcpy(DLG_VBF,"1.0");
            strcpy(DLG_NF,"2.0");
            strcpy(DLG_min,"100");
            strcpy(DLG_max,"1000000");
            strcpy(DLG_mult,"1");
            DLG_GPIB = 18;
            DLG_clip = 0;
            DLG_alt  = 0;
            DLG_0dB  = 0;
            break;

       case IDM_FSQ:

            strcpy(DLG_caption, "");
            strcpy(DLG_carrier,"100E6");
            strcpy(DLG_LO,"");
            strcpy(DLG_IF,"");
            strcpy(DLG_carrier_ampl,"0");
            strcpy(DLG_VBF,"1.0");
            strcpy(DLG_NF,"2.0");
            strcpy(DLG_min,"100");
            strcpy(DLG_max,"1000000");
            strcpy(DLG_mult,"1");
            DLG_GPIB = 20;
            DLG_clip = 0;
            DLG_alt  = 0;
            DLG_0dB  = 0;
            break;

       case IDM_FSP:

            strcpy(DLG_caption, "");
            strcpy(DLG_carrier,"100E6");
            strcpy(DLG_LO,"");
            strcpy(DLG_IF,"");
            strcpy(DLG_carrier_ampl,"0");
            strcpy(DLG_VBF,"1.0");
            strcpy(DLG_NF,"2.0");
            strcpy(DLG_min,"100");
            strcpy(DLG_max,"1000000");
            strcpy(DLG_mult,"1");
            DLG_GPIB = 20;
            DLG_clip = 0;
            DLG_alt  = 0;
            DLG_0dB  = 0;
            break;
         }
      }

   //
   // Launch dialog box
   //

   if (!DialogBox(hInstance,
                  MAKEINTRESOURCE(IDD_PN_PARAMS),
                  hWnd,
                  AcqDlgProc))
      {
      return;
      } 

   //
   // Arrange to write DLG_ parameters back to PN.INI
   //

   strcpy(INI_DLG_caption,      DLG_caption);
   strcpy(INI_DLG_carrier,      DLG_carrier);
   strcpy(INI_DLG_LO,           DLG_LO);
   strcpy(INI_DLG_IF,           DLG_IF);
   strcpy(INI_DLG_carrier_ampl, DLG_carrier_ampl);
   strcpy(INI_DLG_VBF,          DLG_VBF);
   strcpy(INI_DLG_NF,           DLG_NF);
   strcpy(INI_DLG_min,          DLG_min);
   strcpy(INI_DLG_max,          DLG_max);
   strcpy(INI_DLG_mult,         DLG_mult);

   INI_DLG_GPIB      = DLG_GPIB;
   INI_DLG_clip      = DLG_clip;
   INI_DLG_alt       = DLG_alt;
   INI_DLG_0dB       = DLG_0dB;
   INI_DLG_source_ID = DLG_source_ID;

   //
   // Parse and validate acquisition property fields
   //

   DOUBLE td;

   S64    ACQ_carrier      = -1;
   S64    ACQ_LO           = -1;
   S64    ACQ_IF           = -1;
   SINGLE ACQ_carrier_ampl = 10000.0F;
   SINGLE ACQ_min          = 10000.0F;
   SINGLE ACQ_max          = 1000000.0F;
   SINGLE ACQ_VBF          = 1.0F;
   SINGLE ACQ_NF           = 2.0F;
   DOUBLE ACQ_mult         = 1.0;
   S32    ACQ_clip         = DLG_clip;
   S32    ACQ_alt          = DLG_alt;
   S32    ACQ_0dB          = DLG_0dB;
   S32    ACQ_GPIB         = DLG_GPIB;
   C8    *ACQ_caption      = DLG_caption;

   sscanf(DLG_min,"%f",&ACQ_min);
   sscanf(DLG_max,"%f",&ACQ_max);
   sscanf(DLG_VBF,"%f",&ACQ_VBF);

   switch (DLG_source_ID)
      {
      case IDM_E4406A:     // (Only E4406A can handle floating-point RBW values) 
         {
         if (ACQ_min < 1.0F) ACQ_min = 1.0F;          
         if (ACQ_max > 10000000.0F) ACQ_max = 100000000.0F;
         break;
         }

      default:    
         {
         if (ACQ_min < 10.0F) ACQ_min = 10.0F;          
         if (ACQ_max > 10000000.0F) ACQ_max = 100000000.0F;
         break;
         }
      }

   if (strlen(DLG_NF))
      {
      sscanf(DLG_NF,"%f",&ACQ_NF);
      }

   if (strlen(DLG_carrier_ampl))
      {
      sscanf(DLG_carrier_ampl,"%f",&ACQ_carrier_ampl);
      }

   if (strlen(DLG_carrier))
      {
      sscanf(DLG_carrier,"%lf",&td);
      ACQ_carrier = (S64) (td + 0.5);
      }

   if (strlen(DLG_mult))
      {
      sscanf(DLG_mult,"%lf",&ACQ_mult);
      }

   if (strlen(DLG_LO))
      {
      sscanf(DLG_LO,"%lf",&td);
      ACQ_LO = (S64) (td + 0.5);
      }

   if (strlen(DLG_IF))
      {
      sscanf(DLG_IF,"%lf",&td);
      ACQ_IF = (S64) (td + 0.5);
      }

   if ((ACQ_LO > 0) || (ACQ_IF >= 0))
      {
      if ((ACQ_LO < 0) || (ACQ_IF < 0) || (ACQ_carrier == -1))
         {
         SAL_alert_box("Error","When external mixer is used, frequencies at all three ports must be unambiguously specified");
         return;
         }
      }

   //
   // Generate a temporary filename with form <caption text>?.pnp, where ? is 
   // digit from 0 to MAX_TMPFILES-1
   //
   // If no caption text provided, use temp?.pnp
   //
   // The first available temporary filename is assigned to the new trace file...
   // after which we'll delete the next one to free up its slot on an LRU basis
   //

   C8 dir_buffer[MAX_PATH] = "";

   GetCurrentDirectory(sizeof(dir_buffer),
                       dir_buffer);   

   if (strlen(dir_buffer)) 
      {
      if (dir_buffer[strlen(dir_buffer)-1] != '\\')
         {
         strcat(dir_buffer,"\\");
         }
      }

   S32 i;
   C8  ACQ_filename[MAX_PATH];
   C8  oldest_filename[MAX_PATH];
   S64 oldest_file_time = 0x7fffffffffffffff;

   for (i=0; i < MAX_TMPFILES; i++)
      {
      if (DLG_caption[0] == 0)
         {
         sprintf(ACQ_filename,"%stemp%d.pnp",dir_buffer,i);
         }
      else
         {
         sprintf(ACQ_filename,"%s%s%d.pnp",dir_buffer,sanitize_path(DLG_caption,FALSE,FALSE),i);
         }

      FILE *t = fopen(ACQ_filename,"rt");

      if (t == NULL)
         {
         break;
         }

      fclose(t);

      S64 ft = system_time_uS(ACQ_filename);

      if (ft < oldest_file_time)
         {
         oldest_file_time = ft;
         strcpy(oldest_filename, ACQ_filename);
         }
      }

   if (i == MAX_TMPFILES)
      {
      strcpy(ACQ_filename, oldest_filename);
      delete_source_by_filename(ACQ_filename);
      }

   //
   // Launch acquisition routine
   //

   key_hit = FALSE;

   C8 text[256];
   sprintf(text,"Acquiring noise plot, please stand by . . . ");

   VFX_rectangle_fill(stage,
                      28,
                      28,
                      RES_X-28,
                      54,
                      LD_DRAW,
                      RGB_TRIPLET(255,0,0));

   VFX_string_draw(stage,
                   38,
                   38,
                   VFX_default_system_font(),
                   text,
                   VFX_default_font_color_table(VFC_WHITE_ON_XP));

   refresh();

   GPIB_in_progress = TRUE;
   strcpy(cleanup_filename, ACQ_filename);

   BOOL result = FALSE;

   switch (DLG_source_ID)
      {
      case IDM_TEK490:
         {
         result = ACQ_TEK490(ACQ_filename,
                             ACQ_GPIB,
                             ACQ_caption,
                             ACQ_carrier,           
                             ACQ_IF,
                             ACQ_LO,
                             ACQ_VBF,
                             ACQ_mult,
                             ACQ_NF,
                             ACQ_min,
                             ACQ_max,
                             ACQ_clip,
                             ACQ_carrier_ampl);
         break;
         }

      case IDM_TEK2780:
         {
         result = ACQ_TEK2780(ACQ_filename,
                              ACQ_GPIB,
                              ACQ_caption,
                              ACQ_carrier,           
                              ACQ_IF,
                              ACQ_LO,
                              ACQ_VBF,
                              ACQ_mult,
                              ACQ_NF,
                              ACQ_min,
                              ACQ_max,
                              ACQ_clip,
                              ACQ_carrier_ampl);
         break;
         }

      case IDM_HP8560A:
      case IDM_HP8560E:
         {
         result = ACQ_HP8560(ACQ_filename,
                             ACQ_GPIB,
                             ACQ_caption,
                             ACQ_carrier,           
                             ACQ_IF,
                             ACQ_LO,
                             ACQ_VBF,
                             ACQ_mult,
                             ACQ_NF,
                             ACQ_min,
                             ACQ_max,
                             ACQ_clip,
                             ACQ_carrier_ampl);
         break;
         }

      case IDM_HP8590: 
         {
         result = ACQ_HP8590(ACQ_filename,
                             ACQ_GPIB,
                             ACQ_caption,
                             ACQ_carrier,           
                             ACQ_IF,
                             ACQ_LO,
                             ACQ_VBF,
                             ACQ_mult,
                             ACQ_NF,
                             ACQ_min,
                             ACQ_max,
                             ACQ_clip,
                             ACQ_carrier_ampl);
         break;
         }

      case IDM_HP4195A: 
         {
         result = ACQ_HP4195(ACQ_filename,
                             ACQ_GPIB,
                             ACQ_caption,
                             ACQ_carrier,           
                             ACQ_IF,
                             ACQ_LO,
                             ACQ_VBF,
                             ACQ_mult,
                             ACQ_NF,
                             ACQ_min,
                             ACQ_max,
                             ACQ_clip,
                             ACQ_carrier_ampl);
         break;
         }

      case IDM_HP70000: 
         {
         result = ACQ_HP70000(ACQ_filename,
                              ACQ_GPIB,
                              ACQ_caption,
                              ACQ_carrier,           
                              ACQ_IF,
                              ACQ_LO,
                              ACQ_VBF,
                              ACQ_mult,
                              ACQ_NF,
                              ACQ_min,
                              ACQ_max,
                              ACQ_clip,
                              ACQ_carrier_ampl);
         break;
         }

      case IDM_HP3585: 
         {
         result = ACQ_HP3585(ACQ_filename,
                             ACQ_GPIB,
                             ACQ_caption,
                             ACQ_carrier,           
                             ACQ_IF,
                             ACQ_LO,
                             ACQ_VBF,
                             ACQ_mult,
                             ACQ_NF,
                             ACQ_min,
                             ACQ_max,
                             ACQ_clip,
                             ACQ_carrier_ampl);
         break;
         }

      case IDM_R3267:
         {
         result = ACQ_R3267(ACQ_filename,
                            ACQ_GPIB,
                            ACQ_caption,
                            ACQ_carrier,           
                            ACQ_IF,
                            ACQ_LO,
                            ACQ_VBF,
                            ACQ_mult,
                            ACQ_NF,
                            ACQ_min,
                            ACQ_max,
                            ACQ_clip,
                            ACQ_carrier_ampl);
         break;
         }

      case IDM_R3465:
         {
         result = ACQ_R3465(ACQ_filename,
                            ACQ_GPIB,
                            ACQ_caption,
                            ACQ_carrier,           
                            ACQ_IF,
                            ACQ_LO,
                            ACQ_VBF,
                            ACQ_mult,
                            ACQ_NF,
                            ACQ_min,
                            ACQ_max,
                            ACQ_clip,
                            ACQ_carrier_ampl);
         break;
         }

      case IDM_R3132:
         {
         result = ACQ_R3132(ACQ_filename,
                            ACQ_GPIB,
                            ACQ_caption,
                            ACQ_carrier,           
                            ACQ_IF,
                            ACQ_LO,
                            ACQ_VBF,
                            ACQ_mult,
                            ACQ_NF,
                            ACQ_min,
                            ACQ_max,
                            ACQ_clip,
                            ACQ_carrier_ampl);
         break;
         }

      case IDM_R3131:
         {
         result = ACQ_R3131(ACQ_filename,
                            ACQ_GPIB,
                            ACQ_caption,
                            ACQ_carrier,           
                            ACQ_IF,
                            ACQ_LO,
                            ACQ_VBF,
                            ACQ_mult,
                            ACQ_NF,
                            ACQ_min,
                            ACQ_max,
                            ACQ_clip,
                            ACQ_carrier_ampl);
         break;
         }

      case IDM_R3261:
         {
         result = ACQ_R3261(ACQ_filename,
                            ACQ_GPIB,
                            ACQ_caption,
                            ACQ_carrier,           
                            ACQ_IF,
                            ACQ_LO,
                            ACQ_VBF,
                            ACQ_mult,
                            ACQ_NF,
                            ACQ_min,
                            ACQ_max,
                            ACQ_clip,
                            ACQ_carrier_ampl);
         break;
         }

      case IDM_TR417X:
         {
         result = ACQ_TR417X(ACQ_filename,
                             ACQ_GPIB,
                             ACQ_caption,
                             ACQ_carrier,           
                             ACQ_IF,
                             ACQ_LO,
                             ACQ_VBF,
                             ACQ_mult,
                             ACQ_NF,
                             ACQ_min,
                             ACQ_max,
                             ACQ_clip,
                             ACQ_carrier_ampl);
         break;
         }

      case IDM_MS8604A:
         {
         result = ACQ_MS8604A(ACQ_filename,
                              ACQ_GPIB,
                              ACQ_caption,
                              ACQ_carrier,           
                              ACQ_IF,
                              ACQ_LO,
                              ACQ_VBF,
                              ACQ_mult,
                              ACQ_NF,
                              ACQ_min,
                              ACQ_max,
                              ACQ_clip,
                              ACQ_carrier_ampl);
         break;
         }


      case IDM_MS2650:
         {
         result = ACQ_MS2650(ACQ_filename,
                             ACQ_GPIB,
                             ACQ_caption,
                             ACQ_carrier,           
                             ACQ_IF,
                             ACQ_LO,
                             ACQ_VBF,
                             ACQ_mult,
                             ACQ_NF,
                             ACQ_min,
                             ACQ_max,
                             ACQ_clip,
                             ACQ_carrier_ampl);
         break;
         }

      case IDM_E4400:
         {
         result = ACQ_E4400(ACQ_filename,
                            ACQ_GPIB,
                            ACQ_caption,
                            ACQ_carrier,           
                            ACQ_IF,
                            ACQ_LO,
                            ACQ_VBF,
                            ACQ_mult,
                            ACQ_NF,
                            ACQ_min,
                            ACQ_max,
                            ACQ_clip,
                            ACQ_carrier_ampl);
         break;
         }

      case IDM_E4406A:
         {
         result = ACQ_E4406A(ACQ_filename,
                             ACQ_GPIB,
                             ACQ_caption,
                             ACQ_carrier,           
                             ACQ_IF,
                             ACQ_LO,
                             ACQ_VBF,
                             ACQ_mult,
                             ACQ_NF,
                             ACQ_min,
                             ACQ_max,
                             ACQ_clip,
                             ACQ_carrier_ampl);
         break;
         }

      case IDM_HP4396A:
         {
         result = ACQ_HP4396A(ACQ_filename,
                              ACQ_GPIB,
                              ACQ_caption,
                              ACQ_carrier,           
                              ACQ_IF,
                              ACQ_LO,
                              ACQ_VBF,
                              ACQ_mult,
                              ACQ_NF,
                              ACQ_min,
                              ACQ_max,
                              ACQ_clip,
                              ACQ_carrier_ampl);
         break;
         }

      case IDM_HP358XA:
         {
         result = ACQ_HP358XA(ACQ_filename,
                              ACQ_GPIB,
                              ACQ_caption,
                              ACQ_carrier,           
                              ACQ_IF,
                              ACQ_LO,
                              ACQ_VBF,
                              ACQ_mult,
                              ACQ_NF,
                              ACQ_min,
                              ACQ_max,
                              ACQ_clip,
                              ACQ_carrier_ampl);
         break;
         }

      case IDM_FSIQ:
         {
         result = ACQ_FSIQ(ACQ_filename,
                           ACQ_GPIB,
                           ACQ_caption,
                           ACQ_carrier,           
                           ACQ_IF,
                           ACQ_LO,
                           ACQ_VBF,
                           ACQ_mult,
                           ACQ_NF,
                           ACQ_min,
                           ACQ_max,
                           ACQ_clip,
                           ACQ_carrier_ampl,
                           ACQ_alt,
                           500,
                           ACQ_0dB);
         break;
         }

      case IDM_FSU:
         {
         result = ACQ_FSIQ(ACQ_filename,
                           ACQ_GPIB,
                           ACQ_caption,
                           ACQ_carrier,           
                           ACQ_IF,
                           ACQ_LO,
                           ACQ_VBF,
                           ACQ_mult,
                           ACQ_NF,
                           ACQ_min,
                           ACQ_max,
                           ACQ_clip,
                           ACQ_carrier_ampl,
                           ACQ_alt,
                           625,
                           ACQ_0dB);
         break;
         }

      case IDM_FSQ:
         {
         result = ACQ_FSIQ(ACQ_filename,
                           ACQ_GPIB,
                           ACQ_caption,
                           ACQ_carrier,           
                           ACQ_IF,
                           ACQ_LO,
                           ACQ_VBF,
                           ACQ_mult,
                           ACQ_NF,
                           ACQ_min,
                           ACQ_max,
                           ACQ_clip,
                           ACQ_carrier_ampl,
                           ACQ_alt,
                           625,
                           ACQ_0dB);
         break;
         }

      case IDM_FSP:
         {
         result = ACQ_FSIQ(ACQ_filename,
                           ACQ_GPIB,
                           ACQ_caption,
                           ACQ_carrier,           
                           ACQ_IF,
                           ACQ_LO,
                           ACQ_VBF,
                           ACQ_mult,
                           ACQ_NF,
                           ACQ_min,
                           ACQ_max,
                           ACQ_clip,
                           ACQ_carrier_ampl,
                           ACQ_alt,
                           501,
                           ACQ_0dB);
         break;
         }

      case IDM_HP8566B:
      case IDM_HP8567B:
      case IDM_HP8568B:
         {
         result = ACQ_HP8566B_HP8567B_HP8568B(ACQ_filename,
                                              ACQ_GPIB,
                                              ACQ_caption,
                                              ACQ_carrier,           
                                              ACQ_IF,
                                              ACQ_LO,
                                              ACQ_VBF,
                                              ACQ_mult,
                                              ACQ_NF,
                                              ACQ_min,
                                              ACQ_max,
                                              ACQ_clip,
                                              ACQ_carrier_ampl,
                                              DLG_source_ID,
                                              ACQ_alt,
                                              ACQ_0dB);
         break;
         }

      case IDM_HP8566A:
      case IDM_HP8567A:
      case IDM_HP8568A:
         {
         result = ACQ_HP8566A_HP8567A_HP8568A(ACQ_filename,
                                              ACQ_GPIB,
                                              ACQ_caption,
                                              ACQ_carrier,           
                                              ACQ_IF,
                                              ACQ_LO,
                                              ACQ_VBF,
                                              ACQ_mult,
                                              ACQ_NF,
                                              ACQ_min,
                                              ACQ_max,
                                              ACQ_clip,
                                              ACQ_carrier_ampl,
                                              DLG_source_ID,
                                              ACQ_alt,
                                              ACQ_0dB);
         break;                               
         }                                    
      }

   GPIB_in_progress    = FALSE;
   cleanup_filename[0] = 0;

   //
   // Add new file to display list
   //

   if (result)
      {
      current_data_source = n_data_sources++;
      data_source_list[current_data_source].load(ACQ_filename, TRUE);
      }
   else
      {
      _unlink(ACQ_filename);
      }

   force_redraw = TRUE;
   invalid_trace_cache = TRUE;
}

//****************************************************************************
//
// CMD_caption(void)
//
//****************************************************************************

void CMD_caption(S32 data_source)
{
   if (data_source >= n_data_sources)
      {
      return;
      }

   strcpy(DLG_box_caption, "Edit trace caption");

   strcpy(DLG_caption, data_source_list[data_source].caption);

   if (!DialogBox(hInstance,
                  MAKEINTRESOURCE(IDD_PN_CAPTION),
                  hWnd,
                  CapDlgProc))
      {
      return;
      } 

   data_source_list[data_source].set_new_caption(DLG_caption);

   force_redraw = TRUE;
}

//****************************************************************************
//
// CMD_about(void) 
//
//****************************************************************************

void CMD_about(void)
{
   MessageBox(hWnd,
             "Usage: pn <.PNP data file> [...]",
             "Composite Noise Measurement Utility version "VERSION" by John Miles, KE5FX (john@miles.io)",
             MB_OK | MB_ICONINFORMATION);
}

//****************************************************************************
//
// CMD_edit_spur_level(void)
//
//****************************************************************************

void CMD_edit_spur_level(void)
{
   strcpy(DLG_box_caption, "Edit spur clipping threshold");

   if (!DialogBox(hInstance,
                  MAKEINTRESOURCE(IDD_PN_EDIT_SPUR_CLIP),
                  hWnd,
                  SpurDlgProc))
      {
      return;
      }

   force_redraw = TRUE;
   invalid_trace_cache = TRUE;
}

//****************************************************************************
//
// Window message receiver procedure for application
//
//****************************************************************************

long FAR PASCAL WindowProc(HWND   hWnd,   UINT   message,   //)
                           WPARAM wParam, LPARAM lParam)
{
   if (GPIB_in_progress)
      {
      if (message == WM_CHAR)
         {
         key_hit = TRUE;
         }

      if (message == WM_COMMAND)
         {
         SAL_alert_box("Error","Acquisition in progress");
         }

      return DefWindowProc(hWnd, message, wParam, lParam);
      } 

   switch (message)
      {
      case WM_PAINT:
         {
         refresh();
         break;
         }

      case WM_USER:
         {
         //
         // Force default size of file-open/file-save dialogs to be scaled to the parent window
         // size, and center them
         //

         HWND hwndParent = (HWND) wParam;

         RECT parent_rect;
         GetWindowRect(hWnd, &parent_rect);

         S32 width  = parent_rect.right  - parent_rect.left;
         S32 height = parent_rect.bottom - parent_rect.top;

         S32 dlg_width  = width * 8 / 10;
         S32 dlg_height = height * 6 / 10;

         S32 dlg_x = parent_rect.left + ((width - dlg_width) / 2);
         S32 dlg_y = parent_rect.top + ((height - dlg_height) / 2);

         SetWindowPos(hwndParent, HWND_TOP, dlg_x, dlg_y, dlg_width, dlg_height, 0);

         break;
         }

      case WM_KEYDOWN:

         if (GetKeyState(VK_CONTROL) & 0x8000)
            {
            S32 new_smoothing = -1;

            switch (wParam)
               {
               case 'm':      // ctrl-

                  spur_dB--;
                  if (spur_dB < MIN_SPUR_CLIP_DB) spur_dB = MIN_SPUR_CLIP_DB;
                  force_redraw = TRUE;
                  invalid_trace_cache = TRUE;
                  break;

               case 'k':      // ctrl+

                  spur_dB++;
                  if (spur_dB > MAX_SPUR_CLIP_DB) spur_dB = MAX_SPUR_CLIP_DB;
                  force_redraw = TRUE;
                  invalid_trace_cache = TRUE;
                  break;

               case VK_UP:
                  CMD_move_up();
                  break;

               case VK_DOWN:
                  CMD_move_down();
                  break;

               case VK_DELETE: CMD_delete_source(current_data_source); break;

               case 'S': CMD_toggle_spur_reduction(); break;

               case '0': new_smoothing = 0;   break;
               case '1': new_smoothing = 2;   break;
               case '2': new_smoothing = 4;   break;
               case '3': new_smoothing = 8;   break;
               case '4': new_smoothing = 16;  break;
               case '5': new_smoothing = 24;  break;
               case '6': new_smoothing = 32;  break;
               case '7': new_smoothing = 48;  break;
               case '8': new_smoothing = 64;  break;
               case '9': new_smoothing = 96;  break;
               }

            if (new_smoothing != -1)   
               {
               CMD_set_smoothing(new_smoothing);
               break;
               }
            }
         else
            {
            switch (wParam)
               {
               case VK_F1:
                  launch_page("pn.htm");
                  break;

               case VK_DELETE:
                  CMD_close_source(current_data_source);
                  break;

               case VK_HOME:
                  CMD_close_all_sources();
                  break;

               case VK_UP:

                  CMD_prev();
                  break;

               case VK_DOWN:

                  CMD_next();
                  break;
               }
            break;
            }

      case WM_CHAR:

         key_hit = TRUE;

         if (!(GetKeyState(VK_CONTROL) & 0x8000))
            {
            switch (wParam)
               {
               //
               // ESC terminates app
               // 

               case VK_ESCAPE:
               case 'q':
               case 'Q':

                  CMD_quit();
                  break;

               case 'l':

                  CMD_load_PNP();
                  break;

               case 's':
               case 'S':

                  CMD_save(NULL);
                  break;

               case 'x':
               case 'X':

                  CMD_export(NULL);
                  break;

               case 'P':
               case 'p':

                  CMD_print_image();
                  break;

               case 'b':
               case 'B':

                  CMD_browse();
                  break;

               case 'o':
               case 'O':

                  CMD_overlay();
                  break;

               case 'C':   CMD_set_contrast(2); break;
               case 'g':   CMD_toggle_ghost_sample(); break;
               case 'T':   CMD_toggle_thick_trace(); break;
               case 'c':   CMD_toggle_clipping(); break;

               case 'e':   CMD_caption(current_data_source); break;

               case 't':   CMD_edit_spur_level(); break;

               case 'v':   CMD_toggle_visibility(); break;

               case '-':

                  CMD_prev();
                  break;

               case '+':

                  CMD_next();
                  break;

               case '5': CMD_set_resolution(512,384); break;
               case '6': CMD_set_resolution(640,480); break;
               case '7': CMD_set_resolution(800,600); break;
               case '8': CMD_set_resolution(1024,768); break;
               case '9': CMD_set_resolution(1152,864); break;
               case '0': CMD_set_resolution(1280,1024); break;

               case '?': 
                  CMD_about(); 
                  break;

               case ' ':
                  
                  if (INI_DLG_source_ID != -1)
                     {
                     CMD_acquire(INI_DLG_source_ID);
                     }
                  break;
               }
            }
         break;

      case WM_SYSKEYUP:

         if (wParam == VK_RETURN)
            {
            //
            // User has toggled fullscreen_window mode, clearing video memory...
            //

            background_invalid = SAL_get_preference(SAL_MAX_VIDEO_PAGES);
            }
         break;

      case WM_KILLFOCUS:
         
         //
         // Video memory has been lost; set up to reload background image
         // (if any) when control returned
         //

         background_invalid = SAL_get_preference(SAL_MAX_VIDEO_PAGES);
         break;

      case WM_COMMAND:
         {
         switch (wParam)
            {
            case IDM_LOAD_PNP        : CMD_load_PNP();                         break;
            case IDM_SAVE            : CMD_save(NULL);                         break;
            case IDM_EXPORT          : CMD_export(NULL);                       break;
            case IDM_PRINT_IMAGE     : CMD_print_image();                      break;
            case IDM_SORT_MODIFY     : CMD_toggle_sort_modify_date();          break;
            case IDM_QUIT            : CMD_quit();                             break;
            case IDM_BROWSE          : CMD_browse();                           break;
            case IDM_OVERLAY         : CMD_overlay();                          break;
            case IDM_NEXT            : CMD_next();                             break;
            case IDM_PREV            : CMD_prev();                             break;
            case IDM_MOVE_UP         : CMD_move_up();                          break;
            case IDM_MOVE_DOWN       : CMD_move_down();                        break;
            case IDM_VIS             : CMD_toggle_visibility();                break;
            case IDM_CLOSE           : CMD_close_source(current_data_source);  break;
            case IDM_CLOSE_ALL       : CMD_close_all_sources();                break;
            case IDM_DELETE          : CMD_delete_source(current_data_source); break;
            case IDM_384             : CMD_set_resolution(512, 384);           break;
            case IDM_400             : CMD_set_resolution(640, 400);           break; 
            case IDM_480             : CMD_set_resolution(640, 480);           break; 
            case IDM_600             : CMD_set_resolution(800, 600);           break; 
            case IDM_768             : CMD_set_resolution(1024, 768);          break; 
            case IDM_864             : CMD_set_resolution(1152, 864);          break; 
            case IDM_1024            : CMD_set_resolution(1280, 1024);         break; 
            case IDM_LOW_CONTRAST    : CMD_set_contrast(0);                    break;
            case IDM_MED_CONTRAST    : CMD_set_contrast(1);                    break;
            case IDM_HIGH_CONTRAST   : CMD_set_contrast(2);                    break;
            case IDM_ALLOW_CLIPPING  : CMD_toggle_clipping();                  break;
            case IDM_S_ALT           : CMD_toggle_alt_smoothing();             break;
            case IDM_SPUR            : CMD_toggle_spur_reduction();            break;
            case IDM_GHOST           : CMD_toggle_ghost_sample();              break;
            case IDM_THICK_TRACE     : CMD_toggle_thick_trace();               break;
            case IDM_EDIT_CAPTION    : CMD_caption(current_data_source);       break;
            case IDM_EDIT_SPUR       : CMD_edit_spur_level();                  break;
            case IDM_S_NONE          : CMD_set_smoothing(0);                   break;
            case IDM_S_2             : CMD_set_smoothing(2);                   break;
            case IDM_S_4             : CMD_set_smoothing(4);                   break;
            case IDM_S_8             : CMD_set_smoothing(8);                   break;
            case IDM_S_16            : CMD_set_smoothing(16);                  break;
            case IDM_S_24            : CMD_set_smoothing(24);                  break;
            case IDM_S_32            : CMD_set_smoothing(32);                  break;
            case IDM_S_48            : CMD_set_smoothing(48);                  break;
            case IDM_S_64            : CMD_set_smoothing(64);                  break;
            case IDM_S_96            : CMD_set_smoothing(96);                  break;
            case IDM_TEK490          : CMD_acquire(IDM_TEK490);                break;
            case IDM_TEK2780         : CMD_acquire(IDM_TEK2780);               break;
            case IDM_HP8566B         : CMD_acquire(IDM_HP8566B);               break; 
            case IDM_HP8567B         : CMD_acquire(IDM_HP8567B);               break; 
            case IDM_HP8568B         : CMD_acquire(IDM_HP8568B);               break; 
            case IDM_HP8566A         : CMD_acquire(IDM_HP8566A);               break; 
            case IDM_HP8567A         : CMD_acquire(IDM_HP8567A);               break; 
            case IDM_HP8568A         : CMD_acquire(IDM_HP8568A);               break; 
            case IDM_HP8560A         : CMD_acquire(IDM_HP8560A);               break; 
            case IDM_HP8560E         : CMD_acquire(IDM_HP8560E);               break; 
            case IDM_HP8590          : CMD_acquire(IDM_HP8590);                break; 
            case IDM_HP4195A         : CMD_acquire(IDM_HP4195A);               break; 
            case IDM_HP70000         : CMD_acquire(IDM_HP70000);               break; 
            case IDM_HP3585          : CMD_acquire(IDM_HP3585);                break; 
            case IDM_HP358XA         : CMD_acquire(IDM_HP358XA);               break; 
            case IDM_FSIQ            : CMD_acquire(IDM_FSIQ);                  break; 
            case IDM_FSP             : CMD_acquire(IDM_FSP);                   break; 
            case IDM_FSQ             : CMD_acquire(IDM_FSQ);                   break; 
            case IDM_FSU             : CMD_acquire(IDM_FSU);                   break; 
            case IDM_R3267           : CMD_acquire(IDM_R3267);                 break; 
            case IDM_R3465           : CMD_acquire(IDM_R3465);                 break; 
            case IDM_R3131           : CMD_acquire(IDM_R3131);                 break; 
            case IDM_R3132           : CMD_acquire(IDM_R3132);                 break; 
            case IDM_R3261           : CMD_acquire(IDM_R3261);                 break; 
            case IDM_TR417X          : CMD_acquire(IDM_TR417X);                break; 
            case IDM_MS8604A         : CMD_acquire(IDM_MS8604A);               break; 
            case IDM_MS2650          : CMD_acquire(IDM_MS2650);                break; 
            case IDM_E4400           : CMD_acquire(IDM_E4400);                 break; 
            case IDM_E4406A          : CMD_acquire(IDM_E4406A);                break; 
            case IDM_HP4396A         : CMD_acquire(IDM_HP4396A);               break; 

            case IDM_REPEAT_LAST: 

               if (INI_DLG_source_ID != -1)
                  {
                  CMD_acquire(INI_DLG_source_ID);
                  }
               break;

            case IDM_ABOUT: 
            
               CMD_about();                            
               break;

            case IDM_USER_GUIDE:
               {
               launch_page("pn.htm");
               break;
               }

            case IDM_ALL:
               {
               for (S32 i=0; i < MAX_LEGEND_FIELDS; i++)
                  {
                  INI_show_legend_field[i] = 1;
                  }

               force_redraw = TRUE;
               main_menu_update();
               break;
               }

            case IDM_NONE:
               {
               for (S32 i=0; i < MAX_LEGEND_FIELDS; i++)
                  {
                  INI_show_legend_field[i] = 0;
                  }

               force_redraw = TRUE;
               main_menu_update();
               break;
               }

            default:
               {
               if ((wParam >= IDM_S_CAPTION) && (wParam < (IDM_S_CAPTION + MAX_LEGEND_FIELDS)))
                  {
                  S32 e = wParam - IDM_S_CAPTION;

                  INI_show_legend_field[e] ^= 1;

                  force_redraw = TRUE;
                  main_menu_update();
                  break;
                  }

               break;
               }
            }
         }
         break;

      }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

//****************************************************************************
//
// Windows main() function
//
//****************************************************************************

int PASCAL WinMain(HINSTANCE hInst, //)
                   HINSTANCE hPrevInst,
                   LPSTR     lpCmdLine,     
                   int       nCmdShow)
{
   //
   // Make modifiable copy of command line
   //
   // If no entries are present on actual command line, we'll append any PNP_source entries
   // from the PN.INI file to the command line automatically
   //

   static C8 cmd_line[MAX_SOURCES * (MAX_PATH + 16)];
   cmd_line[0] = 0;

   strcpy(cmd_line, lpCmdLine);

   //
   // Initialize system abstraction layer -- must succeed in order to continue
   //

   hInstance = hInst;

   SAL_set_preference(SAL_USE_PARAMON, NO);     // Leave parallel ports alone -- we may be using
                                                // them to control the DUT!
   if (!SAL_startup(hInstance,
                    szAppName,
                    TRUE,
                    WinExit))
      {
      return 0;
      }

   //
   // Get current working directory and make sure it's not the Windows desktop
   // (We don't want to drop our temp files there)
   //
   // If it is, try to change the CWD to the current user's My Documents folder
   // Otherwise, leave the CWD alone to permit use of the "Start In" field on a
   // desktop shortcut
   //
   // (Also do this if the current working directory contains "Program Files")
   //

   C8 docs[MAX_PATH] = "";
   C8 desktop[MAX_PATH] = "";

   SHGetSpecialFolderPath(HWND_DESKTOP,
                          desktop,
                          CSIDL_DESKTOPDIRECTORY,
                          FALSE);

   SHGetSpecialFolderPath(HWND_DESKTOP,
                          docs,
                          CSIDL_PERSONAL,
                          FALSE);

   C8 CWD[MAX_PATH] = "";

   if (GetCurrentDirectory(sizeof(CWD),
                           CWD))
      {
       _strlwr(CWD);
       _strlwr(desktop);
    
      if ((!_stricmp(CWD,desktop)) ||
            (strstr(CWD,"program files") != NULL))
         {
         SetCurrentDirectory(docs);
         strcpy(CWD, docs);
         }
      }

   //
   // Find PN.INI and read it
   //

   GetModuleFileName(NULL, INI_path, sizeof(INI_path)-1);

   _strlwr(INI_path);

   C8 *exe = strstr(INI_path,"pn.exe");

   if (exe != NULL)
      {
      sprintf(exe,"pn.ini");
      }
   else
      {
      strcpy(INI_path,"pn.ini");
      }

   FILE *options = fopen(INI_path,"rt");

   if (options == NULL)
      {
      SAL_alert_box("Warning","Could not find PN.INI");
      }
   else
      {
      while (1)
         {
         //
         // Read input line
         //

         C8 linbuf[512];

         memset(linbuf,
                0,
                sizeof(linbuf));

         C8 *result = fgets(linbuf, 
                            sizeof(linbuf) - 1, 
                            options);

         if ((result == NULL) || (feof(options)))
            {
            break;
            }

         //
         // Skip blank lines, and kill trailing and leading spaces
         //

         S32 l = strlen(linbuf);

         if ((!l) || (linbuf[0] == ';'))
            {
            continue;
            }

         C8 *lexeme  = linbuf;
         C8 *end     = linbuf;
         S32 leading = 1;

         for (S32 i=0; i < l; i++)
            {
            if (!isspace(linbuf[i]))
               {
               if (leading)
                  {
                  lexeme = &linbuf[i];
                  leading = 0;
                  }

               end = &linbuf[i];
               }
            }

         end[1] = 0;

         if ((leading) || (!strlen(lexeme)))
            {
            continue;
            }

         //
         // Terminate key substring at first space
         //

         C8 *value = strchr(lexeme,' ');

         if (value == NULL)
            {
            value = strchr(lexeme,'\t');
            }

         if (value == NULL)
            {
            continue;
            }

         *value = 0;
         value++;

         //
         // Look for key/value pairs
         // Skip spaces and tabs between key and value
         //
         
         while (1)
            {
            if ((*value == ' ') || (*value == '\t'))
               {
               ++value;
               }
            else
               {
               break;
               }
            }

         if (*value == 0)
            {
            continue;
            }

         S32 j = 0;

         assert(MAX_LEGEND_FIELDS == T_END_OF_TABLE);

         for (j=0; j < MAX_LEGEND_FIELDS; j++)
            {
            if (!_stricmp(lexeme,legend_INI_names[j]))
               {
               INI_show_legend_field[j] = atoi(value);
               break;
               }
            }

         if (j == MAX_LEGEND_FIELDS)
            {
            if      (!_stricmp(lexeme,"spot_offset"))          INI_default_spot_offset_Hz      = (SINGLE) fast_atof(&value);
            else if (!_stricmp(lexeme,"L_offset"))             INI_default_L_offset_Hz         = (SINGLE) fast_atof(&value);
            else if (!_stricmp(lexeme,"U_offset"))             INI_default_U_offset_Hz         = (SINGLE) fast_atof(&value);
            else if (!_stricmp(lexeme,"res_x"))                INI_res_x                    = atoi(value);
            else if (!_stricmp(lexeme,"res_y"))                INI_res_y                    = atoi(value);
            else if (!_stricmp(lexeme,"smooth"))               INI_default_smoothing        = atoi(value);
            else if (!_stricmp(lexeme,"contrast"))             INI_default_contrast         = atoi(value);
            else if (!_stricmp(lexeme,"ghost_sample"))         INI_default_ghost_sample     = atoi(value);
            else if (!_stricmp(lexeme,"thick_trace"))          INI_default_thick_trace      = atoi(value);
            else if (!_stricmp(lexeme,"allow_clipping"))       INI_default_allow_clipping   = atoi(value);
            else if (!_stricmp(lexeme,"alt_smoothing"))        INI_default_alt_smoothing    = atoi(value);
            else if (!_stricmp(lexeme,"spur_reduction"))       INI_default_spur_reduction   = atoi(value);
            else if (!_stricmp(lexeme,"spur_dB"))              INI_default_spur_dB          = atoi(value);
            else if (!_stricmp(lexeme,"sort_modify_date"))     INI_default_sort_modify_date = atoi(value);
            else if (!_stricmp(lexeme,"auto_print_mode"))      INI_auto_print_mode          = atoi(value);
            else if (!_stricmp(lexeme,"DLG_source_ID"))        INI_DLG_source_ID            = atoi(value);
            else if (!_stricmp(lexeme,"DLG_GPIB"))             INI_DLG_GPIB                 = atoi(value);
            else if (!_stricmp(lexeme,"DLG_clip"))             INI_DLG_clip                 = atoi(value);
            else if (!_stricmp(lexeme,"DLG_alt"))              INI_DLG_alt                  = atoi(value);
            else if (!_stricmp(lexeme,"DLG_0dB"))              INI_DLG_0dB                  = atoi(value);
            else if (!_stricmp(lexeme,"default_save_dir"))     strcpy(INI_save_directory,   value);
            else if (!_stricmp(lexeme,"auto_save_filename"))   strcpy(INI_auto_save,        value);
            else if (!_stricmp(lexeme,"DLG_caption"))          strcpy(INI_DLG_caption,      value);
            else if (!_stricmp(lexeme,"DLG_carrier"))          strcpy(INI_DLG_carrier,      value);
            else if (!_stricmp(lexeme,"DLG_mult"))             strcpy(INI_DLG_mult,         value);
            else if (!_stricmp(lexeme,"DLG_LO"))               strcpy(INI_DLG_LO,           value);
            else if (!_stricmp(lexeme,"DLG_IF"))               strcpy(INI_DLG_IF,           value);
            else if (!_stricmp(lexeme,"DLG_carrier_ampl"))     strcpy(INI_DLG_carrier_ampl, value);
            else if (!_stricmp(lexeme,"DLG_VBF"))              strcpy(INI_DLG_VBF,          value);
            else if (!_stricmp(lexeme,"DLG_NF"))               strcpy(INI_DLG_NF,           value);
            else if (!_stricmp(lexeme,"DLG_min"))              strcpy(INI_DLG_min,          value);
            else if (!_stricmp(lexeme,"DLG_max"))              strcpy(INI_DLG_max,          value);
            else if (!_stricmp(lexeme,"PNP_source"))
               {
               if ((!strlen(lpCmdLine)) && 
                   (strlen(cmd_line) + strlen(value) + 16 < sizeof(cmd_line)))
                  {
                  strcat(cmd_line, "\"");
                  strcat(cmd_line, value);
                  strcat(cmd_line, "\" ");
                  }
               }

            else SAL_alert_box("Warning","Unknown PN.INI entry '%s'",lexeme);
            }
         }

      fclose(options);
      }

   SAL_debug_printf("Command line: [%s]\n",cmd_line);

   RES_X             = INI_res_x;
   RES_Y             = INI_res_y;
   requested_RES_X   = INI_res_x;
   requested_RES_Y   = INI_res_y;
   smooth_samples    = INI_default_smoothing;
   spot_offset_Hz    = INI_default_spot_offset_Hz;
   L_offset_Hz       = INI_default_L_offset_Hz;
   U_offset_Hz       = INI_default_U_offset_Hz;
   contrast          = INI_default_contrast;
   sort_modify_date  = INI_default_sort_modify_date;
   ghost_sample      = INI_default_ghost_sample;
   thick_trace       = INI_default_thick_trace;
   allow_clipping    = INI_default_allow_clipping;
   alt_smoothing     = INI_default_alt_smoothing;
   spur_reduction    = INI_default_spur_reduction;
   spur_dB           = INI_default_spur_dB;

   //
   // Create application window
   // 

   SAL_set_preference(SAL_ALLOW_WINDOW_RESIZE, NO);
   SAL_set_preference(SAL_MAXIMIZE_TO_FULLSCREEN, NO);
   SAL_set_preference(SAL_USE_DDRAW_IN_WINDOW, NO);

   SAL_set_application_icon((C8 *) IDI_ICON);

   hWnd = SAL_create_main_window();

   if (hWnd == NULL)
      {
      SAL_shutdown();
      return 0;
      }

   //
   // Register window procedure
   // 

   SAL_register_WNDPROC(WindowProc);

   //
   // Register exit handler and validate command line
   //

   atexit(AppExit);

   //
   // Set RES_X * RES_Y mode at desired pixel depth
   //

   set_display_mode();

   SAL_show_system_mouse();
      
   //
   // If no input file was specified, find all the PNP files 
   // in the directory, up to a maximum of MAX_SOURCES-1
   //

   n_data_sources = 0;

   if (!strlen(cmd_line))
      {
      //
      // No specific sources provided on command line; browse all available
      // files one at a time with +/- by default
      //

      add_files_to_list(NULL,"*.pnp",MAX_SOURCES-1);
      mode = M_OVERLAY;
      }
   else
      {
      //
      // Command tail was provided -- disable auto-save option if present
      //

      INI_auto_save[0] = 0;

      //
      // Parse command line into discrete strings, adding each string (or the
      // set of files it specifies) to the input-source list
      //

      C8 *src = cmd_line;

      S32 n = 0;

      C8 *srcptr = src;
      S32 qp = 0;

      while (*srcptr)
         {
         C8 *input = srcptr;

         while (1)
            {
            if (!*srcptr)
               {
               break;
               }

            if (*srcptr == '\"')
               {
               qp = !qp;
               }

            if ((*srcptr == ' ') && (!qp))
               {
               *srcptr++ = 0;
               break;
               }

            ++srcptr;
            }

         if (*input == 0)
            {
            //
            // No (more) strings to process, exit outer loop
            //

            break;
            }

         //
         // If string contains * or ? wildcards, generate file list based on
         // specification
         //

         if (strchr(input,'*') || strchr(input,'?'))
            {
            C8 *dirspec  = strrchr(input,'\\');
            C8 *filespec = input;

            if (dirspec != NULL)
               {
               *dirspec = 0;
               filespec = dirspec+1;
               dirspec  = input;
               }
            else
               {
               dirspec = "";
               }

            add_files_to_list(dirspec, filespec, MAX_SOURCES);
            continue;
            }

         //
         // Otherwise, string is a filename -- add it to the list
         //

         if ((n_data_sources + 1) <= MAX_SOURCES)
            {
            C8 buffer[MAX_PATH];

            if (input[0] == '\"')
               {
               strcpy(buffer,&input[1]);
               }
            else
               {
               strcpy(buffer,input);
               }

            C8 *q = strchr(buffer,'\"');

            if (q != NULL)
               {
               *q = 0;
               }

            if (data_source_list[n_data_sources].load(buffer, FALSE))
               {
               ++n_data_sources;
               }
            }
         }

      //
      // Overlay multiple explicitly-specified sources by default
      //

      if (n_data_sources == 1)
         {
         mode = M_BROWSE;
         }
      else
         {
         mode = M_OVERLAY;
         }
      }

   //
   // Generate initial plot
   //

   force_redraw = TRUE;
   invalid_trace_cache = TRUE;
   current_data_source = 0;

   // -------------------------------
   // Main plot loop
   // -------------------------------

   while (1)
      {
      //
      // Check Windows message queue
      //

      Sleep(10);

      SAL_serve_message_queue();

      //
      // Read mouse
      //

      last_left_button  = left_button;
      last_right_button = right_button;

      left_button  = (GetKeyState(VK_LBUTTON) & 0x8000);
      right_button = (GetKeyState(VK_RBUTTON) & 0x8000);

      POINT cur;
      GetCursorPos(&cur);

      SAL_WINAREA area;
      SAL_client_area(&area);

      mouse_x = cur.x - area.x;
      mouse_y = cur.y - area.y;

      //
      // Do we need to perform RMS integration during rendering?
      //
      
      RMS_needed = INI_show_legend_field[T_RESID_FM]    ||    
                   INI_show_legend_field[T_JITTER_DEGS] ||
                   INI_show_legend_field[T_JITTER_RADS] ||
                   INI_show_legend_field[T_JITTER_SECS] ||
                   INI_show_legend_field[T_CNR];  

      //
      // Update the plot 
      //

      if (force_redraw)
         {
         force_redraw = FALSE;
         render_source_list(TRUE);
         background_invalid = 1;
         }

      if (background_invalid)
         {
         refresh();
         background_invalid--;
         }

      //
      // Check for selection of spot and RMS columns generated during last plot
      //

      if (SAL_is_app_active() && 
         (left_button || right_button))
         {
         SINGLE selected_offset = -10000;

         if ((mouse_x >= spot_range_x0) && (mouse_x <= spot_range_x1) &&
             (mouse_y >= spot_range_y0) && (mouse_y <= spot_range_y1))
            {
            S32 c    = 0;
            S32 dist = LONG_MAX;

            for (S32 i=0; i < n_spot_cols; i++)
               {
               S32 d = abs(mouse_x - spot_col_x[i]);

               if (d < dist)
                  {
                  dist = d;
                  c    = i;
                  }
               }

            selected_offset = spot_col_freq[c]; 
            force_redraw = TRUE;
            }

         if (selected_offset >= 0)
            {
            if (INI_show_legend_field[T_SPOT_AMP] 
                &&
               (!(GetKeyState(VK_CONTROL) & 0x8000))
                &&
               (!right_button))
               {
               spot_offset_Hz = selected_offset;
               }

            if (RMS_needed
                &&
               (GetKeyState(VK_CONTROL) & 0x8000)
                &&
               (!(last_left_button || last_right_button)))
               {
               if (left_button)
                  L_offset_Hz = selected_offset;
               else
                  U_offset_Hz = selected_offset;

               if (L_offset_Hz > U_offset_Hz)   
                  {
                  SINGLE temp = L_offset_Hz;
                  L_offset_Hz = U_offset_Hz;
                  U_offset_Hz = temp;
                  }
               }
            }
         }

      //
      // Do auto save/print if requested
      //

      if (INI_auto_save[0])
         {
         CMD_save(INI_auto_save);
         break;
         }

      if (INI_auto_print_mode)
         {
         CMD_print_image();
         INI_auto_print_mode = 0;
         }

      //
      // Detect requested display resolution changes
      //

      if ((requested_RES_X != RES_X) ||
          (requested_RES_Y != RES_Y))
         {
         RES_X = requested_RES_X;
         RES_Y = requested_RES_Y;
         set_display_mode();
         force_redraw = TRUE;
         invalid_trace_cache = TRUE;
         }
      }

   return 0;
}

