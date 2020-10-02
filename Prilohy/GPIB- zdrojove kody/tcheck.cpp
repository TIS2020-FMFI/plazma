//
// TCHECK.CPP: VNA performance test, based on Rohde & Schwarz application note 1EZ43
//
// Author: John Miles, KE5FX (john@miles.io)
//

#define OLD_WINCON 1

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
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
#include "xy.h"

#include "spline.cpp"
#include "sparams.cpp"
#include "gfxfile.cpp"

char szAppName[] = "KE5FX T-Check Viewer";

#define VERSION "1.56"

#define DEFAULT_IMAGE_FILE_SUFFIX "GIF"

#define XY_BKGND_COLOR   RGB_TRIPLET(116,0,70)     
#define XY_SCALE_COLOR   RGB_TRIPLET(255,255,255)
#define XY_MAJOR_COLOR   RGB_TRIPLET(0,0,0) 
#define XY_MINOR_COLOR   RGB_TRIPLET(0,0,0)
#define XY_CURSOR_COLOR  RGB_TRIPLET(192,0,192)  
#define XY_RUN_COLOR     RGB_TRIPLET(255,255,0)

//
// Screen, pane, and data-array sizes
//

#define XY_HEIGHT  480
#define X_MARGIN   60
#define Y_MARGIN   52

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
// The list of MAX_SOURCES objects allows additional traces to be
// overlaid atop the first one loaded
//

const S32 MAX_SOURCES = 4;

U32 XY_TRACE_COLOR[MAX_SOURCES] = 
   {
   RGB_TRIPLET(255,255,255),
   RGB_TRIPLET(0,255,255),
   RGB_TRIPLET(255,0,255),
   RGB_TRIPLET(255,255,0)
   };

struct SOURCE
{
   struct SPARAMS *S;
   
   C8 filename[MAX_PATH];
   C8 pathname[MAX_PATH];

   DOUBLE workspace[4096];
};

SOURCE sources[MAX_SOURCES];

XY *xy;
S32 xy_points = 0;    

DOUBLE Y_range = 25.0;

//
// Misc. globals
//

HINSTANCE hInstance;

C8 DLG_box_caption[256];

HCURSOR waitcur;

U16 transparent_font_CLUT[256];
S32 font_height;

S32 done = 0;     // goes high when quit command received

bool spline_interp = TRUE;

//
// Menu command tokens
//

#define IDM_QUIT         100
#define IDM_LOAD         110
#define IDM_SAVE         115
#define IDM_SCREENSHOT   133
#define IDM_PRINT        134
#define IDM_CLOSE        140
#define IDM_CLOSE_ALL    141
#define IDM_DELETE       142
#define IDM_SPLINE       200
#define IDM_Y_INC        210
#define IDM_Y_DEC        220
#define IDM_USER_GUIDE   300
#define IDM_ABOUT        301

S32 n_loaded_sources(SOURCE **next = NULL)
{
   S32 n = 0;

   for (S32 i=0; i < MAX_SOURCES; i++)
      {
      if (sources[i].S == NULL)
         {
         if (next != NULL) 
            {
            *next = &sources[i];
            }

         break;
         }

      ++n;
      }

   return n;
}

