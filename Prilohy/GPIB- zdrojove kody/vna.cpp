//############################################################################
//##                                                                        ##
//##  VNA.CPP                                                               ##
//##                                                                        ##
//##  HP VNA utilities                                                      ##
//##                                                                        ##
//##  V0.90 of 23-Oct-12: Initial                                           ##
//##                                                                        ##
//##  john@miles.io                                                         ##
//##                                                                        ##
//############################################################################

#define WINCON

#include <stdio.h>
#include <stdlib.h>
#include <dos.h>
#include <string.h>
#include <conio.h>
#include <ctype.h>
#include <assert.h>
#include <time.h>
#include <math.h>
#include <float.h>
#include <malloc.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>
#include <mmsystem.h>
#include <windowsx.h>
#include <setupapi.h>
#include <commdlg.h>

#include "vnares.h"
#include "typedefs.h"

#include "gpiblib.h"
#include "stdtpl.h"

#include "spline.cpp"
#include "sparams.cpp"

extern "C" 
{
__declspec(dllimport) HWND WINAPI RealChildWindowFromPoint(HWND hwndParent, POINT ptParentClientCoords);
}

#define FTOI(x) (S32((x)+0.5))

//
// Configuration
//

#define VERSION              "1.63"
#define TIMER_SERVICE_MS      100
#define HOVER_HELP_MS         500

//
// Misc globals
//

char szAppName[]="VNAUtilities";

HWND    hwnd;
HCURSOR hourglass;
HWND    hProgress;
HWND    hDCEntry;
HWND    hFiletype;
HWND    hQuery;

S32 waiting = 0;

C8 instrument_name[512];

/*********************************************************************/
//
// Utility functions
//
/*********************************************************************/

bool serve(void)
{
   MSG msg;

   while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
      {
      if (msg.message == WM_QUIT)
         {
         return FALSE;
         }

      if (msg.message == WM_KEYDOWN)
         {
         switch (msg.wParam)
            {
            case 27:
               {
               PostQuitMessage(0);
               break;
               }
            }
         }

      if (!IsDialogMessage(hwnd,&msg))
         {
         TranslateMessage(&msg);
         DispatchMessage(&msg);
         }
      }

   return TRUE;
}

HCURSOR wait(bool show_progress_bar)
{
   ++waiting;

   EnableWindow(hwnd, FALSE);

   if (show_progress_bar)
      {
      ShowWindow(hProgress, TRUE);
      }

   HCURSOR result = SetCursor(hourglass);

   UpdateWindow(hwnd);

   return result;
}

void end_wait(HCURSOR prev)
{
   --waiting;

   EnableWindow(hwnd, TRUE);

   ShowWindow(hProgress, FALSE);

   SendMessage(hProgress, PBM_SETPOS, 0, 0);

   SetCursor(prev);

   UpdateWindow(hwnd);
}

bool is_Prologix(void)
{
   GPIB_CTYPE type = GPIB_connection_type();

   return (type == GC_PROLOGIX_SERIAL) || (type == GC_PROLOGIX_LAN);
}

bool is_8753(void)
{
   return (strstr(instrument_name,"8753") != NULL) || 
          (strstr(instrument_name,"8702") != NULL) ||
          (strstr(instrument_name,"8703") != NULL);
}

bool is_8752(void)
{
   return (strstr(instrument_name,"8752") != NULL);
}

bool is_8720(void)
{
   return (strstr(instrument_name,"8719") != NULL) || 
          (strstr(instrument_name,"8720") != NULL) ||
          (strstr(instrument_name,"8722") != NULL);
}

bool is_8510(void)
{
   return (strstr(instrument_name,"8510") != NULL);
}

bool is_8510C(void)
{
   return (strstr(instrument_name,"8510C") != NULL);
}

void instrument_setup(S32 SCPI, bool debug_mode = TRUE)
{
   if (SCPI)
      {
      C8 *data = GPIB_query("*IDN?");
      _snprintf(instrument_name,sizeof(instrument_name)-1,"%s",data);
      }
   else
      {
      if (debug_mode)
         {
         GPIB_printf("DEBUON;");
         }

      C8 *data = GPIB_query("OUTPIDEN");
      _snprintf(instrument_name,sizeof(instrument_name)-1,"%s",data);
      }

   C8 *d = &instrument_name[strlen(instrument_name)-1];

   while ((d >= instrument_name) 
            && 
         ((*d == 10) || (*d == 13)))
      {
      *d = 0;
      }

   if (SCPI)
      GPIB_query("INIT:CONT 0;*OPC?");
   else
      GPIB_printf("HOLD;"); 
}

//
// Automatically generate tempfile pathname, optionally open the file, and ensure file is 
// closed/deleted when it goes out of scope
//
// Command-line utilities should use TEMPFNs or TEMPFILEs instead of calling GetTempFileName() directly, to ensure
// they're well-behaved in a (possibly concurrent) batch job.  Just declare a TEMPFILE with an 
// optional suffix param for the generated filename, and access the filename with .name.
//
// To take advantage of automatic cleanup, don't call exit() while TEMPFNs are on the stack!
//

void __cdecl alert_box(C8 *caption, C8 *fmt, ...);

enum TFMSGLVL
{
   TF_DEBUG = 0,  // Debugging traffic
   TF_VERBOSE,    // Low-level notice
   TF_NOTICE,     // Standard notice
   TF_WARNING,    // Warning
   TF_ERROR       // Error
};

struct TEMPFN
{
   C8   path_buffer        [MAX_PATH-32];   // leave some room for 8.3 tempname and user-supplied suffix if any
   C8   original_temp_name [MAX_PATH-32];   // e.g., c:\temp\TF43AE.tmp
   C8   name               [MAX_PATH];      // e.g., c:\temp\TF43AE.tmp.wav (if .wav was passed as suffix)
   bool keep;
   bool show;
   bool active;

   virtual void message_sink(TFMSGLVL level,   
                             C8      *text)
      {
      if (level >= TF_ERROR)
         alert_box("Error","%s", text);
      else  
         ::printf("%s\n", text);
      }

   virtual void message_printf(TFMSGLVL level,
                               C8 *fmt,
                               ...)
      {
      C8 buffer[4096] = { 0 };

      va_list ap;

      va_start(ap, 
               fmt);

      _vsnprintf(buffer,
                 sizeof(buffer)-1,
                 fmt,
                 ap);

      va_end(ap);

      //
      // Remove trailing whitespace
      //

      C8 *end = &buffer[strlen(buffer)-1];

      while (end > buffer)
         {
         if (!isspace((U8) *end)) 
            {
            break;
            }

         *end = 0;
         end--;
         }

      message_sink(level,
                   buffer);
      }

   TEMPFN(C8 *suffix = NULL, bool keep_files=FALSE)
      {
      keep = keep_files;
      active = TRUE;

      path_buffer[0]        = 0;
      original_temp_name[0] = 0;
      name[0]               = 0;

      if (!GetTempPath(sizeof(path_buffer)-1,
                       path_buffer))
         {
         message_printf(TF_ERROR, "TEMPFN: GetTempPath() failed, code 0x%X",GetLastError());
         active = FALSE;
         return;
         }

      if (!GetTempFileName(path_buffer,
                          "TF",
                           0,
                           original_temp_name))
         {
         message_printf(TF_ERROR, "TEMPFN: GetTempFileName() failed, code 0x%X",GetLastError());
         active = FALSE;
         return;
         }

      strcpy(name, original_temp_name);

      if (suffix == NULL)
         {
         //
         // No suffix was provided, caller will use the tempfile the OS gave us
         // 

         original_temp_name[0] = 0;       
         }
      else
         {
         //
         // A suffix was provided: the caller will create *another* tempfile whose name 
         // will begin with the guaranteed-unique name of the first one.  This file also
         // must be cleaned up
         //

         strncat(name, suffix, 15); 
         }

      message_printf(TF_VERBOSE, "TEMPFN: original_temp_name: %s",original_temp_name);
      message_printf(TF_VERBOSE, "TEMPFN: name: %s",name);
      }

   virtual ~TEMPFN()           
      {                  
      //
      // Ensure that both the original tempfile and the user-extended version (if any)  
      // are cleaned up at end of scope                                                 
      //

      if (!keep)
         {
         if (original_temp_name[0])
            {
            message_printf(TF_VERBOSE, "TEMPFN: Deleting %s",original_temp_name);
            _unlink(original_temp_name);
            original_temp_name[0] = 0;
            }

         if (name[0])
            {
            message_printf(TF_VERBOSE, "TEMPFN: Deleting %s",name);
            _unlink(name);
            name[0] = 0;
            }
         }
      }

   virtual bool status(void)
      {
      return active;
      }
};

struct TEMPFILE : public TEMPFN
{
   FILE *file;

   TEMPFILE(C8  *suffix         = NULL, 
            C8  *file_operation = NULL, 
            bool keep_files     = FALSE) : TEMPFN (suffix, keep_files)  
            
      {
      if (file_operation == NULL)
         {
         file = NULL;
         return;    
         }
      
      file = fopen(name, file_operation);

      if (file == NULL)
         {
         message_printf(TF_ERROR, "TEMPFILE: Couldn't open %s for %s",
            name,
            (tolower((U8) file_operation[0]) == 'r') ? "reading" : "writing");

         active = FALSE;
         return;
         }
      }

