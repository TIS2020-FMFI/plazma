//
// SSM.CPP: Rising-raster spectrum display for various Tektronix, HP, and Advantest 
// spectrum analyzers
//
// Author: John Miles, KE5FX (john@miles.io)
//

#define WINCON 1

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <zmouse.h>

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

#include "typedefs.h"
#include "ssmres.h"
#include "w32sal.h"
#include "winvfx.h"
#include "gnss.h"
#include "xy.h"
#include "waterfall.h"
#include "recorder.h"
#include "gpiblib.h"
#include "specan.h"
#include "gfxfile.cpp"

char szAppName[] = "KE5FX Spectrum Surveillance Monitor";

#define ACQ_FILENAME         "session.ssm"
#define DEFAULT_PAL_FILENAME "colors.bin"

#define VERSION        "1.51"

#define MAX_SIM_ROWS   4096       // Limit size of file created in test mode to about 10 MB
#define MAX_LIVE_SIZE  2000000000 // Limit size of acquired file to 2 GB+header
#define MAX_ID_LEN     40         // Maximum printable length of instrument ID header

#define DEFAULT_IMAGE_FILE_SUFFIX "BMP"

#define XY_BKGND_COLOR        RGB_TRIPLET(6,0,70)     
#define XY_TRACE_COLOR        RGB_TRIPLET(0,255,0)
#define XY_SCALE_COLOR        RGB_TRIPLET(255,255,255)
#define XY_MAJOR_COLOR        RGB_TRIPLET(16,110,230) 
#define XY_MINOR_COLOR        RGB_TRIPLET(16,110,230)  
#define XY_CURSOR_COLOR       RGB_TRIPLET(192,0,192)  
#define XY_RUN_COLOR          RGB_TRIPLET(255,255,0)
#define RISE_UNDERRANGE_COLOR RGB_TRIPLET(0,0,0)
#define RISE_OVERRANGE_COLOR  RGB_TRIPLET(0,255,0)
#define RISE_BLANK_COLOR      RGB_TRIPLET(0,0,0)

//
// Screen, pane, and data-array sizes
//

#define SCREEN_WIDTH   800
#define SCREEN_HEIGHT  600

#define XY_PANE_LEFT   64
#define XY_PANE_TOP    296
#define XY_PANE_WIDTH  640
#define XY_PANE_HEIGHT 260

#define WATERFALL_PANE_LEFT   64
#define WATERFALL_PANE_TOP    32
#define WATERFALL_PANE_WIDTH  640
#define WATERFALL_PANE_HEIGHT 260

//
// Window handle created by SAL
//

HWND hwnd;

//
// Menu handle
//

HMENU hmenu = NULL;

//
// Windows/panes
//

S32 screen_w;
S32 screen_h;

VFX_WINDOW *stage;
PANE       *stage_pane;
S32         stage_locked_for_writing = FALSE;

PANE       *xy_pane;
PANE       *xy_outer_pane;

PANE       *rise_pane;
PANE       *rise_outer_pane;

//
// Playback-control button list
//

#define    PLAYBACK_BUTTON_W 14
#define    PLAYBACK_BUTTON_H 13
           
#define    HOME_BUTTON_X 0
#define    HOME_BUTTON_Y 36

#define    FAST_REWIND_BUTTON_X 0
#define    FAST_REWIND_BUTTON_Y (HOME_BUTTON_Y + PLAYBACK_BUTTON_H + 2)
           
#define    REWIND_BUTTON_X 0
#define    REWIND_BUTTON_Y (FAST_REWIND_BUTTON_Y + PLAYBACK_BUTTON_H + 2)
           
#define    FORWARD_BUTTON_X 0
#define    FORWARD_BUTTON_Y (REWIND_BUTTON_Y + PLAYBACK_BUTTON_H + 2)
           
#define    FAST_FORWARD_BUTTON_X 0
#define    FAST_FORWARD_BUTTON_Y (FORWARD_BUTTON_Y + PLAYBACK_BUTTON_H + 2)

#define    END_BUTTON_X 0
#define    END_BUTTON_Y (FAST_FORWARD_BUTTON_Y + PLAYBACK_BUTTON_H + 2)

PANE_LIST *playback_buttons;          // Pane list used to represent clickable buttons
                                     
S32        BTN_FRWD;                  // Pane list index for fast-rewind button
S32        BTN_RWD;                   // Pane list index for rewind button
S32        BTN_FWD;                   // Pane list index for forward button
S32        BTN_FFWD;                  // Pane list index for fast-forward button
S32        BTN_HOME;                  // Pane list index for home button
S32        BTN_END;                   // Pane list index for end button

S32        selected_playback_button;  // Mouse cursor is over this button (-1 if none)

//
// Data recorder/playback object, and associated variables
//

RECORDER *recorder = NULL;

S32 playback_position = 0;   // last field drawn
S32 last_scroll_direction;   // +1=playback pos at top, -1=playback pos at bottom
S32 n_readable_records;      // # of records in currently-loaded file (playback mode only)
S32 n_visible_rows;          // # of data (not screen!) rows visible in rising-raster display
S32 wheel_scroll;            // Mouse wheel scrolling offset
bool paused;                 // TRUE if scroll lock has been turned on

//
// Data source attributes and state
//

SA_STATE *SOURCE_state;                  // Read/write structure for communication with SA access library
S32       SOURCE_address  = -1;
C8        SOURCE_ID_string[1024];
SINGLE    SOURCE_top_cursor    =  5.0F;
SINGLE    SOURCE_bottom_cursor = -105.0F;
S32       SOURCE_rows_acquired =  0;             

//
// GNSS state
//

GNSS_STATE *GNSS;

//
// Mouse state
//

S32 mouse_x;
S32 mouse_y;

S32 mouse_over_menu;

S32 mouse_left;
S32 last_mouse_left;
S32 app_active;

//
// Graph objects
//

XY        *xy   = NULL;   // X/Y graph object
WATERFALL *rise = NULL;   // Rising-raster graph object

//
// Array of data points passed into graph routines (max 64K)
//

SINGLE data[65536];

//
// Graph state
//

S32            n_data_points         =  640;
RESAMPLE_OP    resamp_op             =  RT_SPLINE;
RESAMPLE_OP    last_resamp_op        = (RESAMPLE_OP) -1;
S32            accum_op              =  0;
S32            last_accum_op         = -1;
XY_GRAPHSTYLE  style                 =  XY_CONNECTED_LINES;
S32            max_hold              =  0;
S32            lerp_colors           =  1;
S32            moving_avg            =  0;
S32            grat_x                =  1;
S32            grat_y                =  1;
S32            show_wifi_band        =  0;
S32            show_GPS_band         =  0;
S32            show_fps              =  1;
S32            show_annotation       =  0;
S32            last_window_mode      = -1;
time_t         acquisition_time      =  0;
S32            apply_thresholds      =  0;
S32            use_delta_marker      =  0;
S64            autorefresh_interval  =  0;

C8 annotation[128] = "Lorem ipsum quia dolor sit amet, consectetur adipisci velit. Vestibulum ut ante id";

enum OPERATION
{
   STOP,
   ACQUIRE,
   PLAYBACK
};

OPERATION operation;

//
// Program-relative paths
//

C8 colors_filename[MAX_PATH];
C8 colors_path[MAX_PATH];
C8 session_bin_path[MAX_PATH];
C8 currently_open_filename[MAX_PATH];

//
// Default waterfall scale
//

#define N_RISE_COLORS 22

SINGLE RISE_VALUES[N_RISE_COLORS] = 
   {
   -180.0F,
   -170.0F,
   -160.0F,
   -150.0F,
   -140.0F,
   -130.0F,
   -120.0F,
   -110.0F,
   -100.0F,
   -90.0F,
   -80.0F,
   -70.0F,
   -60.0F,
   -50.0F,
   -40.0F,
   -30.0F,
   -20.0F,
   -10.0F,
   0.0F,
   10.0F,
   20.0F,
   30.0F,
   };

VFX_RGB RISE_COLORS[N_RISE_COLORS];

//
// Misc. globals
//

HINSTANCE hInstance;

C8 DLG_box_caption[256];

HCURSOR waitcur;

U16 transparent_font_CLUT[256];

S32 done = 0;     // goes high when quit command received

S32 menu_mode = 0;

void select_operation(OPERATION new_operation, S32 GPIB_address=-1, S32 preserve_current_controls=FALSE);

//
// Menu command tokens
//

#define IDM_QUIT         100
#define IDM_4            102
#define IDM_16           103
#define IDM_32           104
#define IDM_64           105
#define IDM_128          106
#define IDM_320          107
#define IDM_640          108
#define IDM_CONTINUOUS   112
#define IDM_DOTS         113
#define IDM_BARS         114
#define IDM_MIN          115
#define IDM_MAX          116
#define IDM_AVG          117
#define IDM_PTSAMPLE     240
#define IDM_SPLINE       241
#define IDM_MAXHOLD      118
#define IDM_LERP         120
#define IDM_F5           127
#define IDM_AMIN         121
#define IDM_AMAX         122
#define IDM_AAVG         123
#define IDM_GRATX        124
#define IDM_GRATY        125
#define IDM_WIFI         126
#define IDM_GPS          128
#define IDM_SAVE         129
#define IDM_PLAYBACK     130
#define IDM_SAVE_PAL     131
#define IDM_LOAD_PAL     132
#define IDM_SCREENSHOT   133
#define IDM_PRINT        134
#define IDM_EXPORT_CSV   135
#define IDM_EXPORT_CSVLF 136
#define IDM_EXPORT_KML   137
#define IDM_LAUNCH_MAPS  138
#define IDM_SHOW_FPS     231
#define IDM_ANNOTATE     233
#define IDM_FS_TOGGLE    232
#define IDM_STOP_START   279
#define IDM_PAUSE_RESUME 280
#define IDM_HI_SPEED     330
#define IDM_856XA        331
#define IDM_358XA        332
#define IDM_8569B        333
#define IDM_3585         334
#define IDM_ADVANTEST    337
#define IDM_R3261        338
#define IDM_SCPI         340
#define IDM_HOUND        335
#define IDM_ABOUT        400
#define IDM_USER_GUIDE   401
#define IDM_THRESHOLDS   500
#define IDM_DELTA_MARKER 501
#define IDM_OFFSETS      502
#define IDM_MULTISEG     503

//
// WiFi band display parameters
//

CHANNEL CHANS_WiFi[3] = {{ 2412.0, 11.0, "Ch 1",  200, 0,   0   },
                         { 2437.0, 11.0, "Ch 6",  0,   200, 0   }, 
                         { 2462.0, 11.0, "Ch 11", 120, 160, 255 }};

BAND WiFi = { CHANS_WiFi,
              3,
              0 };

CHANNEL CHANS_GPS[1] = {{ 1575.42, 1.0, "L1 C/A", 200, 0, 0 }};         // primary occupied BW of L1 C/A
//                        { 1575.42, 10.0, "L1",    0,   200, 0 },      // total spectral footprint for L1
//                        { 1575.42, 16.0, "GPS",   0,   0, 200 }};     // entire GPS band allocation

BAND GPS = { CHANS_GPS,
             1,
             0 };

//****************************************************************************
//*                                                                          
//* Color picker routines                                                     
//*                                                                          
//****************************************************************************

HANDLE CCthread = INVALID_HANDLE_VALUE;

#define CC_INIT           0
#define CC_ACTIVE         1
#define CC_EXIT_REQUESTED 2
#define CC_EXIT_OK        3
#define CC_EXIT_ERR      -1

COLORREF custom_colors[16] = 
{
   0x000000,      // default palette
   0xaa0000,
   0x00aa00,
   0xaaaa00,
   0x0000aa,
   0xaa00aa,
   0x0055aa,
   0xaaaaaa,
   0x555555,
   0xff5555,
   0x55ff55,
   0xffff55,
   0x5555ff,
   0xff55ff,
   0x55ffff,
   0xffffff
};

volatile S32    picker_color;
volatile bool picker_result = FALSE;
volatile bool picker_active = FALSE;

//
// Round float to nearest int
//

static S32 round_to_nearest(SINGLE f)
{
   if (f >= 0.0F) 
      f += 0.5F;
   else
      f -= 0.5F;

   return S32(f);
}

static S32 round_to_nearest(DOUBLE f)
{
   if (f >= 0.0) 
      f += 0.5;
   else
      f -= 0.5;

   return S32(f);
}

/*********************************************************************/
//
// Error diagnostics
//
/*********************************************************************/

void show_last_error(C8 *caption = NULL, ...)
{
   LPVOID lpMsgBuf = "Hosed";       // Default message for Win9x (FormatMessage() is NT-only)

   FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 
                 NULL, 
                 GetLastError(), 
                 0,
        (LPTSTR) &lpMsgBuf, 
                 0, 
                 NULL);

   if (caption == NULL)
      {
      caption = "Error";
      }
   else
      {
      static char work_string[4096];

      va_list ap;

      va_start(ap, 
               caption);

      vsprintf(work_string, 
               caption, 
               ap);

      va_end  (ap);

      caption = work_string;
      }

   MessageBox(NULL, 
    (LPCTSTR) lpMsgBuf, 
              caption,
              MB_OK | MB_ICONINFORMATION); 

   if (strcmp((C8 *) lpMsgBuf,"Hosed"))
      {
      LocalFree(lpMsgBuf);
      }
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
         case '€': d = 'C'; break;
         case '': d = 'u'; break;
         case '‚': d = 'e'; break;
         case 'ƒ': d = 'a'; break;
         case '„': d = 'a'; break;
         case '…': d = 'a'; break;
         case '†': d = 'a'; break;
         case '‡': d = 'c'; break;
         case 'ˆ': d = 'e'; break;
         case '‰': d = 'e'; break;
         case 'Š': d = 'e'; break;
         case '‹': d = 'i'; break;
         case 'Œ': d = 'i'; break;
         case '': d = 'i'; break;
         case 'Ž': d = 'A'; break;
         case '': d = 'A'; break;
         case '': d = 'E'; break;
         case '“': d = 'o'; break;
         case '”': d = 'o'; break;
         case '•': d = 'o'; break;
         case '–': d = 'u'; break;
         case '—': d = 'u'; break;
         case '˜': d = 'y'; break;
         case '™': d = 'o'; break;
         case 'š': d = 'u'; break;
         case '›': d = 'c'; break;
         case 'œ': d = '$'; break;
         case '': d = 'Y'; break;
         case 'Ÿ': d = 'f'; break;
         case ' ': d = 'a'; break;
         case '¡': d = 'i'; break;
         case '¢': d = 'o'; break;
         case '£': d = 'u'; break;
         case '¤': d = 'n'; break;
         case '¥': d = 'n'; break;    
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
// Ensure string is valid by replacing any negative ASCII
// characters with spaces
//
//****************************************************************************

static C8 *sanitize_VFX(C8 *input)
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
      C8 ch = input[i];

      if (((U8) ch) >= 0x80) ch = ' ';

      *ptr++ = ch;
      }

   *ptr++ = 0;

   return output;
}

/*********************************************************************/
//
// Color chooser hook procedure
//
/*********************************************************************/

UINT CALLBACK CCHookProc(HWND hdlg,      // handle to the dialog box window //)
                         UINT uiMsg,     // message identifier
                         WPARAM wParam,  // message parameter
                         LPARAM lParam)  // message parameter
{
   if (uiMsg == WM_INITDIALOG)
      {
      RECT us;
      GetWindowRect(hdlg, &us);

      RECT outer;
      GetWindowRect(GetParent(hdlg), &outer);

      S32 w = us.right - us.left;
      S32 h = us.bottom - us.top;

      S32 dx = (outer.right - outer.left) - w;
      S32 dy = (outer.bottom - outer.top) - h;

      S32 x = outer.left + (dx/2);
      S32 y = outer.top  + (dy/2);

      MoveWindow(hdlg, 
                 x,
                 y,
                 w,
                 h,
                 TRUE);
      }

   if (uiMsg != WM_MOUSEMOVE)
      {
      return 0;
      }

   //
   // On WM_MOUSEMOVE, read color from palette sample patch
   //

   HDC hdc = GetDC(hdlg);

   GdiFlush();
   picker_color = (S32) GetPixel(hdc, 256, 224);

   ReleaseDC(hdlg, hdc);

   return 0;
}

UINT WINAPI CC_thread_proc(LPVOID pParam)
{
   CHOOSECOLOR cc;

   cc.lStructSize       = sizeof(cc);
   cc.hwndOwner         = hwnd;
   cc.hInstance         = NULL;
   cc.rgbResult         = RGB(((picker_color & 0xff0000) >> 16),
                              ((picker_color & 0x00ff00) >> 8),
                               (picker_color & 0xff));
   cc.lpCustColors      = custom_colors;
   cc.Flags             = CC_ANYCOLOR   | 
                          CC_FULLOPEN   | 
                          CC_RGBINIT    |
                          CC_ENABLEHOOK | 
                          CC_SOLIDCOLOR;
   cc.lCustData         = NULL;
   cc.lpfnHook          = CCHookProc;
   cc.lpTemplateName    = NULL;

   picker_result = ChooseColor(&cc) != 0;

   picker_color  = cc.rgbResult;
   picker_active = FALSE;

   return TRUE;
}

/*********************************************************************/
//
// launch_color_picker()
//
/*********************************************************************/

bool launch_color_picker(VFX_RGB *default_color)
{
   picker_active = TRUE;
   picker_result = FALSE;
   picker_color  = (S32(default_color->r) << 16) | (S32(default_color->g) << 8) | S32(default_color->b);

#if 0
   if (CCthread != INVALID_HANDLE_VALUE)
      {
      picker_active = FALSE;
      return FALSE; 
      }

   unsigned int stupId;

   CCthread = (HANDLE) _beginthreadex(NULL,                   
                                      0,                              
                                      CC_thread_proc,
                                      NULL,
                                      0,                              
                                     &stupId);
   return FALSE;
#else
   CC_thread_proc(0);

   picker_active = FALSE;
   return picker_result;
#endif
}

#if 0
/*********************************************************************/
//
// close_color_picker()
//
/*********************************************************************/

void close_color_picker(void)
{
   if (CCthread == INVALID_HANDLE_VALUE)
      {
      return;
      }

   WaitForSingleObject(CCthread,
                       3000);

   CloseHandle(CCthread);
   CCthread = INVALID_HANDLE_VALUE;

   picker_active = FALSE;
}