SOURCE *last_loaded_source(void)
{
   SOURCE *last = NULL;

   for (S32 i=0; i < MAX_SOURCES; i++)
      {
      if (sources[i].S == NULL)
         {
         break;
         }

      last = &sources[i];
      }

   return last;
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
   fn.lpstrFilter       = "Touchstone files (*.S2P)\0*.S2P\0All files (*.*)\0*.*\0";
   fn.lpstrCustomFilter = NULL;
   fn.nMaxCustFilter    = 0;
   fn.nFilterIndex      = 1;
   fn.lpstrFile         = fn_buff;
   fn.nMaxFile          = sizeof(fn_buff);
   fn.lpstrFileTitle    = NULL;
   fn.nMaxFileTitle     = 0;
   fn.lpstrInitialDir   = NULL;
   fn.lpstrTitle        = "Open .S2P File";
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
   fn.lpstrDefExt       = ".S2P";
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
   fn.lpstrFilter       = "Touchstone files (*.S2P)\0*.S2P\0All files (*.*)\0*.*\0";
   fn.lpstrCustomFilter = NULL;
   fn.nMaxCustFilter    = 0;
   fn.nFilterIndex      = 1;
   fn.lpstrFile         = fn_buff;
   fn.nMaxFile          = sizeof(fn_buff);
   fn.lpstrFileTitle    = NULL;
   fn.nMaxFileTitle     = 0;
   fn.lpstrInitialDir   = NULL;
   fn.lpstrTitle        = "Save .S2P File";
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
   fn.lpstrDefExt       = "S2P";
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

BOOL get_screenshot_filename(C8 *string)
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
Image files (*.GIF)\0*.GIF\0\
Image files (*.BMP)\0*.BMP\0\
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
   AppendMenu(pop, MF_STRING, IDM_LOAD, "Load .S2P file...\tl");
   AppendMenu(pop, MF_STRING, IDM_SAVE, "Save most-recently-loaded .S2P file...\ts");
   AppendMenu(pop, MF_SEPARATOR, 0, NULL);
   AppendMenu(pop, MF_STRING, IDM_SCREENSHOT, "Save .BMP, .GIF, .TGA, or .PCX screen shot...\tS");
   AppendMenu(pop, MF_STRING, IDM_PRINT, "Print screen image\tp");
   AppendMenu(pop, MF_SEPARATOR, 0, NULL);
   AppendMenu(pop, MF_STRING, IDM_CLOSE_ALL,"Unload all loaded .S2P files \t Home");
   AppendMenu(pop, MF_STRING, IDM_CLOSE,    "Unload most-recently-loaded .S2P file \t Del");
   AppendMenu(pop, MF_STRING, IDM_DELETE,   "Unload and delete most-recently-loaded .S2P file\t Ctrl-Del");
   AppendMenu(pop, MF_SEPARATOR, 0, NULL);
   AppendMenu(pop, MF_STRING, IDM_QUIT, "Quit\tEsc");
   AppendMenu(hmenu, MF_POPUP, (UINT_PTR) pop, "&File");

   pop = CreateMenu();
   AppendMenu(pop, MF_STRING, IDM_SPLINE, "Spline interpolation\ti");
   AppendMenu(pop, MF_SEPARATOR, 0, NULL);
   AppendMenu(pop, MF_STRING, IDM_Y_INC, "Increment Y range\t]");
   AppendMenu(pop, MF_STRING, IDM_Y_DEC, "Decrement Y range\t[");
   AppendMenu(hmenu, MF_POPUP, (UINT_PTR) pop, "&Trace");

   CheckMenuItem(hmenu, IDM_SPLINE, spline_interp ? MF_CHECKED : MF_UNCHECKED);

   pop = CreateMenu();

   AppendMenu(pop, MF_STRING, IDM_USER_GUIDE,   "&Application note \t F1");
   AppendMenu(pop, MF_SEPARATOR, 0, NULL);
   AppendMenu(pop, MF_STRING, IDM_ABOUT,        "&About");

   AppendMenu(hmenu, MF_POPUP, (UINT_PTR) pop,     "&Help");

   SetMenu(hwnd,hmenu);
   DrawMenuBar(hwnd);
}

void set_Y_range(DOUBLE range)
{
   xy->set_Y_range((SINGLE)  range,
                   (SINGLE) -range,
                   "%+.0lf",
                   (SINGLE)  5.0 * 2.0F,
                   (SINGLE)  5.0);
}

void adjust_Y_range(DOUBLE offset)
{
   Y_range += offset;

   if (Y_range > 100.0) Y_range = 100.0;
   if (Y_range < 5.0) Y_range = 5.0;

   set_Y_range(Y_range);
}

//****************************************************************************
//
// Load .S2P file
//
//****************************************************************************

