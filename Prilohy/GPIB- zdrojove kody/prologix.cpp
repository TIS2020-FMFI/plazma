//############################################################################
//##                                                                        ##
//##  PROLOGIX.CPP                                                          ##
//##                                                                        ##
//##  Prologix board configurator for KE5FX GPIB Toolkit                    ##
//##                                                                        ##
//##  V1.00 of 17-Mar-07: Initial                                           ##
//##                                                                        ##
//##  john@miles.io                                                         ##
//##                                                                        ##
//############################################################################

#include <stdio.h>
#include <stdlib.h>
#include <dos.h>
#include <string.h>
#include <conio.h>
#include <ctype.h>
#include <assert.h>
#include <time.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>
#include <mmsystem.h>
#include <windowsx.h>
#include <setupapi.h>

#include "prores.h"
#include "typedefs.h"

#include "gpiblib/comblock.h"
#include "gpiblib/ni488.h"

#ifndef SIO_UDP_CONNRESET
   #define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR,12)
#endif

extern "C" 
{
__declspec(dllimport) HWND WINAPI RealChildWindowFromPoint(HWND hwndParent, POINT ptParentClientCoords);
}

//
// Configuration
//

#define VERSION              "1.18"
#define MAX_PORTS             256
#define MAX_SOCKETS           128
#define TIMER_SERVICE_MS      100
#define N_LINES               100
#define MAX_INCOMING_TEXT_LEN 65536
#define HOVER_HELP_MS         500
#define DEFAULT_BAUD          115200

//
// GPIB-ETHERNET equates
//

#define NETFINDER_PORT          3040
#define NF_MAGIC                0x5A
#define NF_IDENTIFY             0
#define NF_IDENTIFY_REPLY       1
#define NF_ASSIGNMENT           2
#define NF_ASSIGNMENT_REPLY     3
#define NF_FLASH_ERASE          4
#define NF_FLASH_ERASE_REPLY    5
#define NF_BLOCK_SIZE           6
#define NF_BLOCK_SIZE_REPLY     7
#define NF_BLOCK_WRITE          8
#define NF_BLOCK_WRITE_REPLY    9
#define NF_VERIFY               10
#define NF_VERIFY_REPLY         11
#define NF_REBOOT               12

enum IP_TYPE
{
    IP_TYPE_DYNAMIC = 0x00,
    IP_TYPE_STATIC = 0x01
};

typedef struct NETDISPLAY_ENTRY 
{
   unsigned char alert_level;
   
   unsigned int event1_days;
   unsigned char event1_hours;
   unsigned char event1_minutes;   
   unsigned char event1_seconds;

   IP_TYPE ip_type;
   unsigned char mac[6];
   unsigned char ip[4];
   unsigned short port; // Get port from UDP header
   unsigned char subnet[4];
   unsigned char gateway[4];
   
} 
NETDISPLAY_ENTRY;

//
// Misc globals
//

char szAppName[]="Prologix";

HWND hwnd;
HCURSOR hourglass;

S32 baud_rate = DEFAULT_BAUD;
S32 inhibit_NI = FALSE;
S32 inhibit_net = FALSE;

//
// List of ports, populated by enumerate_ports()
//

enum EPTYPE
{
   EP_TCP,
   EP_SERIAL,
   EP_NI,
};

struct ENUMERATED_PORT
{
   C8     name              [512];
   C8     interface_settings[512];
   EPTYPE type;

   C8     MAC[6];    // Valid for EP_TCP type
};

ENUMERATED_PORT enum_ports[MAX_PORTS];
S32             n_enum_ports = 0;

SOCKET broadcast_sockets[MAX_SOCKETS] = { INVALID_SOCKET };
S32    n_broadcast_sockets = 0;

U16    broadcast_rand;

//
// Currently-selected port object (TCP/IP or RS232)
//

COMBLOCK *C;

//
// Board state
//

enum BOARD_MODE
{
   BM_DEVICE,
   BM_CONTROLLER
};

enum EOS_MODE
{
   EOS_CRLF,
   EOS_CR,
   EOS_LF,
   EOS_NONE
};

struct BOARD_STATE
{
   BOOL communicable;   // TRUE if valid Prologix GPIB-USB or GPIB-ETHERNET; FALSE if NI or otherwise invalid
   S32  port_index;     // what port we're using (-1 if not valid)

   BOOL has_312c;       // (these are always TRUE for GPIB-ETHERNET) 
   BOOL has_440;        //
   BOOL has_V4;         //

   BOARD_MODE mode;
   S32 addr;

   S32 auto_rd;
   S32 assert_EOI;
   S32 receive_EOT;
   S32 EOT_char;
   S32 mrtime;

   EOS_MODE EOS;
};

BOARD_STATE current;