/*********************************************************************/
//
// color_picker_enabled()
//
/*********************************************************************/

bool color_picker_active(void)
{
   return picker_active;
}

/*********************************************************************/
//
// color_picker_result()
//
/*********************************************************************/

bool color_picker_result(void)
{
   if (picker_active)
      {
      return FALSE;
      }

   return picker_result;   
}
#endif

/*********************************************************************/
//
// picked_color()
//
/*********************************************************************/

VFX_RGB *picked_color(void)
{
   static VFX_RGB result;

   result.r = S8((picker_color >> 0)  & 0xff);
   result.g = S8((picker_color >> 8)  & 0xff);
   result.b = S8((picker_color >> 16) & 0xff);

   return &result;
}

/*********************************************************************/
//
// Return TRUE if instrument supports multisweep mode
//
/*********************************************************************/

bool multisweep(DOUBLE *mn_Hz, DOUBLE *mx_Hz)
{
   if (SOURCE_state->CTRL_freq_specified)
      {
      DOUBLE mn = SOURCE_state->CTRL_start_Hz;
      DOUBLE mx = SOURCE_state->CTRL_stop_Hz;

      if (mx > mn)
         {
         *mn_Hz = mn;
         *mx_Hz = mx;
         }
      else
         {
         *mn_Hz = mx;
         *mx_Hz = mn;
         }
      }
   else
      {
      *mn_Hz = SOURCE_state->min_Hz;
      *mx_Hz = SOURCE_state->max_Hz;
      }

   return (SOURCE_state->type == SATYPE_E4406A);
}

/*********************************************************************/
//
// OffsetDlgProc()
//
/*********************************************************************/

BOOL CALLBACK OffsetDlgProc (HWND   hDlg,  
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

         _snprintf(text,sizeof(text),"%.03lf",SOURCE_state->A_offset);
         SetWindowText(GetDlgItem(hDlg,IDC_A_OFFSET), text);

         _snprintf(text,sizeof(text),"%.03lf",SOURCE_state->F_offset);
         SetWindowText(GetDlgItem(hDlg,IDC_F_OFFSET), text);

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
               case IDC_F_OFFSET:
                  {
                  SetDlgItemText(hDlg, IDC_HELPGROUP, "Frequency offset");
                  SetDlgItemText(hDlg, IDC_HELPBOX,   "\
This field allows you to specify the frequency offset associated with an external converter.\n\n\
You can enter a positive or negative value in Hz to be added to all recorded frequency values.  For example, specify 1500000000 or 1.5E9 for a downconverter with a 1.5-GHz LO.");
                  break;
                  }

               case IDC_A_OFFSET:
                  {
                  SetDlgItemText(hDlg, IDC_HELPGROUP, "Amplitude offset");
                  SetDlgItemText(hDlg, IDC_HELPBOX,   "\
This field allows you to specify the gain or loss associated with any external components in the signal path.\n\n\
Enter a positive or negative value in dB to be added to all recorded amplitude values.  For example, specify -20 if a 20-dB preamplifier is in use, or 7 to compensate for 7 dB of loss in an external mixer.");
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
               // Copy values from control windows back to globals 
               // prior to returning
               //

               C8 text[256];

               memset(text, 0, sizeof(text));      
               GetWindowText(GetDlgItem(hDlg, IDC_A_OFFSET), text, sizeof(text)-1);
               sscanf(text,"%lf",&SOURCE_state->A_offset);

               memset(text, 0, sizeof(text));      
               GetWindowText(GetDlgItem(hDlg, IDC_F_OFFSET), text, sizeof(text)-1);
               sscanf(text,"%lf",&SOURCE_state->F_offset);

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
// SweepDlgProc() 
//
/*********************************************************************/

BOOL CALLBACK SweepDlgProc (HWND   hDlg,  
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

         _snprintf(text,sizeof(text),"%.06lf",SOURCE_state->CTRL_start_Hz / 1E6);
         SetWindowText(GetDlgItem(hDlg, IDC_START_MHZ), text);

         _snprintf(text,sizeof(text),"%.06lf",SOURCE_state->CTRL_stop_Hz / 1E6);
         SetWindowText(GetDlgItem(hDlg, IDC_STOP_MHZ), text);

         if (SOURCE_state->CTRL_freq_specified)
            Button_SetCheck(GetDlgItem(hDlg, IDC_USE_RANGE), 1);
         else
            Button_SetCheck(GetDlgItem(hDlg, IDC_USE_RANGE), 0);

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
               case IDC_START_MHZ:
               case IDC_STOP_MHZ:
                  {
                  SetDlgItemText(hDlg, IDC_HELPGROUP, "Start/stop frequency");
                  SetDlgItemText(hDlg, IDC_HELPBOX,   "\
These fields allow you to specify an extended sweep range for spectrum analyzers with frequency span limitations.\n\nCurrently \
this feature supports the Agilent E4406A only.  Frequencies ranging from DC-4 GHz may be specified.");
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
               // Copy values from control windows back to globals 
               // prior to returning
               //

               C8 text[256];

               memset(text, 0, sizeof(text));      
               GetWindowText(GetDlgItem(hDlg, IDC_START_MHZ), text, sizeof(text)-1);
               sscanf(text,"%lf",&SOURCE_state->CTRL_start_Hz);
               SOURCE_state->CTRL_start_Hz *= 1E6;

               memset(text, 0, sizeof(text));      
               GetWindowText(GetDlgItem(hDlg, IDC_STOP_MHZ), text, sizeof(text)-1);
               sscanf(text,"%lf",&SOURCE_state->CTRL_stop_Hz);
               SOURCE_state->CTRL_stop_Hz *= 1E6;

               if (Button_GetCheck(GetDlgItem(hDlg, IDC_USE_RANGE)))
                  SOURCE_state->CTRL_freq_specified = TRUE;
               else
                  SOURCE_state->CTRL_freq_specified = FALSE;

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
// Launch HTML page from program directory
//
//****************************************************************************

void WINAPI launch_page(C8 *filename)
{
   C8 path[MAX_PATH];

   GetModuleFileName(NULL, path, sizeof(path)-1);

   _strlwr(path);

   C8 *exe = strstr(path,"ssm.exe");

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

//****************************************************************************
//
// Launch Google Maps
//
//****************************************************************************

void WINAPI launch_maps(DOUBLE latitude, DOUBLE longitude)
{
   if ((latitude  == INVALID_LATLONGALT) || 
       (longitude == INVALID_LATLONGALT))
      {
      latitude  = 37.235;
      longitude = -115.811111;
      }

   C8 path[MAX_PATH];
   sprintf(path,"http://maps.google.com?q=%lf,%lf", latitude, longitude);

   ShellExecute(NULL,    // hwnd
               "open",   // verb
                path,    // filename
                NULL,    // parms
                NULL,    // dir
                SW_SHOWNORMAL);
}

//*************************************************************
//
// Return numeric value of string, with optional
// location of first non-numeric character
//
/*************************************************************/

S64 ascnum(C8 *string, U32 base, C8 **end)
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

//****************************************************************************
//
// Set waterfall-display palette
//
//****************************************************************************

void set_waterfall_palette(void)
{
   if (rise != NULL)
      {
      rise->set_graph_colors(RISE_COLORS,
                             RISE_UNDERRANGE_COLOR,
                             RISE_OVERRANGE_COLOR,
                             RISE_BLANK_COLOR);
      }
}

//****************************************************************************
//
// Load default palette
//
//****************************************************************************

bool load_default_palette(void)
{
   bool colors_OK = FALSE;

   FILE *colors = fopen(colors_path,"rb");

   if (colors != NULL)
      {
      if (fread(RISE_COLORS, sizeof(RISE_COLORS), 1, colors) == 1)
         {
         colors_OK = TRUE;
         }

      fclose(colors);
      }

   if (!colors_OK)
      {
      S32 dc_di = (255 - 40) / N_RISE_COLORS;
      S32 c     = 40;

      for (S32 i=0; i < N_RISE_COLORS; i++)
         {
         RISE_COLORS[i].r = (U8) c;
         RISE_COLORS[i].g = (U8) c;
         RISE_COLORS[i].b = (U8) c;

         c += dc_di;
         }
      }

   set_waterfall_palette();

   if (operation == PLAYBACK)
      {
      select_operation(PLAYBACK, -1, TRUE);
      }

   return colors_OK;
}

//
// Brian K's default filename generator
//
// FileName contains the obvious, FileName is global in this example
//
 
C8 FileName[MAX_PATH] = "";

void GetFileName(C8 *suffix)
{
   char buffer1[48];
   struct tm *newtime;
   time_t ltime;
   int tmp;

   time(&ltime);
   newtime = gmtime(&ltime);
   tmp = newtime->tm_year;
   if(tmp<100) {
        sprintf(FileName,"%2d%2d",19,tmp);
   }
   else {
        sprintf(FileName,"%2d%2d",20,tmp-100);
        if((tmp - 100) < 10)
             FileName[2] = '0';
   }
   tmp = (newtime->tm_mon) + 1;  //  Jan. = 0
   sprintf(buffer1,"%2d",tmp);
   if(tmp<10)
        buffer1[0] = '0';
   strcat(FileName,buffer1);

   tmp = newtime->tm_mday;
   sprintf(buffer1,"%2d",tmp);
   if(tmp<10)
        buffer1[0] = '0';
   strcat(FileName,buffer1);

   tmp = newtime->tm_hour;
   sprintf(buffer1,"%2d",tmp);
   if(tmp<10)
        buffer1[0] = '0';
   strcat(FileName,buffer1);

   tmp = newtime->tm_min;
   sprintf(buffer1,"%2d",tmp);
   if(tmp<10)
        buffer1[0] = '0';
   strcat(FileName,buffer1);

   tmp = newtime->tm_sec;
   sprintf(buffer1,"%2d.%s",tmp,suffix);
   if(tmp<10)
        buffer1[0] = '0';
   strcat(FileName,buffer1);
   strcpy(FileName,sanitize_path(FileName,FALSE,TRUE));
}

//****************************************************************************
//
// Center with respect to another window (from KB140722)
//
// Specifying NULL for hwndParent centers hwndChild relative to the
// screen
//
//****************************************************************************

bool CenterWindow(HWND hwndChild, HWND hwndParent)
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
                       SWP_NOSIZE | SWP_NOZORDER) != 0;
}

//****************************************************************************
//*                                                                          *
//*  Utility hook procedure to center save-filename dialogs                  *
//*                                                                          *
//****************************************************************************

UINT CALLBACK SFNHookProc(HWND hdlg,      // handle to the dialog box window //)
                          UINT uiMsg,     // message identifier
                          WPARAM wParam,  // message parameter
                          LPARAM lParam)  // message parameter
{
   if (uiMsg == WM_NOTIFY)
      {
      LPNMHDR pnmh = (LPNMHDR) lParam;

      if (pnmh->code == CDN_INITDONE)
         {
         CenterWindow(GetParent(hdlg), hwnd);
         }
      }

   return 0;
}

//****************************************************************************
//*                                                                          *
//*  Utility hook procedure to center filename dialogs                       *
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

      if (pnmh->code == CDN_INITDONE)
         {
         CenterWindow(GetParent(hdlg), hwnd);
         }
      }

   return 0;
}

//****************************************************************************
//*                                                                          *
//* Get filename of image to open                                            *
//*                                                                          *
//****************************************************************************

S32 get_open_filename(C8 *string)
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
   fn.hwndOwner         = hwnd;
   fn.hInstance         = NULL;
   fn.lpstrFilter       = "SSM recording files (*.SSM)\0*.SSM\0Pre-1.10 SSM recordings (*.RRT)\0*.RRT\0All files (*.*)\0*.*\0";
   fn.lpstrCustomFilter = NULL;
   fn.nMaxCustFilter    = 0;
   fn.nFilterIndex      = 1;
   fn.lpstrFile         = fn_buff;
   fn.nMaxFile          = sizeof(fn_buff);
   fn.lpstrFileTitle    = NULL;
   fn.nMaxFileTitle     = 0;
   fn.lpstrInitialDir   = NULL;
   fn.lpstrTitle        = "Open SSM Recording File";
   fn.Flags             = OFN_EXPLORER |
                          OFN_FILEMUSTEXIST |
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

   if (!GetOpenFileName(&fn))
      {
      return 0;
      }

   strcpy(string, fn_buff);
   return 1;
}

//****************************************************************************
//*                                                                          *
//* Get filename of image to save                                            *
//*                                                                          *
//****************************************************************************

S32 get_save_filename(C8 *string)
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
   fn.hwndOwner         = hwnd;
   fn.hInstance         = NULL;
   fn.lpstrFilter       = "SSM recording files (*.SSM)\0*.SSM\0All files (*.*)\0*.*\0";
   fn.lpstrCustomFilter = NULL;
   fn.nMaxCustFilter    = 0;
   fn.nFilterIndex      = 1;
   fn.lpstrFile         = fn_buff;
   fn.nMaxFile          = sizeof(fn_buff);
   fn.lpstrFileTitle    = NULL;
   fn.nMaxFileTitle     = 0;
   fn.lpstrInitialDir   = NULL;
   fn.lpstrTitle        = "Save SSM Recording File";
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
   fn.lpstrDefExt       = "SSM";
   fn.lCustData         = NULL;
   fn.lpfnHook          = SFNHookProc;
   fn.lpTemplateName    = NULL;

   if (!GetSaveFileName(&fn))
      {
      return 0;
      }

   strcpy(string, fn_buff);
   return 1;
}

//****************************************************************************
//*                                                                          *
//* Get filename for .CSV or .KML export                                     *
//*                                                                          *
//****************************************************************************

S32 get_CSV_filename(C8 *string)
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
   fn.hwndOwner         = hwnd;
   fn.hInstance         = NULL;
   fn.lpstrFilter       = "Comma-separated value files (*.CSV)\0*.CSV\0All files (*.*)\0*.*\0";
   fn.lpstrCustomFilter = NULL;
   fn.nMaxCustFilter    = 0;
   fn.nFilterIndex      = 1;
   fn.lpstrFile         = fn_buff;
   fn.nMaxFile          = sizeof(fn_buff);
   fn.lpstrFileTitle    = NULL;
   fn.nMaxFileTitle     = 0;
   fn.lpstrInitialDir   = NULL;
   fn.lpstrTitle        = "Export to CSV File";
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
   fn.lpstrDefExt       = "CSV";
   fn.lCustData         = NULL;
   fn.lpfnHook          = SFNHookProc;
   fn.lpTemplateName    = NULL;

   if (!GetSaveFileName(&fn))
      {
      return 0;
      }

   strcpy(string, fn_buff);
   return 1;
}

S32 get_KML_filename(C8 *string)
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
   fn.hwndOwner         = hwnd;
   fn.hInstance         = NULL;
   fn.lpstrFilter       = "Keyhole Markup Language files (*.KML)\0*.KML\0All files (*.*)\0*.*\0";
   fn.lpstrCustomFilter = NULL;
   fn.nMaxCustFilter    = 0;
   fn.nFilterIndex      = 1;
   fn.lpstrFile         = fn_buff;
   fn.nMaxFile          = sizeof(fn_buff);
   fn.lpstrFileTitle    = NULL;
   fn.nMaxFileTitle     = 0;
   fn.lpstrInitialDir   = NULL;
   fn.lpstrTitle        = "Export to KML File";
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
   fn.lpstrDefExt       = "KML";
   fn.lCustData         = NULL;
   fn.lpfnHook          = SFNHookProc;
   fn.lpTemplateName    = NULL;

   if (!GetSaveFileName(&fn))
      {
      return 0;
      }

   strcpy(string, fn_buff);
   return 1;
}

//****************************************************************************
//*                                                                          *
//* Get filename for screenshot                                              *
//*                                                                          *
//****************************************************************************