   virtual ~TEMPFILE()
      {
      close();
      }

   virtual void close(void)
      {
      if (file != NULL)
         {
         fclose(file);
         file = NULL;
         }
      }
};

DynArray<TEMPFN *> TF_list;

/*********************************************************************/
//
// Error diagnostics
//
/*********************************************************************/

void __cdecl alert_box(C8 *caption, C8 *fmt, ...)
{
   static char work_string[4096];

   if ((fmt == NULL) || (strlen(fmt) > sizeof(work_string)))
      {
      strcpy(work_string, "(String missing or too large)");
      }
   else
      {
      va_list ap;

      va_start(ap, 
               fmt);

      vsprintf(work_string, 
               fmt, 
               ap);

      va_end  (ap);
      }

   if (caption == NULL)
      {
      MessageBox(hwnd, 
                 work_string,
                "Error", 
                 MB_OK);
      }
   else
      {
      MessageBox(hwnd, 
                 work_string,
                 caption,
                 MB_OK);
      }
}

void __cdecl show_last_error(C8 *caption = NULL, ...)
{
   LPVOID lpMsgBuf = "Hosed";       // Default message for Win9x (FormatMessage() is NT-only)

   FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 
                 hwnd,
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

C8 *timestamp(void)
{
   static union
      {
      FILETIME ftime;
      S64      itime;
      }
   T;

   GetSystemTimeAsFileTime(&T.ftime);

   static C8 text[1024];

   sprintf(text,
          "%s %s",
           date_text(T.itime / 10),
           time_text(T.itime / 10));

   return text;
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
// Win32Exec: Create process and return immediately, without doing a
//            WaitForInputIdle() call like WinExec()                
//                                                                  
// Optionally, wait for process to terminate                        
//****************************************************************************

BOOL Win32Exec(LPSTR lpCmdLine, BOOL bWait)
{
   STARTUPINFO         StartInfo;
   PROCESS_INFORMATION ProcessInfo;
   BOOL                result;
   
   memset(&StartInfo,   0, sizeof(StartInfo));
   memset(&ProcessInfo, 0, sizeof(ProcessInfo));

   StartInfo.cb = sizeof(StartInfo);

   printf("Win32Exec(%s)\n",lpCmdLine);

   result = CreateProcess(NULL,         // Image name (if NULL, module name must be first whitespace-delimited token on lpCmdLine or ambiguous results may occur)
                          lpCmdLine,    // Command line
                          NULL,         // Process security
                          NULL,         // Thread security
                          FALSE,        // Do not inherit handles
                          0,            // Creation flags
                          NULL,         // Inherit parent environment
                          NULL,         // Keep current working directory
                         &StartInfo,    // Startup info structure
                         &ProcessInfo); // Process info structure

   if (bWait)
      {
      WaitForSingleObject(ProcessInfo.hProcess,
                          INFINITE);
      }

   return result;
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
                       SWP_NOSIZE | SWP_NOZORDER) ? true : false;
}

//****************************************************************************
//*                                                                          *
//* Standardized GetOpenFileName() wrapper                                   *
//*                                                                          *
//* Always centers the dialog with respect to the parent window, but         *
//* doesn't support auto-sizing unless a custom hook proc is provided        *
//*                                                                          *
//****************************************************************************

UINT_PTR CALLBACK AHOFNHookProc(HWND hdlg,      // handle to the dialog box window
                                UINT uiMsg,     // message identifier
                                WPARAM wParam,  // message parameter
                                LPARAM lParam)  // message parameter
{
   if (uiMsg == WM_NOTIFY)
      {
      LPNMHDR pnmh = (LPNMHDR) lParam;
      HWND hwndParent = GetParent(hdlg);

      if (pnmh->code == CDN_INITDONE)
         {
         CenterWindow(hwndParent, hdlg);                      
         }
      }

   return 0;
}

//****************************************************************************
//*                                                                          *
//* Get filename to save                                                     *
//*                                                                          *
//****************************************************************************

bool get_save_filename(HWND          hWnd,               // parent window
                       C8           *dest,               // where the filename will be written (MAX_PATH)
                       C8           *dialog_title,       // title for file dialog
                       C8           *filter)             // filter specifier
{
   OPENFILENAME fn;
   C8           fn_buff[MAX_PATH] = { 0 };

   memset(&fn, 0, sizeof(fn));
   fn.lStructSize       = sizeof(fn);
   fn.hwndOwner         = hWnd;
   fn.hInstance         = NULL;
   fn.lpstrFilter       = filter;
   fn.lpstrCustomFilter = NULL;
   fn.nMaxCustFilter    = 0;
   fn.nFilterIndex      = 1;
   fn.lpstrFile         = fn_buff;
   fn.nMaxFile          = sizeof(fn_buff);
   fn.lpstrFileTitle    = NULL;
   fn.nMaxFileTitle     = 0;
   fn.lpstrInitialDir   = NULL;
   fn.lpstrTitle        = dialog_title;
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
   fn.lpfnHook          = AHOFNHookProc;
   fn.lpTemplateName    = NULL;

   if (!GetSaveFileName(&fn))
      {
      return FALSE;
      }
   
   strcpy(dest, fn_buff);

   return TRUE;
}


bool get_open_filename(HWND          hWnd,               // parent window
                       C8           *dest,               // where the filename will be written (MAX_PATH)
                       C8           *dialog_title,       // title for file dialog
                       C8           *filter)             // filter specifier
{
   OPENFILENAME fn;
   C8           fn_buff[MAX_PATH];

   fn_buff[0] = 0;

   if (strlen(dest))
      {
      strcpy(fn_buff, dest);
      }

   memset(&fn, 0, sizeof(fn));
   fn.lStructSize       = sizeof(fn);
   fn.hwndOwner         = hWnd;
   fn.hInstance         = NULL;
   fn.lpstrFilter       = filter;
   fn.lpstrCustomFilter = NULL;
   fn.nMaxCustFilter    = 0;
   fn.nFilterIndex      = 1;
   fn.lpstrFile         = fn_buff;
   fn.nMaxFile          = sizeof(fn_buff);
   fn.lpstrFileTitle    = NULL;
   fn.nMaxFileTitle     = 0;
   fn.lpstrInitialDir   = NULL;
   fn.lpstrTitle        = dialog_title;
   fn.Flags             = OFN_EXPLORER      |
                          OFN_LONGNAMES     |
                          OFN_ENABLESIZING  |
                          OFN_NOCHANGEDIR   |
                          OFN_ENABLEHOOK    |
                          OFN_PATHMUSTEXIST |
                          OFN_HIDEREADONLY;
   fn.nFileOffset       = 0;
   fn.nFileExtension    = 0;
   fn.lpstrDefExt       = NULL;
   fn.lCustData         = NULL;
   fn.lpfnHook          = AHOFNHookProc;
   fn.lpTemplateName    = NULL;

   if (!GetOpenFileName(&fn))
      {
      return FALSE;
      }
   
   strcpy(dest, fn_buff);

   return TRUE;
}

// ---------------------------------------------------------------------------
// Dialog control helpers
// ---------------------------------------------------------------------------

void DlgItemPrintf(HWND hwnd,
                   S32 control,
                   C8 *fmt,
                   ...)
{
   C8 buffer[512];

   va_list ap;

   va_start(ap, 
            fmt);

   _vsnprintf(buffer,
              sizeof(buffer),
              fmt,
              ap);

   va_end(ap);

   SetDlgItemText(hwnd, 
                  control, 
                  buffer);
}

S32 DlgItemVal(HWND hwnd,
               S32  control)
{
   C8 text[256] = { 0 };
   GetDlgItemText(hwnd, control, text, sizeof(text)-1);
   
   return atoi(text);
}

DOUBLE DlgItemDbl(HWND   hwnd,
                  S32    control,
                  DOUBLE def)
{
   C8 text[256] = { 0 };
   GetDlgItemText(hwnd, control, text, sizeof(text)-1);
   
   DOUBLE v = def;

   sscanf(text,"%lf",&v);

   return v;
}

C8 *DlgItemTxt(HWND   hwnd,
               S32    control,
               C8    *def = NULL)
{
   static C8 text[256];
   memset(text, 0, sizeof(text));

   if (def != NULL) 
      {
      _snprintf(text,sizeof(text)-1,"%s",def);
      }

   GetDlgItemText(hwnd, control, text, sizeof(text)-1);
   
   return text;
}

// ---------------------------------------------------------------------------
// Update help text based on the control that has the input focus
// ---------------------------------------------------------------------------

void update_help(void)
{
   static HWND last_focus = NULL;
   static HWND help_focus = NULL;
   static HWND last_clicked_focus = NULL;
   static S32  focus_set_at_time = -1;

   HWND clicked_focus = GetFocus();
   HWND cur_focus;

   if (clicked_focus != last_clicked_focus)
      {
      last_clicked_focus = clicked_focus;
      cur_focus          = clicked_focus;
      }
   else
      {
      POINT pos,cli;
      RECT rect;

      GetCursorPos(&pos);
      GetClientRect(hwnd, &rect);
      cli.x = rect.left;
      cli.y = rect.top;
      ClientToScreen(hwnd, &cli);

      POINT point;
      point.x = pos.x - cli.x;
      point.y = pos.y - cli.y;

      cur_focus = RealChildWindowFromPoint(hwnd, point);

      if (cur_focus == NULL)
         {
         return;
         }

      S32 t = timeGetTime();

      if (cur_focus != last_focus)
         {
         focus_set_at_time = t;
         last_focus = cur_focus;
         }

      if ((t - focus_set_at_time) < HOVER_HELP_MS)
         {
         return;
         }
      }

   if (cur_focus == help_focus)
      {
      return;
      }

   help_focus = cur_focus;

   S32 ID = GetDlgCtrlID(help_focus);

   switch (ID)
      {
      case R_QUERY:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "This option specifies the query command used to retrieve the S-parameter data from an 87xx or 8510 VNA.  It has no effect with SCPI instruments.\n\nIn most cases, the query command should be \
left at the default setting (OUTPDATA).  However, because OUTPDATA traces from the 87xx analyzers have undergone calibration correction but not post-processing functions such as electrical delay, port extensions, trace math, or time-domain gating, \n\
it may be appropriate to select OUTPFORM on these instruments when these functions are required.\n\n\
In order to retrieve valid S-parameter data with OUTPFORM, it may be necessary to select a Smith chart or other polar display on the analyzer.\nRefer to the programming documentation for your specific VNA model for further information.");
         break;
         }

      case R_SAVE:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "Saves S11, S22, or all two-port S-parameters (S11, S21, S12, S22) to a Touchstone .S1P or .S2P file.\n\n\
This function supports HP 8510, 8702/8753, and 8719/8720-series network analyzers, as well as Agilent/Keysight FieldFox models.  (See the help field for the SCPI checkbox for more information.)\n\n\
SCPI analyzers, including FieldFox models, should be configured to display S11 or S22 prior to saving an .S1P file. \
Note that only linear sweeps are supported on the 8510 and SCPI models.\n\n\
Before saving the file, ensure that the desired network parameter format and reference resistance are selected at right, \
as well as the file type you wish to save.\n\n\
Depending on the analyzer state, the operation may take 30 seconds or more to complete.\n\n");
         break;
         }

      case R_FILETYPE:
         {
         SetDlgItemText(hwnd, R_HELPTXT, ".S2P: Saves all four S-parameters (S11, S21, S12, S22) to a Touchstone .S2P-compatible file.\n\n\
.S1P (S11): Saves only the S11 data to an .S1P file.\n\n\
.S1P (S21): Saves only the S21 data to an .S1P file.\n\n\
.S1P (S22): Saves only the S22 data to an .S1P file.\n\n\
SCPI analyzers, including FieldFox models, should be configured to display the desired S11, S21, or S22 data prior to saving an .S1P file.");
         break;
         }

      case R_R:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "Enter the reference resistance R, where R is a positive number of ohms (the real impedance to which the parameters are \
normalized).\n\nThe default reference resistance is 50 ohms.");
         break;
         }
         
      case R_DB:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "Select this option to save .S2P files with network parameters in dB-angle format (DB).\n\nIn this format, angles are given in degrees, while magnitudes are expressed as 20 * log10(magnitude).");
         break;
         }

      case R_MA:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "Select this option to save .S2P files with network parameters in magnitude-angle format (MA).  Angles are given in degrees."); 
         break;
         }

      case R_RI:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "Select this option to save .S2P files with network parameters in real-imaginary format (RI)."); 
         break;
         }

      case R_FREQ_FMT:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "Specifies the desired frequency unit.\n\nFrequencies in .S2P files are expressed in GHz by default, but some third-party applications may \
require frequency values to be expressed in Hz, kHz, or MHz instead.");
         break;
         }

      case R_DC:        
         {
         SetDlgItemText(hwnd, R_HELPTXT, "Specifies whether or not to include an initial entry at 0 Hz with unity magnitude and zero phase.\n\nMost users should leave this option at its default setting (None).");
         break;
         }

      case R_TCHECK:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "Performs a mathematical check to assess VNA calibration and measurement uncertainty.\n\nThe T-Check viewer is compatible with all VNAs that can export .S2P data files.  It may be launched independently from the Start menu program group. For more information, see Rohde && Schwarz Application Note 1EZ43, \"T-Check Accuracy Test \
for Vector Network Analyzers.\"\n\n\
CAUTION: Ensure that the network analyzer has undergone a full two-port calibration with a properly-defined calibration kit prior to running the T-Check function.  Additionally, a high-quality T adapter with \
low loss at the frequencies of concern is required.  The T adapter should be connected at the calibration reference plane, using test cables (where applicable) with good phase stability.\n\nSCPI analyzers, including FieldFox models, should be configured to display all four S-parameters prior to running T-Check.");
         break;
         }

      case R_SAVE_S:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "Saves the control state of the VNA to a file for later recall.  Calibration data is not saved.\n\nThis function supports HP 8510, 8702/8752/8753, and 8719/8720-series network analyzers.");
         break;
         }

      case R_SAVE_SC:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "Saves both the control and calibration state of the VNA to a file for later recall.\n\nThis function supports HP 8510, 8702/8752/8753, and 8719/8720-series network analyzers.  Depending on the analyzer state, this process may take 30 seconds or more to complete.");
         break;
         }

      case R_RECALL:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "Recalls the state of the VNA from a previously-saved file.  Calibration data is recalled only if it was \
saved by selecting \"Save state+cal\".\n\nThis function supports HP 8510, 8702/8752/8753, and 8719/8720-series network analyzers.  Depending on the complexity of the state being recalled, this process may take 30 seconds or more to complete.\n\n\
CAUTION: On the HP 8510, calibration data will be restored to its original slot (1-8), overwriting any existing data in that calibration set.\nIf errors are reported on the 8510, it may be necessary to delete one or more existing calibrations."); 
         break;
         }

      case R_PRESET:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "Issues a preset command to the VNA.\n\nThis function supports HP 8510, 8702/8752/8753, 8719/8720-series, and FieldFox network analyzers.");
         break;
         }

      case R_FPRESET:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "Presets the analyzer to the factory default state.\n\n\
This function supports HP 8510C, 8702/8752/8753, 8719/8720-series, and FieldFox network analyzers.  It will always invoke the factory preset \
state on these models, even if the analyzer is currently set up to recall a user preset state when the front-panel PRESET key is pressed.\n\nFor more information on \
user preset and factory default states, see your analyzer's documentation.");
         break;
         }

      case R_ADDR:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "Enter the GPIB address of the network analyzer here.  The factory default address for many HP VNA models is 16.\n\n\
Network analyzers must be set to Talk/Listen mode to allow them to be addressed by the PC's GPIB controller (e.g., Local->Talker/Listener for the HP 8753 series).  Be sure the controller is connected to the correct GPIB port -- some models have more than one.");
         break;
         }

      case R_SCPI:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "Check this box to communicate with a SCPI-compatible network analyzer.\n\nCurrently this feature has been tested only \
under the Agilent/Keysight FieldFox series of portable VNAs, and only for saving .S2P files.  It should be left unchecked when connecting to HP 85xx or 87xx models.\n\n\
To connect to a FieldFox VNA, the GPIB address field above is not used.  Instead, you must run the toolkit's GPIB configurator program and select the \"Edit CONNECT.INI\" option.  Enter the \
FieldFox's IP address manually on the \"interface_settings\" line, using port 5025.  (E.g., 192.168.1.14:5025 to communicate with a FieldFox at \
192.168.1.14.)  You must also set the \"is_Prologix\" field to 0.");
         break;
         }

      case R_CALIONE2:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "Check this box before using the \"Save state + cal\" button to save the state of an HP 87xx-series analyzer with \
one-path 2-port calibration.\n\nThis calibration type is not supported by all models, particularly those with built-in S-parameter test sets such as the HP 8719C. \
When using an instrument that does not support the CALIONE2 calibration type, checking this box may cause lockups or other undefined behavior.");
         break;
         }

      case R_EXITBTN:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "Exits the program.");
         break;
         }

      default:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "VNA Utility V"VERSION" of "__DATE__" by John Miles, KE5FX\nwww.ke5fx.com\
\n\nThis program may be copied and distributed freely.");
         break;
         }
      }
}