// *****************************************************************************
//
// Misc. Windows file utilities
//
// *****************************************************************************

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
      ::printf("%s\n",text);
      }

   virtual void message_printf(TFMSGLVL level,
                               C8 *fmt,
                               ...)
      {
      C8 buffer[4096];

      va_list ap;

      va_start(ap, 
               fmt);

      memset(buffer, 0, sizeof(buffer));

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

//
// Encapsulate various Windows file/directory behavior and policies
//

#include <shlobj.h>

struct APPDIRS
{
   C8 EXE     [MAX_PATH];    // Directory containing .exe corresponding to current process       
   C8 DOCS    [MAX_PATH];    // Directory where user documents are to be loaded/saved by default 
   C8 ICW     [MAX_PATH];    // Current working directory when object was created
   C8 VLDATA  [MAX_PATH];    // Directory for user-specific vendor .INI files and data
   C8 VCDATA  [MAX_PATH];    // Directory for vendor .INI files and data common to all users
   C8 LOCDATA [MAX_PATH];    // Directory for user-specific application data (not good for .INI files unless they're genuinely user-specific)
   C8 COMDATA [MAX_PATH];    // Directory for application .INI files common to all users
   C8 DESKTOP [MAX_PATH];    // User's desktop

   static void trailslash(C8 *target)
      {
      if (target[strlen(target)-1] != '\\')
         {
         strcat(target,"\\");
         }
      }

   APPDIRS()
      {
      EXE    [0] = 0;     
      DOCS   [0] = 0;     
      ICW    [0] = 0;
      VLDATA [0] = 0;
      VCDATA [0] = 0;
      LOCDATA[0] = 0;
      COMDATA[0] = 0;
      DESKTOP[0] = 0;
      }

   bool initialized(void)
      {
      return (EXE[0] != 0);
      }

   void init(C8 *vendor_name=NULL,     // Vendor name that refers to VLDATA and VCDATA subdirectories beneath %localappdata% and %programdata%
             C8 *app_name=NULL)        // App name used to reference LOCDATA and COMDATA subdirectories underneath VLDATA and VCDATA
      {
      //
      // Get CWD and .exe directories
      //

      if (GetCurrentDirectory(sizeof(ICW), ICW))
         trailslash(ICW);

      if (!GetModuleFileName(NULL, EXE, sizeof(EXE)-1))
         strcpy(EXE,".\\");
      else
         {
         C8 *path = strrchr(EXE,'\\');

         if (path != NULL)
            path[1] = 0;
         else
            strcpy(EXE,".\\");
         }

      //
      // See http://msdn.microsoft.com/en-us/library/ms995853.aspx for data and settings
      // management guidelines.  Vista replaced CSIDL identifiers with GUIDs but the
      // legacy functions are still used for backwards compatibility
      //
      // Furthermore, SHGetSpecialFolderPath() was deprecated in favor of SHGetFolderPath(), 
      // but the latter has various Windows header/macro-definition dependencies, and 
      // there doesn't seem to be any real reason to switch to it
      //
      // Typical paths on WinXP:
      //
      //    CSIDL_PERSONAL          c:\documents and settings\johnm\my documents
      //    CSIDL_DESKTOPDIRECTORY  c:\documents and settings\johnm\desktop
      //
      // Note 3 different application data locations:
      //
      //    CSIDL_LOCAL_APPDATA     c:\documents and settings\johnm\local settings (hidden)\application data
      //    CSIDL_APPDATA           c:\documents and settings\johnm\application data
      //    CSIDL_COMMON_APPDATA    c:\documents and settings\all users\application data (hidden)
      //
      // On Windows 7:
      //
      //    CSIDL_PERSONAL          c:\users\johnm\documents
      //    CSIDL_DESKTOPDIRECTORY  c:\users\johnm\desktop
      //
      //    CSIDL_LOCAL_APPDATA     c:\users\johnm\appdata\local    (%localappdata% in both OS and Inno Setup)
      //    CSIDL_APPDATA           c:\users\johnm\appdata\roaming  (%appdata%)
      //    CSIDL_COMMON_APPDATA    c:\ProgramData                  (%programdata%, or %commonappdata% in Inno Setup)
      //

      C8 local_appdata [MAX_PATH] = "";
      C8 common_appdata[MAX_PATH] = "";
      C8 mydocs        [MAX_PATH] = "";

      SHGetSpecialFolderPath(HWND_DESKTOP,
                             DESKTOP,
                             CSIDL_DESKTOPDIRECTORY,
                             FALSE);

      SHGetSpecialFolderPath(HWND_DESKTOP,
                             mydocs,
                             CSIDL_PERSONAL,
                             FALSE);

      SHGetSpecialFolderPath(HWND_DESKTOP,
                             local_appdata,
                             CSIDL_LOCAL_APPDATA, 
                             FALSE);

      SHGetSpecialFolderPath(HWND_DESKTOP,
                             common_appdata,
                             CSIDL_COMMON_APPDATA, 
                             FALSE);

      trailslash(DESKTOP);
      trailslash(mydocs);
      trailslash(local_appdata);
      trailslash(common_appdata);

      //
      // Create paths to vendor- and app-specific common data directories
      //
      // These directories are assumed to have been created at install time and
      // are available for all users
      //

      strcpy(VCDATA, common_appdata);

      if ((vendor_name != NULL) && (vendor_name[0] != 0))
         {
         strcat(VCDATA, vendor_name);
         trailslash(VCDATA);
         CreateDirectory(VCDATA, NULL);
         }

      strcpy(COMDATA, VCDATA);

      if ((app_name != NULL) && (app_name[0] != 0))
         {
         strcat(COMDATA, app_name);
         trailslash(COMDATA);
         CreateDirectory(COMDATA, NULL);
         }

      //
      // Create paths to vendor- and app-specific local data directories,
      // e.g., c:\documents and settings\johnm\local settings\application data\vendor_name\app_name\
      //
      // These directories may not have been created at install time if a different user 
      // (e.g, an administrator) ran the installer
      //

      strcpy(VLDATA, local_appdata);

      if ((vendor_name != NULL) && (vendor_name[0] != 0))
         {
         strcat(VLDATA, vendor_name);
         trailslash(VLDATA);
         CreateDirectory(VLDATA, NULL);
         }

      strcpy(LOCDATA, VLDATA);

      if ((app_name != NULL) && (app_name[0] != 0))
         {
         strcat(LOCDATA, app_name);
         trailslash(LOCDATA);
         CreateDirectory(LOCDATA, NULL);
         }

      //
      // By default, for development convenience, the docs directory is set
      // to the current working directory.  This also permits use of the "Start In" field
      // in a desktop shortcut.
      //
      // If the CWD is the Windows desktop, contains "Program Files", or could not 
      // be determined, the default docs directory is set to the current user's My Documents folder
      //

      strcpy(DOCS, ICW);

      if ((!strlen(DOCS)) 
           ||
          (!_stricmp(DOCS, DESKTOP)) 
           ||
          (strstr(DOCS,"Program Files") != NULL))
         {
         strcpy(DOCS, mydocs);   

         SetCurrentDirectory(DOCS);
         }
      }
};

APPDIRS global_dirs;
C8 INI_pathname[MAX_PATH];

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
                "RAD Error", 
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

C8 *address_string(sockaddr_in *address, 
                   S32          show_port)
{
   static C8 result[512];

   if (show_port)
      {
      wsprintf(result,"%s:%d",
         inet_ntoa(address->sin_addr),
             ntohs(address->sin_port));
      }
   else
      {
      wsprintf(result,"%s",
         inet_ntoa(address->sin_addr));
      }

   return result;
}

void UDP_enumerator_startup(void)
{
   if (n_broadcast_sockets > 0)
      {
      return;
      }

   //
   // Start WinSock and init empty list of broadcast sockets
   //

   WSADATA wsadata;

   WORD wRequestVer = MAKEWORD(2,2);

   if (WSAStartup(wRequestVer, &wsadata))
      {
      show_last_error("Couldn't open WinSock");
      return;
      }

   n_broadcast_sockets = 0;

   //
   // Get list of network adapters on local machine 
   //

   SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);

   if (sock == INVALID_SOCKET)
      {
      show_last_error("socket() failed, code %d",WSAGetLastError());
      }

   INTERFACE_INFO info[MAX_SOCKETS] = { 0 };
   DWORD dwSize = sizeof(info);

   DWORD result = WSAIoctl(sock,
                           SIO_GET_INTERFACE_LIST,
                           NULL,
                           0,
                           info,
                           dwSize,
                          &dwSize,
                           NULL, 
                           NULL);

   closesocket(sock);
   sock = INVALID_SOCKET;

   if (result != 0)
      {
      show_last_error("WSAIoctl() failed, code %d",WSAGetLastError());
      return;
      }

   //
   // Create and bind a non-blocking broadcast socket to each interface that supports them
   // (use of INADDR_ANY is likely to fail on multihomed machines)
   //

   S32 n = dwSize / sizeof(info[0]);

   for (S32 i=0; i < n; i++)
      {
      if (!(info[i].iiFlags & IFF_BROADCAST))
         {
         continue;
         }

      printf("Interface #%d: %s\n",i, address_string(&info[i].iiAddress.AddressIn, FALSE));

      sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
   
      if (sock == INVALID_SOCKET)
         {
         show_last_error("socket() failed, code %d",WSAGetLastError());
         return;
         }

      sockaddr_in service;
      service.sin_family      = AF_INET;
      service.sin_addr.s_addr = info[i].iiAddress.AddressIn.sin_addr.s_addr;
      service.sin_port        = htons(0);             // Use an ephemeral port to receive replies

      if (bind(sock, (SOCKADDR*) &service, sizeof(service)) == SOCKET_ERROR) 
         {
         show_last_error("bind() failed, code %d",WSAGetLastError());
         closesocket(sock);
         return;
         }

      BOOL broadcast = TRUE;

      if (setsockopt(sock, 
                     SOL_SOCKET, 
                     SO_BROADCAST, 
           (char *) &broadcast, 
                     sizeof(broadcast)) != 0)
         {
         show_last_error("Unable to set SO_BROADCAST, code %d",WSAGetLastError());
         closesocket(sock);
         return;
         }

      DWORD dwVal = 1;

      ioctlsocket(sock,
                  FIONBIO,
                 &dwVal);

      //
      // From http://support.microsoft.com/default.aspx?scid=kb;en-us;263823
      // Disable "new behavior" using IOCTL: SIO_UDP_CONNRESET. Without this call,
      // recvfrom() can fail, repeatedly, after an unreceived sendto() call
      //

      DWORD dwBytesReturned = 0;
      BOOL  bNewBehavior = FALSE;

      WSAIoctl(sock, 
               SIO_UDP_CONNRESET,
              &bNewBehavior, 
               sizeof(bNewBehavior),
               NULL, 
               0, 
              &dwBytesReturned,
               NULL, 
               NULL);

      //
      // Add socket to list
      //

      broadcast_sockets[n_broadcast_sockets++] = sock;
      }

   //
   // Generate random cookie for multiple-host disambiguation
   //

   srand((unsigned) time(NULL));

   do
      {
      broadcast_rand = rand();
      } 
   while (broadcast_rand == 0);
}

void UDP_enumerator_shutdown(void)
{
   for (S32 i=0; i < n_broadcast_sockets; i++)
      {
      if (broadcast_sockets[i] != INVALID_SOCKET)
         {
         closesocket(broadcast_sockets[i]);
         broadcast_sockets[i] = INVALID_SOCKET;
         }
      }

   n_broadcast_sockets = 0;
}

void UDP_enumerator_broadcast(void)
{
   unsigned char buff[12] = { 0 };

   buff[0] = NF_MAGIC;
   buff[1] = NF_IDENTIFY;

   buff[2] = (broadcast_rand >> 8);
   buff[3] = (broadcast_rand & 0x00FF);
   
   memset(&buff[4], 0xFF, 6);

   sockaddr_in UDP_address;          
   memset(&UDP_address, 0, sizeof(UDP_address));
   
   UDP_address.sin_family      = AF_INET;
   UDP_address.sin_addr.s_addr = htonl(INADDR_BROADCAST);      
   UDP_address.sin_port        = htons(NETFINDER_PORT);

   //
   // Broadcast via all available interfaces
   //
   // We don't trap Winsock errors here because they can result from
   // innocuous causes (such as an unreachable gateway)
   //

   for (S32 i=0; i < n_broadcast_sockets; i++)
      {
      sendto(broadcast_sockets[i],
      (C8 *) buff,
             sizeof(buff),
             0,    
            (sockaddr *) &UDP_address,
             sizeof(UDP_address));
      }
}