bool get_screenshot_filename(C8 *string)
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
   fn.hwndOwner         = hwnd;
   fn.hInstance         = NULL;

   fn.lpstrFilter       = "\
Image files (*.BMP)\0*.BMP\0\
Image files (*.GIF)\0*.GIF\0\
Image files (*.TGA)\0*.TGA\0\
Image files (*.PCX)\0*.PCX\0\
";

   fn.lpstrCustomFilter = NULL;
   fn.nMaxCustFilter    = 0;
   fn.nFilterIndex      = 1;
   fn.lpstrFile         = fn_buff;
   fn.nMaxFile          = sizeof(fn_buff);
   fn.lpstrFileTitle    = NULL;
   fn.nMaxFileTitle     = 0;
   fn.lpstrInitialDir   = NULL;

   fn.lpstrTitle        = "Save Screen Image";
   fn.Flags             = OFN_EXPLORER |
                          OFN_LONGNAMES |
                          OFN_ENABLESIZING |
                          OFN_NOCHANGEDIR |
                          OFN_ENABLEHOOK |
                          OFN_PATHMUSTEXIST |
                          OFN_HIDEREADONLY;
   fn.nFileOffset       = 0;
   fn.nFileExtension    = 0;
   fn.lpstrDefExt       = DEFAULT_IMAGE_FILE_SUFFIX;
   fn.lCustData         = NULL;
   fn.lpfnHook          = SFNHookProc;
   fn.lpTemplateName    = NULL;

   if (!GetOpenFileName(&fn))
      {
      return FALSE;
      }
   
   strcpy(string, fn_buff);

   //
   // Force string to end in a valid suffix
   //

   C8 *suffixes[] = 
      {
      ".gif",
      ".tga",
      ".bmp",
      ".pcx",
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

// Return rectangle containing client-area boundaries in screenspace

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

bool PrintWindowToDC(HWND hWnd)
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

bool PrintBackBufferToDC(void)
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
//
// Return width of printed string in pixels
// 
//****************************************************************************

static S32 string_width(C8       *string, //)
                        VFX_FONT *font)
{
   S32 w = 0;

   for (U32 i=0; i < strlen(string); i++)
      {
      w += VFX_character_width(font,
                               string[i]);
      }

   return w;
}

//****************************************************************************
//
// Break GPIB connection with source instrument
//
//****************************************************************************

void SOURCE_disconnect(bool final_exit = FALSE)
{
   SA_disconnect(final_exit);
}
                            
//****************************************************************************
//
// Establish GPIB connection with source instrument
//
//****************************************************************************

bool SOURCE_connect(void)
{
   SOURCE_rows_acquired = 0;

   bool result = SA_connect(SOURCE_address);

   if (result == NULL)
      {
      strcpy(SOURCE_ID_string,"(Offline)");
      }
   else
      {
      strcpy(SOURCE_ID_string, SOURCE_state->ID_string);

      S32 len = strlen(SOURCE_ID_string);

      if (len > MAX_ID_LEN)
         {
         strcpy(&SOURCE_ID_string[MAX_ID_LEN-4],"...");
         }
      }

   return result;
}

//****************************************************************************
//
// Fetch data from receiver and return time of acquisition (as long integer) 
//
//****************************************************************************

time_t SOURCE_fetch_data(SINGLE *dest)
{
   static time_t acq_time;
   time(&acq_time);

   static DOUBLE dest64[65536];

   //
   // Multisweep range?
   // 

   DOUBLE mn_Hz = 0.0;
   DOUBLE mx_Hz = 0.0;
   bool mswp = multisweep(&mn_Hz, &mx_Hz);

   if (!mswp)
      {
      //
      // No, fetch single trace
      // 

      SA_fetch_trace();

      if (SOURCE_state->n_trace_points == 0)
         {
         select_operation(STOP);
         return acq_time;
         }

      //
      // Copy trace to dest array, inverse-transforming to n_data_points points
      //

      SA_resample_data(SOURCE_state->dBm_values, SOURCE_state->n_trace_points,
                       dest64, n_data_points,
                       resamp_op);

      for (S32 i=0; i < n_data_points; i++)
         {
         dest[i] = (SINGLE) dest64[i];
         }
      }
   else
      {
      //
      // Yes, sweep user-specified range
      //

      static DOUBLE destcnt[65536];
      static DOUBLE src64 [65536];
      S32 n_src_points = 0;

      switch (resamp_op)
         {
         default:
            {
            memset(destcnt, 0, n_data_points * sizeof(destcnt[0]));
            memset(dest64,  0, n_data_points * sizeof(dest64[0]));
            break;
            }

         case RT_MAX:
            {
            for (S32 i=0; i < n_data_points; i++)
               {
               dest64[i] = -999.0;
               }
            break;
            }

         case RT_MIN:
            {
            for (S32 i=0; i < n_data_points; i++)
               {
               dest64[i] = 999.0;
               }
            break;
            }
         }

      DOUBLE dest_Hz = mn_Hz;
      DOUBLE dd_Hz = (mx_Hz - mn_Hz) / (n_data_points - 1);

      const DOUBLE span_size_Hz = 10E6;               // E4406A max span

      DOUBLE start_Hz = mn_Hz;
      S32 last_d = 0;

      while (start_Hz < mx_Hz)
         {
         DOUBLE stop_Hz = min(mx_Hz, start_Hz + span_size_Hz);

         DOUBLE span_Hz = min(span_size_Hz, stop_Hz - start_Hz);
         DOUBLE CF_Hz   = start_Hz + (span_Hz / 2.0);

         SA_query(":SENS:FREQ:CENT %lf HZ;*OPC?\n", CF_Hz); 
         SA_query(":SENS:SPEC:FREQ:SPAN %lf HZ;*OPC?\n", span_Hz);

         SOURCE_state->n_trace_points = 0;            // (# of points can vary between spans, so don't flag any errors)
         SA_fetch_trace();

         if (SOURCE_state->n_trace_points == 0)
            {
            select_operation(STOP);
            return acq_time;
            }

         S32 n_src_points = SOURCE_state->n_trace_points;

         DOUBLE src_Hz = start_Hz;
         DOUBLE ds_Hz = span_Hz / (n_src_points - 1);

         switch (resamp_op)
            {
            default:
               {
               for (S32 i=0; i < n_src_points; i++, src_Hz += ds_Hz)
                  {
                  DOUBLE v = pow(10.0, SOURCE_state->dBm_values[i] / 10.0);

                  S32 next_d = min(n_data_points-1, (S32) (((src_Hz - mn_Hz) / dd_Hz) + 0.5));

                  for (S32 d=last_d; d <= next_d; d++)
                     {
                     dest64[d] += v;
                     destcnt[d]++;
                     }

                  last_d = next_d;
                  }

               break;
               }

            case RT_MIN:
               {
               for (S32 i=0; i < n_src_points; i++, src_Hz += ds_Hz)
                  {
                  DOUBLE v = SOURCE_state->dBm_values[i];

                  S32 next_d = min(n_data_points-1, (S32) (((src_Hz - mn_Hz) / dd_Hz) + 0.5));

                  for (S32 d=last_d; d <= next_d; d++)
                     {
                     dest64[d] = min(dest64[d], v);
                     }

                  last_d = next_d;
                  }

               break;
               }

            case RT_MAX:
               {
               for (S32 i=0; i < n_src_points; i++, src_Hz += ds_Hz)
                  {
                  DOUBLE v = SOURCE_state->dBm_values[i];

                  S32 next_d = min(n_data_points-1, (S32) (((src_Hz - mn_Hz) / dd_Hz) + 0.5));

                  for (S32 d=last_d; d <= next_d; d++)
                     {
                     dest64[d] = max(dest64[d], v);
                     }

                  last_d = next_d;
                  }

               break;
               }

            case RT_POINT:
               {
               for (S32 i=0; i < n_src_points; i++, src_Hz += ds_Hz)
                  {
                  DOUBLE v = SOURCE_state->dBm_values[i];

                  S32 next_d = min(n_data_points-1, (S32) (((src_Hz - mn_Hz) / dd_Hz) + 0.5));

                  for (S32 d=last_d; d <= next_d; d++)
                     {
                     dest64[d] = v;
                     }

                  last_d = next_d;
                  }

               break;
               }
            }

         start_Hz = stop_Hz;
         }

      //
      // Copy trace to dest array, inverse-transforming to n_data_points points
      //

      if ((resamp_op == RT_MIN) ||
          (resamp_op == RT_MAX) || 
          (resamp_op == RT_POINT))
               
         {
         for (S32 i=0; i < n_data_points; i++)
            {
            dest[i] = (SINGLE) dest64[i];
            }
         }
      else
         {
         for (S32 i=0; i < n_data_points; i++)
            {
            dest[i] = (SINGLE) (10.0 * log10(dest64[i] / max(destcnt[i],1)));
            }
         }
      }


   //
   // Keep track of # of rows returned
   //

   ++SOURCE_rows_acquired;

   //
   // Return acquisition time for this data set
   //

   return acq_time;
}

//****************************************************************************
//*                                                                          *
//*  Configure graph attributes                                              *
//*                                                                          *
//****************************************************************************

void graph_setup(bool force_recreate=FALSE)
{
   //
   // Re-create graph objects
   //

   if (force_recreate)
      {
      //
      // Clear entire staging pane, wiping all graphs and scales
      //

      if (!stage_locked_for_writing)
         {
         VFX_lock_window_surface(stage, VFX_BACK_SURFACE);
         }

      VFX_pane_wipe(stage_pane, RGB_TRIPLET(0,0,0));

      if (!stage_locked_for_writing)
         {
         VFX_unlock_window_surface(stage, FALSE);
         }

      //
      // Calculate dBm values (including overrange/underrange margin) at 
      // top and bottom of graph area
      //
      // If X/Y object already exists and the source address has not changed,
      // preserve the amplitude cursor values (if they're still onscreen)
      // 

      DOUBLE graph_top_dBm    = SOURCE_state->max_dBm + SOURCE_state->dB_division; 
      DOUBLE graph_bottom_dBm = SOURCE_state->min_dBm - SOURCE_state->dB_division;

      static S32 last_source_address = -1;

      if ((xy != NULL) && (SOURCE_address == last_source_address))
         {
         if ((xy->top_cursor_val()    >= graph_bottom_dBm) &&
             (xy->top_cursor_val()    <= graph_top_dBm   ) &&
             (xy->bottom_cursor_val() >= graph_bottom_dBm) &&
             (xy->bottom_cursor_val() <= graph_top_dBm   ))
            {
            SOURCE_top_cursor    = xy->top_cursor_val();
            SOURCE_bottom_cursor = xy->bottom_cursor_val();
            }
         }

      last_source_address = SOURCE_address;

      last_accum_op = -1;
      last_resamp_op = (RESAMPLE_OP) -1;

      //
      // Destroy old X/Y and rising-raster objects, if they exist
      //

      if (xy != NULL)
         {
         delete xy;
         xy = NULL;
         }

      if (rise != NULL)
         {
         delete rise;
         rise = NULL;
         }

      //
      // Initialize X/Y graph
      //

      xy = new XY(xy_pane, xy_outer_pane);

      //
      // Disable zoom mode
      //

      xy->set_zoom_mode(FALSE, FALSE);

      //
      // Configure X/Y graph range based on source
      //
      // Display precision hardwired to 1 kHz for now
      //

      DOUBLE mn_Hz = 0.0;
      DOUBLE mx_Hz = 0.0;
      multisweep(&mn_Hz, &mx_Hz);

      DOUBLE max_MHz = mx_Hz / 1000000.0;
      DOUBLE min_MHz = mn_Hz / 1000000.0;

      DOUBLE span = max_MHz - min_MHz;

      xy->set_X_range(min_MHz,
                      max_MHz,
                     "%.3lf",
                      span / 4.0,
                      span / 40.0);

      xy->set_X_cursors(min_MHz + (span / 10.0),
                        max_MHz - (span / 10.0));

      xy->set_Y_range((SINGLE) graph_top_dBm, 
                      (SINGLE) graph_bottom_dBm,
                               "%.1lf",
                      (SINGLE) SOURCE_state->dB_division * 2.0F,
                      (SINGLE) SOURCE_state->dB_division);

      xy->set_Y_cursors(SOURCE_top_cursor,
                        SOURCE_bottom_cursor);

      //
      // Initialize rising-raster graph
      //

      rise = new WATERFALL(rise_pane, rise_outer_pane);

      //
      // Tell X/Y graph to expect n_data_points input points
      //
      // This call will fail if the width of the inner graph pane cannot be
      // divided evenly by n_data_points, or vice versa
      //
      // Calling set_input_width() blows away the contents of the persistent
      // display, so we only want to call it when the display width actually
      // changes
      //
      // We don't support undersampling
      // 

      if ((n_data_points > XY_PANE_WIDTH) || (!xy->set_input_width(n_data_points)))
         {
         SAL_alert_box("Error","Non-integral relationship between display width and # of data_points");
         exit(0);
         }

      //
      // Spread source amplitude range out across palette, leaving 10 dB margins at top and bottom
      // entry
      //

      S32 min_dBm = round_to_nearest(SOURCE_state->min_dBm);
      S32 max_dBm = round_to_nearest(SOURCE_state->max_dBm);

      RISE_VALUES[0]               = (SINGLE) (min_dBm - 10);
      RISE_VALUES[N_RISE_COLORS-1] = (SINGLE) (max_dBm + 10);

      SINGLE dc  = (N_RISE_COLORS-2) - 1;     // # of palette entries to spread
      SINGLE da  = (SINGLE) (max_dBm - min_dBm);
      SINGLE val = (SINGLE) min_dBm;

      for (S32 i=1; i <= N_RISE_COLORS-2; i++)
         {
         RISE_VALUES[i] = (SINGLE) round_to_nearest(val);
         val += (da / dc);
         }

      //
      // Set rising-raster graph attributes, including point size
      //
      // We want square cells, so use display width / n_data_points as
      // cell height
      //

      rise->set_graph_attributes(max(1, WATERFALL_PANE_WIDTH / n_data_points),
                                 RISE_VALUES,
                                 RISE_COLORS,
                                 N_RISE_COLORS,
                                 RISE_VALUES[0] - 10,
                                 RISE_VALUES[N_RISE_COLORS-1] + 10,
                                 RISE_UNDERRANGE_COLOR,
                                 RISE_OVERRANGE_COLOR,
                                 RISE_BLANK_COLOR);
      }

   //
   // Set X/Y graph operating mode
   // 

   xy->set_graph_attributes(style,
                            0,         // (We don't use the XY graph's pixel combiner; input width 
                            accum_op,  // is always equal to display width)
                            max_hold,
                            XY_BKGND_COLOR,
                            XY_TRACE_COLOR,
                            XY_SCALE_COLOR,
                            XY_MAJOR_COLOR,
                            XY_MINOR_COLOR,
                            XY_CURSOR_COLOR,
                            XY_RUN_COLOR);

   //
   // Check/uncheck appropriate menu entries
   //

   CheckMenuItem(hmenu, IDM_4,            MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_16,           MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_32,           MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_64,           MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_128,          MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_320,          MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_640,          MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_CONTINUOUS,   MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_DOTS,         MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_BARS,         MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_MIN,          MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_MAX,          MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_AVG,          MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_PTSAMPLE,     MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_SPLINE,       MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_F5,           MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_AMIN,         MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_AMAX,         MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_AAVG,         MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_MAXHOLD,      MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_LERP,         MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_GRATX,        MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_GRATY,        MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_WIFI,         MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_GPS,          MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_STOP_START,   MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_PAUSE_RESUME, paused ? MF_CHECKED : MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_HI_SPEED,     MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_856XA,        MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_8569B,        MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_3585,         MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_358XA,        MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_SCPI,         MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_HOUND,        MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_ADVANTEST,    MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_R3261,        MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_SAVE,         MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_PLAYBACK,     MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_SCREENSHOT,   MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_EXPORT_CSV,   MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_EXPORT_CSVLF, MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_EXPORT_KML,   MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_LAUNCH_MAPS,  MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_PRINT,        MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_SAVE_PAL,     MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_LOAD_PAL,     MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_SHOW_FPS,     MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_ANNOTATE,     MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_THRESHOLDS,   MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_DELTA_MARKER, MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_OFFSETS,      MF_UNCHECKED);
   CheckMenuItem(hmenu, IDM_MULTISEG,     MF_UNCHECKED);

   switch (operation)
      {
      case PLAYBACK: CheckMenuItem(hmenu, IDM_PLAYBACK, MF_CHECKED); break;
      }

   switch (n_data_points)
      {
      case 4:   CheckMenuItem(hmenu, IDM_4  , MF_CHECKED); break;
      case 16:  CheckMenuItem(hmenu, IDM_16 , MF_CHECKED); break; 
      case 32:  CheckMenuItem(hmenu, IDM_32 , MF_CHECKED); break; 
      case 64:  CheckMenuItem(hmenu, IDM_64 , MF_CHECKED); break; 
      case 128: CheckMenuItem(hmenu, IDM_128, MF_CHECKED); break; 
      case 320: CheckMenuItem(hmenu, IDM_320, MF_CHECKED); break; 
      case 640: CheckMenuItem(hmenu, IDM_640, MF_CHECKED); break; 
      }

   switch (style)
      {
      case XY_CONNECTED_LINES: CheckMenuItem(hmenu, IDM_CONTINUOUS,  MF_CHECKED); break;
      case XY_DOTS:            CheckMenuItem(hmenu, IDM_DOTS,        MF_CHECKED); break;
      case XY_BARS:            CheckMenuItem(hmenu, IDM_BARS,        MF_CHECKED); break;
      }

   switch (resamp_op)
      {
      case RT_AVG:    CheckMenuItem(hmenu, IDM_AVG,      MF_CHECKED); break;
      case RT_MIN:    CheckMenuItem(hmenu, IDM_MIN,      MF_CHECKED); break;
      case RT_MAX:    CheckMenuItem(hmenu, IDM_MAX,      MF_CHECKED); break;
      case RT_POINT:  CheckMenuItem(hmenu, IDM_PTSAMPLE, MF_CHECKED); break;
      case RT_SPLINE: CheckMenuItem(hmenu, IDM_SPLINE,   MF_CHECKED); break;
      }

   if (accum_op & XY_AVERAGE) CheckMenuItem(hmenu, IDM_AAVG, MF_CHECKED);
   if (accum_op & XY_MAXIMUM) CheckMenuItem(hmenu, IDM_AMAX, MF_CHECKED);
   if (accum_op & XY_MINIMUM) CheckMenuItem(hmenu, IDM_AMIN, MF_CHECKED);

   if ((resamp_op != last_resamp_op) || (accum_op != last_accum_op))
      {
      last_resamp_op = resamp_op;
      last_accum_op  = accum_op;

      xy->reset_running_values();

      if (accum_op & (XY_AVERAGE | XY_MAXIMUM | XY_MINIMUM))
         {
         xy->enable_running_overlay(TRUE);
         }
      else
         {
         xy->enable_running_overlay(FALSE);
         }
      }

   switch (max_hold)
      {
      case 1: CheckMenuItem(hmenu, IDM_MAXHOLD, MF_CHECKED); break;
      }

   switch (lerp_colors)
      {
      case 1: CheckMenuItem(hmenu, IDM_LERP, MF_CHECKED); break;
      }

   switch (SOURCE_state->hi_speed_acq)  
      {
      case 1: CheckMenuItem(hmenu, IDM_HI_SPEED, MF_CHECKED); break;
      }

   switch (SOURCE_state->HP856XA_mode)
      {
      case 1: CheckMenuItem(hmenu, IDM_856XA, MF_CHECKED); break;
      }

   switch (SOURCE_state->HP358XA_mode)
      {
      case 1: CheckMenuItem(hmenu, IDM_358XA, MF_CHECKED); break;
      }

   switch (SOURCE_state->HP8569B_mode)
      {
      case 1: CheckMenuItem(hmenu, IDM_8569B, MF_CHECKED); break;
      }

   switch (SOURCE_state->HP3585_mode)
      {
      case 1: CheckMenuItem(hmenu, IDM_3585, MF_CHECKED); break;
      }

   switch (SOURCE_state->SCPI_mode)
      {
      case 1: CheckMenuItem(hmenu, IDM_SCPI, MF_CHECKED); break;
      }

   switch (SOURCE_state->SA44_mode)
      {
      case 1: CheckMenuItem(hmenu, IDM_HOUND, MF_CHECKED); break;
      }

   switch (SOURCE_state->R3261_mode)
      {
      case 1: CheckMenuItem(hmenu, IDM_R3261, MF_CHECKED); break;
      }

   switch (SOURCE_state->Advantest_mode)
      {
      case 1: CheckMenuItem(hmenu, IDM_ADVANTEST, MF_CHECKED); break;
      }

   switch (show_fps)
      {
      case 1: CheckMenuItem(hmenu, IDM_SHOW_FPS, MF_CHECKED); break;
      }

   switch (show_annotation)
      {
      case 1: CheckMenuItem(hmenu, IDM_ANNOTATE, MF_CHECKED); break;
      }

   switch (apply_thresholds)
      {
      case 1: CheckMenuItem(hmenu, IDM_THRESHOLDS, MF_CHECKED); break;
      case 0: CheckMenuItem(hmenu, IDM_THRESHOLDS, MF_UNCHECKED); break;
      }

   switch (use_delta_marker)
      {
      case 1: CheckMenuItem(hmenu, IDM_DELTA_MARKER, MF_CHECKED); break;
      case 0: CheckMenuItem(hmenu, IDM_DELTA_MARKER, MF_UNCHECKED); break;
      }

   switch (grat_x)
      {
      case 1: CheckMenuItem(hmenu, IDM_GRATX, MF_CHECKED); break;
      case 0: CheckMenuItem(hmenu, IDM_GRATX, MF_UNCHECKED); break;
      }

   switch (grat_y)
      {
      case 1: CheckMenuItem(hmenu, IDM_GRATY, MF_CHECKED); break;
      case 0: CheckMenuItem(hmenu, IDM_GRATY, MF_UNCHECKED); break;
      }

   switch (show_wifi_band)
      {
      case 1: CheckMenuItem(hmenu, IDM_WIFI, MF_CHECKED); break;
      case 0: CheckMenuItem(hmenu, IDM_WIFI, MF_UNCHECKED); break;
      }

   switch (show_GPS_band)
      {
      case 1: CheckMenuItem(hmenu, IDM_GPS, MF_CHECKED); break;
      case 0: CheckMenuItem(hmenu, IDM_GPS, MF_UNCHECKED); break;
      }


}

//****************************************************************************
//*                                                                          *
//*  Initialize playback buttons                                             *
//*                                                                          *
//****************************************************************************

void init_playback_buttons(void)
{
   //
   // Create pane list with 4 buttons
   // 

   playback_buttons = VFX_pane_list_construct(6);     

   BTN_HOME = VFX_pane_list_add_area(playback_buttons,
                                     stage,
                                     HOME_BUTTON_X,
                                     HOME_BUTTON_Y,
                                     HOME_BUTTON_X + PLAYBACK_BUTTON_W - 1,
                                     HOME_BUTTON_Y + PLAYBACK_BUTTON_H - 1);

   BTN_FRWD = VFX_pane_list_add_area(playback_buttons,
                                     stage,
                                     FAST_REWIND_BUTTON_X,
                                     FAST_REWIND_BUTTON_Y,
                                     FAST_REWIND_BUTTON_X + PLAYBACK_BUTTON_W - 1,
                                     FAST_REWIND_BUTTON_Y + PLAYBACK_BUTTON_H - 1);
                       
   BTN_RWD = VFX_pane_list_add_area(playback_buttons,
                                    stage,
                                    REWIND_BUTTON_X,
                                    REWIND_BUTTON_Y,
                                    REWIND_BUTTON_X + PLAYBACK_BUTTON_W - 1,
                                    REWIND_BUTTON_Y + PLAYBACK_BUTTON_H - 1);
              
   BTN_FWD = VFX_pane_list_add_area(playback_buttons,
                                    stage,
                                    FORWARD_BUTTON_X,
                                    FORWARD_BUTTON_Y,
                                    FORWARD_BUTTON_X + PLAYBACK_BUTTON_W - 1,
                                    FORWARD_BUTTON_Y + PLAYBACK_BUTTON_H - 1);

   BTN_FFWD = VFX_pane_list_add_area(playback_buttons,
                                     stage,
                                     FAST_FORWARD_BUTTON_X,
                                     FAST_FORWARD_BUTTON_Y,
                                     FAST_FORWARD_BUTTON_X + PLAYBACK_BUTTON_W - 1,
                                     FAST_FORWARD_BUTTON_Y + PLAYBACK_BUTTON_H - 1);

   BTN_END = VFX_pane_list_add_area(playback_buttons,
                                     stage,
                                     END_BUTTON_X,
                                     END_BUTTON_Y,
                                     END_BUTTON_X + PLAYBACK_BUTTON_W - 1,
                                     END_BUTTON_Y + PLAYBACK_BUTTON_H - 1);

   selected_playback_button = -1;
}

//****************************************************************************
//*                                                                          *
//*  Display playback controls and keep track of which button is currently   *
//*  being selected by mouse cursor (if any)                                 *
//*                                                                          *
//****************************************************************************

void display_playback_buttons(void)
{
   PANE *target;
   S32 bk,fg,bh,b;

   //
   // See if mouse cursor is over a button
   //
   // Buttons are grayed-out and unselectable when not in playback mode
   //

   if (operation == PLAYBACK)
      {
      bh  = RGB_TRIPLET(255,255,0);       // bright yellow background if highlighted with mouse
      bk  = RGB_TRIPLET(150,150,0);       // dark yellow background otherwise
      fg  = RGB_TRIPLET(127,127,127);     // gray foreground

      selected_playback_button = VFX_pane_list_identify_point(playback_buttons,
                                                              mouse_x,
                                                              mouse_y);
      }
   else
      {
      bk = RGB_TRIPLET(64,64,64);         // dark gray background
      fg = RGB_TRIPLET(127,127,127);      // gray foreground
      bh = bk;

      selected_playback_button = -1;
      }

   //
   // Get pane-relative bottom, right edge coordinates
   //

   S32 w = PLAYBACK_BUTTON_W-1;
   S32 h = PLAYBACK_BUTTON_H-1;

   //
   // Draw home button
   //

   b = (selected_playback_button == 0) ? bh : bk;

   target = VFX_pane_list_get_entry(playback_buttons, BTN_HOME);

   VFX_pane_wipe(target, b);
   VFX_rectangle_draw(target, 0, 0, w, h, LD_DRAW, fg);

   VFX_line_draw(target,
                 (w/4)+1,
                 (h-(h/4))-2,
                 (w/2)+1,
                 (h/2)-2,
                 LD_DRAW,
                 RGB_TRIPLET(0,0,0));

   VFX_line_draw(target,
                 (w-1-(w/4))+1,
                 (h-(h/4))-2,
                 (w/2)+1,
                 (h/2)-2,
                 LD_DRAW,
                 RGB_TRIPLET(0,0,0));

   VFX_line_draw(target,
                 (w/4)+1,
                 (h-(h/4))-1,
                 (w/2)+1,
                 (h/2)-1,
                 LD_DRAW,
                 RGB_TRIPLET(0,0,0));

   VFX_line_draw(target,
                 (w-1-(w/4))+1,
                 (h-(h/4))-1,
                 (w/2)+1,
                 (h/2)-1,
                 LD_DRAW,
                 RGB_TRIPLET(0,0,0));

   //
   // Draw fast-rewind ('<<') button
   //

   b = (selected_playback_button == 1) ? bh : bk;

   target = VFX_pane_list_get_entry(playback_buttons, BTN_FRWD);

   VFX_pane_wipe(target, b);
   VFX_rectangle_draw(target, 0, 0, w, h, LD_DRAW, fg);

   VFX_character_draw(target,
                      w/2-4,
                      h/2-3,
                      VFX_default_system_font(),
                      '<',
                      VFX_default_font_color_table(VFC_BLACK_ON_XP));

   VFX_character_draw(target,
                      w/2+0,
                      h/2-3,
                      VFX_default_system_font(),
                      '<',
                      VFX_default_font_color_table(VFC_BLACK_ON_XP));

   //
   // Draw rewind ('<') button
   //

   b = (selected_playback_button == 2) ? bh : bk;

   target = VFX_pane_list_get_entry(playback_buttons, BTN_RWD);

   VFX_pane_wipe(target, b);
   VFX_rectangle_draw(target, 0, 0, w, h, LD_DRAW, fg);

   VFX_character_draw(target,
                      w/2-2,
                      h/2-3,
                      VFX_default_system_font(),
                      '<',
                      VFX_default_font_color_table(VFC_BLACK_ON_XP));

   //
   // Draw forward ('>') button
   //

   b = (selected_playback_button == 3) ? bh : bk;

   target = VFX_pane_list_get_entry(playback_buttons, BTN_FWD);

   VFX_pane_wipe(target, b);
   VFX_rectangle_draw(target, 0, 0, w, h, LD_DRAW, fg);

   VFX_character_draw(target,
                      w/2-2,
                      h/2-3,
                      VFX_default_system_font(),
                      '>',
                      VFX_default_font_color_table(VFC_BLACK_ON_XP));

   //
   // Draw fast-forward ('>>') button
   //

   b = (selected_playback_button == 4) ? bh : bk;

   target = VFX_pane_list_get_entry(playback_buttons, BTN_FFWD);

   VFX_pane_wipe(target, b);
   VFX_rectangle_draw(target, 0, 0, w, h, LD_DRAW, fg);

   VFX_character_draw(target,
                      w/2-4,
                      h/2-3,
                      VFX_default_system_font(),
                      '>',
                      VFX_default_font_color_table(VFC_BLACK_ON_XP));

   VFX_character_draw(target,
                      w/2+0,
                      h/2-3,
                      VFX_default_system_font(),
                      '>',
                      VFX_default_font_color_table(VFC_BLACK_ON_XP));

   //
   // Draw end button
   //

   b = (selected_playback_button == 5) ? bh : bk;

   target = VFX_pane_list_get_entry(playback_buttons, BTN_END);

   VFX_pane_wipe(target, b);
   VFX_rectangle_draw(target, 0, 0, w, h, LD_DRAW, fg);

   VFX_line_draw(target,
                 (w/4)+1,
                 ((h/4))+1,
                 (w/2)+1,
                 (h/2)+1,
                 LD_DRAW,
                 RGB_TRIPLET(0,0,0));

   VFX_line_draw(target,
                 (w-1-(w/4))+1,
                 ((h/4))+1,
                 (w/2)+1,
                 (h/2)+1,
                 LD_DRAW,
                 RGB_TRIPLET(0,0,0));

   VFX_line_draw(target,
                 (w/4)+1,
                 ((h/4))+2,
                 (w/2)+1,
                 (h/2)+2,
                 LD_DRAW,
                 RGB_TRIPLET(0,0,0));

   VFX_line_draw(target,
                 (w-1-(w/4))+1,
                 ((h/4))+2,
                 (w/2)+1,
                 (h/2)+2,
                 LD_DRAW,
                 RGB_TRIPLET(0,0,0));

}

//****************************************************************************
//*                                                                          *
//*  Handle mouse click on playback buttons                                  *
//*                                                                          *
//****************************************************************************

void process_playback_button(S32 selection)
{
   //
   // If there aren't enough records in the file to fill the entire
   // display, disallow scrolling
   //
   // This allows us to assume that new rows are always scrolled in at the 
   // bottom or top of the display
   //

   if (n_readable_records < n_visible_rows)
      {
      return;
      }

   //
   // Determine requested screen scrolling direction and magnitude
   //

   S32 scroll_direction = 0;
   S32 seek_direction = 0;
   S32 magnitude = 0;
   S32 insert_row = 0;

   if ((selection == BTN_FWD)  || 
       (selection == BTN_FFWD) ||
       (selection == BTN_END))   
      {
      scroll_direction = -1;                    // scroll up to display later data in file
      seek_direction   =  1;                    // seek forward to fetch later data in file
      insert_row       =  n_visible_rows - 1;   // insert new rows at bottom
      }
   else if ((selection == BTN_RWD)  ||  
            (selection == BTN_FRWD) ||
            (selection == BTN_HOME))   
      {
      scroll_direction =  1;                    // scroll down to display earlier data in file
      seek_direction   = -1;                    // seek backward to fetch earlier data in file
      insert_row       =  0;                    // insert new rows at bottom
      }

   if ((selection == BTN_FFWD) || 
       (selection == BTN_FRWD))
      {
      magnitude = n_visible_rows / 4;    // scroll 1/4 screen at a time
      }
   else if ((selection == BTN_HOME) ||
            (selection == BTN_END))
      {
      magnitude = n_readable_records;    // scroll far enough to cover the entire file
      }
   else
      {
      magnitude = 1;                     // scroll 1 row at a time
      }

   //
   // If we are reversing the direction of scrolling, we must 
   // adjust the file playback position by the size of the displayed page
   //

   if ((scroll_direction == 1) && (last_scroll_direction == -1))
      {
      playback_position -= (n_visible_rows - 1);
      }

   if ((scroll_direction == -1) && (last_scroll_direction == 1))
      {
      playback_position += (n_visible_rows - 1);
      }

   last_scroll_direction = scroll_direction;

   assert(playback_position >= 0);
   assert(playback_position <= n_readable_records);

   //
   // Adjust scrolling magnitude to accommodate file limits
   //

   if (magnitude <= 1)
      {
      magnitude = 1;
      }

   while (magnitude > 0)
      {
      S32 end_pos = playback_position + (seek_direction * magnitude);

      if ((end_pos >= 0) && (end_pos < n_readable_records))
         {
         break;
         }

      --magnitude;
      }

   if (magnitude <= 0)
      {
      //
      // Scrolling limit reached, exit
      //

      return;
      }

   //
   // Scroll the rising-raster window
   //

   rise->scroll_graph(scroll_direction * magnitude);

   //
   // Load and display fields from file
   //

   HCURSOR norm             = NULL;
   S32     orig_magnitude   = magnitude;
   S32     next_update_time = timeGetTime();
   S32     sw               = -1;
   C8      string[512];

   while (magnitude--)
      {
      playback_position += seek_direction;

      //
      // Update percent-complete display during lengthy reads, maintaining
      // video page locking state
      //

      S32 cur_time   = timeGetTime();
      S32 delta_time = cur_time - next_update_time;

      if ((delta_time > 0) && (magnitude >= n_visible_rows))
         {
         if (norm == NULL) norm = SetCursor(waitcur);          

         next_update_time = cur_time + 250;

         sprintf(string,"   Processing, %d%%   ",round_to_nearest(((orig_magnitude-magnitude) * 100.0F) / orig_magnitude));

         if (!stage_locked_for_writing)
            {
            VFX_lock_window_surface(stage, VFX_BACK_SURFACE);
            }

         if (sw == -1)
            {
            sw = string_width(string, 
                              VFX_default_system_font());
            }

         VFX_string_draw(rise_pane,
                       ((rise_pane->x1 - rise_pane->x0) - sw) / 2,
                        (rise_pane->y1 - rise_pane->y0) / 2,
                         VFX_default_system_font(),
                         string,
                         VFX_default_font_color_table(VFC_WHITE_ON_BLACK));

         if (!stage_locked_for_writing)
            {
            VFX_unlock_window_surface(stage, TRUE);
            }
         else
            {
            VFX_unlock_window_surface(stage, TRUE);
            VFX_lock_window_surface(stage, VFX_BACK_SURFACE);
            }
         }

      //
      // Read raw input data from file
      //

      DOUBLE latitude      = INVALID_LATLONGALT;
      DOUBLE longitude     = INVALID_LATLONGALT; 
      DOUBLE altitude_m    = INVALID_LATLONGALT; 
      DOUBLE start_Hz      = INVALID_FREQ;
      DOUBLE stop_Hz       = INVALID_FREQ;
      C8     caption[128] = { 0 };

      acquisition_time = recorder->read_record(data,
                                               playback_position,
                                              &latitude,
                                              &longitude,
                                              &altitude_m,
                                              &start_Hz,
                                              &stop_Hz,
                                               caption,
                                               sizeof(caption));

      //
      // Process only the last n row records, where n is the number of visible rows
      //
      // If max-hold or accumulation is enabled, we need to go ahead and send the 
      // data through the X/Y graph processor to ensure it's accumulated to the display.
      // This can slow down an already-slow operation, so we don't do it if it's not
      // necessary
      //

      if ((magnitude >= n_visible_rows) && (!max_hold) && (!accum_op) && (start_Hz == INVALID_FREQ))
         {
         continue;
         }

      //
      // Set specific frequency range for this row if the file provides it
      //

      if ((start_Hz != INVALID_FREQ) && (stop_Hz != INVALID_FREQ))
         {
         DOUBLE max_MHz = stop_Hz / 1000000.0;
         DOUBLE min_MHz = start_Hz / 1000000.0;

         DOUBLE span = max_MHz - min_MHz;

         xy->set_X_range(min_MHz,
                         max_MHz,
                         "%.3lf",
                         span / 4.0,
                         span / 40.0);

         xy->set_X_cursors(min_MHz + (span / 10.0),
                           max_MHz - (span / 10.0));
         }

      //         
      // Send points to X/Y graph for processing
      //

      GNSS->latitude   = latitude;
      GNSS->longitude  = longitude;
      GNSS->altitude_m = altitude_m;

      xy->set_arrayed_points(data);

      xy->process_input_data();

      if (magnitude >= n_visible_rows)
         {
         continue;
         }

      //
      // Render processed row onto X/Y graph
      //

      XY_DATAREQ array = apply_thresholds ? XY_THRESHOLD_ARRAY : XY_COMBINED_ARRAY;

      rise->draw_graph_row(acquisition_time,                       
                           xy->get_graph_data(array),  
                           insert_row + (magnitude * scroll_direction),
                           lerp_colors,
                           latitude,
                           longitude,
                           altitude_m,
                           start_Hz,
                           stop_Hz,
                           caption);                             
      }

   //
   // Draw rising-raster scale and labels
   //
   // (X/Y graph will be updated in AppMain loop)
   //

   rise->draw_scale(mouse_x, mouse_y);

   if (norm != NULL) SetCursor(norm);
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
      SetMenu(hwnd,NULL);
      DestroyMenu(hmenu);
      }

   hmenu = CreateMenu();    

   HMENU pop = CreateMenu();

   C8 loadcmd[512] = { 0 };
   C8 savecmd[512] = { 0 };
   sprintf(loadcmd, "Load palette from %s", colors_filename);
   sprintf(savecmd, "Save palette to %s", colors_filename);

   AppendMenu(pop, MF_STRING, IDM_SAVE,     "Save current acquisition to file...\ts");
   AppendMenu(pop, MF_STRING, IDM_PLAYBACK, "Play recorded data from file...\tl");
   AppendMenu(pop, MF_SEPARATOR, 0, NULL);
   AppendMenu(pop, MF_STRING, IDM_EXPORT_CSV, "Export X/Y trace to .CSV file...");
   AppendMenu(pop, MF_STRING, IDM_EXPORT_CSVLF, "Export individual X/Y points to .CSV file...\te");
   AppendMenu(pop, MF_STRING, IDM_EXPORT_KML, "Export GPS path to .KML file...\tCtrl-k");
   AppendMenu(pop, MF_SEPARATOR, 0, NULL);
   AppendMenu(pop, MF_STRING, IDM_LAUNCH_MAPS, "Launch Google Maps at cursor location\tCtrl-m");
   AppendMenu(pop, MF_SEPARATOR, 0, NULL);
   AppendMenu(pop, MF_STRING, IDM_SCREENSHOT, "Save .BMP, .GIF, .TGA, or .PCX screen shot...\tS");
   AppendMenu(pop, MF_STRING, IDM_PRINT, "Print screen image\tp");
   AppendMenu(pop, MF_SEPARATOR, 0, NULL);
   AppendMenu(pop, MF_STRING, IDM_LOAD_PAL, loadcmd);
   AppendMenu(pop, MF_STRING, IDM_SAVE_PAL, savecmd);
   AppendMenu(pop, MF_SEPARATOR, 0, NULL);
   AppendMenu(pop, MF_STRING, IDM_QUIT, "Quit\tEsc");

   AppendMenu(hmenu, MF_POPUP, (UINT) pop, "&File");

   pop = CreateMenu();

   AppendMenu(pop, MF_STRING, IDM_4  , "4");  
   AppendMenu(pop, MF_STRING, IDM_16 , "16"); 
   AppendMenu(pop, MF_STRING, IDM_32 , "32"); 
   AppendMenu(pop, MF_STRING, IDM_64 , "64"); 
   AppendMenu(pop, MF_STRING, IDM_128, "128");
   AppendMenu(pop, MF_STRING, IDM_320, "320");
   AppendMenu(pop, MF_STRING, IDM_640, "640");
   AppendMenu(pop, MF_SEPARATOR, 0, NULL);
   AppendMenu(pop, MF_STRING, IDM_MIN,        "Select minimum");
   AppendMenu(pop, MF_STRING, IDM_MAX,        "Select maximum");
   AppendMenu(pop, MF_STRING, IDM_AVG,        "Select average");
   AppendMenu(pop, MF_STRING, IDM_PTSAMPLE,   "Point-sample");
   AppendMenu(pop, MF_STRING, IDM_SPLINE,     "Cubic spline");

   AppendMenu(hmenu, MF_POPUP, (UINT) pop,   "&Points");

   pop = CreateMenu();

   AppendMenu(pop, MF_STRING, IDM_CONTINUOUS, "Connected lines");
   AppendMenu(pop, MF_STRING, IDM_DOTS,       "Dots");
   AppendMenu(pop, MF_STRING, IDM_BARS,       "Bars");

   AppendMenu(hmenu, MF_POPUP, (UINT) pop,   "&Style");

   pop = CreateMenu();

   AppendMenu(pop, MF_STRING, IDM_F5,         "Recompose display\tF5");
   AppendMenu(pop, MF_SEPARATOR, 0, NULL);
   AppendMenu(pop, MF_STRING, IDM_AMIN,       "Accumulate minimum");
   AppendMenu(pop, MF_STRING, IDM_AMAX,       "Accumulate maximum\tM");
   AppendMenu(pop, MF_STRING, IDM_AAVG,       "Accumulate average");
   AppendMenu(pop, MF_SEPARATOR, 0, NULL);
   AppendMenu(pop, MF_STRING, IDM_MAXHOLD,    "X/Y max hold\tm");
   AppendMenu(pop, MF_SEPARATOR, 0, NULL);
   AppendMenu(pop, MF_STRING, IDM_LERP,       "Smooth waterfall colors");
   AppendMenu(pop, MF_SEPARATOR, 0, NULL);
   AppendMenu(pop, MF_STRING, IDM_GRATX,      "Graticule X\tx");
   AppendMenu(pop, MF_STRING, IDM_GRATY,      "Graticule Y\ty");
   AppendMenu(pop, MF_SEPARATOR, 0, NULL);
   AppendMenu(pop, MF_STRING, IDM_WIFI,       "802.11 WLAN channel boundaries\tw");
   AppendMenu(pop, MF_STRING, IDM_GPS,        "GPS L1 boundaries\tg");
   AppendMenu(pop, MF_SEPARATOR, 0, NULL);
   AppendMenu(pop, MF_STRING, IDM_SHOW_FPS,   "Show frame rate");
// AppendMenu(pop, MF_STRING, IDM_ANNOTATE,   "Caption ...");
   AppendMenu(pop, MF_SEPARATOR, 0, NULL);
   AppendMenu(pop, MF_STRING, IDM_FS_TOGGLE,  "Toggle fullscreen mode\tAlt-Enter");

   AppendMenu(hmenu, MF_POPUP, (UINT) pop,   "&Display");

   pop = CreateMenu();

   AppendMenu(pop, MF_STRING, IDM_THRESHOLDS,   "Apply cursor thresholds");
   AppendMenu(pop, MF_SEPARATOR, 0, NULL);
   AppendMenu(pop, MF_STRING, IDM_OFFSETS,      "Edit frequency and amplitude offsets...\to");
   AppendMenu(pop, MF_SEPARATOR, 0, NULL);
   AppendMenu(pop, MF_STRING, IDM_MULTISEG,     "Edit composite sweep range...\tr");

//   AppendMenu(pop, MF_STRING, IDM_DELTA_MARKER, "Enable delta marker");

   AppendMenu(hmenu, MF_POPUP, (UINT) pop,   "&Measure");

   pop = CreateMenu();

   AppendMenu(pop, MF_STRING, IDM_STOP_START,   "Stop/restart acquisition\tSpace");
   AppendMenu(pop, MF_STRING, IDM_PAUSE_RESUME, "Pause/resume acquisition\tScroll Lock");
   AppendMenu(pop, MF_SEPARATOR, 0, NULL);

   S32 i;
   for (i=0; i <= 30; i++)
      {
      C8 val[500];
      sprintf(val,"Acquire data from supported device at address %d",i);

      AppendMenu(pop, MF_STRING, i+IDM_PAUSE_RESUME+1, val);
      }

   AppendMenu(pop, MF_STRING, IDM_HOUND, "Acquire data from USB Signal Hound");
   AppendMenu(pop, MF_SEPARATOR, 0, NULL);
   AppendMenu(pop, MF_STRING,IDM_856XA,"HP 8566A/8567A/8568A mode");
   AppendMenu(pop, MF_STRING,IDM_3585,"HP 3585A/B mode");
   AppendMenu(pop, MF_STRING,IDM_358XA,"HP 3588A/3589A mode");
   AppendMenu(pop, MF_STRING,IDM_8569B,"HP 8569B/8570A mode");
   AppendMenu(pop, MF_STRING,IDM_R3261,"Advantest R3261/R3361/R3265/R3271 mode");
   AppendMenu(pop, MF_STRING,IDM_ADVANTEST,"Advantest R3100/R3200/R3400 mode");
   AppendMenu(pop, MF_STRING,IDM_SCPI,"SCPI-compatible analyzer mode");

   AppendMenu(pop, MF_SEPARATOR, 0, NULL);
   AppendMenu(pop, MF_STRING,IDM_HI_SPEED,"Acquire at maximum available speed");

   AppendMenu(hmenu, MF_POPUP, (UINT) pop,  "&Acquire");

   pop = CreateMenu();

   AppendMenu(pop, MF_STRING, IDM_USER_GUIDE,   "&User guide \t F1");
   AppendMenu(pop, MF_SEPARATOR, 0, NULL);
   AppendMenu(pop, MF_STRING, IDM_ABOUT,        "&About SSM");

   AppendMenu(hmenu, MF_POPUP, (UINT) pop,   "&Help");

   SetMenu(hwnd,hmenu);
   DrawMenuBar(hwnd);
}