S32 S16_BE(C8 *s)
{
   U8 *p = (U8 *) s;
   return (((S32) p[0]) << 8) + (S32) p[1];
}

//****************************************************************************
//
// GPIB error handler
//
//****************************************************************************

void WINAPI GPIB_error(C8 *msg, S32 ibsta, S32 iberr, S32 ibcntl)
{
   alert_box("GPIB Error",msg);
   exit(1);
}

void preset(S32 addr, S32 SCPI)
{
   HCURSOR norm = wait(TRUE);

   GPIB_connect(addr,       // device_address
                GPIB_error, // handler
                FALSE,      // clear
                5000,       // timeout_msecs
               -1,          // board_address
                FALSE,      // release_system_control
               -1,          // max_read_buffer_len
                FALSE);     // Disable Prologix auto-read mode at startup

   SendMessage(hProgress, PBM_SETPOS, 10, 0);
   serve();

   if (SCPI)
      GPIB_puts("SYST:UPRESET");
   else
      GPIB_puts("PRES;");

   SendMessage(hProgress, PBM_SETPOS, 100, 0);
   serve();

   Sleep(500);              // Delay before disconnecting
   GPIB_disconnect();
   end_wait(norm);
}

void factory_preset(S32 addr, S32 SCPI)
{
   HCURSOR norm = wait(TRUE);

   GPIB_connect(addr,       // device_address
                GPIB_error, // handler
                FALSE,      // clear
                5000,       // timeout_msecs
               -1,          // board_address
                FALSE,      // release_system_control
               -1,          // max_read_buffer_len
                FALSE);     // Disable Prologix auto-read mode at startup

   GPIB_set_EOS_mode(10);
   GPIB_set_serial_read_dropout(10000);

   SendMessage(hProgress, PBM_SETPOS, 10, 0);
   serve();

   instrument_setup(SCPI, FALSE);

   SendMessage(hProgress, PBM_SETPOS, 50, 0);
   serve();

   if (!is_8510()) 
      {
      if (SCPI)
         GPIB_puts("SYST:PRESET");
      else
         GPIB_puts("RST;");
      }
   else
      {
      if (is_8510C())
         GPIB_puts("FACTPRES;");
      else
         GPIB_puts("PRES;");
      }

   SendMessage(hProgress, PBM_SETPOS, 100, 0);
   serve();

   Sleep(500);
   GPIB_disconnect();
   end_wait(norm);
}