void UDP_enumerator_listen(S32 msecs)
{                    
   HCURSOR norm = SetCursor(hourglass);

   //
   // Listen for specified amount of time
   //

   for (S32 pp=0; (pp < (msecs/10)) && (n_enum_ports < MAX_PORTS); pp++)
      {
      Sleep(10);

      U8 buffer[4096] = { 0 };
      sockaddr_in UDP_address = { 0 };

      //
      // Check for replies on every supported interface
      //

      for (S32 b=0; b < n_broadcast_sockets; b++)
         {
         S32 UDP_addr_size = sizeof(UDP_address);
         memset(&UDP_address, 0, UDP_addr_size);
         memset(buffer, 0, sizeof(buffer));

         S32 recv_bytes = recvfrom(broadcast_sockets[b],
                           (C8 *)  buffer,
                                   sizeof(buffer),
                                   0,
                     (SOCKADDR *) &UDP_address,
                                  &UDP_addr_size);

         if (recv_bytes == SOCKET_ERROR)
            {
            S32 WSA_error = WSAGetLastError();

            if (WSA_error != WSAEWOULDBLOCK)
               {
               show_last_error("recvfrom() failed during enumeration, code %d", WSA_error);
               return;
               }

            continue;
            }

         if (buffer[0] != 0)
            {
            break;
            }
         }

      //
      // Verify that this reply corresponds to our request
      //
      // If buffer[0] is still 0 after checking all interfaces, wait 10 msec and try again
      //

      if ((buffer[0] != NF_MAGIC)                 || 
          (buffer[1] != NF_IDENTIFY_REPLY)        || 
          (buffer[2] != (broadcast_rand >> 8))    || 
          (buffer[3] != (broadcast_rand & 0x00FF)))
         {
         continue;      
         }

      //
      // Disregard duplicate replies (each packet will arrive twice, as a 
      // workaround for delayed ACK transmission on the host)
      //

      S32 dupe = FALSE;

      for (S32 k = 0; k < n_enum_ports; k++)
         {
         ENUMERATED_PORT *E = &enum_ports[k];

         if (E->type != EP_TCP) continue;

         if (!memcmp(E->MAC, &buffer[4], 6))
            {
            dupe = TRUE;
            break;
            }
         }

      if (dupe) 
         {
         continue;     
         }

      //
      // Parse reply packet
      //

      NETDISPLAY_ENTRY entry;
      
      entry.alert_level = buffer[18];
      entry.port        = UDP_address.sin_port;

      S32 i = 12;
      
      entry.event1_days  = buffer[i++];
      entry.event1_days |= ((S32) buffer[i++] << 8);

      entry.event1_hours   = buffer[i++];
      entry.event1_minutes = buffer[i++];   
      entry.event1_seconds = buffer[i++];

      entry.ip_type = (buffer[19] == 0) ? IP_TYPE_DYNAMIC : IP_TYPE_STATIC;

      i = 4;

      entry.mac[0] = buffer[i++];
      entry.mac[1] = buffer[i++];
      entry.mac[2] = buffer[i++];
      entry.mac[3] = buffer[i++];
      entry.mac[4] = buffer[i++];
      entry.mac[5] = buffer[i++];

      i = 20;

      entry.ip[0] = buffer[i++];
      entry.ip[1] = buffer[i++];
      entry.ip[2] = buffer[i++];
      entry.ip[3] = buffer[i++];
      
      entry.subnet[0] = buffer[i++];
      entry.subnet[1] = buffer[i++];
      entry.subnet[2] = buffer[i++];
      entry.subnet[3] = buffer[i++];

      entry.gateway[0] = buffer[i++];
      entry.gateway[1] = buffer[i++];
      entry.gateway[2] = buffer[i++];
      entry.gateway[3] = buffer[i++];

      //
      // Add entry to table
      //

      sockaddr_in TCP_address;

      memset(&TCP_address, 0, sizeof(TCP_address));
      memcpy(&TCP_address.sin_addr, entry.ip, sizeof(struct in_addr));

      TCP_address.sin_port = htons(1234);

      _snprintf(enum_ports[n_enum_ports].name,
                sizeof(enum_ports[n_enum_ports].name),
                "Prologix GPIB-ETHERNET (%s)", 
                address_string(&TCP_address, FALSE));

      _snprintf(enum_ports[n_enum_ports].interface_settings,
                sizeof(enum_ports[n_enum_ports].interface_settings), 
                "%s",
                address_string(&TCP_address, TRUE));

      enum_ports[n_enum_ports].type = EP_TCP;

      memcpy(&enum_ports[n_enum_ports].MAC, &buffer[4], 6);

      SendDlgItemMessage(hwnd, R_PORTLIST, LB_ADDSTRING, 0, (LPARAM) enum_ports[n_enum_ports++].name);
      } 

   SetCursor(norm);
}

void enumerate_ports(void)
{
   //
   // Assume no ports available
   //

   n_enum_ports = 0;

   SendDlgItemMessage(hwnd, R_PORTLIST, LB_RESETCONTENT, 0, 0);
   SetDlgItemText(hwnd, R_BOARDID, "");
   UpdateWindow(hwnd);

   //
   // Get GUIDs for serial port classes (which we assume won't exceed MAX_PORTS)
   //

   GUID guid_list[MAX_PORTS];
   DWORD n_guids = 0;

   SetupDiClassGuidsFromNameA("Ports", 
                               guid_list, 
                               MAX_PORTS, 
                              &n_guids);

   if (n_guids != 0)
      {
      //
      // Create device information set based on setup class (first GUID in list)
      //

      HDEVINFO hDevInfoSet = SetupDiGetClassDevsA(guid_list, 
                                                  NULL, 
                                                  NULL, 
                                                  DIGCF_PRESENT);
      if (hDevInfoSet != INVALID_HANDLE_VALUE)
         {
         //
         // Enumerate the device information set
         //

         SP_DEVINFO_DATA devInfo;
         devInfo.cbSize = sizeof(SP_DEVINFO_DATA);

         S32 index = 0;

         while (SetupDiEnumDeviceInfo(hDevInfoSet, 
                                      index, 
                                     &devInfo))
            {
            //
            // Get the registry key which stores the port's settings
            //

            HKEY hDeviceKey = SetupDiOpenDevRegKey(hDevInfoSet,   
                                                  &devInfo, 
                                                   DICS_FLAG_GLOBAL, 
                                                   0, 
                                                   DIREG_DEV, 
                                                   KEY_QUERY_VALUE);
            if (hDeviceKey == 0)
               {
               ++index;
               continue;
               }

            //
            // Get port name and make sure it's an ASCIIZ string
            //

            C8 pszPortName[512];
            DWORD dwSize = sizeof(pszPortName);
            DWORD dwType = 0;

            if ((RegQueryValueEx (hDeviceKey, 
                                 "PortName", 
                                  NULL, 
                                 &dwType, 
                         (LPBYTE) pszPortName, 
                                 &dwSize) != ERROR_SUCCESS) || (dwType != REG_SZ))
               {
               RegCloseKey(hDeviceKey);
               ++index;
               continue;
               }

            RegCloseKey(hDeviceKey);

            //
            // If it looks like a COM port, add it to our list
            //

            if ((n_enum_ports < MAX_PORTS) && ((!_strnicmp(pszPortName, "COM", 3)) && (isdigit(pszPortName[3]))))
               {
               S32 port_num = atoi(&pszPortName[3]);
               C8  friendly[256];
               C8 *f = "";

               DWORD dwSize = sizeof(friendly);
               DWORD dwType = 0;

               if (SetupDiGetDeviceRegistryPropertyA(hDevInfoSet, 
                                                    &devInfo, 
                                                     SPDRP_DEVICEDESC, 
                                                    &dwType, 
                                             (PBYTE) friendly,
                                                     dwSize, 
                                                    &dwSize) && (dwType == REG_SZ))
                  {                   
                  f = friendly;

                  if (!_strnicmp(f,"_`__ ",5))    // Remove garbage from name strings in Win98SE
                     {
                     f = &f[5];
                     }
                  }

               if (strcmp(f,"Toshiba BT Port"))
                  {
                  _snprintf(enum_ports[n_enum_ports].name,
                            sizeof(enum_ports[n_enum_ports].name),
                           "COM%d (%s)", 
                            port_num,
                            f);

                  _snprintf(enum_ports[n_enum_ports].interface_settings,
                            sizeof(enum_ports[n_enum_ports].interface_settings), 
                           "COM%d:baud=%d parity=N data=8 stop=1", 
                            port_num,
                            baud_rate);

                  enum_ports[n_enum_ports].type = EP_SERIAL;

                  SendDlgItemMessage(hwnd, R_PORTLIST, LB_ADDSTRING, 0, (LPARAM) enum_ports[n_enum_ports++].name);
                  }
               }

            index++;
            }

         SetupDiDestroyDeviceInfoList(hDevInfoSet);
         }
      }

   //
   // Poll briefly for GPIB-ETHERNET adapters
   // 10-20 msec is usually enough time
   //

   if (!inhibit_net)
      {
      UDP_enumerator_startup();
      UDP_enumerator_broadcast();
      UDP_enumerator_listen(500);
      UDP_enumerator_shutdown();
      }

   //
   // Enumerate any NI488.2 adapters in GPIB0...GPIB15 range
   // 

   if (!inhibit_NI)
      {
      for (S32 i=0; (i < 16) && (n_enum_ports < MAX_PORTS); i++)
         {
         C8 GPIBID[MAX_PATH] = "GPIB";
         sprintf(&GPIBID[4], "%d", i);

         UINT err = SetErrorMode(SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS);
         S32 found = ibfind(GPIBID);             // avoid errors due to partial uninstall of old NI drivers
         SetErrorMode(err);

         if ((found >= 0) && (!(found & ERR)))
            {
            ibonl(found, 0);

            _snprintf(enum_ports[n_enum_ports].name,
                      sizeof(enum_ports[n_enum_ports].name),
                      "National Instruments adapter (%s)", 
                      GPIBID);

            _snprintf(enum_ports[n_enum_ports].interface_settings,
                      sizeof(enum_ports[n_enum_ports].interface_settings), 
                      "%s",
                      GPIBID);

            enum_ports[n_enum_ports].type = EP_NI;

            SendDlgItemMessage(hwnd, R_PORTLIST, LB_ADDSTRING, 0, (LPARAM) enum_ports[n_enum_ports++].name);
            }
         }
      }
}