bool load_file(C8 *filename)
{
   //
   // Find an unused source slot
   //

   SOURCE *S = NULL;

   for (S32 s=0; s < MAX_SOURCES; s++)
      {
      S = &sources[s];

      if (S->S == NULL)
         {
         break;
         }
      }

   //
   // Load the file if one is specified; otherwise construct a dummy xy graph object 
   // with no data source
   //

   S32    n_points = 201;
   DOUBLE min_MHz  = 0.0;
   DOUBLE max_MHz  = 1000.0;

   if (filename[0])
      {
      SPARAMS *sp = new SPARAMS();

      if (!sp->read_SNP_file(filename,2))
         {
         SAL_alert_box(filename, "Error: %s", sp->message_text);
         return FALSE;
         }

      if ((sp->n_points < 1) || (sp->n_points > 1601))
         {
         SAL_alert_box("Error","File must contain between 1-1601 points (%d found)", sp->n_points);
         return FALSE;
         }

      if (!sp->T_check(S->workspace))
         {
         SAL_alert_box(filename, "Error: %s", sp->message_text);
         }

      SOURCE *prev = last_loaded_source();
         
      if (prev != NULL)
         {
         DOUBLE min_margin = max(prev->S->min_Hz, sp->min_Hz) / 10000.0;

         if ((fabs(sp->min_Hz - prev->S->min_Hz) > min_margin) ||
             (fabs(sp->max_Hz - prev->S->max_Hz) > min_margin))
             {
             SAL_alert_box("Error","All loaded files must have the same start/stop frequencies");
             return FALSE;
             }
         }

      _snprintf(S->pathname, sizeof(S->pathname)-1, "%s", filename);

      C8 *slash = strrchr(filename,'\\');

      if (slash != NULL)
         _snprintf(S->filename, sizeof(S->filename)-1, "%s", sanitize_VFX(&slash[1]));
      else
         _snprintf(S->filename, sizeof(S->filename)-1, "%s", sanitize_VFX(filename));

      n_points = sp->n_points;
      min_MHz = sp->min_Hz / 1E6;
      max_MHz = sp->max_Hz / 1E6;

      S->S = sp;
      }

   //
   // Graphics setup
   //

   if (xy != NULL)
      {
      delete xy;
      xy = NULL;
      }

   xy_points = n_points;

   while (xy_points < 650)
      {
      xy_points += n_points;
      }

   font_height = VFX_font_height(VFX_default_system_font());

   screen_h = Y_MARGIN + ((font_height+1) * n_loaded_sources()) + XY_HEIGHT + Y_MARGIN;
   screen_w = X_MARGIN + xy_points + X_MARGIN;

   screen_w = (screen_w + 3) & (~3);
   screen_h = (screen_h + 3) & (~3);

   if (!VFX_set_display_mode(screen_w,
                             screen_h,
                             16,
                             VFX_WINDOW_MODE,
                             FALSE))
      {
      SAL_alert_box("Error","Couldn't set %d x %d display mode", screen_w, screen_h);
      exit(1);
      }

   transparent_font_CLUT[0] = (U16) RGB_TRANSPARENT;
                 
   //
   // Configure window and clipping region
   //

   if (stage != NULL)
      {
      if (stage_pane != NULL)
         {
         VFX_pane_destroy(stage_pane);
         stage_pane = NULL;
         }

      if (xy_pane != NULL)
         {
         VFX_pane_destroy(xy_pane);
         xy_pane = NULL;
         }

      if (xy_outer_pane != NULL)
         {
         VFX_pane_destroy(xy_outer_pane);
         xy_outer_pane = NULL;
         }


      VFX_window_destroy(stage);
      stage = NULL;
      }

   stage = VFX_window_construct(screen_w, screen_h);

   stage_pane  = VFX_pane_construct(stage,
                                    0,
                                    0,
                                    screen_w-1,
                                    screen_h-1);

   SAL_show_system_mouse();

   //
   // Create inner and outer panes for X/Y graph
   //
   // The X/Y outer pane is the entire bottom portion of the screen 
   // containing the X/Y graph and its surrounding legend
   //

   xy_pane = VFX_pane_construct(stage,
                                X_MARGIN,
                                Y_MARGIN + ((font_height+1) * n_loaded_sources()),
                                X_MARGIN + xy_points - 1,
                                Y_MARGIN + XY_HEIGHT - 2);

   xy_outer_pane = VFX_pane_construct(stage,
                                      0,
                                      0,
                                      screen_w-1,
                                      screen_h-1);

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
   // Initialize X/Y graph objects
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

   DOUBLE span = max_MHz - min_MHz;

   xy->set_X_range(min_MHz,
                   max_MHz,
                   "%.0lf MHz",
                   span / 4.0,
                   span / 20.0);

   set_Y_range(Y_range);

   if (!xy->set_input_width(xy_points))   
      {
      SAL_alert_box("Error","Non-integral relationship between display width (%d) and # of data_points (%d)", screen_w, n_points);
      exit(0);
      }

   xy->set_graph_attributes(XY_CONNECTED_LINES,
                              XY_AVERAGE,
                              XY_AVERAGE,
                              FALSE,
                              XY_BKGND_COLOR,
                              XY_TRACE_COLOR[0],
                              XY_SCALE_COLOR,
                              XY_MAJOR_COLOR,
                              XY_MINOR_COLOR,
                              XY_CURSOR_COLOR,
                              XY_RUN_COLOR,
                              VFX_default_font_color_table(VFC_BLACK_ON_XP));

   return TRUE;
}