void recall_state(S32 addr, S32 SCPI)
{
   if (SCPI)
      {
      alert_box("Error","Save/recall not currently supported for SCPI analyzers");
      return;
      }

   //
   // Get filename to recall
   //

   C8 filename[MAX_PATH] = { 0 };

   if (!get_open_filename(hwnd,
                          filename,
                         "Recall VNA state",
                         "VNA state files (*.vst)\0*.VST\0\0"))
      {
      return;
      }

   //
   // Force filename to end in .vst suffix
   //

   S32 l = strlen(filename);

   if (l >= 4)
      {
      if (_stricmp(&filename[l-4], ".vst"))
         {
         strcat(filename,".vst");
         }
      }

   FILE *in = fopen(filename,"rb");

   if (in == NULL)
      {
      alert_box("Error", "Couldn't open %s", filename);
      return;
      }

   //
   // Establish GPIB connection
   //

   HCURSOR norm = wait(TRUE);

   GPIB_connect(addr,       // device_address
                GPIB_error, // handler
                FALSE,      // clear
               -1,          // Disable timeout checks with timeout_msecs=-1
               -1,          // board_address
                FALSE,      // release_system_control
               -1,          // max_read_buffer_len
                FALSE);     // Disable Prologix auto-read mode at startup

   GPIB_set_EOS_mode(10);
   GPIB_set_serial_read_dropout(10000);

   instrument_setup(SCPI);

   //
   // Read file
   // 

   static C8 data[262144];
   memset(data, 0, sizeof(data));
   S32 len = fread(data, 1, sizeof(data), in);

   fclose(in);
   in = NULL;

   if (len == 0)
      {
      alert_box("Error","Couldn't read from %s", filename);
      return;
      }
      
   //
   // Send data stream to analyzer
   //

   C8 *src = data;

   for (;;)
      {
      S32 cmdlen = *(S32 *) src; src += 4;

      if (cmdlen == 0)
         {
         break;
         }

      SendMessage(hProgress, PBM_SETPOS, 10 + ((src-data+1) * 90 / len), 0);
      serve();

      static C8 cmd[262144];
      memset(cmd, 0, sizeof(cmd));
      memcpy(cmd, src, cmdlen);
      src += cmdlen;

      if (!_strnicmp(cmd, "OPC?", 3))
         {
         if (is_8510())
            {
            printf("%s: ", cmd);
            GPIB_query(cmd);
            printf("Done\n");
            }
         else
            {
            //
            // Translate 8753 commands containing OPC? queries to serial-polled form
            //
            // (Equivalent functionality but much more reliable on Prologix; 
            // queries to 8753C tend to time out after 9KB+ writes)
            //

            C8 cmdbuf[256] = { 0 };
            _snprintf(cmdbuf,sizeof(cmdbuf)-1,"OPC;%s", &cmd[5]);
            printf("(%s) ", cmdbuf);

            GPIB_printf("CLES;SRE 32;ESE 1;");   // Event status register bit 0 = OPC operation done; map it to status bit and enable SRQ on it 
            GPIB_printf(cmdbuf);

            S32 st = timeGetTime();

            while (!(GetKeyState(VK_SPACE) & 0x8000))
               {
               Sleep(100);
               serve();

               U8 result = GPIB_serial_poll();
               printf("%X ", result);

               if (result & 0x40)
                  {
                  printf("Complete in %d ms\n", timeGetTime()-st);
                  break;
                  }
               }

            GPIB_printf("CLES;SRE 0;");
            }
         }           
      else
         {
         printf("cmdlen=%d: ",cmdlen);

         for (S32 i=0; i < 64; i++)
            {
            C8 ch = cmd[i];
            if (ch < 26) break;
            _putch(ch);
            }

         printf("\n");

         Sleep(1500);                      // 250 is sufficient but 100 is not (GPIB-LAN, but also appears to be timing-sensitive under NI)
         GPIB_write_BIN(cmd, cmdlen);
         }
      }

   Sleep(1500);      
   
   GPIB_printf("DEBUOFF;CONT;");

   SendMessage(hProgress, PBM_SETPOS, 100, 0);
   serve();

   Sleep(500);                            // Delay before disconnecting
   GPIB_disconnect();

   end_wait(norm);
}