void enable_control(S32 control, BOOL state)
{
   EnableWindow(GetDlgItem(hwnd, control), state);
}

void check_control(S32 control, BOOL state)
{
   Button_SetCheck(GetDlgItem(hwnd, control), state);
}

S32 read_control(S32 control)
{
   return Button_GetCheck(GetDlgItem(hwnd, control));
}

void disable_all_controls(void)
{
   enable_control(R_MODEBOX , FALSE);
   enable_control(R_MODECTRL, FALSE); 
   enable_control(R_MODEDEV , FALSE); 

   enable_control(R_ADDR    , FALSE); 

   enable_control(R_TERMBOX , FALSE); 
   enable_control(R_RECVTXT , FALSE); 
   enable_control(R_SENDBTN , FALSE); 
   enable_control(R_SENDTXT , FALSE); 

   enable_control(R_ADVBOX  , FALSE); 

   enable_control(R_IFC     , FALSE); 
   enable_control(R_CLR     , FALSE); 
   enable_control(R_TRG     , FALSE); 
   enable_control(R_LOC     , FALSE); 
// enable_control(R_LLO     , FALSE); 
   enable_control(R_SRQ     , FALSE); 
   enable_control(R_SPOLL   , FALSE); 
   enable_control(R_SPADDR  , FALSE); 
   enable_control(R_RST     , FALSE); 

   enable_control(R_AUTORD  , FALSE); 
   enable_control(R_EOI     , FALSE); 
   enable_control(R_EOTBOX  , FALSE); 
   enable_control(R_EOT     , FALSE); 
   enable_control(R_EOTCHAR , FALSE); 
   enable_control(R_EOSBOX  , FALSE); 
   enable_control(R_EOSCRLF , FALSE); 
   enable_control(R_EOSCR   , FALSE); 
   enable_control(R_EOSLF   , FALSE); 
   enable_control(R_EOSNONE , FALSE); 
   enable_control(R_GPIBTXT , FALSE); 
   enable_control(R_ASCTXT  , FALSE); 
   enable_control(R_DEFBTN  , FALSE); 
   enable_control(R_SERBTN  , FALSE); 

   enable_control(R_MANREAD , FALSE); 
   enable_control(R_MRTIME  , FALSE); 

   UpdateWindow(hwnd);
}

void DlgItemPrintf(S32 control,
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

// ---------------------------------------------------------------------------
// Update CONNECT.INI file with changed user options, where applicable
// ---------------------------------------------------------------------------

void update_INI(void)
{
   FILE *read_options = fopen(INI_pathname,"rt");

   if (read_options == NULL)
      {
      return;
      }

   TEMPFILE tempfile(".ini", "wt", FALSE);

   if (!tempfile.active)
      {
      return;
      }

   while (1)
      {
      //
      // Read input line and make a copy
      //

      C8 linbuf[512] = { 0 };

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
         if (fputs(original_line, tempfile.file) == EOF)
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
         if ((!isspace((U8) linbuf[i])) && (linbuf[i] != ';'))      // (Removes trailing semicolons as well as leading ones)
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
         if (fputs(original_line, tempfile.file) == EOF)
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

      S32 da = -1;
      if (current.has_312c) da = 1;

      C8 modified_line[512];
      modified_line[0] = 0;

      if (!_stricmp(lexeme,"interface_settings"))      
         {
         if (current.port_index == -1)                            // if we don't have a valid device, default to NI
            sprintf(modified_line,"interface_settings  GPIB0\n");
         else
            sprintf(modified_line,"interface_settings  %s\n",enum_ports[current.port_index].interface_settings);
         }
      else if (!_stricmp(lexeme,"is_Prologix"))       
         {
         sprintf(modified_line,"is_Prologix    1\n");             // (OK to leave this set to 1 if NI board in use)
         }

      if (modified_line[0])
         {
         if (fputs(modified_line, tempfile.file) == EOF)
            {
            _fcloseall();
            return;
            }
         }
      else
         {
         if (fputs(original_line, tempfile.file) == EOF)
            {
            _fcloseall();
            return;
            }
         }
      }

   //
   // Overwrite original .INI file with our modified copy
   //

   _fcloseall();

   CopyFile(tempfile.name,
            INI_pathname,
            FALSE);
}

// ---------------------------------------------------------------------------
// add_line()
// ---------------------------------------------------------------------------

HWND status;

static C8  scroll[N_LINES * 256];
static U32 scrolllen=0;
static U32 lines=0;

void add_line(C8 *buf)
{
   U32 len = (S32) strlen(buf);

   //
   // Remove initial line(s) from buffer until there's room to fit the new
   // text
   //

   while ((scrolllen+len+3 >= sizeof(scroll)) || (lines >= N_LINES))
      {
      U32 i = 0;

      while (scroll[i] != 10)
         {
         i++;
         }

      memcpy(scroll,scroll+i+1,scrolllen-i);
      scrolllen-=i+1;
      --lines;
      }

   //
   // Add new text, prefixed by CR/LF if buffer not empty
   //

   if (scrolllen)
      {
      memcpy(scroll+scrolllen,"\r\n",3); 
      scrolllen += 2;
      }

   strcpy(scroll+scrolllen,buf);
   scrolllen += len;
   ++lines;

   //
   // Display buffer contents
   //

   SetWindowText(status,scroll);
   SendMessage(status,EM_SETSEL,scrolllen,scrolllen);
   SendMessage(status,EM_SCROLLCARET,0,0);
}

// ---------------------------------------------------------------------------
// Flush any pending received data
// ---------------------------------------------------------------------------

S32 flush_serial_port(void)
{
   if (C == NULL)
      {
      return 0;
      }

   S32 n=0;

   C8 *res = NULL;

   //
   // Issue repetitive read operations with 100-millisecond timeouts,
   // continuing until one of the reads times out or fails, or a total 
   // of 2 seconds is spent in the loop
   // 
   // If we receive data for more than 2 seconds in this loop, the instrument is 
   // probably flooding the line with zeroes (e.g., HP 8568A rev B 
   // in local mode).  Address it to talk by sending a blank line, which stops 
   // the flood... and retry the flush operation once before giving up altogether
   //

   U32    start          = timeGetTime();
   BOOL flood_detected = FALSE;

   while (1)
      {
      if ((timeGetTime() - start) >= 2048) 
         {
         if (flood_detected)
            {
            break;
            }

         C->send("\r\n");

         start = timeGetTime();
         flood_detected = TRUE;
         }

      S32         b = 0;
      TERM_REASON r;

      C->receive(-1,-1,100,100,NULL,&b,&r);

      n += b;

      if (r == SR_ERROR)
         {
         current.communicable = FALSE;
         exit(1);
         }

      if ((r == SR_TIMEOUT) && (b == 0))
         {
         break;
         }
      }

   return n;
}

// ---------------------------------------------------------------------------
// Write ASCII string to board
// ---------------------------------------------------------------------------

S32 __cdecl serial_printf(C8 *fmt, ...)
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
   // Optionally remove any trailing whitespace, append CR/LF, and transmit to port
   //

#if 0
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
#endif

#if 0
   strcat(work_string,"\r\n");
#endif

   S32 result = C->send(work_string, -1, 500);

   return result;
}

// ---------------------------------------------------------------------------
// Read ASCII string from board
// ---------------------------------------------------------------------------

C8 *read_serial_string(S32 timeout   = 500, 
                       C8  term_char = '\n')
{
   static C8 buffer[MAX_INCOMING_TEXT_LEN];
   buffer[0] = 0;

   TERM_REASON reason;
   S32 cnt = 0;

   C8 *result = (C8 *) C->receive(sizeof(buffer)-1,
                                  term_char,
                                  timeout,
                                  50,
                                  NULL,
                                 &cnt,
                                 &reason);

   if (reason == SR_ERROR)
      {
      current.communicable = FALSE;
      exit(1);
      }

   if ((reason == SR_TIMEOUT) || (result == NULL))
      {
      return buffer;
      }

   memcpy(buffer, result, cnt);
   buffer[cnt] = 0;

   return buffer;
}