//****************************************************************************
//
// Select new operation for display (monitor / record / playback)  
//
//****************************************************************************

void select_operation(OPERATION new_operation, 
                      S32       GPIB_address,
                      S32       preserve_current_controls)     // true if we're just refreshing and don't want to change any control settings
{
   recorder->close_readable_file(); 

   recorder->close_writable_file(SOURCE_top_cursor, 
                                 SOURCE_bottom_cursor,
                                 N_RISE_COLORS,
                                 RISE_COLORS);
   switch (operation)
      {
      case PLAYBACK: 
         break;
      }

   switch (new_operation)
      {
      case STOP:
         {
         operation = STOP;
         SOURCE_disconnect();

         graph_setup();
         break;
         }

      case ACQUIRE:
         {
         operation = ACQUIRE;
         SOURCE_disconnect();

         //
         // Identify instrument and read its static parameters
         //

         SOURCE_address = GPIB_address;

         if (!SOURCE_connect())
            {
            exit(1);
            }

         //
         // Configure graph
         //

         SOURCE_top_cursor    = (SINGLE) (SOURCE_state->max_dBm + (SOURCE_state->dB_division / 2)); 
         SOURCE_bottom_cursor = (SINGLE) (SOURCE_state->min_dBm - (SOURCE_state->dB_division / 2)); 

         graph_setup(TRUE);

         //
         // Open file for writing, based on currently-selected # of input
         // data points
         //

         DOUBLE mn_Hz = 0.0;
         DOUBLE mx_Hz = 0.0;
         multisweep(&mn_Hz, &mx_Hz);

         if (!recorder->open_writable_file(session_bin_path,
                                           n_data_points,
                                           mn_Hz,
                                           mx_Hz,
                                  (SINGLE) SOURCE_state->min_dBm,
                                  (SINGLE) SOURCE_state->max_dBm,
                                           SOURCE_state->amplitude_levels,
                                           SOURCE_state->dB_division,
                                           xy->top_cursor_val(),
                                           xy->bottom_cursor_val(),
                                           RISE_COLORS,
                                           N_RISE_COLORS))
            {
            SAL_alert_box("Error","Could not open file %s for writing\n",session_bin_path);
            exit(1);
            }

         //
         // Current file for saving/browsing becomes SESSION.BIN
         //

         strcpy(currently_open_filename, session_bin_path);
         break;
         }

      case PLAYBACK:
         {
         operation = PLAYBACK;
         SOURCE_disconnect();

         //
         // Open file for reading
         //
         // Don't overwrite the palette or the cursors if preserve_current_controls is TRUE
         //

         S32 file_version;
         
         SINGLE file_top_cursor = 0.0F;
         SINGLE file_bottom_cursor = 0.0F;

         SINGLE min_dBm = (SINGLE) SOURCE_state->min_dBm;
         SINGLE max_dBm = (SINGLE) SOURCE_state->max_dBm;

         DOUBLE mn_Hz = 0.0;
         DOUBLE mx_Hz = 0.0;
         multisweep(&mn_Hz, &mx_Hz);

         S32 record_width = recorder->open_readable_file(currently_open_filename,
                                                        &file_version,
                                                        &mn_Hz,
                                                        &mx_Hz,
                                                        &min_dBm,
                                                        &max_dBm,
                                                        &SOURCE_state->amplitude_levels,
                                                        &SOURCE_state->dB_division,
                                                        &file_top_cursor,
                                                        &file_bottom_cursor,
                                                         RISE_COLORS,
                                                         preserve_current_controls ? 0 : N_RISE_COLORS);
         SOURCE_state->min_dBm = min_dBm;
         SOURCE_state->max_dBm = max_dBm;
         SOURCE_state->min_Hz  = mn_Hz;
         SOURCE_state->max_Hz  = mx_Hz;

         if (record_width == 0)
            {
            SAL_alert_box("Error","Could not open file %s\n",currently_open_filename);
            operation = STOP;
            return;
            }

         if (!preserve_current_controls)
            {
            SOURCE_top_cursor    = file_top_cursor;
            SOURCE_bottom_cursor = file_bottom_cursor;
            }

#if 0
         DOUBLE mn_Hz = 0.0;
         DOUBLE mx_Hz = 0.0;
         multisweep(&mn_Hz, &mx_Hz);

         SAL_debug_printf("%d points, %lf to %lf Hz\n%f to %f dBm\nCursors at %f, %f\n",
            n_data_points,
            mn_Hz,
            mx_Hz,
            SOURCE_state->min_dBm,
            SOURCE_state->max_dBm,
            SOURCE_top_cursor,
            SOURCE_bottom_cursor);
#endif

         //
         // Use the palette from the file we just loaded
         //

         set_waterfall_palette();

         //
         // Configure the display for the # of data points saved in the file
         //

         n_data_points = record_width;

         if (xy != NULL)
            {
            //
            // Force cursors to conform to loaded file
            //

            xy->set_Y_cursors(SOURCE_top_cursor,
                              SOURCE_bottom_cursor);
            }

         graph_setup(TRUE);

         //
         // Get total # of records in file, and # visible on page
         //

         n_readable_records = recorder->n_readable_records();
         n_visible_rows     = rise->n_visible_rows();

//         printf("%d records, %d rows, w=%d\n",n_readable_records, n_visible_rows, recorder->read_input_width);
//
//         FILE *hack = fopen("georgetown.ssm","rb");
//         static C8 buff[70000000] = { 0 };
//         S32 len = fread(buff,1,70000000,hack);
//         fclose(hack);
//                   
//         len -= (70 * recorder->bytes_per_record(recorder->read_input_width));
//         
//         hack = fopen("test.ssm","wb");
//         fwrite(buff, len, 1, hack);
//         fclose(hack);

         //
         // Fetch and display initial page of data from file
         //

         S32 need_unlock = FALSE;

         if (!stage_locked_for_writing)
            {
            VFX_lock_window_surface(stage, VFX_BACK_SURFACE);
            stage_locked_for_writing = TRUE;
            need_unlock = TRUE;
            }

         rise->wipe_graph(RGB_TRIPLET(0,0,0));

         S32 n_visible_records = min(n_readable_records, 
                                     n_visible_rows);

         S32 justify_at_bottom = 0;

         if (n_visible_records < n_visible_rows)
            justify_at_bottom = n_visible_rows - n_visible_records;

         S32 i;
         for (i=0; i < n_visible_records; i++)
            {
            //
            // Read raw input data from file
            //

            C8 caption[128] = { 0 };

            DOUBLE start_Hz = INVALID_FREQ;
            DOUBLE stop_Hz  = INVALID_FREQ;

            acquisition_time = recorder->read_record(data,
                                                     i,
                                                    &GNSS->latitude,
                                                    &GNSS->longitude,
                                                    &GNSS->altitude_m,
                                                    &start_Hz,
                                                    &stop_Hz,
                                                     caption,
                                                     sizeof(caption));

            //         
            // Send points to X/Y graph for processing
            //

            xy->set_arrayed_points(data);

            xy->process_input_data();

            //
            // Render processed row onto X/Y graph
            //

            XY_DATAREQ array = apply_thresholds ? XY_THRESHOLD_ARRAY : XY_COMBINED_ARRAY;

            rise->draw_graph_row(acquisition_time,                       
                                 xy->get_graph_data(array),                // get data
                                 i+justify_at_bottom,                      // draw at current row
                                 lerp_colors,
                                 GNSS->latitude,
                                 GNSS->longitude,
                                 GNSS->altitude_m,
                                 start_Hz,
                                 stop_Hz,
                                 caption);
            }

         //
         // Set initial playback position = last row shown+1
         //

         playback_position     = i - 1;
         last_scroll_direction = -1;
         wheel_scroll          = 0;

         //
         // Draw rising-raster scale and labels
         //
         // (X/Y graph will be updated in AppMain loop)
         //

         rise->draw_scale(mouse_x, mouse_y);

         if (need_unlock)
            {
            VFX_unlock_window_surface(stage, FALSE);
            stage_locked_for_writing = FALSE;
            }

         //
         // Always begin playback at end of file
         //

         process_playback_button(BTN_END);
         break;
         }
      }
}