void save_state(S32  addr,
                S32  SCPI,
                S32  CALIONE2,
                bool include_cal)
{
   if (SCPI)
      {
      alert_box("Error","Save/recall not currently supported for SCPI analyzers");
      return;
      }

   //
   // Get filename to save
   //

   C8 filename[MAX_PATH] = { 0 };

   if (!get_save_filename(hwnd,
                         filename,
                        "Save VNA state",
                        "VNA state files (*.vst)\0*.VST\0\0"))
      {
      return;
      }

   //
   // Force filename to end in .vst suffix
   //

   S32 l = strlen(filename);

   if (l >= 4)
      {
      if (_stricmp(&filename[l-4], ".vst"))
         {
         strcat(filename,".vst");
         }
      }

   FILE *out = fopen(filename,"wb");

   if (out == NULL)
      {
      alert_box("Error", "Couldn't open %s\n", out);
      return;
      }

   //
   // Establish GPIB connection
   //

   HCURSOR norm = wait(TRUE);

   GPIB_connect(addr,       // device_address
                GPIB_error, // handler
                FALSE,      // clear
               -1,          // timeout_msecs=-1 to disable
               -1,          // board_address
                FALSE,      // release_system_control
               -1,          // max_read_buffer_len
                FALSE);     // Disable Prologix auto-read mode at startup

   GPIB_set_EOS_mode(10);
   GPIB_set_serial_read_dropout(10000);

   instrument_setup(SCPI);

   SendMessage(hProgress, PBM_SETPOS, 5, 0);
   serve();

   //
   // Request learn string
   //

   C8 learn_string[65536] = { 0 };
   S32 learn_string_size = 0;

   U16 learn_N = 0;
   U16 learn_H = 0;

   Sleep(500);

   GPIB_write("FORM1;OUTPLEAS;");

   //
   // Verify its header 
   //

   S32 actual = 0;
   C8 *data = GPIB_read_BIN(2, TRUE, FALSE, &actual); 

   if (actual != 2)
      {
      end_wait(norm);
      alert_box("Error", "OUTPLEAS header query returned %d bytes", actual);
      return;
      }

   if ((data[0] != '#') || (data[1] != 'A'))
      {
      end_wait(norm);
      alert_box("Error", "OUTPLEAS FORM1 block header was 0x%.2X 0x%.2X", data[0], data[1]);
      return;
      }

   learn_H = *(U16 *) data;

   //
   // Get length in bytes of learn string
   //

   actual = 0;
   data = GPIB_read_BIN(2, TRUE, FALSE, &actual);

   S32 len = S16_BE(data);

   if (actual != 2)
      {
      end_wait(norm);
      alert_box("Error","OUTPLEAS string len returned %d bytes", actual);
      return;
      }

   learn_N = *(U16 *) data;

   //
   // Get learn string data
   //

   data = GPIB_read_BIN(len, TRUE, FALSE, &learn_string_size);

   if (learn_string_size != len)
      {
      end_wait(norm);
      alert_box("Error","OUTPLEAS string len = %d, received %d", len, learn_string_size);
      return;
      }

   memcpy(learn_string, data, learn_string_size);

   SendMessage(hProgress, PBM_SETPOS, 15, 0);
   serve();

   //
   // Write learn string command to file
   //
   //
   // Note that 8753C with firmware V4.12 does not support INPUTLEAS, according to
   // na.support.keysight.com/8753/firmware/history.htm
   //

   S32 cmdlen = 15 + 4 + learn_string_size;
   fwrite(&cmdlen, 4, 1, out);
   fprintf(out,"FORM1;INPULEAS;");
   fwrite(&learn_H, 2, 1, out);
   fwrite(&learn_N, 2, 1, out);
   fwrite(learn_string, learn_string_size, 1, out);

   //
   // Store calibration data
   //

   if (include_cal)
      {
      if (is_8510())
         {
         //
         // Determine active HP 8510 calibration type and set #
         //

         const C8 *types[] = { "RESPONSE", "RESPONSE & ISOL'N", "S11 1-PORT", "S22 1-PORT", "2-PORT"   };
         const C8 *names[] = { "CALIRESP", "CALIRAI",           "CALIS111",   "CALIS221",   "CALIFUL2" };
         const S32 cnts [] = {  1,          2,                   3,            3,            12        };

         S32 active_cal_type = -1;
         S32 active_cal_set  = -1;

         S32 n_types = ARY_CNT(cnts);

         C8 *result = GPIB_query("CALI?;");
         C8 *term = strrchr(result,'"');                          // Remove leading and trailing quotes returned by 8510

         if (term != NULL) 
            {
            *term = 0;
            result++;
            }

         for (S32 i=0; i < n_types; i++)
            {
            if (!_stricmp(result, types[i]))
               {
               active_cal_type = i;
               active_cal_set  = atoi(GPIB_query("CALS?;"));
               break;
               }
            }

         if ((active_cal_type != -1) && (active_cal_set > 0))
            {
            const C8 *active_cal_name = names[active_cal_type];   // Get name of active calibration type
            S32       n_arrays        = cnts[active_cal_type];    // Get # of data arrays for active calibration type

            data = GPIB_query("POIN;OUTPACTI;");                  // Get # of points in calibrated trace 
            DOUBLE fnpts = 0.0;
            sscanf(data, "%lf", &fnpts);
            S32 trace_points = FTOI(fnpts);

            if ((trace_points < 1) || (trace_points > 1000000))
               {
               end_wait(norm);
               alert_box("Error","trace_points = %d\n", trace_points);
               return;
               }

            S32 array_bytes = trace_points * 6;
            S32 total_bytes = array_bytes * n_arrays;

            S32 cmdlen = 20 + strlen(active_cal_name);            // Write command to file that will select this calibration type
            fwrite(&cmdlen, 4, 1, out);
            fprintf(out, "CORROFF;HOLD;FORM1;%s;", active_cal_name);

            //
            // For each array....
            // 

            S32 n = 0;

            for (S32 j=0; j < n_arrays; j++)
               {
               n += array_bytes;

               SendMessage(hProgress, PBM_SETPOS, 15 + ((n+1) * 85 / total_bytes), 0);
               serve();

               GPIB_printf("FORM1;OUTPCALC%d%d;", (j+1)/10, (j+1)%10);

               data = GPIB_read_BIN(2); 

               if ((data[0] != '#') || (data[1] != 'A'))
                  {
                  end_wait(norm);
                  alert_box("Error", "OUTPCALC FORM1 block header was 0x%.2X 0x%.2X", data[0], data[1]);
                  return;
                  }

               U16 H = *(U16 *) data;

               data = GPIB_read_BIN(2);
               
               len = S16_BE(data);
               
               if (len != array_bytes)
                  {
                  end_wait(norm);
                  alert_box("Error", "OUTPCALC returned %d bytes, expected %d", len, array_bytes);
                  return;
                  }

               U16 N = *(U16 *) data;
               
               cmdlen = 11 + 4 + len;
               fwrite(&cmdlen, 4, 1, out);
               fprintf(out,"INPUCALC%d%d;", (j+1)/10, (j+1)%10);
               fwrite(&H, 2, 1, out);
               fwrite(&N, 2, 1, out);

               C8 *IQ = GPIB_read_BIN(array_bytes, TRUE, FALSE, &actual);

               if (actual != array_bytes)
                  {
                  end_wait(norm);
                  alert_box("Error", "OUTPCALC%d%d returned %d bytes, expected %d", (j+1)/10, (j+1)%10, actual, array_bytes);
                  return;
                  }

               fwrite(IQ, 1, array_bytes, out);
               }

            assert(n == total_bytes);

            cmdlen = 21;
            fwrite(&cmdlen, 4, 1, out);
            fprintf(out,"SAVC; CALS%d; CORROFF;", active_cal_set);

            //
            // Write second copy of learn string command to file
            //
            // On the 8510, the learn string state must be restored before its 
            // accompanying calibration data.  Otherwise, SAVC will incorrectly 
            // associate the prior instrument state with the cal set's limited 
            // instrument state.  This would be fine except that the CALIxxxx commands 
            // switch to single-parameter display mode, regardless of what was saved 
            // in the learn string.
            //
            // So, we work around this lameness by recording the learn string twice, once
            // before the calibration data and again after it...
            //

            cmdlen = 15 + 4 + learn_string_size;
            fwrite(&cmdlen, 4, 1, out);
            fprintf(out,"FORM1;INPULEAS;");
            fwrite(&learn_H, 2, 1, out);
            fwrite(&learn_N, 2, 1, out);
            fwrite(learn_string, learn_string_size, 1, out);
            }
         }
      else
         {
         //
         // Determine active HP 8753 calibration type
         //
         // TODO: what about CALIONE2? (and CALITRL2 and CALRCVR on 8510?)
         //
         //   8510-90280 p. C-13: CALIONE2 not recommended for use with S-param sets, since the same
         //   forward error terms will be used in both directions.  Use CALIONE2 with T/R sets.
         //
         //   8510-90280 p. PC-41: CALI? apparently does not return CALITRL or CALIONE2
         //              p. I-4: If INPUCALC'ing CALIONE2, you must issue CALIFUL2 with all 12 coeffs
         //
         // Note: on 8753A/B/C at least, only the active channel's calibration type query returns '1'.
         // E.g., CALIS111 and CALIS221 may both be valid and available, but only one will be 
         // saved with the instrument state.  (Or neither, if an uncalibrated response parameter is 
         // being displayed in the active channel.)
         //
         // V1.56: added CALIONE2 for 8753C per G. Anagnostopoulos email of 25-Apr-16
         // V1.57: added check box for CALIONE2 to avoid 8719C lockups, per M. Swanberg email of 28-Oct-16
         //
         
         const C8 *types[] = { "CALIRESP", "CALIRAI", "CALIS111", "CALIS221", "CALIFUL2", "CALIONE2" };
         const S32 cnts [] = {  1,          2,         3,          3,          12,         12 };

         S32 active_cal_type = -1;
         S32 n_types = ARY_CNT(cnts);

         if (!CALIONE2)
            {
            n_types--;
            }

         for (S32 i=0; i < n_types; i++)        
            {
            GPIB_printf("%s?;", types[i]);
            C8 *result = GPIB_read_ASC();

            if (result[0] == '1')
               {
               active_cal_type = i;
               break;
               }
            }

         if (active_cal_type != -1)
            {
            const C8 *active_cal_name = types[active_cal_type];   // Get name of active calibration type
            S32       n_arrays        = cnts[active_cal_type];    // Get # of data arrays for active calibration type

            data = GPIB_query("FORM3;POIN?;");                    // Get # of points in calibrated trace
            DOUBLE fnpts = 0.0;
            sscanf(data, "%lf", &fnpts);
            S32 trace_points = FTOI(fnpts);

            S32 array_bytes = trace_points * 6;
            S32 total_bytes = array_bytes * n_arrays;

            S32 cmdlen = 15 + strlen(active_cal_name);             // Write command to file that will select this calibration type
            fwrite(&cmdlen, 4, 1, out);
            fprintf(out, "CORROFF;FORM1;%s;", active_cal_name);

            //
            // For each array....
            // 

            S32 n = 0;

            for (S32 j=0; j < n_arrays; j++)
               {
               n += array_bytes;

               SendMessage(hProgress, PBM_SETPOS, 15 + ((n+1) * 85 / total_bytes), 0);
               serve();

               GPIB_printf("FORM1;OUTPCALC%d%d;", (j+1)/10, (j+1)%10);

               data = GPIB_read_BIN(2); 

               if ((data[0] != '#') || (data[1] != 'A'))
                  {
                  end_wait(norm);
                  alert_box("Error", "OUTPCALC FORM1 block header was 0x%.2X 0x%.2X", data[0], data[1]);
                  return;
                  }

               U16 H = *(U16 *) data;

               data = GPIB_read_BIN(2);
               
               len = S16_BE(data);
               
               if (len != array_bytes)
                  {
                  end_wait(norm);
                  alert_box("Error", "OUTPCALC returned %d bytes, expected %d", len, array_bytes);
                  return;
                  }

               U16 N = *(U16 *) data;
               
               S32 cmdlen = 11 + 4 + len;
               fwrite(&cmdlen, 4, 1, out);
               fprintf(out,"INPUCALC%d%d;", (j+1)/10, (j+1)%10);
               fwrite(&H, 2, 1, out);
               fwrite(&N, 2, 1, out);

               C8 *IQ = GPIB_read_BIN(array_bytes, TRUE, FALSE, &actual);

               if (actual != array_bytes)
                  {
                  end_wait(norm);
                  alert_box("Error", "OUTPCALC%d%d returned %d bytes, expected %d", (j+1)/10, (j+1)%10, actual, array_bytes);
                  return;
                  }

               fwrite(IQ, 1, array_bytes, out);
               }

            assert(n == total_bytes);

            cmdlen = 10;
            fwrite(&cmdlen, 4, 1, out);

            fprintf(out,"OPC?;SAVC;");
            }
         }
      }

   //
   // Terminate file by writing zero-length field
   //

   cmdlen = 0;
   fwrite(&cmdlen, 4, 1, out);

   fclose(out);
   out = NULL;

   Sleep(500);

   GPIB_printf("DEBUOFF;CONT;");

   SendMessage(hProgress, PBM_SETPOS, 100, 0);
   serve();

   Sleep(500);
   GPIB_disconnect();

   end_wait(norm);
}