//****************************************************************************
//
// Unload/delete .S2P file
//
//****************************************************************************

bool unload_last_file(bool delet)
{
   SOURCE *S = last_loaded_source();

   if ((S == NULL) || (S->S == NULL))
      {
      return FALSE;
      }

   delete S->S;
   S->S = NULL;

   if (delet)
      {
      _unlink(S->pathname);
      }

   return TRUE;
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
   // Destroy X/Y graph
   //

   if (xy != NULL)
      {
      delete xy;
      xy = NULL;
      }

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
         {
         switch (wParam)
            {
            case IDM_LOAD:
               {
load_cmd:
               if (n_loaded_sources() == MAX_SOURCES)
                  {
                  SAL_alert_box("Error", "%d files already loaded -- you must use the Home key to unload all files before any others can be displayed", MAX_SOURCES);
                  break;
                  }

               FileName[0] = 0;

               if (get_open_filename(FileName))
                  {
                  load_file(FileName);
                  }

               break;
               }

            case IDM_SAVE:
               {
save_cmd:
               SOURCE *S = last_loaded_source();

               if (S == NULL)
                  {
                  SAL_alert_box("Error","No file loaded");
                  break;
                  }

               GetFileName("S2P");

               if (get_save_filename(FileName))
                  {
                  if (!S->S->write_SNP_file(FileName))
                     {
                     SAL_alert_box("Error","%s", S->S->message_text);
                     }
                  else
                     {
                     C8 *name = strrchr(FileName,'\\');

                     if (name == NULL) 
                        name = FileName;
                     else
                        name++;

                     strcpy(S->filename, name);
                     }
                  }

               break;
               }

            case IDM_SCREENSHOT:
               {
               //
               // Pass default filename to save-file dialog
               //
screenshot_cmd:
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

            case IDM_PRINT:
               PrintBackBufferToDC();
               break;

            case IDM_QUIT:
               done = 1;
               break;

            case IDM_SPLINE:
               {
interp_cmd:
               spline_interp = !spline_interp;
               main_menu_init();
               break;
               }

            case IDM_Y_INC:
               {
               adjust_Y_range(5.0);
               break;
               }

            case IDM_Y_DEC:
               {
               adjust_Y_range(-5.0);
               break;
               }

            case IDM_CLOSE:
               {
               unload_last_file(FALSE);
               break;
               }

            case IDM_CLOSE_ALL:
               {
               while (unload_last_file(FALSE));
               break;
               }

            case IDM_DELETE:
               {
               unload_last_file(TRUE);
               break;
               }

            case IDM_USER_GUIDE:
               {
               ShellExecute(NULL,                                    // hwnd
                           "open",                                   // verb
                           "http://www.ke5fx.com/tcheck.pdf",        // name
                            NULL,                                    // parms
                            NULL,                                    // dir
                            SW_SHOWNORMAL);
               break;
               }

            case IDM_ABOUT:
               {
               MessageBox(hWnd,
                         "Usage: tcheck [.S2P data file]\n\nSelect Help->Application note for more information, or see\nRohde & Schwarz \
Application Note 1EZ43, \"T-Check Accuracy Test for Vector Network Analyzers utilizing a Tee-junction.\"",
                         "T-Check Viewer version " VERSION " by John Miles, KE5FX (john@miles.io)",
                         MB_OK | MB_ICONINFORMATION);
               break;
               }

            }
         break;

      case WM_KEYDOWN:
         {
         if (GetKeyState(VK_CONTROL) & 0x8000)
            {
            switch (wParam)
               {
               case VK_DELETE: 
                  {
                  unload_last_file(TRUE);
                  break;
                  }
               }
            }
         else
            {
            switch (wParam)
               {
               case VK_F1:
                  {
                  ShellExecute(NULL,                                    // hwnd
                              "open",                                   // verb
                              "http://www.ke5fx.com/tcheck.pdf",        // name
                               NULL,                                    // parms
                               NULL,                                    // dir
                               SW_SHOWNORMAL);
                  break;
                  }

               case VK_DELETE: 
                  {
                  unload_last_file(FALSE);
                  break;
                  }

               case VK_HOME:
                  {
                  while (unload_last_file(FALSE));
                  break;
                  }
               }
            }
         break;
         }

      case WM_CHAR:

         switch (wParam)
            {
            //
            // ESC: Quit
            //

            case 27:
               {
               done = 1;
               break;
               }

            case 'l':
            case 'L':
               {
               goto load_cmd;
               break;
               }

            case 'S':
               {
               goto screenshot_cmd;
               break;
               }

            case 's':
               {
               goto save_cmd;
               break;
               }

            case 'i':
               {
               goto interp_cmd;
               break;
               }

            case ']':
               {
               adjust_Y_range(5.0);
               break;
               }

            case '[':
               {
               adjust_Y_range(-5.0);
               break;
               }


            }
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

   S32 initial_mode = VFX_WINDOW_MODE;

   main_menu_init();

   if (!load_file(lpCmdLine))
      {
      exit(0);
      }

   done       = 0;
   mouse_left = 0;

   //
   // Main loop
   //

   for (;;)
      {
      Sleep(100);
      SAL_serve_message_queue();

      if (done)
         {
         break;
         }

      update_mouse_vars();

      // ------------------------------------------------------------------
      // Lock the buffer
      // ------------------------------------------------------------------

      assert(!stage_locked_for_writing);
      VFX_lock_window_surface(stage, VFX_BACK_SURFACE);
      stage_locked_for_writing = TRUE;

      // ------------------------------------------------------------------
      // Draw X/Y graph
      // ------------------------------------------------------------------

      VFX_pane_wipe(xy_outer_pane, RGB_TRIPLET(255, 255, 255));

      S32 h = xy_pane->y1 - xy_pane->y0 + 1;
      DOUBLE cy = (h-1.0) / 2.0;

      for (DOUBLE y=0; y < h; y += 1.0)
         {
         DOUBLE pct = (fabs(cy - y) * (Y_range * 2.0)) / h;

         U32 color = RGB_TRIPLET(180,75,75);

              if (pct < 10.0) color = RGB_TRIPLET(75,180,75);
         else if (pct < 15.0) color = RGB_TRIPLET(180,180,75);

         VFX_line_draw(xy_pane,
                       0,
                       (S32) (y + 0.5),
                       xy_pane->x1,
                       (S32) (y + 0.5),
                       LD_DRAW,
                       color);
         }

      S32 ypos = GetSystemMetrics(SM_CYMENU) + 18;

      xy->draw_scale(10, 10);

      S32 n_sources = n_loaded_sources();

      for (S32 s=0; s < n_sources; s++)
         {
         xy->set_graph_attributes(XY_CONNECTED_LINES,
                                  XY_AVERAGE,
                                  XY_AVERAGE,
                                  FALSE,
                                  XY_BKGND_COLOR,
                                  XY_TRACE_COLOR[s],
                                  XY_SCALE_COLOR,
                                  XY_MAJOR_COLOR,
                                  XY_MINOR_COLOR,
                                  XY_CURSOR_COLOR,
                                  XY_RUN_COLOR,
                                  VFX_default_font_color_table(VFC_BLACK_ON_XP));

         SOURCE *S = &sources[s];

         if (S->S != NULL)
            {
            DOUBLE outspace[4096] = { 0 };

            if (spline_interp)
               {
               //
               // Spline interpolation
               //

               ispline_t(S->workspace, S->S->n_points, 
                         outspace,     xy_points);
               }
            else
               {
               //
               // Simple inverse transform, no filtering
               //

               DOUBLE ds_dx = (DOUBLE) S->S->n_points / (DOUBLE) xy_points;
               DOUBLE dd_dx = 1.0;

               DOUBLE s = 0.0;
               DOUBLE d = 0.0;

               for (S32 i=0; i < xy_points; i++)
                  {
                  outspace[(S32) d] = S->workspace[(S32) s];

                  s += ds_dx;
                  d += dd_dx;
                  }
               }

            xy->set_arrayed_points(outspace);
            xy->process_input_data();

            xy->draw_graph();

            S32 X_offset = 50;

            if (n_sources > 1)
               {
               X_offset = 100;

               VFX_rectangle_draw(stage_pane,
                                  X_MARGIN + 49,
                                  ypos,
                                  X_MARGIN + 91,
                                  ypos + font_height - 2,
                                  LD_DRAW,
                                  RGB_TRIPLET(0,0,0));

               VFX_rectangle_fill(stage_pane,
                                  X_MARGIN + 50,
                                  ypos + 1,
                                  X_MARGIN + 90,
                                  ypos + font_height - 3,
                                  LD_DRAW,
                                  XY_TRACE_COLOR[s]);
               }

            C8 string[MAX_PATH] = { 0 };

            _snprintf(string,sizeof(string)-1,"%s", S->filename);

            VFX_string_draw(stage_pane,
                            X_MARGIN + X_offset,
                            ypos,
                            VFX_default_system_font(),
                            string,
                            VFX_default_font_color_table(VFC_BLACK_ON_WHITE));

            ypos += (font_height+1);
            }
         }

      VFX_string_draw(stage_pane,
                      0,
                      (S32) (cy + 0.5) + xy_pane->y0 - (font_height/2),
                      VFX_default_system_font(),
                      "    100%  ",
                      VFX_default_font_color_table(VFC_BLACK_ON_WHITE));

      // ------------------------------------------------------------------
      // Release lock and flip surface
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

   hInstance = hInst;

   SAL_set_preference(SAL_USE_PARAMON, NO);
                                           
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

   SAL_set_preference(SAL_ALLOW_WINDOW_RESIZE, NO);
   SAL_set_preference(SAL_MAXIMIZE_TO_FULLSCREEN, NO);
   SAL_set_preference(SAL_USE_DDRAW_IN_WINDOW, NO);
   SAL_set_preference(SAL_SLEEP_WHILE_INACTIVE, 0);
   SAL_set_preference(SAL_MAX_VIDEO_PAGES, 1);
   SAL_set_preference(SAL_USE_PAGE_FLIPPING, NO);

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
   // Register exit handler 
   //

   atexit(AppExit);

   //
   // Call AppMain() function (does not return)
   //

   AppMain(lpCmdLine);

   return 0;
}