// ---------------------------------------------------------------------------
// Check for text from board, and send it to the terminal window
// Returns TRUE if any text was received
// ---------------------------------------------------------------------------

BOOL poll_for_incoming_text(void)
{
   static C8 linebuf[MAX_INCOMING_TEXT_LEN];
   linebuf[0] = 0;
   S32 d = 0;

   S32 timeout = 1;
   HCURSOR norm = 0;

   while (1)
      {
      if (d >= MAX_INCOMING_TEXT_LEN-1)
         {
         break;
         }

      C8 *result = read_serial_string(timeout,'\n');

      if (!result[0])
         {
         break;
         }

      if (timeout == 1)
         {
         norm = SetCursor(hourglass);
         timeout = 250;
         }

      S32 l = strlen(result);

      for (S32 i=0; i < l; i++)
         {
         C8 c = result[i];

         if ((c == 13) || (c == 10))
            {
            if ((c == 13) || ((c == 10) && ((i == 0) || (result[i-1] != 13))))
               {
               linebuf[d++] = 0;
               add_line(linebuf);
               d = 0;
               }                                     
            }
         else
            {
            linebuf[d++] = c;
            }
         }
      }
                 
   if (d > 0)
      {
      linebuf[d++] = 0;
      add_line(linebuf);
      d = 0;
      }

   if (timeout == 250)
      {
      SetCursor(norm);
      return TRUE;
      }

   return FALSE;
}

// ---------------------------------------------------------------------------
// Send query to board and return reply as both string and numeric value
// ---------------------------------------------------------------------------

S32 query_board(C8  *query, 
                C8 **reply      = NULL, 
                S32  timeout_ms = 100)
{
   serial_printf("++");  // send start of command string to shut up the incoming data
   Sleep(25);            // wait for those chars to go out
   flush_serial_port();  // now drain the serial input buffers
   serial_printf("%s\r\n",query, FALSE);  // complete the command
   
   C8 *result = read_serial_string();

   if (reply != NULL)
      {
      *reply = result;
      }

   if (!(result[0]))
      {
      return 0;
      }

   return atoi(result);
}

// ---------------------------------------------------------------------------
// Fill in a BOARD_STATE instance by querying the hardware state
//
// Also enable/disable UI controls based on board version
// ---------------------------------------------------------------------------

void board_to_controls(void)
{
   if (!current.has_V4)
      {
      current.mode = BM_CONTROLLER;       // (else we wouldn't have been able to detect it!)
      }
   else
      {
      current.mode = (BOARD_MODE) query_board("mode");
      }

   switch (current.mode)
      {
      case BM_CONTROLLER:
         check_control(R_MODECTRL, TRUE);
         check_control(R_MODEDEV,  FALSE);
         break;

      case BM_DEVICE:
         check_control(R_MODEDEV,  TRUE);
         check_control(R_MODECTRL, FALSE);
         break;
      }

   current.addr = query_board("addr");
   DlgItemPrintf(R_ADDR, "%d", current.addr);         

   C8 text[512] = "";

   GetDlgItemText(hwnd,
                  R_SPADDR,
                  text,
                  sizeof(text));

   if ((current.has_312c) && (current.mode == BM_CONTROLLER))
      {
      if ((text[0] == 0) )
         {
         DlgItemPrintf(R_SPADDR, "%d", current.addr);      // fill in a valid address if blank
         }
      }
   else
      {
      DlgItemPrintf(R_SPADDR, "", current.addr);
      }

   if (current.mode == BM_CONTROLLER)
      {
      current.auto_rd = query_board("auto");
      check_control(R_AUTORD, current.auto_rd);
      }

   current.assert_EOI = query_board("eoi");
   check_control(R_EOI, current.assert_EOI);

   if (!current.has_V4)
      {
      current.receive_EOT = FALSE;
      current.EOT_char    = 0;
      DlgItemPrintf(R_EOTCHAR, "");
      }
   else
      {
      current.receive_EOT = query_board("eot_enable");
      check_control(R_EOT, current.receive_EOT);

      current.EOT_char = query_board("eot_char");
      DlgItemPrintf(R_EOTCHAR, "%d", current.EOT_char);
      }

   current.EOS = (EOS_MODE) query_board("eos");

   switch (current.EOS)
      {
      case EOS_CRLF:
         check_control(R_EOSCRLF, TRUE); 
         check_control(R_EOSCR,   FALSE);
         check_control(R_EOSLF,   FALSE);
         check_control(R_EOSNONE, FALSE);
         break;

      case EOS_CR:
         check_control(R_EOSCRLF, FALSE); 
         check_control(R_EOSCR,   TRUE);
         check_control(R_EOSLF,   FALSE);
         check_control(R_EOSNONE, FALSE);
         break;

      case EOS_LF:
         check_control(R_EOSCRLF, FALSE); 
         check_control(R_EOSCR,   FALSE);
         check_control(R_EOSLF,   TRUE);
         check_control(R_EOSNONE, FALSE);
         break;

      case EOS_NONE:
         check_control(R_EOSCRLF, FALSE); 
         check_control(R_EOSCR,   FALSE);
         check_control(R_EOSLF,   FALSE);
         check_control(R_EOSNONE, TRUE);
         break;
      }

   if ((current.has_440) && (current.mode == BM_CONTROLLER))
      {
      current.mrtime = query_board("read_tmo_ms");
      DlgItemPrintf(R_MRTIME, "%d", current.mrtime);
      }
   else
      {
      DlgItemPrintf(R_MRTIME, "");
      }

   enable_control(R_MODECTRL, current.has_V4);
   enable_control(R_MODEDEV,  current.has_V4);
   enable_control(R_EOTBOX,   current.has_V4); 
   enable_control(R_EOT,      current.has_V4); 
   enable_control(R_EOTCHAR,  current.has_V4 && current.receive_EOT);
   enable_control(R_ASCTXT,   current.has_V4); 
   enable_control(R_AUTORD,   current.has_312c && (current.mode == BM_CONTROLLER));
   enable_control(R_SPOLL   , current.has_312c && (current.mode == BM_CONTROLLER)); 
   enable_control(R_SPADDR  , current.has_312c && (current.mode == BM_CONTROLLER)); 
   enable_control(R_MANREAD , current.has_440 && (current.mode == BM_CONTROLLER) && (!current.auto_rd));
   enable_control(R_MRTIME  , current.has_440 && (current.mode == BM_CONTROLLER) && (!current.auto_rd));

   enable_control(R_MODEBOX , TRUE);  
   enable_control(R_TERMBOX , TRUE); 
   enable_control(R_RECVTXT , TRUE); 
   enable_control(R_SENDBTN , TRUE); 
   enable_control(R_SENDTXT , TRUE); 
   enable_control(R_ADVBOX  , TRUE); 
   enable_control(R_HELPBOX , TRUE); 

   enable_control(R_ADDR    , TRUE); 
   enable_control(R_GPIBTXT , TRUE); 

   enable_control(R_DEFBTN  , (current.mode == BM_CONTROLLER) || (current.has_V4));
   enable_control(R_IFC     , current.mode == BM_CONTROLLER); 
   enable_control(R_CLR     , current.mode == BM_CONTROLLER);
   enable_control(R_TRG     , current.mode == BM_CONTROLLER);
   enable_control(R_LOC     , current.mode == BM_CONTROLLER);
// enable_control(R_LLO     , current.mode == BM_CONTROLLER && (current.has_440));
   enable_control(R_SRQ     , current.mode == BM_CONTROLLER);
   enable_control(R_RST     , TRUE); 

   enable_control(R_EOI     , TRUE); 
   enable_control(R_EOSBOX  , TRUE); 
   enable_control(R_EOSCRLF , TRUE); 
   enable_control(R_EOSCR   , TRUE); 
   enable_control(R_EOSLF   , TRUE); 
   enable_control(R_EOSNONE , TRUE); 

   enable_control(R_SERBTN  , INI_pathname[0] != 0);
   enable_control(R_EDITBTN , INI_pathname[0] != 0);

   UpdateWindow(hwnd);
}

// ---------------------------------------------------------------------------
// Send text from terminal transmission-string input control
// ---------------------------------------------------------------------------

void send_text(void)
{
   C8 text[512] = "";

   flush_serial_port();

   if (GetDlgItemText(hwnd,
                      R_SENDTXT,
                      text,
                      sizeof(text)))
      {
      serial_printf("%s\r\n",text);

      if (!_stricmp(text,"++rst"))
         {
         HCURSOR norm = SetCursor(hourglass);
         Sleep(7000);
         SetCursor(norm);
         }

      if ((text[0] == '+') && (text[1] == '+'))
         {
         if (_stricmp(text,"++read"))
            {
            Sleep(100);
            poll_for_incoming_text();
            board_to_controls();
            }
         }
      }
}

// ---------------------------------------------------------------------------
// Poll control values and update the hardware if any have changed
// ---------------------------------------------------------------------------