bool read_complex_trace(C8             *param, 
                        C8             *query,
                        COMPLEX_DOUBLE *dest, 
                        S32             cnt,
                        S32             progress_fraction)
{  
   U8 mask = 0x40;

   if (is_8753()) { GPIB_printf("CLES;SRE 4;ESNB 1;"); mask = 0x40; }  // Extended register bit 0 = SING sweep complete; map it to status bit and enable SRQ on it 
   if (is_8752()) { GPIB_printf("CLES;SRE 4;ESNB 1;"); mask = 0x40; }
   if (is_8720()) { GPIB_printf("CLES;SRE 4;ESNB 1;"); mask = 0x40; }
   if (is_8510()) { GPIB_printf("CLES;");              mask = 0x10; }

   GPIB_printf("%s;FORM4;SING;", param);

   if (is_8510())             // Skip first reading to ensure EOS bit is clear, but only on 8510 
      {                       // (SRQ bit auto-resets after the first successful poll on 8753)   
      GPIB_serial_poll();     
      }                       

   S32 st = timeGetTime();

   for (;;)
      {
      Sleep(100);
      serve();

      U8 result = GPIB_serial_poll();

      if (result & mask)
         {
         printf("%s sweep finished in %d ms\n", param, timeGetTime()-st);
         break;
         }
      }

   if (is_8753()) { GPIB_printf("CLES;SRE 0;"); }
   if (is_8752()) { GPIB_printf("CLES;SRE 0;"); }
   if (is_8720()) { GPIB_printf("CLES;SRE 0;"); }
   if (is_8510()) { GPIB_printf("CLES;");       }

   GPIB_printf("%s;",query);

   for (S32 i=0; i < cnt; i++)
      {
      C8 *data = GPIB_read_ASC(-1, FALSE);

      DOUBLE I = DBL_MIN;
      DOUBLE Q = DBL_MIN;

      sscanf(data,"%lf, %lf", &I, &Q);

      if ((I == DBL_MIN) || (Q == DBL_MIN))
         {
         alert_box("Error", "VNA read timed out reading %s (point %d of %d points)", param, i, cnt);
         return FALSE;
         }

      dest[i].real = I;
      dest[i].imag = Q;

      SendMessage(hProgress, PBM_SETPOS, ((i * 20) / cnt) + progress_fraction, 0);
      serve();
      }

   return TRUE;
}

bool read_complex_trace_SCPI(S32             param, 
                             COMPLEX_DOUBLE *dest, 
                             S32             cnt,
                             S32             progress_fraction)
{  
   U8 mask = 0;

   S32 st = timeGetTime();

   GPIB_printf("CALC:PAR%d:SEL",param);

   GPIB_query("INIT:IMM;*OPC?");

   GPIB_printf("CALC:DATA:SDAT?");

   C8 *data = GPIB_read_ASC(-1, FALSE);
   C8 *src = data;

   for (S32 i=0; i < cnt; i++)
      {
      C8 *next_comma = strchr(src,',');

      if  (next_comma == NULL)
         {
         alert_box("Error", "Expected comma reading parameter %d (point %dI of %d points)", param, i, cnt);
         return FALSE;
         }

      next_comma = strchr(&next_comma[1],',');

      if (next_comma != NULL)
         {
         *next_comma = 0;
         }
      else
         {
         if (i != cnt-1)
            {
            alert_box("Error", "Expected comma reading parameter %d (point %dQ of %d points)", param, i, cnt);
            return FALSE;
            }
         }

      DOUBLE I = DBL_MIN;
      DOUBLE Q = DBL_MIN;

      sscanf(src,"%lf, %lf", &I, &Q);

      if ((I == DBL_MIN) || (Q == DBL_MIN))
         {
         alert_box("Error", "Bad data read (%s) for parameter %d (point %d of %d points)", src, param, i, cnt);
         return FALSE;
         }

      dest[i].real = I;
      dest[i].imag = Q;

      SendMessage(hProgress, PBM_SETPOS, ((i * 20) / cnt) + progress_fraction, 0);
      serve();

      src = &next_comma[1];
      }

   return TRUE;
}