//****************************************************************************
//
// Set new # of input data points, checking to make sure we aren't
// in the middle of recording or playback
//
//****************************************************************************

void set_n_points(S32 n_points)
{
   if (operation == PLAYBACK)
      {
      select_operation(STOP);
      }

   if (operation != STOP)
      {
      SAL_alert_box("Invalid operation",
         "Cannot change input data record size while acquiring data\n\
Select Acquire->Stop/restart acquisition first\n");
      return;
      }

   n_data_points = n_points;          
   graph_setup(TRUE); 

   select_operation(STOP);
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

   //
   // Destroy recorder object, closing any open files
   //

   if (recorder != NULL)
      {
      recorder->close_readable_file(); 

      recorder->close_writable_file(SOURCE_top_cursor, 
                                    SOURCE_bottom_cursor,
                                    N_RISE_COLORS,
                                    RISE_COLORS);
      delete recorder;
      recorder = NULL;
      }

   //
   // Destroy X/Y and rising-raster graph objects
   //

   if (rise != NULL)
      {
      delete rise;
      rise = NULL;
      }

   if (xy != NULL)
      {
      delete xy;
      xy = NULL;
      }

   //
   // Disconnect GPIB connection (if any), shut down SAL, and terminate
   //

   SOURCE_disconnect(TRUE);

   SA_shutdown();

   GNSS_shutdown(GNSS);

   SAL_shutdown();
}

void WINAPI WinExit(void)
{
   if (!exit_handler_active)
      {
      WinClean();
      }

   exit(0);
}

void AppExit(void)
{
   if (!exit_handler_active)
      {
      WinClean();
      }

   return;
}

//****************************************************************************
//
// Keep track of mouse activity
//
//****************************************************************************

void update_mouse_vars(void)
{
   SAL_WINAREA wndrect;
   SAL_client_area(&wndrect);    // Includes menu

   POINT cursor;
   GetCursorPos(&cursor);

   mouse_x = cursor.x - wndrect.x;
   mouse_y = cursor.y - wndrect.y;

   mouse_over_menu = FALSE;

   if (GetMenu(hwnd) != NULL)
      {
      if (mouse_y < GetSystemMetrics(SM_CYMENU))
         {
         mouse_over_menu = TRUE;
         }
      }

   last_mouse_left = mouse_left;
   app_active      = SAL_is_app_active();
   mouse_left      = (GetKeyState(VK_LBUTTON) & 0x8000) && app_active;
}