void controls_to_board(void)
{
   S32 changed = FALSE;

   HWND focus = GetFocus();

#if 0    // If the receive window can't obtain the focus, we can't cut and paste text from it
   if (focus == GetDlgItem(hwnd, R_RECVTXT))
      {
      SetFocus(GetDlgItem(hwnd, R_SENDTXT));
      }
#endif

   if (read_control(R_MODECTRL) && (current.mode != BM_CONTROLLER))
      {
      serial_printf("++mode 1\r\n"); Sleep(100);
      current.auto_rd = -1;
      changed = TRUE;
      }

   if (read_control(R_MODEDEV) && (current.mode != BM_DEVICE))
      {
      serial_printf("++mode 0\r\n"); Sleep(100); 
      changed = TRUE;
      }

   C8 text[512] = "";

   if (focus != GetDlgItem(hwnd, R_ADDR))    // (don't try to read address while it's being changed)
      {
      if (GetDlgItemText(hwnd,
                         R_ADDR,
                         text,
                         sizeof(text)))
         {
         S32 v = atoi(text);

         if ((v >= 0) && (v <= 30))
            {
            if (v != current.addr)
               {
               serial_printf("++addr %d\r\n",v); Sleep(100); 
               changed = TRUE;
               }
            }
         }
      }

   if (focus != GetDlgItem(hwnd, R_MRTIME))    // (don't try to read manual-read timeout while it's being changed)
      {
      if (GetDlgItemText(hwnd,
                         R_MRTIME,
                         text,
                         sizeof(text)))
         {
         S32 v = atoi(text);

         if (v < 500)
            {
            v = 500;
            DlgItemPrintf(R_MRTIME, "500");
            }
         else if (v > 4000)
            {
            v = 4000;
            DlgItemPrintf(R_MRTIME, "4000");
            }

         if (v != current.mrtime)
            {
            serial_printf("++read_tmo_ms %d\r\n",v); Sleep(100); 
            changed = TRUE;
            }
         }
      }

   S32 auto_rd = read_control(R_AUTORD);

   if ((auto_rd != current.auto_rd) && (current.mode == BM_CONTROLLER))
      {
      serial_printf("++auto %d\r\n",auto_rd); Sleep(100); 
      changed = TRUE;
      }

   S32 assert_EOI = read_control(R_EOI);

   if (assert_EOI != current.assert_EOI)
      {
      serial_printf("++eoi %d\r\n",assert_EOI); Sleep(100); 
      changed = TRUE;
      }

   S32 receive_EOT = read_control(R_EOT);

   if (receive_EOT != current.receive_EOT)
      {
      serial_printf("++eot_enable %d\r\n",receive_EOT); Sleep(100); 
      changed = TRUE;
      }

   if (focus != GetDlgItem(hwnd, R_EOTCHAR))    // (don't try to read EOT char while it's being changed)
      {
      if (GetDlgItemText(hwnd,
                         R_EOTCHAR,
                         text,
                         sizeof(text)))
         {
         S32 v = atoi(text);

         if (v != current.EOT_char)
            {
            serial_printf("++eot_char %d\r\n",v); Sleep(100); 
            changed = TRUE;
            }
         }
      }

   EOS_MODE EOS = EOS_NONE;

        if (read_control(R_EOSCRLF)) EOS = EOS_CRLF;
   else if (read_control(R_EOSCR))   EOS = EOS_CR;
   else if (read_control(R_EOSLF))   EOS = EOS_LF;

   if (EOS != current.EOS)
      {
      serial_printf("++eos %d\r\n",EOS); Sleep(100);
      changed = TRUE;
      }
   
   if (changed)
      {
      board_to_controls();
      }
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
      case R_PORTLIST:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "Select the COM port or IP address corresponding to the desired Prologix or National Instruments GPIB controller.\
\n\nLegacy Prologix adapters equipped with DIP switches must be set to Controller mode, or they will not be detected.");
         break;
         }

      case R_MODECTRL:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "Configures the Prologix adapter to act as a GPIB controller, allowing communication with a remote device \
at the specified address.");
         break;
         }

      case R_MODEDEV:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "Configures the Prologix adapter to act as a GPIB device at the specified address.\n\nIn this mode, the host PC can \
talk and listen to other devices when commanded to do by a GPIB controller, but cannot initiate control of a remote device on its own.");
         break;
         }

      case R_ADDR:
         {
         if (current.mode == BM_CONTROLLER)
            {
            SetDlgItemText(hwnd, R_HELPTXT, "In Controller mode, this is the address of a remote device on the GPIB bus with which the host PC will communicate.\n\n\
The GPIB Toolkit applications can control the instrument address automatically, so it does not need to be set here for use with programs like 7470, PN and SSM.");
            }
         else
            {
            SetDlgItemText(hwnd, R_HELPTXT, "In Device mode, this is the GPIB address assigned to the Prologix adapter.\nRemote devices may initiate communication with the adapter at this address.\
\n\nWhen using the Prologix adapter for plotter emulation, keep in mind that the default address for many HP-GL/2 plotters is 5.");
            }
         break;
         }

      case R_RECVTXT:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "This window contains ASCII text received from the remote device, or from the Prologix\
 adapter itself.  You can highlight and copy text from the window.\n\nThe terminal window supports only Prologix-based communication, not NI488.2 or other serial devices.");
         break;
         }

      case R_SENDTXT:
      case R_SENDBTN:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "Text from this field will be transmitted to the remote device (or to the Prologix adapter\
 itself) when you press the Send button or the Enter key.");
         break;
         }

      case R_REFBTN:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "Refreshes the list of available devices.  This button should be pressed after connecting or\
 disconnecting any devices.");
         break;
         }

      case R_IFC:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "Causes the Prologix GPIB adapter to issue an Interface Clear (IFC) operation on the bus.");
         break;
         }

      case R_CLR:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "Causes the Prologix GPIB adapter to issue a Device Clear (CLR) operation on the bus.");
         break;
         }

      case R_TRG:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "Causes the Prologix GPIB adapter to issue a Device Trigger (TRG) operation on the bus.");
         break;
         }

      case R_LOC:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "Causes the Prologix GPIB adapter to issue a Reset to Local (LOC) operation that will return the addressed device to local control.");
         break;
         }

      case R_LLO:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "Causes the Prologix GPIB adapter to issue a Local Lock Out (LLO) operation that will disable local control of the device.\n\nAvailable on V4.40 and later GPIB-USB firmware.");
         break;
         }

      case R_SRQ:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "Causes the Prologix GPIB adapter to read the Service Request (SRQ) signal on the GPIB bus.\
\n\nA status of '1', as displayed in the Terminal window above, indicates that at least one device is requesting service.");
         break;
         }

      case R_SPOLL:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "Causes the Prologix GPIB adapter to perform a Serial Poll of the device at the specified address.\n\nDevices which support serial polling will return a status value to the Terminal window above.");
         break;
         }

      case R_SPADDR:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "Specifies the device which should be serial-polled.  This does not have to be the same device as the one currently addressed by the Prologix controller.");
         break;
         }

      case R_RST:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "Causes the Prologix adapter to undergo a cold-restart cycle.  Should not normally be necessary.\n\nThis operation requires approximately 7 seconds.  GPIB-ETHERNET adapter connections will be closed.");
         break;
         }

      case R_AUTORD:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "When selected, the Prologix adapter will automatically address the device to talk after each command is sent to it.\n\nMost applications should not need to alter this setting.");
         break;
         }

      case R_EOI:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "When selected, the Prologix adapter will assert the EOI (End Or Interrupt) signal with the last byte it transmits to the remote device.");
         break;
         }

      case R_EOT:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "When selected, the Prologix adapter will automatically return the specified character when the remote device issues an EOI (End Or Interrupt) signal.\n\
This option is particularly useful when working with devices that do not return linefeed-terminated strings.");
         break;
         }

      case R_EOTCHAR:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "The ASCII code, in decimal, to be returned as the End Of Text character when EOT reception is enabled.");
         break;
         }

      case R_EOSCRLF:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "Causes the Prologix adapter to append a CR/LF pair to all outgoing transmissions.\nAny user-supplied CR/LF characters are removed.\n\n\
This setting does not affect transmissions to the adapter from the host PC.  Such transmissions must always be terminated with an LF.");
         break;
         }

      case R_EOSCR:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "Causes the Prologix adapter to append a CR (ASCII 13) to all outgoing transmissions.  Any user-supplied CR/LF characters are removed.\n\n\
This setting does not affect transmissions to the adapter from the host PC.  Such transmissions must always be terminated with an LF.");
         break;
         }

      case R_EOSLF:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "Causes the Prologix adapter to append an LF (ASCII 10) to all outgoing transmissions.  Any user-supplied CR/LF characters are removed.\n\n\
This setting does not affect transmissions to the adapter from the host PC.  Such transmissions must always be terminated with an LF.");
         break;
         }

      case R_EOSNONE:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "When selected, the Prologix adapter will not append any CR/LF codes to outgoing transmissions.  Any user-supplied CR/LF characters are removed.\n\n\
This setting does not affect transmissions to the adapter from the host PC.  Such transmissions must always be terminated with an LF.");
         break;
         }

      case R_SERBTN:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "This button is available if the KE5FX GPIB Toolkit is installed.  It \
automatically updates the GPIB Toolkit's CONNECT.INI file to use the currently-selected interface.");
         break;
         }

      case R_EDITBTN:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "If your serial or TCP/IP interface is not supported by the GPIB Toolkit directly, you may be able to use it \
by editing CONNECT.INI manually to specify its connection parameters.  You can also optimize your Prologix configuration with this feature.  Refer to the comments in CONNECT.INI for further information.");
         break;
         }

      case R_DEFBTN:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "Restores the selected Prologix adapter to its factory default settings.");
         break;
         }

      case R_MRTIME:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "Sets the timeout in milliseconds for manually-initiated read operations.");
         break;
         }

      case R_MANREAD:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "Addresses the instrument to talk until EOI is received or the specified timeout period expires with no further traffic, whichever comes first.  After the read operation completes, the instrument will be unaddressed.\n\nAvailable only when 'Auto read after write' is disabled.");
         break;
         }

      case R_EXITBTN:
         {
         }

      default:
         {
         SetDlgItemText(hwnd, R_HELPTXT, "GPIB Configurator V"VERSION" of "__DATE__" by John Miles, KE5FX\nwww.ke5fx.com\
\n\nThis program may be copied and distributed freely.\nFor help with any control, simply move the mouse cursor over it!");
         break;
         }
      }
}