bool save_SnP(S32       SnP,        // 1 or 2
              C8       *param,
              C8       *query,
              S32       addr,
              S32       SCPI,
              DOUBLE    R_ohms,
              const C8 *data_format,
              const C8 *freq_format,
              S32       DC_entry,
              const C8 *explicit_filename)
{
   //
   // Get filename to save
   //

   C8 filename[MAX_PATH] = { 0 };

   if ((explicit_filename != NULL) && (explicit_filename[0]))
      {
      strcpy(filename, explicit_filename);
      }
   else
      {
      if (SnP == 1)
         {
         if (!get_save_filename(hwnd,
                                filename,
                               "Save Touchstone .S1P file",
                               ".S1P files (*.S1P)\0*.S1P\0\0"))
            {
            return FALSE;
            }
         }
      else
         {
         if (!get_save_filename(hwnd,
                                filename,
                               "Save Touchstone .S2P file",
                               ".S2P files (*.S2P)\0*.S2P\0\0"))
            {
            return FALSE;
            }
         }
      }

   //
   // Force filename to end in .SnP suffix
   //

   S32 l = strlen(filename);

   if (l >= 4)
      {
      if (SnP == 1)
         {
         if (_stricmp(&filename[l-4], ".S1P"))
            {
            strcat(filename,".S1P");
            }
         }
      else
         {
         if (_stricmp(&filename[l-4], ".S2P"))
            {
            strcat(filename,".S2P");
            }
         }
      }

   //
   // Establish GPIB connection
   //

   HCURSOR norm = wait(TRUE);

   GPIB_connect(addr,       // device_address
                GPIB_error, // handler
                FALSE,      // clear
                10000,      // timeout = 10s
               -1,          // board_address
                FALSE,      // release_system_control
               -1,          // max_read_buffer_len
                FALSE);     // Disable Prologix auto-read mode at startup

   GPIB_set_EOS_mode(10);
   GPIB_set_serial_read_dropout(10000);

   instrument_setup(SCPI);

   //
   // Get start/stop freq and # of trace points
   //

   S32    n        = 0;
   DOUBLE start_Hz = 0.0;
   DOUBLE stop_Hz  = 0.0;

   if (SCPI)
      {
      C8 *data = GPIB_query(":SENS:FREQ:START?");

      sscanf(data, "%lf", &start_Hz);

      data = GPIB_query(":SENS:FREQ:STOP?");

      sscanf(data, "%lf", &stop_Hz);

      SendMessage(hProgress, PBM_SETPOS, 5, 0);
      serve();

      data = GPIB_query(":SENS:SWEEP:POINTS?");

      DOUBLE fn = 0.0;
      sscanf(data, "%lf", &fn);
      n = (S32) (fn + 0.5);

      if ((n < 1) || (n > 1000000))
         {
         end_wait(norm);
         GPIB_disconnect();
         alert_box("Error","n_points = %d\n", n);
         return FALSE;
         }

      if (n > 1601)
         {
         end_wait(norm);
         GPIB_disconnect();
         alert_box("Error","Trace exceeds 1601-point support limit for SCPI option");
         return FALSE;
         }
      }
   else
      {
      C8 *data = GPIB_query("FORM4;STAR;OUTPACTI;");     // (STAR/STOP/POIN? queries don't work on 8510, per VT)
      sscanf(data, "%lf", &start_Hz);

      data = GPIB_query("STOP;OUTPACTI;");
      sscanf(data, "%lf", &stop_Hz);

      SendMessage(hProgress, PBM_SETPOS, 5, 0);
      serve();

      data = GPIB_query("POIN;OUTPACTI;");

      DOUBLE fn = 0.0;
      sscanf(data, "%lf", &fn);
      n = (S32) (fn + 0.5);

      if ((n < 1) || (n > 1000000))
         {
         end_wait(norm);
         GPIB_disconnect();
         alert_box("Error","n_points = %d\n", n);
         return FALSE;
         }
      }

   //
   // Reserve space for DC term if requested
   // 

   bool include_DC = (DC_entry != 0);

   S32 n_alloc_points = n;
   S32 n_AC_points    = n;
   S32 first_AC_point = 0;

   if (include_DC)
      {
      n_alloc_points++;
      first_AC_point = 1;
      }

   DOUBLE *freq_Hz = (DOUBLE *) alloca(n_alloc_points * sizeof(freq_Hz[0])); memset(freq_Hz, 0, n_alloc_points * sizeof(freq_Hz[0]));

   COMPLEX_DOUBLE *S11 = (COMPLEX_DOUBLE *) alloca(n_alloc_points * sizeof(S11[0])); memset(S11, 0, n_alloc_points * sizeof(S11[0]));
   COMPLEX_DOUBLE *S21 = (COMPLEX_DOUBLE *) alloca(n_alloc_points * sizeof(S21[0])); memset(S21, 0, n_alloc_points * sizeof(S21[0])); 
   COMPLEX_DOUBLE *S12 = (COMPLEX_DOUBLE *) alloca(n_alloc_points * sizeof(S12[0])); memset(S12, 0, n_alloc_points * sizeof(S12[0])); 
   COMPLEX_DOUBLE *S22 = (COMPLEX_DOUBLE *) alloca(n_alloc_points * sizeof(S22[0])); memset(S22, 0, n_alloc_points * sizeof(S22[0])); 

   if (include_DC)
      {
      S11[0].real = 1.0;
      S21[0].real = 1.0;
      S12[0].real = 1.0;
      S22[0].real = 1.0;
      }

   //
   // Construct frequency array
   //
   // For 8510 and SCPI, we support only linear sweeps (8510 does not support log sweeps, only lists)
   //
   // For non-8510 analyzers, if LINFREQ? indicates a linear sweep is in use, we construct 
   // the array directly.  If a nonlinear sweep is in use, we obtain the frequencies from
   // an OUTPLIML query (08753-90256 example 3B).
   //
   // Note that the frequency parameter in .SnP files taken in POWS or CWTIME mode
   // will reflect the power or time at each point, rather than the CW frequency
   //

   bool lin_sweep = TRUE;

   if ((!SCPI) && (!is_8510()))
      {
      lin_sweep = (GPIB_query("LINFREQ?;")[0] == '1');
      }

   if (lin_sweep)
      {
      SendMessage(hProgress, PBM_SETPOS, 10, 0);
      serve();

      for (S32 i=0; i < n_AC_points; i++)
         {
         freq_Hz[i+first_AC_point] = start_Hz + (((stop_Hz - start_Hz) * i) / (n_AC_points-1)); 
         }
      }
   else
      {
      GPIB_printf("OUTPLIML;");

      for (S32 i=0; i < n_AC_points; i++)
         {
         C8 *data = GPIB_read_ASC(-1, FALSE);

         DOUBLE f = DBL_MIN;
         sscanf(data,"%lf", &f);

         if (f == DBL_MIN)
            {
            end_wait(norm);
            alert_box("Error", "VNA read timed out reading OUTPLIML (point %d of %d points)", i, n_AC_points);
            GPIB_disconnect();
            return FALSE;
            }
   
         freq_Hz[i+first_AC_point] = f;

         SendMessage(hProgress, PBM_SETPOS, 5 + (i * 5 / n_AC_points), 0);
         serve();
         }
      }

   //
   // If this is an 8753 or 8720, determine what the active parameter is so it can be
   // restored afterward
   //
   // (S12 and S22 queries are not supported on 8752 or 8510)
   //

   S32 active_param = 0;
   C8 param_names[4][4] = { "S11", "S21", "S12", "S22" };

   if (is_8753() || is_8720())
      {
      for (active_param=0; active_param < 4; active_param++)
         {
         C8 text[512] = { 0 };
         _snprintf(text,sizeof(text)-1,"%s?", param_names[active_param]);

         if (GPIB_query(text)[0] == '1')
            {
            break;
            }
         }
      }

   SendMessage(hProgress, PBM_SETPOS, 15, 0);
   serve();

   //
   // Read data from VNA
   //

   bool result = FALSE;

   if (!SCPI)
      {
      if (SnP == 1)
         {
         result = read_complex_trace(param, query, &S11[first_AC_point], n_AC_points, 50);
         }
      else
         {
         result = read_complex_trace("S11", query, &S11[first_AC_point], n_AC_points, 20) && 
                  read_complex_trace("S21", query, &S21[first_AC_point], n_AC_points, 40) && 
                  read_complex_trace("S12", query, &S12[first_AC_point], n_AC_points, 60) && 
                  read_complex_trace("S22", query, &S22[first_AC_point], n_AC_points, 80);
         }
      }
   else
      {
      GPIB_query("FORM:DATA ASC,0;*OPC?");

      result = TRUE;

      S32 n = 0;
      C8 *data = GPIB_query("CALC:PAR:COUN?");
      sscanf(data,"%d", &n);

      S32 incr = 100 / (n + 1);                   // bar graph increment
      S32 pct = 20;

      for (S32 p=1; (p <= n) && result; p++)      // query only the visible S-params, leaving others at 0,0
         {                                        // (some FieldFoxes don't support all four)
         GPIB_printf("CALC:PAR%d:DEF?", p); 
         data = GPIB_read_ASC();

         if (SnP == 1)
            {
// TODO, when sparams.cpp supports single-param files other than S11...
//            if (!_strnicmp(data,param,3))
//               {
//               if (param[1] == '1')
                  { result &= read_complex_trace_SCPI(p, &S11[first_AC_point], n_AC_points, pct); pct += incr; }
//               else
//                  { result &= read_complex_trace_SCPI(p, &S22[first_AC_point], n_AC_points, pct); pct += incr; }
//               }
            }
         else
            {
            if (!_strnicmp(data,"S11",3))
               result &= read_complex_trace_SCPI(p, &S11[first_AC_point], n_AC_points, pct); pct += incr;

            if (!_strnicmp(data,"S21",3))
               result &= read_complex_trace_SCPI(p, &S21[first_AC_point], n_AC_points, pct); pct += incr;

            if (!_strnicmp(data,"S12",3))
               result &= read_complex_trace_SCPI(p, &S12[first_AC_point], n_AC_points, pct); pct += incr;

            if (!_strnicmp(data,"S22",3))
               result &= read_complex_trace_SCPI(p, &S22[first_AC_point], n_AC_points, pct); pct += incr;
            }
         }
      }

   //
   // Create S-parameter database, fill it with received data, and save it
   //

   if (result)
      {
      SPARAMS S;

      if (!S.alloc(SnP, n_alloc_points))
         {
         alert_box("Error","%s", S.message_text);
         }
      else
         {
         S.min_Hz = include_DC ? 0.0 : start_Hz;
         S.max_Hz = stop_Hz;
         S.Zo     = R_ohms;

         for (S32 i=0; i < n_alloc_points; i++)
            {
            S.freq_Hz[i] = freq_Hz[i];

            if (SnP == 1)
               {
// TODO, when sparams.cpp supports single-param files other than S11...
//               if (param[1] == '1')
                  { S.RI[0][0][i] = S11[i]; S.valid[0][0][i] = SNPTYPE::RI; }
//               else
//                  { S.RI[1][1][i] = S22[i]; S.valid[1][1][i] = SNPTYPE::RI; }
               }
            else
               {
               S.RI[0][0][i] = S11[i]; S.valid[0][0][i] = SNPTYPE::RI;
               S.RI[1][0][i] = S21[i]; S.valid[1][0][i] = SNPTYPE::RI; 
               S.RI[0][1][i] = S12[i]; S.valid[0][1][i] = SNPTYPE::RI; 
               S.RI[1][1][i] = S22[i]; S.valid[1][1][i] = SNPTYPE::RI; 
               }
            }

         C8 header[1024] = { 0 };

         _snprintf(header,sizeof(header)-1,
            "! Touchstone 1.1 file saved by VNA.EXE V%s (www.ke5fx.com/gpib/readme.htm)\n"
            "! %s\n"
            "!\n"
            "!    Source: %s\n", VERSION, timestamp(), instrument_name);

         if (!S.write_SNP_file(filename, data_format, freq_format, header, param))
            {
            alert_box("Error","%s", S.message_text);
            }
         }
      }

   //
   // Restore active parameter and exit
   //

   if ((is_8753() || is_8720()) 
        && 
       (active_param <= 3))
      {
      GPIB_printf(param_names[active_param]);
      Sleep(500);
      }

   if (SCPI)
      GPIB_query("INIT:CONT 1;*OPC?");
   else
      GPIB_printf("DEBUOFF;CONT;");

   SendMessage(hProgress, PBM_SETPOS, 100, 0);
   serve();

   Sleep(500);
   GPIB_disconnect();

   end_wait(norm);

   return TRUE;
}

// ---------------------------------------------------------------------------
// show_TCheck()
// ---------------------------------------------------------------------------

void show_TCheck(C8 *filename)
{
   C8 path[MAX_PATH+16] = { 0 };
   sprintf(path,"tcheck %s", filename);

   Win32Exec(path, FALSE);
}

// ---------------------------------------------------------------------------
// shutdown()
// ---------------------------------------------------------------------------

void shutdown(void)
{
   for (S32 i=0; i <= TF_list.max_index(); i++) 
      {
      delete TF_list[i];
      TF_list[i] = NULL;
      }

   GPIB_disconnect();
   _fcloseall();
}