//****************************************************************************
//
// CMD_edit_offsets(void)
//
//****************************************************************************

void CMD_edit_offsets(void)
{
   if (operation == PLAYBACK)
      {
      select_operation(STOP);
      }

   if (operation != STOP)
      {
      SAL_alert_box("Invalid operation",
         "Cannot edit frequency/amplitude offsets while acquiring data\n\
Select Acquire->Stop/restart acquisition first\n");
      return;
      }

   strcpy(DLG_box_caption, "Edit frequency/amplitude offsets");

   if (!DialogBox(hInstance,
                  MAKEINTRESOURCE(IDD_EDIT_OFFSETS),
                  hwnd,
                  OffsetDlgProc))
      {
      return;
      }
      
   graph_setup(TRUE);  
}

//****************************************************************************
//
// CMD_edit_sweep_range(void)
//
//****************************************************************************

void CMD_edit_sweep_range(void)
{
   if (operation == PLAYBACK)
      {
      select_operation(STOP);
      }

   if (operation != STOP)
      {
      SAL_alert_box("Invalid operation",
         "Cannot edit sweep range while acquiring data\n\
Select Acquire->Stop/restart acquisition first\n");
      return;
      }

   strcpy(DLG_box_caption, "Edit sweep range");

   if (!DialogBox(hInstance,
                  MAKEINTRESOURCE(IDD_EDIT_SWEEP_RANGE),
                  hwnd,
                  SweepDlgProc))
      {
      return;
      }
      
   graph_setup(TRUE);  
}

//****************************************************************************
//
// CMD_save_KML(void)
//
//****************************************************************************

void CMD_save_KML(C8 *filename)
{
   RECORDER R;
   S32 file_version;
   
   SINGLE file_top_cursor = 0.0F;
   SINGLE file_bottom_cursor = 0.0F;

   SINGLE min_dBm = 0.0F;
   SINGLE max_dBm = 0.0F;
   DOUBLE min_Hz = 0.0;
   DOUBLE max_Hz = 0.0;

   S32 amplitude_levels = 0;
   S32 dB_division = 0;

   S32 record_width = R.open_readable_file(currently_open_filename,
                                          &file_version,
                                          &min_Hz,
                                          &max_Hz,
                                          &min_dBm,
                                          &max_dBm,
                                          &amplitude_levels,
                                          &dB_division,
                                          &file_top_cursor,
                                          &file_bottom_cursor,
                                           NULL,
                                           0);
   if (record_width == 0)
      {
      SAL_alert_box("Error","Could not open file %s\n",currently_open_filename);
      return;
      }

   //
   // Open KML output file
   //

   FILE *KML = fopen(filename,"wt");

   if (KML == NULL)
      {
      SAL_alert_box("Error","Could not open file %s\n",filename);
      return;
      }

   fprintf(KML,"\
<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\
<kml xmlns=\"http://www.opengis.net/kml/2.2\" xmlns:gx=\"http://www.google.com/kml/ext/2.2\" xmlns:kml=\"http://www.opengis.net/kml/2.2\" xmlns:atom=\"http://www.w3.org/2005/Atom\">\n\
<Document>\n\
	<name>%s</name>\n\
	<Style id=\"officialPathStyle\">\n\
		<LineStyle>\n\
			<color>888d1bFF</color>\n\
			<width>4</width>\n\
		</LineStyle>\n\
	</Style>\n\
	<Placemark>\n\
		<name>Segment 1</name>\n\
		<styleUrl>#officialPathStyle</styleUrl>\n\
		<LineString>\n\
			<tessellate>1</tessellate>\n\
			<coordinates>\n\
", currently_open_filename);

   //
   // Iterate through records
   //

   DOUBLE last_latitude   = -1000.0;
   DOUBLE last_longitude  = -1000.0;
   DOUBLE last_altitude_m = -1000.0;

   S32 n_readable_records = R.n_readable_records();

   for (S32 i=0; i < n_readable_records; i++)
      {
      DOUBLE latitude   = INVALID_LATLONGALT;
      DOUBLE longitude  = INVALID_LATLONGALT;
      DOUBLE altitude_m = INVALID_LATLONGALT;

      DOUBLE start_Hz   = INVALID_FREQ;
      DOUBLE stop_Hz    = INVALID_FREQ;

      C8 caption[128] = { 0 };

      acquisition_time = R.read_record(data,
                                       i,
                                      &latitude,
                                      &longitude,
                                      &altitude_m,
                                      &start_Hz,
                                      &stop_Hz,
                                       caption,
                                       sizeof(caption));

      if ((latitude   == INVALID_LATLONGALT)  || 
          (longitude  == INVALID_LATLONGALT) || 
          (altitude_m == INVALID_LATLONGALT))
         {
         continue;
         }

      if ((latitude   == last_latitude)  && 
          (longitude  == last_longitude) && 
          (altitude_m == last_altitude_m))
         {
         continue;
         }

      fprintf(KML, "%lf,%lf,%lf", longitude, latitude, altitude_m);

      if (i != n_readable_records-1)
         {
         fprintf(KML," ");
         }
      }

   //
   // Close KML file
   //

   fprintf(KML,"\
			</coordinates>\n\
		</LineString>\n\
	</Placemark>\n\
</Document>\n\
</kml>\n\
");

   fclose(KML);
}

//****************************************************************************
//
// Window message receiver procedure for application
//
//****************************************************************************

long FAR PASCAL WindowProc(HWND   hWnd,   UINT   message,   //)
                           WPARAM wParam, LPARAM lParam)
{
   switch (message)
      {
      case WM_PAINT:
         {
         if (SAL_window_status() != SAL_FULLSCREEN)
            {
            SAL_flip_surface();
            }

         break;
         }

      case WM_COMMAND:

         switch (wParam)
            {
            case IDM_PRINT:
select_print:
               PrintBackBufferToDC();
               break;

            case IDM_LAUNCH_MAPS:
               {
               launch_maps(GNSS->latitude, GNSS->longitude);
               break;
               }

            case IDM_EXPORT_KML:
               {
export_KML:
               if (!currently_open_filename[0])
                  {
                  SAL_alert_box("Error","No data has been loaded or acquired");
                  break;
                  }

               GetFileName("KML");

               if (!get_KML_filename(FileName))
                  {
                  break;
                  }

               CMD_save_KML(FileName);
               break;
               }

            case IDM_EXPORT_CSV:
            case IDM_EXPORT_CSVLF:
               {
export_CSV:
               if (!currently_open_filename[0])
                  {
                  SAL_alert_box("Error","No data has been loaded or acquired");
                  break;
                  }

               GetFileName("CSV");

               if (!get_CSV_filename(FileName))
                  {
                  break;
                  }

               FILE *outfile = fopen(FileName,"wb");

               if (outfile == NULL)
                  {
                  SAL_alert_box("Error","Export failed (file write-protected or disk full?)");
                  break;
                  }

               DOUBLE x_scale;      // (this will be valid even if cursor is above or below X/Y display, e.g. in rising-raster display)
               SINGLE xy_amplitude; // (this will be valid only if cursor is within X/Y display, i.e. if xy->query_graph returns 1)
               SINGLE y_scale;      // (this will be valid only if cursor is within X/Y display, i.e. if xy->query_graph returns 1) 

               for (S32 x=0; x < XY_PANE_WIDTH; x++)
                  {
                  if (xy->query_graph(x + xy_pane->x0,
                                      xy_pane->y0,
                                      XY_DISPLAYED_ARRAY,
                                     &xy_amplitude,
                                     &y_scale,
                                     &x_scale))
                     {
                     fprintf(outfile,"%lf,%f",x_scale * 1E6, xy_amplitude); // freq in Hz, amplitude in dBm

                     if (x != XY_PANE_WIDTH-1)
                        {
                        if (wParam == IDM_EXPORT_CSVLF)
                           fprintf(outfile,"\n");
                        else
                           fprintf(outfile,",");
                        }
                     }
                  }

               fprintf(outfile,"\n");
               fclose(outfile);

               break;
               }

            case IDM_SCREENSHOT:
               {
select_screenshot:
               //
               // Pass default filename to save-file dialog
               //

               GetFileName(DEFAULT_IMAGE_FILE_SUFFIX);

               if (!get_screenshot_filename(FileName))
                  {
                  break;
                  }

               C8 *suffix = strrchr(FileName,'.');
               assert(suffix);

               stage_pane->y0 += (GetSystemMetrics(SM_CYMENU) - 8);

               if (!_stricmp(suffix,".tga"))
                  {
                  TGA_write_16bpp(stage_pane, FileName);
                  }
               else if (!_stricmp(suffix,".gif"))
                  {
                  GIF_write_16bpp(stage_pane, FileName);
                  }
               else if (!_stricmp(suffix,".bmp"))
                  {
                  BMP_write_16bpp(stage_pane, FileName);
                  }
               else if (!_stricmp(suffix,".pcx"))
                  {
                  PCX_write_16bpp(stage_pane, FileName);
                  }

               stage_pane->y0 -= (GetSystemMetrics(SM_CYMENU) - 8);

               break;
               }

            case IDM_SAVE:
               {
select_save:
               if (!currently_open_filename[0])
                  {
                  SAL_alert_box("Error","No data has been loaded or acquired");
                  break;
                  }

               S32 was_playing = (operation == PLAYBACK);

               if (!was_playing)
                  {
                  select_operation(STOP);
                  }

               //
               // Pass default filename to save-file dialog
               //

               GetFileName("SSM");

               if (!get_save_filename(FileName))
                  {
                  break;
                  }

               //
               // Copy current file to new filename, ignoring errors that can occur
               // if files with similar names (e.g., \test.ssm, c:\test.ssm) slip past the
               // stricmp() check
               //

               HCURSOR norm = SetCursor(waitcur);

               if (_stricmp(currently_open_filename, 
                            FileName))
                  {
                  if (CopyFile(currently_open_filename,
                               FileName,
                               FALSE)) 
                     {
                     strcpy(currently_open_filename, FileName);
                     }
                  else
                     {
                     if (GetLastError() != ERROR_SHARING_VIOLATION)
                        {
                        show_last_error("CopyFile");
                        }
                     }
                  }

               if (!update_physical_file(currently_open_filename,
                                         SOURCE_top_cursor, 
                                         SOURCE_bottom_cursor,
                                         N_RISE_COLORS,
                                         RISE_COLORS))
                  {
                  show_last_error("Unable to update file");
                  }

               SetCursor(norm);

               //
               // Enter playback mode with new filename
               //

               if (was_playing)
                  {
                  operation = PLAYBACK;
                  }
               else
                  {
                  select_operation(PLAYBACK);
                  }
               break;
               }

            case IDM_PLAYBACK:
               {
               //
               // Get name of file to open
               // File must exist
               // 
               // Default filename will be empty, or name of last file recorded
               //
select_playback:
               if (!get_open_filename(FileName))
                  {
                  break;
                  }

               strcpy(currently_open_filename, FileName);

               select_operation(PLAYBACK);
               break;
               }

            case IDM_SAVE_PAL:
               {
               if (colors_path[0])
                  {
                  FILE *colors = fopen(colors_path,"wb");
                  fwrite(RISE_COLORS, sizeof(RISE_COLORS), 1, colors);
                  fclose(colors);
                  }
               break;
               }

            case IDM_LOAD_PAL:
               {
               load_default_palette();
               break;
               }

            case IDM_QUIT:       done = 1;           break;
            case IDM_4:          set_n_points(4);    break;
            case IDM_16:         set_n_points(16);   break; 
            case IDM_32:         set_n_points(32);   break; 
            case IDM_64:         set_n_points(64);   break; 
            case IDM_128:        set_n_points(128);  break; 
            case IDM_320:        set_n_points(320);  break; 
            case IDM_640:        set_n_points(640);  break; 
            case IDM_CONTINUOUS: style = XY_CONNECTED_LINES;                               graph_setup(); break;
            case IDM_DOTS:       style = XY_DOTS;                                          graph_setup(); break;
            case IDM_BARS:       style = XY_BARS;                                          graph_setup(); break;
            case IDM_SPLINE:     resamp_op = RT_SPLINE;                                    graph_setup(); break;
            case IDM_PTSAMPLE:   resamp_op = RT_POINT;                                     graph_setup(); break;
            case IDM_MIN:        resamp_op = RT_MIN;                                       graph_setup(); break;
            case IDM_MAX:        resamp_op = RT_MAX;                                       graph_setup(); break;
            case IDM_AVG:        resamp_op = RT_AVG;                                       graph_setup(); break;
            case IDM_AMIN:       accum_op ^= XY_MINIMUM;                                   graph_setup(); break;
            case IDM_AMAX:       accum_op ^= XY_MAXIMUM;                                   graph_setup(); break;
            case IDM_AAVG:       accum_op ^= XY_AVERAGE;                                   graph_setup(); break;
            case IDM_MAXHOLD:    max_hold = !max_hold; xy->clear_max_hold();               graph_setup(); break;
            case IDM_LERP:       lerp_colors = !lerp_colors;                               graph_setup(); break;
            case IDM_HI_SPEED:   SOURCE_state->hi_speed_acq = !SOURCE_state->hi_speed_acq; graph_setup(); break;
            case IDM_ABOUT:      SAL_alert_box("About SSM.EXE",
                                               "Spectrum surveillance monitor V"VERSION"\n\nAuthor: John Miles, KE5FX (john@miles.io)");
                                               break;

            case IDM_856XA:      
               {
               SOURCE_state->HP856XA_mode   = !SOURCE_state->HP856XA_mode;                     
               SOURCE_state->HP8569B_mode   = FALSE;
               SOURCE_state->HP3585_mode    = FALSE;
               SOURCE_state->HP358XA_mode   = FALSE;
               SOURCE_state->SCPI_mode      = FALSE;
               SOURCE_state->SA44_mode      = FALSE;
               SOURCE_state->Advantest_mode = FALSE;
               SOURCE_state->R3261_mode     = FALSE;
               graph_setup(); 
               break;
               }

            case IDM_8569B:      
               {
               SOURCE_state->HP8569B_mode   = !SOURCE_state->HP8569B_mode;                     
               SOURCE_state->HP3585_mode    = FALSE;
               SOURCE_state->HP856XA_mode   = FALSE;
               SOURCE_state->HP358XA_mode   = FALSE;
               SOURCE_state->SCPI_mode      = FALSE;
               SOURCE_state->SA44_mode      = FALSE;
               SOURCE_state->Advantest_mode = FALSE;
               SOURCE_state->R3261_mode     = FALSE;
               graph_setup(); 
               break;
               }

            case IDM_358XA:      
               {
               SOURCE_state->HP358XA_mode   = !SOURCE_state->HP358XA_mode;
               SOURCE_state->HP3585_mode    = FALSE;
               SOURCE_state->HP8569B_mode   = FALSE;
               SOURCE_state->HP856XA_mode   = FALSE;
               SOURCE_state->SCPI_mode      = FALSE;
               SOURCE_state->SA44_mode      = FALSE;
               SOURCE_state->Advantest_mode = FALSE;
               SOURCE_state->R3261_mode     = FALSE;
               graph_setup(); 
               break;
               }

            case IDM_3585:      
               {
               SOURCE_state->HP3585_mode    = !SOURCE_state->HP3585_mode;
               SOURCE_state->HP358XA_mode   = FALSE;
               SOURCE_state->HP8569B_mode   = FALSE;
               SOURCE_state->HP856XA_mode   = FALSE;
               SOURCE_state->SCPI_mode      = FALSE;
               SOURCE_state->SA44_mode      = FALSE;
               SOURCE_state->Advantest_mode = FALSE;
               SOURCE_state->R3261_mode     = FALSE;
               graph_setup(); 
               break;
               }

            case IDM_R3261:
               {
               SOURCE_state->R3261_mode     = !SOURCE_state->R3261_mode;
               SOURCE_state->HP3585_mode    = FALSE;
               SOURCE_state->HP358XA_mode   = FALSE;
               SOURCE_state->HP8569B_mode   = FALSE;
               SOURCE_state->HP856XA_mode   = FALSE;
               SOURCE_state->SCPI_mode      = FALSE;
               SOURCE_state->SA44_mode      = FALSE;
               SOURCE_state->Advantest_mode = FALSE;
               graph_setup(); 
               break;
               }

            case IDM_ADVANTEST:
               {
               SOURCE_state->Advantest_mode = !SOURCE_state->Advantest_mode;
               SOURCE_state->HP3585_mode    = FALSE;
               SOURCE_state->HP358XA_mode   = FALSE;
               SOURCE_state->HP8569B_mode   = FALSE;
               SOURCE_state->HP856XA_mode   = FALSE;
               SOURCE_state->SCPI_mode      = FALSE;
               SOURCE_state->SA44_mode      = FALSE;
               SOURCE_state->R3261_mode     = FALSE;
               graph_setup(); 
               break;
               }

            case IDM_SCPI:       
               {
               SOURCE_state->SCPI_mode      = !SOURCE_state->SCPI_mode;                           
               SOURCE_state->SA44_mode      = FALSE;
               SOURCE_state->HP856XA_mode   = FALSE;
               SOURCE_state->HP3585_mode    = FALSE;
               SOURCE_state->HP8569B_mode   = FALSE;
               SOURCE_state->HP358XA_mode   = FALSE;
               SOURCE_state->Advantest_mode = FALSE;
               SOURCE_state->R3261_mode     = FALSE;
               graph_setup(); 
               break;
               }

            case IDM_F5:         
               {
refresh:
               if (operation == PLAYBACK)
                  {
                  select_operation(PLAYBACK, -1, TRUE);
                  }
               break;
               }

            case IDM_USER_GUIDE:
               {
               launch_page("ssm.htm");
               break;
               }

            case IDM_GRATX:      grat_x = !grat_x;                     graph_setup(); break;
            case IDM_GRATY:      grat_y = !grat_y;                     graph_setup(); break;
            case IDM_WIFI:       show_wifi_band = !show_wifi_band;     graph_setup(); break;
            case IDM_GPS:        show_GPS_band  = !show_GPS_band;      graph_setup(); break;
            case IDM_SHOW_FPS:   show_fps = !show_fps;                 graph_setup(); break;
            case IDM_ANNOTATE:   show_annotation = !show_annotation;   graph_setup(); break;

            case IDM_THRESHOLDS:   apply_thresholds = !apply_thresholds; graph_setup(); break;
            case IDM_DELTA_MARKER: use_delta_marker = !use_delta_marker; graph_setup(); break;

            case IDM_OFFSETS: CMD_edit_offsets(); break;
            case IDM_MULTISEG: CMD_edit_sweep_range(); break;

            case IDM_STOP_START: goto stop_start;
            case IDM_PAUSE_RESUME:
               {
               paused = !paused;
               graph_setup();
               break;
               }

            case IDM_FS_TOGGLE: PostMessage(hwnd, WM_SYSKEYUP, VK_RETURN, 0); break;

            case IDM_HOUND:
               {
               if ((operation == STOP) || (operation == PLAYBACK))
                  {
                  //
                  // If not acquiring: for convenience, start acquiring if the option is 
                  // selected, even if it's already checked 
                  //

                  SOURCE_state->SA44_mode = TRUE;
                  }
               else
                  {
                  //
                  // If acquiring: if we unchecked the menu option, stop the acquisition
                  //

                  SOURCE_state->SA44_mode = !SOURCE_state->SA44_mode;

                  if (!SOURCE_state->SA44_mode)
                     {
                     select_operation(PLAYBACK);
                     }
                  }
                  
               SOURCE_state->SCPI_mode      = FALSE;
               SOURCE_state->HP856XA_mode   = FALSE;
               SOURCE_state->HP3585_mode    = FALSE;
               SOURCE_state->HP8569B_mode   = FALSE;
               SOURCE_state->HP358XA_mode   = FALSE;
               SOURCE_state->Advantest_mode = FALSE;
               SOURCE_state->R3261_mode     = FALSE;

               graph_setup();

               if (SOURCE_state->SA44_mode)
                  {
                  select_operation(ACQUIRE, -1);
                  }

               break;
               }

            default:
               {
               if (SOURCE_state->SA44_mode)
                  {
                  SOURCE_state->SA44_mode = FALSE;
                  graph_setup();
                  }

               for (S32 i=0; i <= 30; i++)
                  {
                  if ((S32) wParam == (IDM_PAUSE_RESUME+1+i))
                     {
                     select_operation(ACQUIRE, wParam - (IDM_PAUSE_RESUME + 1));
                     break;
                     }
                  }
               break;
               }

            }
         break;

      case WM_MOUSEWHEEL:
         {
         if (operation == PLAYBACK)
            {
            if (S32(wParam) < 0)
               {
               ++wheel_scroll;
               }
            else
               {
               --wheel_scroll;
               }
            }
         break;
         }

      case WM_KEYDOWN:

         if (GetKeyState(VK_CONTROL) & 0x8000)
            {
            switch (wParam)
               {
               case 'M':
                  {
                  launch_maps(GNSS->latitude, GNSS->longitude);
                  break;
                  }

               case 'K':
                  {
                  goto export_KML;
                  break;
                  }
               }
            }
         else
            {
            switch (wParam)
               {
               case VK_F1:

                  launch_page("ssm.htm");
                  break;

               case VK_F5:
                  goto refresh;

               case VK_SCROLL:
                  {
                  if (GetKeyState(VK_SCROLL) & 1)
                     paused = TRUE;
                  else
                     paused = FALSE;
                  break;
                  }
               }
            }
         break;

      case WM_CHAR:

         switch (wParam)
            {
            //
            // Shift-G/Shift-H: Turn CRT power supply/readout on/off
            //

            case 'G':
            case 'H':
               {
               if ((SOURCE_state->type == SATYPE_HP8566B_8568B) || 
                   (SOURCE_state->type == SATYPE_HP8566A_8568A))
                  {
                  SA_printf(wParam == 'G' ? "KSg" : "KSh");
                  }
               else if (SOURCE_state->type == SATYPE_TEK_490P)
                  {
                  SA_printf(wParam == 'G' ? "REDOUT OFF;" : "REDOUT ON;");
                  }
               else if (SOURCE_state->type == SATYPE_TEK_271X)
                  {
                  SA_printf(wParam == 'G' ? "REDOUT OFF;" : "REDOUT ON;");
                  }
               break;
               }

            //
            // ESC: Quit
            //

            case 27:

               done = 1;
               break;

            //
            // Space: Toggle connection
            //

            case 32:
               {
stop_start:
               paused = FALSE;

               if (autorefresh_interval > 0)
                  {
                  autorefresh_interval = 0;
                  break;
                  }

               if (operation == ACQUIRE)
                  {
                  select_operation(PLAYBACK);
                  }
               else
                  {
                  select_operation(ACQUIRE, SOURCE_address);
                  }

               break;
               }

            //
            // L: Playback
            //

            case 'l':
            case 'L':
               goto select_playback;

            //
            // S: Save
            //

            case 's':
               goto select_save;

            case 'S':
               goto select_screenshot;

            case 'e':
               wParam = IDM_EXPORT_CSVLF;
               goto export_CSV;

            //
            // P/p: Print
            //

            case 'P':
            case 'p':
               goto select_print;

            //
            // M: Toggle/clear max accumulation
            //

            case 'M':
               
               accum_op ^= XY_MAXIMUM;
               xy->reset_running_values();
               graph_setup();
               break;

            //
            // m: Toggle/clear max hold
            //

            case 'm':

               max_hold = !max_hold;
               xy->clear_max_hold();
               graph_setup();
               break;

            //
            // x/y: Toggle graticule
            //

            case 'x':
               grat_x = !grat_x;
               graph_setup();
               break;

            case 'y':
               grat_y = !grat_y;
               graph_setup();
               break;

            //
            // W: Toggle WiFi band boundaries
            //

            case 'w':
            case 'W':
               show_wifi_band = !show_wifi_band;     
               graph_setup(); 
               break; 

            //
            // g: Toggle GPS band boundaries
            //

            case 'g':
               show_GPS_band = !show_GPS_band;     
               graph_setup(); 
               break; 

            // 
            // R: Edit range
            //

            case 'r':
            case 'R':
               CMD_edit_sweep_range();
               break;

            //
            // O: Edit offsets
            //

            case 'o':
            case 'O':

               CMD_edit_offsets();
               break;

            //
            // A/Z: Move border between X/Y and rising pane
            //

            case 'a':
            case 'A':

               if (operation == PLAYBACK)
                  {
                  select_operation(STOP);
                  }

               if (operation != STOP)
                  {
                  SAL_alert_box("Invalid operation",
                     "Cannot change graph configuration while acquiring data\n\
Select Acquire->Stop/restart acquisition first\n");
                  break;
                  }

               if (xy_pane->y0 > 300)  // (Don't overwrite the dBm color key)
                  {
                  xy_pane->y0         -= 5;
                  xy_outer_pane->y0   -= 5;
                  rise_pane->y1       -= 5;
                  rise_outer_pane->y1 -= 5;

                  graph_setup(TRUE);
                  }

               break;

            case 'z':
            case 'Z':

               if (operation == PLAYBACK)
                  {
                  select_operation(STOP);
                  }

               if (operation != STOP)
                  {
                  SAL_alert_box("Invalid operation",
                     "Cannot change graph configuration while acquiring data\n\
Select Acquire->Stop/restart acquisition first\n");
                  break;
                  }

               if (xy_pane->y0 < (SCREEN_HEIGHT - 60))
                  {
                  xy_pane->y0         += 5;
                  xy_outer_pane->y0   += 5;
                  rise_pane->y1       += 5;
                  rise_outer_pane->y1 += 5;

                  graph_setup(TRUE);
                  }
               break;
            }
      }

   return DefWindowProc(hWnd, message, wParam, lParam);
}