// ---------------------------------------------------------------------------
// Clean up resources associated with current port selection, if any
// ---------------------------------------------------------------------------

void close_port(void)
{
   current.port_index   = -1;
   current.communicable = FALSE;

   if (C != NULL)
      {
      delete C;
      C = NULL;
      }
}

// ---------------------------------------------------------------------------
// Select new serial port and configure controls to access the Prologix board
// found there, if any
// ---------------------------------------------------------------------------

void open_serial_port(S32 index)
{
   //
   // Initialize desired port and poll for a Prologix board
   //

   C8 *error = NULL;

   C = new SERBLOCK(enum_ports[index].interface_settings, &error);

   if (error != NULL)
      {
      SetDlgItemText(hwnd, R_BOARDID, error);
      }
   else
      {
      if (!serial_printf("++"))
         {
         SetDlgItemText(hwnd, R_BOARDID, "(No device found)");
         }
      else
         {
         Sleep(100);
         flush_serial_port();
         serial_printf("ver\r\n");
         C8 *ver = read_serial_string();

         if (ver[0] == 0)
            {
            serial_printf("\r\n++");
            Sleep(100);
            flush_serial_port();    // try twice before giving up
            serial_printf("ver\r\n");
            ver = read_serial_string();
            }

         if (ver[0] == 0)
            {
            SetDlgItemText(hwnd, R_BOARDID, "(No response from device)");
            }
         else
            {
            DlgItemPrintf(R_BOARDID, "%s", ver);

            C8 *v = strstr(ver,"ersion ");

            if (v != NULL)
               {
               S32 major_version;   
               S32 minor_version;
               C8  letter_version = 0;

               memset(&current, 0, sizeof(current));

               sscanf(v,"ersion %d.%d",
                  &major_version,
                  &minor_version);

               if (major_version == 3)
                  {
                  letter_version = v[11];
                  }

               if (major_version > 2) 
                  {
                  S32 n = (major_version * 100) + minor_version;

                  if (n >= 400) 
                     {
                     current.has_312c = current.has_V4 = TRUE;

                     if (n >= 440) current.has_440 = TRUE;
                     }
                  else
                     {
                     if ((n > 312) || ((n == 312) && (letter_version >= 'c')))
                        {
                        current.has_312c = TRUE;
                        }
                     }

                  board_to_controls();

                  current.port_index   = index;
                  current.communicable = TRUE;
                  }
               }
            }
         }
      }
}

// ---------------------------------------------------------------------------
// Select new TCP/IP port and configure controls to access the Prologix board
// found there, if any
// ---------------------------------------------------------------------------

void open_TCP_port(S32 index)
{
   //
   // Initialize desired port and poll for a Prologix board
   //

   C8 *error = NULL;

   C = new TCPBLOCK(enum_ports[index].interface_settings, &error, 1234);

   if (error != NULL)
      {
      SetDlgItemText(hwnd, R_BOARDID, error);
      }
   else
      {
      if (!serial_printf("++"))
         {
         SetDlgItemText(hwnd, R_BOARDID, "(No device found)");
         }
      else
         {
         Sleep(100);
         flush_serial_port();
         serial_printf("ver\r\n");
         C8 *ver = read_serial_string();

         if (ver == NULL)
            {
            SetDlgItemText(hwnd, R_BOARDID, "(No Prologix adapter found)");
            }
         else
            {
            if (ver[0])
               {
               DlgItemPrintf(R_BOARDID, "%s", ver);

               current.has_312c = current.has_V4 = current.has_440 = TRUE;
               board_to_controls();

               current.port_index   = index;
               current.communicable = TRUE;
               }
            else
               {
               SetDlgItemText(hwnd, R_BOARDID, "(No response from device)");
               }
            }
         }
      }
}

// ---------------------------------------------------------------------------
// shutdown()
// ---------------------------------------------------------------------------

void shutdown(void)
{
   UDP_enumerator_shutdown();
   close_port();
}