INT_PTR CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
   switch (message)
      {
      case WM_SETCURSOR:
         {
         if (waiting > 0)
            {
            SetCursor(hourglass);
            return TRUE;
            }

         break;
         }

      case WM_TIMER:
         {
         if (GetForegroundWindow() == hwnd)
            {
            update_help();
            }
         break;
         }

      case WM_SETFOCUS:
         {
         if (waiting > 0)  
            {
            break;
            }

         HWND h = GetWindow(hwnd,GW_CHILD);

         while (h) 
            {
            if ((GetWindowLong(h,GWL_STYLE) & 0x2f)==BS_DEFPUSHBUTTON) 
               {
               SetFocus(h);
               goto found;
               }

            h=GetNextWindow(h,GW_HWNDNEXT);
            }

          SetFocus(GetWindow(hwnd,GW_CHILD));
found:
          break;
          }

      case WM_COMMAND:
         {
         if (waiting > 0)
            {
            break;
            }

         S32 id  = LOWORD(wParam);
         S32 cmd = HIWORD(wParam);

         switch (id)
            {
            case R_EXITBTN:
               {
               PostQuitMessage(0);
               break;
               }

            case R_SAVE:
               {
               C8 *fmt = NULL;

                    if (Button_GetCheck(GetDlgItem(hwnd, R_MA))) fmt = "MA";
               else if (Button_GetCheck(GetDlgItem(hwnd, R_DB))) fmt = "DB"; 
               else if (Button_GetCheck(GetDlgItem(hwnd, R_RI))) fmt = "RI"; 

               C8 *query = "OUTPDATA";
               if (ComboBox_GetCurSel(hQuery) == 1) query = "OUTPFORM";

               S32 SnP = 1;
               C8 *param = "";

                    if (ComboBox_GetCurSel(hFiletype) == 0) { SnP = 2; param = "";    }
               else if (ComboBox_GetCurSel(hFiletype) == 1) { SnP = 1; param = "S11"; } 
               else if (ComboBox_GetCurSel(hFiletype) == 2) { SnP = 1; param = "S21"; } 
               else if (ComboBox_GetCurSel(hFiletype) == 3) { SnP = 1; param = "S22"; } 

               save_SnP(SnP,
                        param,
                        query,
                        DlgItemVal(hwnd, R_ADDR), 
                        Button_GetCheck(GetDlgItem(hwnd, R_SCPI)),
                        DlgItemDbl(hwnd, R_R, 50.0), 
                        fmt, 
                        DlgItemTxt(hwnd, R_FREQ_FMT), 
                        ComboBox_GetCurSel(hDCEntry), 
                        NULL);
               break;
               }

            case R_TCHECK:
               {
               TEMPFN *TF = new TEMPFN(".s2p");
               TF_list.add(TF);

               C8 *fmt = NULL;

                    if (Button_GetCheck(GetDlgItem(hwnd, R_MA))) fmt = "MA";
               else if (Button_GetCheck(GetDlgItem(hwnd, R_DB))) fmt = "DB"; 
               else if (Button_GetCheck(GetDlgItem(hwnd, R_RI))) fmt = "RI"; 

               C8 *query = "OUTPDATA";
               if (ComboBox_GetCurSel(hQuery) == 1) query = "OUTPFORM";

               if (save_SnP(2,
                            "",
                            query,
                            DlgItemVal(hwnd, R_ADDR),
                            Button_GetCheck(GetDlgItem(hwnd, R_SCPI)),
                            DlgItemDbl(hwnd, R_R, 50.0), 
                            fmt, 
                            DlgItemTxt(hwnd, R_FREQ_FMT), 
                            ComboBox_GetCurSel(hDCEntry), 
                            TF->name))
                  {
                  show_TCheck(TF->name);
                  }

               break;
               }

            case R_SAVE_S:
               {
               S32 addr = DlgItemVal(hwnd, R_ADDR);
               S32 SCPI = Button_GetCheck(GetDlgItem(hwnd, R_SCPI));

               save_state(addr, SCPI, FALSE, FALSE);
               break;
               }

            case R_SAVE_SC:
               {
               S32 addr     = DlgItemVal(hwnd, R_ADDR);
               S32 SCPI     = Button_GetCheck(GetDlgItem(hwnd, R_SCPI));
               S32 CALIONE2 = Button_GetCheck(GetDlgItem(hwnd, R_CALIONE2));

               save_state(addr, SCPI, CALIONE2, TRUE);
               break;
               }

            case R_RECALL:
               {
               S32 addr = DlgItemVal(hwnd, R_ADDR);
               S32 SCPI = Button_GetCheck(GetDlgItem(hwnd, R_SCPI));

               recall_state(addr, SCPI);
               break;
               }

            case R_PRESET:
               {
               S32 addr = DlgItemVal(hwnd, R_ADDR);

               S32 SCPI = Button_GetCheck(GetDlgItem(hwnd, R_SCPI));
               preset(addr, SCPI);
               break;
               }

            case R_FPRESET:
               {
               S32 addr = DlgItemVal(hwnd, R_ADDR);

               S32 SCPI = Button_GetCheck(GetDlgItem(hwnd, R_SCPI));
               factory_preset(addr, SCPI);
               break;
               }
            }

         break;
         }

      case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
      }

   return DefWindowProc(hwnd,message,wParam,lParam);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int PASCAL WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR _lpszCmdLine, int nCmdShow)
{
   INITCOMMONCONTROLSEX initex = { 0 };
   initex.dwSize = sizeof(initex);
   initex.dwICC = ICC_PROGRESS_CLASS;
   InitCommonControlsEx(&initex);

   hourglass = LoadCursor(0, IDC_WAIT);

   WNDCLASS wndclass;

   if (!hPrevInstance)
      {
      wndclass.lpszClassName = szAppName;
      wndclass.lpfnWndProc   = (WNDPROC) WndProc;
      wndclass.style         = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
      wndclass.hInstance     = hInstance;
      wndclass.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON)); 
      wndclass.hCursor       = LoadCursor(NULL,IDC_ARROW);
      wndclass.hbrBackground = (HBRUSH)(COLOR_3DFACE+1);
      wndclass.cbClsExtra    = 0;
      wndclass.cbWndExtra    = DLGWINDOWEXTRA;
      wndclass.lpszMenuName  = NULL;

      RegisterClass(&wndclass);
      }

   hwnd = CreateDialog(hInstance,szAppName,0,0);

   if (hwnd==0) 
      {
      show_last_error("Couldn't create dialog box");
      return(0);
      }

   //
   // Dialog box uses WndProc in window class, not DlgProc, so WM_INITDIALOG is not sent...
   //

   hFiletype = GetDlgItem(hwnd, R_FILETYPE); 
   EnableWindow(hFiletype, TRUE);
   ShowWindow(hFiletype, TRUE);

   SendMessage(hFiletype,  CB_RESETCONTENT, 0, (LPARAM) 0);
   SendMessage(hFiletype,  CB_ADDSTRING, 0, (LPARAM) ".S2P");
   SendMessage(hFiletype,  CB_ADDSTRING, 0, (LPARAM) ".S1P (S11)");
   SendMessage(hFiletype,  CB_ADDSTRING, 0, (LPARAM) ".S1P (S21)");
   SendMessage(hFiletype,  CB_ADDSTRING, 0, (LPARAM) ".S1P (S22)");
   SendMessage(hFiletype, CB_SETCURSEL, 0, 0);

   hQuery = GetDlgItem(hwnd, R_QUERY); 
   EnableWindow(hQuery, TRUE);
   ShowWindow(hQuery, TRUE);

   SendMessage(hQuery,  CB_RESETCONTENT, 0, (LPARAM) 0);
   SendMessage(hQuery,  CB_ADDSTRING, 0, (LPARAM) "OUTPDATA");
   SendMessage(hQuery,  CB_ADDSTRING, 0, (LPARAM) "OUTPFORM");
   SendMessage(hQuery, CB_SETCURSEL, 0, 0);

   HWND hFreqFmt = GetDlgItem(hwnd, R_FREQ_FMT); 
   EnableWindow(hFreqFmt, TRUE);
   ShowWindow(hFreqFmt, TRUE);

   SendMessage(hFreqFmt,  CB_RESETCONTENT, 0, (LPARAM) 0);
   SendMessage(hFreqFmt,  CB_ADDSTRING, 0, (LPARAM) "Hz");
   SendMessage(hFreqFmt,  CB_ADDSTRING, 0, (LPARAM) "kHz");
   SendMessage(hFreqFmt,  CB_ADDSTRING, 0, (LPARAM) "MHz");
   SendMessage(hFreqFmt,  CB_ADDSTRING, 0, (LPARAM) "GHz");
   SendMessage(hFreqFmt, CB_SETCURSEL, 3, 0);

   hDCEntry = GetDlgItem(hwnd, R_DC); 
   EnableWindow(hDCEntry, TRUE);
   ShowWindow(hDCEntry, TRUE);

   SendMessage(hDCEntry,  CB_RESETCONTENT, 0, (LPARAM) 0);
   SendMessage(hDCEntry,  CB_ADDSTRING, 0, (LPARAM) "None");
   SendMessage(hDCEntry,  CB_ADDSTRING, 0, (LPARAM) "Unity mag");
   SendMessage(hDCEntry, CB_SETCURSEL, 0, 0);

   hProgress = GetDlgItem(hwnd, R_PROGRESS);
   SendMessage(hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 99));
   ShowWindow(hProgress, FALSE);

   DlgItemPrintf(hwnd, R_ADDR, "16");
   DlgItemPrintf(hwnd, R_R,    "50");
   Button_SetCheck(GetDlgItem(hwnd, R_MA), TRUE);

   GPIB_CTYPE type = GPIB_connection_type();

   if (type == GC_OTHER_LAN)     // probably a FieldFox
      {
      Button_SetCheck(GetDlgItem(hwnd, R_SCPI), TRUE);   
      }

   //
   // Register shutdown handler for resource cleanup
   //

   atexit(shutdown);

   ShowWindow(hwnd, nCmdShow);

   //
   // Arrange timer service for terminal window
   //
   // This allows for continued service even when the window is being dragged
   //

   SetTimer(hwnd, 1, TIMER_SERVICE_MS, NULL);

   //
   // Main message loop
   //

   for (;;)
      {
      if (!serve())
         {
         break;
         }

      //
      // Allow other threads a chance to run
      //

      Sleep(10);
      }

   return 1;
}