//****************************************************************************
//
// Main app function
//
//****************************************************************************

void AppMain(LPSTR _lpCmdLine)
{
   C8 lpCmdLine[MAX_PATH];

   memset(lpCmdLine, 0, sizeof(lpCmdLine));
   strcpy(lpCmdLine, _lpCmdLine);

   //
   // Check for CLI options
   //

   strcpy(colors_filename, DEFAULT_PAL_FILENAME);

   SOURCE_address = -1;

   lerp_colors    = TRUE;
   n_data_points  = XY_PANE_WIDTH;
   
   S32 initial_mode = VFX_WINDOW_MODE;

   C8 *option = NULL;
   C8 *end    = NULL;
   C8 *beg    = NULL;
   C8 *ptr    = NULL;

   while (1)
      {
      //
      // Remove leading and trailing spaces from command line
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
      // -min,-max,-avg,-point,-spline: Request trace resampling 
      //

      option = strstr(lpCmdLine,"-min");

      if (option != NULL)
         {
         resamp_op = RT_MIN;
         memmove(option, &option[4], strlen(&option[4])+1);
         continue;
         }

      option = strstr(lpCmdLine,"-max");

      if (option != NULL)
         {
         resamp_op = RT_MAX;
         memmove(option, &option[4], strlen(&option[4])+1);
         continue;
         }

      option = strstr(lpCmdLine,"-avg");

      if (option != NULL)
         {
         resamp_op = RT_AVG;
         memmove(option, &option[4], strlen(&option[4])+1);
         continue;
         }

      option = strstr(lpCmdLine,"-point");

      if (option != NULL)
         {
         resamp_op = RT_POINT;
         memmove(option, &option[6], strlen(&option[6])+1);
         continue;
         }

      option = strstr(lpCmdLine,"-spline");

      if (option != NULL)
         {
         resamp_op = RT_SPLINE;
         memmove(option, &option[7], strlen(&option[7])+1);
         continue;
         }

      //
      // -n: Set # of data points
      //

      option = strstr(lpCmdLine,"-n:");

      if (option != NULL)
         {
         n_data_points = (S32) ascnum(&option[3], 10, &end);
         memmove(option, end, strlen(end)+1);
         continue;
         }

      //
      // -m: Enable max-hold mode in X/Y graph
      //

      option = strstr(lpCmdLine,"-m");

      if (option != NULL)
         {
         max_hold = TRUE;
         memmove(option, &option[2], strlen(&option[2])+1);
         continue;
         }

      //
      // -M: Enable max-accum mode in X/Y graph
      //

      option = strstr(lpCmdLine,"-M");

      if (option != NULL)
         {
         accum_op |= XY_MAXIMUM;
         memmove(option, &option[2], strlen(&option[2])+1);
         continue;
         }

      //
      // -g: Enable GPS band boundary display
      //

      option = strstr(lpCmdLine,"-g");

      if (option != NULL)
         {
         show_GPS_band = TRUE;
         memmove(option, &option[2], strlen(&option[2])+1);
         continue;
         }

      //
      // -w: Enable WLAN band boundary display
      //

      option = strstr(lpCmdLine,"-w");

      if (option != NULL)
         {
         show_wifi_band = TRUE;
         memmove(option, &option[2], strlen(&option[2])+1);
         continue;
         }

      //
      // -x/-y: Disable graticule x/y
      //

      option = strstr(lpCmdLine,"-x");

      if (option != NULL)
         {
         grat_x = FALSE;
         memmove(option, &option[2], strlen(&option[2])+1);
         continue;
         }

      option = strstr(lpCmdLine,"-y");

      if (option != NULL)
         {
         grat_y = FALSE;
         memmove(option, &option[2], strlen(&option[2])+1);
         continue;
         }

      //
      // -F: Initial mode is fullscreen
      //

      option = strstr(lpCmdLine,"-F");

      if (option != NULL)
         {
         initial_mode = VFX_FULLSCREEN_MODE;
         memmove(option, &option[2], strlen(&option[2])+1);
         continue;
         }

      //
      // -cs: Disable color interpolation (smoothing)
      //

      option = strstr(lpCmdLine,"-cs");

      if (option != NULL)
         {
         lerp_colors = FALSE;
         memmove(option, &option[3], strlen(&option[3])+1);
         continue;
         }

      //
      // -pal: Alternate palette filename
      //

      option = strstr(lpCmdLine,"-pal:\"");

      if (option != NULL)
         {
         beg = &option[6];
         end = beg;
         ptr = colors_filename;

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
      // -tail: Frame autorefresh interval
      //

      option = strstr(lpCmdLine,"-tail");

      if (option != NULL)
         {
         ptr = &option[6];

         autorefresh_interval = 0;

         while ((*ptr) && (*ptr != ' '))
            {
            autorefresh_interval = (autorefresh_interval * 10LL) + (*ptr++ - '0');
            }

         memmove(option, ptr, strlen(ptr)+1);
         continue;
         }

      //
      // Exit loop when no more CLI options are recognizable
      //

      break;
      }

   //
   // Pass command line to spectrum-analyzer access library
   //

   SA_parse_command_line(lpCmdLine);

   if (SOURCE_state->CTRL_freq_specified)   
      {
      if (SOURCE_state->CTRL_stop_Hz - SOURCE_state->CTRL_start_Hz < 0.001)
         {                         
         SAL_alert_box("Error","Zero span not supported -- must specify a valid nonzero span/div");
         return;
         }
      }

   //
   // Pass command line to GNSS access library and connect to receiver, if requested
   //

   GNSS_parse_command_line(GNSS, lpCmdLine);

   if (GNSS->type != GNSS_NONE)
      {
      if (!GNSS_connect(GNSS))
         {
         SAL_alert_box("Error","GNSS: %s", GNSS->error_text);
         exit(1);
         }
      }

   //
   // Allow app to keep running in windowed mode when alt-tabbed away
   //

   SAL_set_preference(SAL_SLEEP_WHILE_INACTIVE, 0);

   //
   // Inhibit page-flipping (scrolling video memory is extremely slow on
   // many machines)
   //

   SAL_set_preference(SAL_MAX_VIDEO_PAGES, 1);
   SAL_set_preference(SAL_USE_PAGE_FLIPPING, NO);

   //
   // Set display mode
   //

   screen_w = SCREEN_WIDTH;
   screen_h = SCREEN_HEIGHT;

   if (!VFX_set_display_mode(screen_w,
                             screen_h,
                             16,
                             initial_mode,
                             TRUE))
      {
      exit(1);
      }

   transparent_font_CLUT[0] = (U16) RGB_TRANSPARENT;

   //
   // Set path for session.ssm file
   // 

   GetCurrentDirectory(sizeof(session_bin_path),
                       session_bin_path);
   _strlwr(session_bin_path);

   if (strlen(session_bin_path)) 
      {
      if (session_bin_path[strlen(session_bin_path)-1] != '\\')
         {
         strcat(session_bin_path,"\\");
         }
      }

   strcat(session_bin_path,ACQ_FILENAME);

   //
   // Load color file, setting default colors if it's not available
   //

   GetModuleFileName(NULL, colors_path, sizeof(colors_path)-1);
   _strlwr(colors_path);

   C8 *exe = strstr(colors_path,"ssm.exe");

   if (exe != NULL)
      {
      sprintf(exe,colors_filename);
      }
   else
      {
      strcpy(colors_path,colors_filename);
      }

   load_default_palette();

   //
   // Create menu
   //

   main_menu_init();

   //
   // Create data recorder
   //

   recorder = new RECORDER;

   //
   // Configure window and clipping region
   //

   stage = VFX_window_construct(screen_w,screen_h);

   stage_pane  = VFX_pane_construct(stage,
                                    0,
                                    0,
                                    screen_w-1,
                                    screen_h-1);

   //
   // Leave mouse visible (requires a hardware cursor, otherwise will flicker)
   //

   SAL_show_system_mouse();

   //
   // Initialize pane list used for playback controls (ffwd, rewind, etc.)
   //

   init_playback_buttons();

   //
   // Create inner and outer panes for X/Y graph
   //
   // The X/Y outer pane is the entire bottom portion of the screen 
   // containing the X/Y graph and its surrounding legend
   //

   xy_pane = VFX_pane_construct(stage,
                                XY_PANE_LEFT,
                                XY_PANE_TOP,
                                (XY_PANE_WIDTH + XY_PANE_LEFT) - 1,
                                (XY_PANE_HEIGHT + XY_PANE_TOP) - 1);

   xy_outer_pane = VFX_pane_construct(stage,
                                      0,
                                      XY_PANE_TOP - 4,
                                      SCREEN_WIDTH-1,
                                      SCREEN_HEIGHT-1);

   //
   // Create inner and outer panes for rising-raster graph
   //
   // The rising-raster outer pane is the upper portion of the screen 
   // containing the rising-raster graph and its surrounding legend
   //

   rise_pane = VFX_pane_construct(stage,
                                  WATERFALL_PANE_LEFT,
                                  WATERFALL_PANE_TOP,
                                 (WATERFALL_PANE_WIDTH + WATERFALL_PANE_LEFT) - 1,
                                 (WATERFALL_PANE_HEIGHT + WATERFALL_PANE_TOP) - 1);

   rise_outer_pane = VFX_pane_construct(stage,
                                        0,
                                        WATERFALL_PANE_TOP,
                                        SCREEN_WIDTH-1,
                                        XY_PANE_TOP-1);
   //
   // Initial operation = stopped
   //

   operation = STOP;

   //
   // Create initial graph objects
   //

   graph_setup(TRUE);

   last_window_mode = SAL_window_status();

   done                = 0;
   mouse_left          = 0;
   menu_mode           = 0;

   S32 last_frame_time = 0;
   S32 start           = 0;

   if (lpCmdLine[0])
      {
      C8 *end = NULL;
      S32 n = (S32) ascnum(lpCmdLine, 10, &end);

      if (((n >= 0) && (n <= 30)) 
            &&
            (end > lpCmdLine) 
            &&
          ((*end == ' ') || (*end == 0)))
         {
         select_operation(ACQUIRE, n);
         }
      else
         {
         if ((strstr(lpCmdLine,"-sa44") != NULL) ||     // special case: no GPIB address needed for Signal Hound
             (strstr(lpCmdLine,"-SA44") != NULL))
            {
            select_operation(ACQUIRE, -1);
            }                            
         else
            {
            C8 *filename = lpCmdLine;

            if (filename[0] == '-')                     // This is enforced when a GNSS or specan-specific argument is present, since they can't
               {                                        // remove their args from our command line...
               SAL_alert_box("Error","First command-line option must be filename or instrument address");
               exit(1);
               }

            if (filename[0] == '\"')
               {
               ++filename;

               C8 *end_quote = strchr(filename,'\"');

               if (end_quote != NULL)
                  {
                  *end_quote = 0;
                  }
               }

            strcpy(currently_open_filename, filename);
            select_operation(PLAYBACK);
            }
         }
      }

   //
   // Main loop
   //

   S64 frame_num = 0;

   for (;;)
      {
      frame_num++;

      Sleep(5);     // Don't hog the whole CPU
      SAL_serve_message_queue();

      if (done)
         {
         break;
         }

      if ((autorefresh_interval > 0) && (frame_num > autorefresh_interval) && (!mouse_left))
         {
         select_operation(PLAYBACK, -1, TRUE);
         frame_num = 0;
         }

      update_mouse_vars();

      // ------------------------------------------------------------------
      // Get GPS fix
      // ------------------------------------------------------------------

      if (GNSS->C != NULL)
         {
         GNSS_update(GNSS);
         }
      else
         {
         GNSS->latitude   = INVALID_LATLONGALT; 
         GNSS->longitude  = INVALID_LATLONGALT; 
         GNSS->altitude_m = INVALID_LATLONGALT; 
         }

      // ------------------------------------------------------------------
      // In fullscreen DirectDraw mode, show the menu only if the user
      // reaches for it with the mouse
      //
      // At that point, we have to stop refreshing the screen as long as
      // the menu is visible.  If they click on the menu, it'll take the focus
      // away from SAL so it's no longer our problem... but as long as
      // we're in the main refresh loop, we have to skip the lock/unlock/flip cycle
      // ------------------------------------------------------------------

      bool fs = (SAL_window_status() == SAL_FULLSCREEN);

      if ((fs) && (mouse_over_menu))
         {
         if (!menu_mode)
            {
            menu_mode = 1;

            DrawMenuBar(hwnd);
            SAL_FlipToGDISurface();
            }
         }
      else
         {
         menu_mode = 0;
         }

      if (menu_mode)
         {
         continue;
         }

      // ------------------------------------------------------------------
      // Lock the buffer and log current frame time for FPS calculation
      // ------------------------------------------------------------------

      assert(!stage_locked_for_writing);
      VFX_lock_window_surface(stage, VFX_BACK_SURFACE);
      stage_locked_for_writing = TRUE;

      last_frame_time = start;
      start           = timeGetTime();

      // ------------------------------------------------------------------
      // If video memory was wiped due to mode change, wipe rising-raster 
      // display and its internal buffers to clear the time scale, then
      // reinitialize playback state (if applicable)
      // ------------------------------------------------------------------

      if (last_window_mode != SAL_window_status())
         {
         rise->wipe_graph(RGB_TRIPLET(0,0,0));
         last_window_mode = SAL_window_status();

         if (operation == PLAYBACK)
            {
            select_operation(PLAYBACK, -1, TRUE);
            }
         }

      // ------------------------------------------------------------------
      // Wipe text status area
      // ------------------------------------------------------------------

      VFX_rectangle_fill(stage_pane, 
                         0, 22, 
                         SCREEN_WIDTH-1, 30, 
                         LD_DRAW, 
                         RGB_TRIPLET(0,0,0));

      // ------------------------------------------------------------------
      // If we're monitoring in real time, get this record's 
      // amplitude data, and the time at which it was acquired, and render
      // it to the rising-raster display
      //
      // (If we're playing back a recorded file, the rising-raster display
      // is updated when we call process_playback_button() below)
      // ------------------------------------------------------------------

      S32 clicking = mouse_left;

      if ((operation == ACQUIRE) && (!clicking))
         {
         //
         // Get timestamped data
         //

         acquisition_time = SOURCE_fetch_data(data);

         if ((operation == ACQUIRE) && (!paused)) // (i.e., if not aborted by hardware layer and not paused with Scroll Lock key)
            {
            //
            // Record it to tempfile
            //

            recorder->set_arrayed_points (data);

            C8 caption[128] = { 0 };

            if (!recorder->write_record(acquisition_time, 
                                        GNSS->latitude, 
                                        GNSS->longitude, 
                                        GNSS->altitude_m,
                                        INVALID_FREQ,       // INVALID_FREQ means the row inherits the file's frequency span
                                        INVALID_FREQ,
                                        caption))
               {
               SAL_alert_box("Error","Unable to write to %s -- disk full?",session_bin_path);
               exit(1);
               }

            //
            // Send data to X/Y graph for processing
            //

            xy->set_arrayed_points(data);

            //
            // Allow X/Y graph class to process and combine data as necessary
            //

            xy->process_input_data();

            //
            // Add contents of most-recently-plotted X/Y graph to rising-raster
            // graph
            //

            rise->scroll_graph(-1);

            XY_DATAREQ array = apply_thresholds ? XY_THRESHOLD_ARRAY : XY_COMBINED_ARRAY;

            rise->draw_graph_row(acquisition_time,                       
                                 xy->get_graph_data(array),                // get data
                                 rise->n_visible_rows() - 1,               // always add at bottom row
                                 lerp_colors,
                                 GNSS->latitude,
                                 GNSS->longitude,
                                 GNSS->altitude_m,
                                 INVALID_FREQ,
                                 INVALID_FREQ,
                                 caption);

            if (SOURCE_state->type == SATYPE_INVALID)
               {
               if (SOURCE_rows_acquired >= MAX_SIM_ROWS)
                  {
                  select_operation(STOP);
                  }
               }
            else
               {
               S32 row_bytes_written = SOURCE_rows_acquired * recorder->bytes_per_record(n_data_points);

               if (row_bytes_written >= MAX_LIVE_SIZE)      // (Does not include small .SSM file header)
                  {
                  select_operation(STOP);
                  }
               }
            }
         }

      // ------------------------------------------------------------------
      // Draw rising-raster scale and labels, and hit-test the labels
      // ------------------------------------------------------------------

      S32 palette_hit = rise->draw_scale(mouse_x, mouse_y);

      static S32 initial_palette_hit = -1;

      if (((!mouse_left) && (!last_mouse_left)) || (palette_hit == -1))
         {
         initial_palette_hit = -1;
         }

      if (palette_hit != -1)
         {
         bool pick_new_color  = FALSE;
         bool create_gradient = FALSE;

         if (mouse_left && (initial_palette_hit == -1)) 
            {
            initial_palette_hit = palette_hit;
            }

         if ((initial_palette_hit != -1) && (last_mouse_left) && (!mouse_left))
            {
            if (palette_hit == initial_palette_hit)
               {
               pick_new_color = TRUE;
               }
            else
               {
               create_gradient = TRUE;
               }
            }

         if (pick_new_color)
            {
            if (launch_color_picker(&RISE_COLORS[palette_hit]))
               {
               RISE_COLORS[palette_hit] = *picked_color();
               }
            }

         if (create_gradient)
            {
            SINGLE ar = RISE_COLORS[initial_palette_hit].r;
            SINGLE ag = RISE_COLORS[initial_palette_hit].g;
            SINGLE ab = RISE_COLORS[initial_palette_hit].b;
            SINGLE br = RISE_COLORS[palette_hit].r;
            SINGLE bg = RISE_COLORS[palette_hit].g;
            SINGLE bb = RISE_COLORS[palette_hit].b;

            S32    d  = palette_hit - initial_palette_hit;
            SINGLE dr = (br-ar) / d;
            SINGLE dg = (bg-ag) / d;
            SINGLE db = (bb-ab) / d;

            if (d > 0)
               {
               for (S32 i=initial_palette_hit; i <= palette_hit; i++)
                  {
                  RISE_COLORS[i].r = U8(ar+0.5F);
                  RISE_COLORS[i].g = U8(ag+0.5F);
                  RISE_COLORS[i].b = U8(ab+0.5F);

                  ar += dr;
                  ag += dg;
                  ab += db;
                  }
               }
            else
               {
               for (S32 i=palette_hit; i <= initial_palette_hit; i++)
                  {
                  RISE_COLORS[i].r = U8(br+0.5F);
                  RISE_COLORS[i].g = U8(bg+0.5F);
                  RISE_COLORS[i].b = U8(bb+0.5F);

                  br += dr;
                  bg += dg;
                  bb += db;
                  }
               }
            }

         if (create_gradient || pick_new_color)
            {
            rise->set_graph_colors(RISE_COLORS,
                                   RISE_UNDERRANGE_COLOR,
                                   RISE_OVERRANGE_COLOR,
                                   RISE_BLANK_COLOR);
            }
         }

      // ------------------------------------------------------------------
      // Draw X/Y graph reflecting last row sent to rising-raster
      // display, and update the cursors 
      // ------------------------------------------------------------------

      VFX_pane_wipe(xy_outer_pane, RGB_TRIPLET(0,0,0));

      xy->draw_background();

      xy->draw_scale(grat_x, 
                     grat_y);

      if (show_wifi_band) xy->draw_band_markers(&WiFi);
      if (show_GPS_band)  xy->draw_band_markers(&GPS);

      xy->draw_graph();

      if (app_active)
         {
         xy->update_cursors(mouse_x,
                            mouse_y,
                            mouse_left);
         }

      xy->draw_cursors();

      SOURCE_top_cursor    = xy->top_cursor_val();
      SOURCE_bottom_cursor = xy->bottom_cursor_val();

      display_playback_buttons();

      if (show_annotation && annotation[0])
         {
         transparent_font_CLUT[1] = (U16) RGB_NATIVE(255,255,255);

         S32 sw = string_width(annotation, 
                                 VFX_default_system_font());

         VFX_string_draw(xy_pane,
                         ((xy_pane->x1 - xy_pane->x0) - sw) / 2,
                          (xy_pane->y1 - xy_pane->y0) / 8,
                           VFX_default_system_font(),
                           annotation,
                           VFX_default_font_color_table(VFC_WHITE_ON_XP));
         }

      // ------------------------------------------------------------------
      // Show data source and frame rate, if requested
      // ------------------------------------------------------------------

      char string[256] = { 0 };
      char location_string[256] = { 0 };
      char caption_string[128] = { 0 };

      if (last_frame_time != 0)
         {
         if (show_fps)
            {
            transparent_font_CLUT[1] = (U16) RGB_NATIVE(255,255,255);
            sprintf(string,"FPS: %3.1f",1000.0F / ((start - last_frame_time) + 1));

            VFX_string_draw(stage_pane,
                            0,
                            22,
                            VFX_default_system_font(),
                            string,
                            VFX_default_font_color_table(VFC_WHITE_ON_BLACK));
            }
         }

      if (SOURCE_state->type != SATYPE_INVALID)
         {
         if (!paused)
            {
            transparent_font_CLUT[1] = (U16) RGB_NATIVE(0,255,0);
            sprintf(string,"Connected to %s",SOURCE_ID_string);
            }
         else
            {
            transparent_font_CLUT[1] = (U16) RGB_NATIVE(255,255,0);
            sprintf(string,"%s (Paused)",SOURCE_ID_string);
            }
         }
      else
         {
         if (operation == PLAYBACK)
            {
            transparent_font_CLUT[1] = (U16) RGB_NATIVE(255,255,0);
            strcpy(string,currently_open_filename);
            }
         else
            {
            transparent_font_CLUT[1] = (U16) RGB_NATIVE(255,0,0);
            sprintf(string,(operation == ACQUIRE) ? "Test mode" : "Offline");
            }
         }

      VFX_string_draw(stage_pane,
                      64,
                      22,
                      VFX_default_system_font(),
                      string,
                      transparent_font_CLUT);

      // ------------------------------------------------------------------
      // Display statistics for X/Y graph point or rising-raster point under 
      // mouse cursor, if any
      // ------------------------------------------------------------------

      string[0] = 0;
      location_string[0] = 0;
      caption_string[0] = 0;

      if (acquisition_time > 0)
         {
         DOUBLE x_scale;      // (this will be valid even if cursor is above or below X/Y display, e.g. in rising-raster display)
         SINGLE xy_amplitude; // (this will be valid only if cursor is within X/Y display, i.e. if xy->query_graph returns 1)
         SINGLE y_scale;      // (this will be valid only if cursor is within X/Y display, i.e. if xy->query_graph returns 1) 

         if (xy->query_graph(mouse_x,
                             mouse_y,
                             XY_COMBINED_ARRAY,
                            &xy_amplitude,
                            &y_scale,
                            &x_scale))
            {
            sprintf(string,"%s: %3.1f dBm @ %5.3lf MHz",
               ctime(&acquisition_time),
               y_scale,
               x_scale);

            strcpy(string,sanitize_VFX(string));
            }

         DOUBLE start_Hz = INVALID_FREQ;
         DOUBLE stop_Hz  = INVALID_FREQ;

         SINGLE  rise_amplitude;       // amplitude of spot under mouse X,Y
         time_t  rise_acq_time;        // time at which row under mouse Y was acquired
         SINGLE *row_record;           // raw data stored for this row

         C8 caption[128] = { 0 };  

         if (rise->query_graph(mouse_x,
                               mouse_y,
                              &rise_acq_time,
                              &rise_amplitude,
                               NULL,
                              &row_record,
                              &GNSS->latitude,
                              &GNSS->longitude,
                              &GNSS->altitude_m,
                              &start_Hz,
                              &stop_Hz,
                              caption,
                              sizeof(caption)))
            {
            if ((start_Hz != INVALID_FREQ) && (stop_Hz != INVALID_FREQ))
               {
               DOUBLE max_MHz = stop_Hz / 1000000.0;
               DOUBLE min_MHz = start_Hz / 1000000.0;

               DOUBLE span = max_MHz - min_MHz;

               xy->set_X_range(min_MHz,
                               max_MHz,
                               "%.3lf",
                               span / 4.0,
                               span / 40.0);

               xy->set_X_cursors(min_MHz + (span / 10.0),
                                 max_MHz - (span / 10.0));

               xy->query_graph(mouse_x,
                               mouse_y,
                               XY_COMBINED_ARRAY,
                              &xy_amplitude,
                              &y_scale,
                              &x_scale);
               }

            if ((GNSS->latitude != INVALID_LATLONGALT) && (GNSS->longitude != INVALID_LATLONGALT))
               {
               sprintf(location_string,"%lf%c, %lf%c  ",
                  abs(GNSS->longitude),
                  GNSS->longitude < 0.0 ? 'W' : 'E',
                  abs(GNSS->latitude),
                  GNSS->latitude < 0.0 ? 'S' : 'N');
               }

            if (caption[0])
               {
               memset(caption_string, 0, sizeof(caption_string));
               _snprintf(caption_string, sizeof(caption_string)-1, "%s          ", caption);
               }

            if (clicking && (n_data_points == XY_PANE_WIDTH))
               {
               acquisition_time = rise_acq_time;
               xy->set_arrayed_points(row_record);
               xy->process_input_data();
               }

            sprintf(string,"%s: %3.1f dBm @ %5.3lf MHz",
               ctime(&rise_acq_time),
               rise_amplitude,
               x_scale);

            strcpy(string,sanitize_VFX(string));
            }
         }

      VFX_string_draw(stage_pane,
                      470,
                      22,
                      VFX_default_system_font(),
                      string,
                      VFX_default_font_color_table(VFC_WHITE_ON_BLACK));

      if (location_string[0])
         {
         VFX_string_draw(stage_pane,
                         300,
                         22,
                         VFX_default_system_font(),
                         location_string,
                         VFX_default_font_color_table(VFC_WHITE_ON_BLACK));
         }

      if (caption_string[0])
         {
         VFX_string_draw(stage_pane,
                         150,
                         540,
                         VFX_default_system_font(),
                         caption_string,
                         VFX_default_font_color_table(VFC_WHITE_ON_BLACK));
         }

      if (operation == PLAYBACK)
         {
         while (wheel_scroll > 0) 
            { 
            process_playback_button(BTN_FFWD); 
            wheel_scroll--; 
            }

         while (wheel_scroll < 0)
            {
            process_playback_button(BTN_FRWD); 
            wheel_scroll++; 
            }

         S32 selection = selected_playback_button;

         if (app_active && ((!mouse_left) || (selection == -1)))
            {
                 if (GetKeyState(VK_UP)    & 0x8000) selection = BTN_RWD;
            else if (GetKeyState(VK_DOWN)  & 0x8000) selection = BTN_FWD;
            else if (GetKeyState(VK_PRIOR) & 0x8000) selection = BTN_FRWD;
            else if (GetKeyState(VK_NEXT)  & 0x8000) selection = BTN_FFWD;
            else if (GetKeyState(VK_HOME)  & 0x8000) selection = BTN_HOME;
            else if (GetKeyState(VK_END)   & 0x8000) selection = BTN_END;
            else                                     selection = -1;
            }

         if (selection != -1)
            {
            process_playback_button(selection);
            }
         }

      // ------------------------------------------------------------------
      // Release lock and do page flip
      // ------------------------------------------------------------------

      assert(stage_locked_for_writing);
      VFX_unlock_window_surface(stage, TRUE);
      stage_locked_for_writing = FALSE;
      }
}

//****************************************************************************
//
// Windows main() function
//
//****************************************************************************

int PASCAL WinMain(HINSTANCE hInst, //)
                   HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine,     
                   int nCmdShow)
{
   //
   // Initialize system abstraction layer -- must succeed in order to continue
   //

   printf("[%s]\n", lpCmdLine);

   hInstance = hInst;

   SAL_set_preference(SAL_ALLOW_WINDOW_RESIZE, NO);
   SAL_set_preference(SAL_USE_PARAMON, NO);     // Leave parallel ports alone -- we may be using
                                                // them to control the DUT!
   if (!SAL_startup(hInstance,
                    szAppName,
                    TRUE,              // Allow more than one instance to run at a time
                    WinExit))
      {
      return 0;
      }

   waitcur = LoadCursor(0,IDC_WAIT);

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
   // Create application window
   // 

   SAL_set_application_icon((C8 *) IDI_ICON);

   hwnd = SAL_create_main_window();

   if (hwnd == NULL)
      {
      SAL_shutdown();
      return 0;
      }

   //
   // Register window procedure
   // 

   SAL_register_WNDPROC(WindowProc);

   //
   // Initialize GNSS services
   //

   GNSS = GNSS_startup();

   //
   // Initialize spectrum analyzer access library and record the
   // address of its state structure
   //

   SOURCE_state = SA_startup();

   //
   // Register exit handler 
   //

   atexit(AppExit);

   //
   // Call AppMain() function (does not return)
   //

   AppMain(lpCmdLine);

   return 0;
}