LRESULT WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
   HWND h;

   switch (message)
      {
      case WM_TIMER:
         {
         if (GetForegroundWindow() == hwnd)
            {
            if (current.communicable)
               {
               poll_for_incoming_text();
               controls_to_board();
               }

            update_help();
            }
         break;
         }

      case WM_INITDIALOG:
         {
         break;
         }

      case WM_SETFOCUS:
         {
         h = GetWindow(hwnd,GW_CHILD);

         while (h) 
            {
            if ((GetWindowLong(h,GWL_STYLE)&0x2f)==BS_DEFPUSHBUTTON) 
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
         S32 id  = LOWORD(wParam);
         S32 cmd = HIWORD(wParam);

         switch (id)
            {
            case R_REFBTN:
               {
               disable_all_controls();
               close_port();
               enumerate_ports();
               break;
               }

            case R_EXITBTN:
               {
               PostQuitMessage(0);
               break;
               }

            case R_EDITBTN:
               {
               C8 buffer[MAX_PATH] = { 0 };
               sprintf(buffer,"notepad \"%s\"", INI_pathname);

               Win32Exec(buffer, FALSE);
               break;
               }

            case R_SERBTN:
               {
               update_INI();

               C8 buffer[MAX_PATH] = { 0 };
               _snprintf(buffer,sizeof(buffer)-1,"Updated %s",INI_pathname);
               add_line(buffer);
               break;
               }

            case R_DEFBTN:
               {
               if ((current.mode == BM_CONTROLLER) || (current.has_V4))
                  {
                  HCURSOR norm = SetCursor(hourglass);

                  if (current.has_V4)
                     {
                     serial_printf("++mode 1\r\n");
                     Sleep(100);
                     }

                  serial_printf("++addr 5\r\n");
                  Sleep(100);

                  if (current.has_312c)
                     {
                     serial_printf("++auto 1\r\n");
                     Sleep(100);
                     }

                  if (current.has_440)
                     {
                     serial_printf("++read_tmo_ms 500\r\n");
                     Sleep(100);
                     }

                  serial_printf("++eoi 1\r\n");
                  Sleep(100);

                  serial_printf("++eos 0\r\n");
                  Sleep(100);

                  if (current.has_V4)
                     {
                     serial_printf("++eot_enable 0\r\n");
                     Sleep(100);

                     serial_printf("++eot_char 0\r\n");
                     Sleep(100);
                     }

                  board_to_controls();
                  SetCursor(norm);
                  }

               break;
               }

            case R_SENDBTN:
               {
               send_text();
               break;
               }

            case R_IFC:  
               {
               serial_printf("++ifc\r\n");
               break;
               }

            case R_CLR:  
               {
               serial_printf("++clr\r\n");
               break;
               }

            case R_TRG:  
               {
               serial_printf("++trg\r\n");
               break;
               }

            case R_LOC:  
               {
               serial_printf("++loc\r\n");
               break;
               }

            case R_LLO:  
               {
               serial_printf("++llo\r\n");
               break;
               }

            case R_SRQ:  
               {
               serial_printf("++srq\r\n");
               break;
               }

            case R_MANREAD:
               {
               serial_printf("++read\r\n");
               break;
               }

            case R_SPOLL:
               {
               C8 text[512] = "";

               GetDlgItemText(hwnd,
                              R_SPADDR,
                              text,
                              sizeof(text));

               if (text[0] != 0)
                  {
                  S32 addr = atoi(text);

                  if ((addr >= 0) && (addr <= 30))
                     {
                     serial_printf("++spoll %d\r\n",addr);
                     }
                  else
                     {
                     add_line("Error: serial poll address range must be [0-30]");
                     UpdateWindow(hwnd);
                     }
                  }

               break;
               }

            case R_RST:  
               {
               HCURSOR norm = SetCursor(hourglass);
               serial_printf("++rst\r\n");
               Sleep(7000);
               SetCursor(norm);
               board_to_controls();
               break;
               }

            case R_PORTLIST:
               {
               switch (cmd)
                  {
                  case LBN_SELCHANGE:
                     {
                     disable_all_controls();

                     S32 selection = SendDlgItemMessage(hwnd, R_PORTLIST, LB_GETCURSEL, 0, 0);
                     S32 i;

                     for (i=0; i < n_enum_ports; i++)
                        {
                        S32 str = SendDlgItemMessage(hwnd, R_PORTLIST, LB_FINDSTRING, -1, (LPARAM) enum_ports[i].name);

                        if (str == selection)
                           {
                           break;
                           }
                        }

                     close_port();

                     if (i < n_enum_ports)
                        {
                        switch (enum_ports[i].type)
                           {
                           case EP_SERIAL:
                              {
                              open_serial_port(i);
                              break;
                              }

                           case EP_TCP:
                              {
                              open_TCP_port(i);
                              break;
                              }

                           case EP_NI:
                              {
                              current.port_index   = i;
                              current.communicable = FALSE;            // (interaction with NI boards not supported)

                              enable_control(R_SERBTN,  TRUE);
                              enable_control(R_RECVTXT, TRUE);         // TODO: why?

                              SetDlgItemText(hwnd, R_BOARDID, enum_ports[i].name);
                              break;
                              }
                           }
                        }
                     else
                        {
                        current.port_index   = -1;
                        current.communicable = FALSE;         

                        enable_control(R_SERBTN,  TRUE);
                        enable_control(R_RECVTXT, TRUE);

                        SetDlgItemText(hwnd, R_BOARDID, "");
                        }
                     break;
                     }
                  }
               break;
               }
            }

         break;
         }

      case WM_CREATE:
         {
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
   global_dirs.init("KE5FX", "GPIB");

   //
   // Check for CLI options
   //

   C8 lpCmdLine[MAX_PATH];
   C8 lpUCmdLine[MAX_PATH];

   memset(lpCmdLine, 0, sizeof(lpCmdLine));
   memset(lpUCmdLine, 0, sizeof(lpUCmdLine));
   strcpy(lpCmdLine, _lpszCmdLine);
   strcpy(lpUCmdLine, _lpszCmdLine);
   _strupr(lpUCmdLine);

   baud_rate = DEFAULT_BAUD;

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
      // -baud: Set baud rate used to communicate with Prologix boards
      //

      option = strstr(lpCmdLine,"-baud:");

      if (option != NULL)
         {
         baud_rate = (S32) ascnum(&option[6], 10, &end);
         memmove(option, end, strlen(end)+1);
         continue;
         }

      //
      // -noni: Inhibit enumeration of National Instruments adapters
      //

      option = strstr(lpCmdLine,"-noni");

      if (option != NULL)
         {
         inhibit_NI = TRUE;
         memmove(option, &option[5], strlen(&option[5])+1);
         continue;
         }

      //
      // -nonet: Inhibit UDP broadcast enumeration of GPIB-ETHERNET adapters
      //

      option = strstr(lpCmdLine,"-nonet");

      if (option != NULL)
         {
         inhibit_net = TRUE;
         memmove(option, &option[6], strlen(&option[6])+1);
         continue;
         }

      //
      // Exit loop when no more CLI options are recognizable
      //

      break;
      }

   //
   // Take ownership of port semaphore to make sure GPIBLIB applications
   // aren't able to run concurrently
   //

   HANDLE hPrologixSem = CreateSemaphore(NULL,     
                                         0,        
                                         1,        
                                        "Prologix");

   if ((hPrologixSem != NULL) && (GetLastError() == ERROR_ALREADY_EXISTS))
      {
      MessageBox(0,"Port in use -- please exit from all other GPIB Toolkit applications first","Error",MB_OK | MB_ICONSTOP);
      return(0);
      }

   WNDCLASS wndclass;

   if (!hPrevInstance)
      {
      wndclass.lpszClassName = szAppName;
      wndclass.lpfnWndProc   = (WNDPROC) WndProc;
      wndclass.style         = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
      wndclass.hInstance     = hInstance;
      wndclass.hIcon         = LoadIcon(hInstance,"PROICON");
      wndclass.hCursor       = LoadCursor(NULL,IDC_ARROW);
      wndclass.hbrBackground = (HBRUSH)(COLOR_3DFACE+1);
      wndclass.cbClsExtra    = 0;
      wndclass.cbWndExtra    = DLGWINDOWEXTRA;
      wndclass.lpszMenuName  = NULL;

      RegisterClass(&wndclass);
      }

   InitCommonControls();

   hwnd = CreateDialog(hInstance,szAppName,0,NULL);

   if (hwnd==0) 
      {
      MessageBox(0,"Couldn't create dialog box.","Error",MB_OK | MB_ICONSTOP);
      return(0);
      }

   hourglass = LoadCursor(0, IDC_WAIT);
   
   status = GetDlgItem(hwnd, R_RECVTXT);
   
   disable_all_controls();

   //
   // If .INI file has not yet been created, do it now
   //
   // (Can't do this at installation time because the installer may be run
   // by a different user)
   //

   C8 INI_user_pathname[MAX_PATH] = { 0 };
   _snprintf(INI_user_pathname, sizeof(INI_user_pathname)-1, "%sconnect.ini", global_dirs.LOCDATA);

   FILE *test = fopen(INI_user_pathname,"rt");

   if (test != NULL) 
      {
      fclose(test);
      }
   else  
      {
      C8 default_INI_pathname[MAX_PATH] = { 0 } ;
      _snprintf(default_INI_pathname, sizeof(default_INI_pathname)-1, "%sdefault_connect.ini", global_dirs.EXE);

      CopyFile(default_INI_pathname, 
               INI_user_pathname,
               FALSE);
      }

   //
   // If current working directory contains an .INI file, use it in preference to the user-global one
   //

   C8 INI_working_pathname[MAX_PATH] = { 0 };
   _snprintf(INI_working_pathname, sizeof(INI_working_pathname)-1, "%sconnect.ini", global_dirs.ICW);

   test = fopen(INI_working_pathname, "rt");
   
   if (test != NULL)
      {
      strcpy(INI_pathname, INI_working_pathname);
      fclose(test);
      }
   else
      {
      test = fopen(INI_user_pathname, "rt");

      if (test != NULL)
         {
         strcpy(INI_pathname, INI_user_pathname);
         fclose(test);
         }
      else
         {
         MessageBox(0,"Couldn't find CONNECT.INI -- please reinstall the GPIB Toolkit","Error",MB_OK | MB_ICONSTOP);
         return(0);
         }
      }

   //
   // Register shutdown handler for resource cleanup
   //

   atexit(shutdown);

   ShowWindow(hwnd, nCmdShow);

   //
   // Populate list of ports 
   // 

   enumerate_ports();

#if 0
   SendDlgItemMessage(hwnd, R_PORTLIST, LB_SETCURSEL, 0, 0);
   PostMessage(hwnd, WM_COMMAND, (LBN_SELCHANGE << 16) | R_PORTLIST, 0);

   SetFocus(GetDlgItem(hwnd, R_PORTLIST));
#endif

   //
   // Arrange timer service for terminal window
   //
   // This allows for continued service even when the window is being dragged
   //

   SetTimer(hwnd, 1, TIMER_SERVICE_MS, NULL);

   //
   // Main message loop
   //

   S32 done = 0;
   MSG msg;

   while (!done)
      {
      while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
         {
         if (msg.message == WM_QUIT)
            {
            done = 1;
            break;
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

               case VK_RETURN:
                  {
                  if (current.communicable)
                     {
                     HWND focus = GetFocus();

                     if (focus == GetDlgItem(hwnd, R_SENDTXT))
                        {
                        send_text();
                        }
                     else if (focus == GetDlgItem(hwnd, R_ADDR))
                        {
                        SetFocus(GetDlgItem(hwnd, R_MODECTRL));
                        }
                     else if (focus == GetDlgItem(hwnd, R_EOTCHAR))
                        {
                        SetFocus(GetDlgItem(hwnd, R_EOSCRLF));
                        }
                     else if (focus == GetDlgItem(hwnd, R_SPADDR))
                        {
                        SetFocus(GetDlgItem(hwnd, R_MANREAD));
                        }
                     else if (focus == GetDlgItem(hwnd, R_MRTIME))
                        {
                        SetFocus(GetDlgItem(hwnd, R_RST));
                        }
                     }
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

      //
      // Allow other threads a chance to run
      //

      Sleep(10);
      }

   return 1;
}

