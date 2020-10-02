// *****************************************************************************
//
// Misc. Windows file utilities
//
// *****************************************************************************

#pragma once

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
   C8       path_buffer        [MAX_PATH-32];   // leave some room for 8.3 tempname and user-supplied suffix if any
   C8       original_temp_name [MAX_PATH-32];   // e.g., c:\temp\TF43AE.tmp
   C8       name               [MAX_PATH];      // e.g., c:\temp\TF43AE.tmp.wav (if .wav was passed as suffix)
   bool     keep;
   bool     show;
   bool     active;
   TFMSGLVL verbosity;

   virtual void message_sink(TFMSGLVL level,   
                             C8      *text)
      {
      ::printf("%s\n",text);
      }

   virtual void message_printf(TFMSGLVL level,
                               C8 *fmt,
                               ...)
      {
      if (level < verbosity)
         {
         return;
         }

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

   TEMPFN(C8 *suffix = NULL, bool keep_files=FALSE, TFMSGLVL v=TF_NOTICE) 
      {
      keep = keep_files;
      active = TRUE;

      path_buffer[0]        = 0;
      original_temp_name[0] = 0;
      name[0]               = 0;
      verbosity             = v;

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

   TEMPFILE(C8      *suffix         = NULL, 
            C8      *file_operation = NULL, 
            bool     keep_files     = FALSE,
            TFMSGLVL verbosity      = TF_NOTICE) : TEMPFN (suffix, keep_files, verbosity)  
            
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
   C8 EXE         [MAX_PATH];    // Directory containing .exe corresponding to current process       
   C8 DOCS        [MAX_PATH];    // Directory where user documents are to be loaded/saved by default 
   C8 ICW         [MAX_PATH];    // Current working directory when object was created
   C8 VLDATA      [MAX_PATH];    // Directory for user-specific vendor .INI files and data
   C8 VCDATA      [MAX_PATH];    // Directory for vendor-specific .INI files and data common to all users
   C8 VCDOCS      [MAX_PATH];    // Directory for vendor-specific preinstalled data files common to all users
   C8 LOCDATA     [MAX_PATH];    // Directory for user-specific application data (not good for .INI files unless they're genuinely user-specific)
   C8 COMDATA     [MAX_PATH];    // Directory for application-specific .INI files common to all users
   C8 COMDOCS     [MAX_PATH];    // Directory for application-specific preinstalled data files common to all users
   C8 DESKTOP     [MAX_PATH];    // User's desktop

   C8 vendor_name [MAX_PATH];    // Vendor and app name passed to init()
   C8 app_name    [MAX_PATH];

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
      VCDOCS [0] = 0;
      LOCDATA[0] = 0;
      COMDATA[0] = 0;
      DESKTOP[0] = 0;
      }

   void init(const C8   *vname  = NULL,     // Vendor name that refers to VLDATA, VCDATA, and VCDOCS subdirectories beneath %localappdata% and %programdata%
             const C8   *aname     = NULL,     // App name used to reference LOCDATA, COMDATA, and COMDOCS subdirectories underneath VLDATA, VCDATA and (optionally) VCDOCS
             bool        use_VCDOCS   = TRUE)     // E.g., 3120A package puts COMDOCS under Symmetricom/3120A, not 3120A, while TimeLab doesn't use the vendor name
      {
      strcpy(vendor_name, vname);
      strcpy(app_name, aname);

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
      //    CSIDL_COMMON_DOCUMENTS  c:\documents and settings\all users\documents
      //
      // Note 3 different application data locations on XP:
      //
      //    CSIDL_LOCAL_APPDATA     c:\documents and settings\johnm\local settings (hidden)\application data
      //    CSIDL_APPDATA           c:\documents and settings\johnm\application data
      //    CSIDL_COMMON_APPDATA    c:\documents and settings\all users\application data (hidden)
      //    CSIDL_COMMON_DOCUMENTS  c:\documents and settings\all users\documents
      //
      // On Windows 7:
      //
      //    CSIDL_PERSONAL          c:\users\johnm\documents
      //    CSIDL_DESKTOPDIRECTORY  c:\users\johnm\desktop
      //
      //    CSIDL_LOCAL_APPDATA     c:\users\johnm\appdata\local    (%localappdata% in both OS and Inno Setup)
      //    CSIDL_APPDATA           c:\users\johnm\appdata\roaming  (%appdata%)
      //    CSIDL_COMMON_APPDATA    c:\ProgramData                  (%programdata%, or %commonappdata% in Inno Setup)
      //    CSIDL_COMMON_DOCUMENTS  c:\users\public\documents       (%commondocs% in Inno Setup)
      //

      C8 local_appdata [MAX_PATH] = "";
      C8 common_appdata[MAX_PATH] = "";
      C8 common_docs   [MAX_PATH] = "";
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

      SHGetSpecialFolderPath(HWND_DESKTOP,
                             common_docs,
                             CSIDL_COMMON_DOCUMENTS,
                             FALSE);

      trailslash(DESKTOP);
      trailslash(mydocs);
      trailslash(local_appdata);
      trailslash(common_appdata);
      trailslash(common_docs);

      //
      // Create paths to vendor- and app-specific common data directories
      //
      // These directories are assumed to be available for all users
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

      strcpy(VCDOCS, common_docs);

      if ((vendor_name != NULL) && (vendor_name[0] != 0))
         {
         strcat(VCDOCS, vendor_name);
         trailslash(VCDOCS);
         CreateDirectory(VCDOCS, NULL);
         }

      if (use_VCDOCS)
         strcpy(COMDOCS, VCDOCS);   
      else
         strcpy(COMDOCS, common_docs);   

      if ((app_name != NULL) && (app_name[0] != 0))
         {
         strcat(COMDOCS, app_name);
         trailslash(COMDOCS);
         CreateDirectory(COMDOCS, NULL);
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

      printf("Desktop:  %s\n",DESKTOP);
      printf("VLDATA:   %s\n",VLDATA);
      printf("VCDATA:   %s\n",VCDATA);
      printf("VCDOCS:   %s\n",VCDOCS);
      printf("LOCDATA:  %s\n",LOCDATA);
      printf("COMDATA:  %s\n",COMDATA);
      printf("COMDOCS:  %s\n",COMDOCS);
      printf("AppExe:   %s\n",EXE);
      printf("InitCWD:  %s\n",ICW);
      printf("Docs/CWD: %s\n",DOCS);
      }
};

void ____________________________String_helper_functions_____________________________________________(){}

//
// Return next whitespace character in string, or EOS
//

static C8 *next_space(C8 *src)
{
   while (*src && (!isspace((U8) *src)))
      {
      ++src;
      } 

   return src;
}

//
// Return first non-whitespace character in string
//

static C8 *skip_space(C8 *src)
{
   while (isspace((U8) *src))
      {
      ++src;
      }

   return src;
}

//
// Terminate string at the next whitespace character, and return
// a pointer to the first non-whitespace character after that
//
// If no more substrings exist, return a pointer to EOS
//

static C8 *next_substring(C8 *src)
{
   src = next_space(src);

   if (!*src)
      {
      return src;
      }

   *src++ = 0;
   return skip_space(src);
}

void ____________________________KVAL_KVSTORE________________________________________________________(){}

// -----------------------------------------------------------------------------------
// Generic .INI-style key/value database
//
// Maintains a list of ASCII key/value pairs, providing fast key lookup, 
// .INI-style backing storage, Windows dialog-box support, and some handy declaration macros
//
// Keys are ASCII strings
// Values are pointers to strings owned by the database, with optional user variables
// that can be read/written (more or less) automatically
//
// IMPORTANT: Methods that allocate/free memory should not be declared virtual, to avoid 
// accidental calls to the wrong heap routines when passing database objects between DLLs.
// Objects allocated in one module must be freed in the same module, but this is difficult
// to enforce when instances have different VMTs...
// -----------------------------------------------------------------------------------

#ifdef STDTPL_H

enum KVMSGLVL
{
   KVAL_DEBUG = 0,      // Debugging traffic
   KVAL_VERBOSE,        // Low-level notice
   KVAL_NOTICE,         // Standard notice
   KVAL_WARNING,        // Warning (does not imply failure of operation)
   KVAL_ERROR           // Error (implies failure of operation)
};

#ifndef VALSTR_LEN      // String values are 1024 bytes long by default
#define VALSTR_LEN 1023
#endif                  

const U32 KVAL_MULTIPLE  = 0x00000001;  // Database can contain more than one entry with this key
const U32 KVAL_TEMP      = 0x00000002;  // Key should not be written to any files
const U32 KVAL_READ_ONLY = 0x00000004;  // TRUE if the value can't be updated after creation
const U32 KVAL_REQUIRED  = 0x00000008;  // TRUE if a value must be specified in the list passed to from_args()

enum KVAL_TYPE
{
   KVT_NONE,            // Uninitialized
   KVT_STR,             // String
   KVT_NUM,             // Signed 32-bit int                                   
   KVT_HEX,             // Unsigned 32-bit int represented in string form as a hex value
   KVT_DBL,             // Double-precision float                              
   KVT_NUM64,           // Signed 64-bit int                                   
   KVT_BOOL,            // Boolean
#ifdef GDIPVER
   KVT_COLOR            // Instance of GDI+ Color object
#endif
};

const U32 KVF_FILE_MUST_EXIST = 0x00000001;  // Report error if file does not exist
const U32 KVF_DO_NOT_ADD      = 0x00000002;  // Do not create new entries while reading file, just update existing ones

class KVAL : public HASHLIST_CONTENTS        // Base class for contents stored in a KVSTORE
{
protected:
   C8 *value_string;                         // Cached string representation, not directly accessible to the outside world
                                              
   void release_owned_value(void)
      {
      if (value_string != NULL)
         {
         free(value_string);
         value_string = NULL;
         }
      }

public:
   U32         KV_flags;           // Database flags
   KVAL_TYPE   type;               // Stored data type
   U32         user_flags;         // App-specific flags (serialized in keydefs)
   void       *user_ptr;           // App-specific metadata
   void       *valaddr;            // Optional address of backing variable
   const void *defaddr;            // Optional address of default value in application space (not yet initialized by all types, TODO)

   void initialize(const void *V) 
      {
      HASHLIST_CONTENTS::initialize(V);

      value_string = NULL;
      valaddr      = NULL;
      defaddr      = NULL;
      user_flags   = 0;
      user_ptr     = NULL;
      KV_flags     = 0;
      type         = KVT_NONE;
      }

   void shutdown(void)    
      {
      HASHLIST_CONTENTS::shutdown();
      release_owned_value();
      }

   static bool to_bool(const C8 *string)
      {
      U8 *src = (U8 *) string;

      for (;;)
         {
         U8 v = (U8) tolower(*src++);

         if (isspace(v)) 
            {
            continue;
            }

         if (v == 0)
            {
            break;
            }

         return (v != '0') && (v != 'f');
         }

      return TRUE;      // Empty values default to TRUE 
      }

   static DOUBLE to_double(const C8 *string) // Convert string to double, handling simple rational expressions (a/b and a*b) as well
      {                                      // as literal reals
      S32 l = (S32) strlen(string);

      C8 *buff = (C8 *) alloca(l+1);         // Make a working copy of the string and remove any spaces from it before calling sscanf 
      strcpy(buff, string);                  // (this allows the user to enter notation like '10 000 000')

      for (S32 i=0; i < l; i++)              
         {
         if (buff[i] == ' ')
            {
            U8 ch = 0;
            S32 j = i+1;

            while (j < l)
               {
               ch = (U8) buff[j];
               if (ch != ' ') break;
               j++;
               }

            if ((j == l) || (ch == 0))
               {
               break;
               }

            C8 *nxt = &buff[j];
            memmove(&buff[i], nxt, strlen(nxt)+1);
            }
         }

      DOUBLE dbl_val = 0.0;
      const C8 *divisor = strchr(buff,'/');

      if (divisor != NULL)
         {
         DOUBLE a=0.0,b=1.0;
         sscanf(buff,       "%lf", &a);
         sscanf(&divisor[1],"%lf", &b);

         if (fabs(b) < 1E-30) 
            {
            return 0.0;
            }

         return a/b;
         }

      const C8 *mult = strchr(buff,'*');

      if (mult != NULL)
         {
         DOUBLE a=0.0,b=0.0;
         sscanf(buff,    "%lf", &a);
         sscanf(&mult[1],"%lf", &b);

         return a*b;
         }

      sscanf(buff,"%lf", &dbl_val);
      return dbl_val;
      }

   C8 *to_keydef(C8 *dest, S32 dest_bytes, C8 *prepend_tag = NULL)  // NULL=no tag, ""=tag is standard type ID
      {                                                             // Note that value_string must be up to date before calling
      memset(dest, 0, dest_bytes);

      if (prepend_tag == NULL)
         {
         _snprintf(dest,dest_bytes-1,"0x%08X \"%s\" %s", user_flags, name, str());
         }
      else
         {
         if (prepend_tag[0] != 0)
            {
            _snprintf(dest,dest_bytes-1,"%s 0x%08X \"%s\" %s", prepend_tag, user_flags, name, value_string); 
            }
         else
            {
            switch (type)     
               {
               case KVT_STR:   _snprintf(dest,dest_bytes-1,"STR 0x%08X \"%s\" %s", user_flags, name, value_string); break; 
               case KVT_NUM:   _snprintf(dest,dest_bytes-1,"S32 0x%08X \"%s\" %s", user_flags, name, value_string); break;  
               case KVT_HEX:   _snprintf(dest,dest_bytes-1,"HEX 0x%08X \"%s\" %s", user_flags, name, value_string); break;  
               case KVT_DBL:   _snprintf(dest,dest_bytes-1,"DBL 0x%08X \"%s\" %s", user_flags, name, value_string); break;  
               case KVT_NUM64: _snprintf(dest,dest_bytes-1,"S64 0x%08X \"%s\" %s", user_flags, name, value_string); break;  
               case KVT_BOOL:  _snprintf(dest,dest_bytes-1,"BLN 0x%08X \"%s\" %s", user_flags, name, value_string); break;
#ifdef GDIPVER
               case KVT_COLOR: _snprintf(dest,dest_bytes-1,"RGB 0x%08X \"%s\" %s", user_flags, name, value_string); break;
#endif
               default: break;
               }
            }
         }

      return dest;
      }

   void to_var(void *val)        // Copy internal string representation (which is assumed up to date) to destination variable
      {                          // Dest can be the built-in user valaddr member or an arbitrary external object
      if (val == NULL)              
         {
         return;
         }

      switch (type)     
         {
         case KVT_STR:   
            {
            strcpy((C8 *) val, value_string); 
            break;
            }

         case KVT_NUM:   
            {
            *(S32 *) val = 0;
            sscanf(value_string,"%d",(S32 *) val);
            break;
            }

         case KVT_HEX:   
            {
            *(U32 *) val = 0;
            sscanf(value_string,"0x%X",(U32 *) val);
            break;
            }

#ifdef GDIPVER
         case KVT_COLOR:
            {
            S32 A=255,R=255,G=255,B=0;

            if (toupper(value_string[0]) == 'A')
               sscanf(value_string,"ARGB(%d,%d,%d,%d)",&A,&R,&G,&B);
            else
               sscanf(value_string,"RGB(%d,%d,%d)",&R,&G,&B);

            *(Color *) val = Color(255, (BYTE) R, (BYTE) G, (BYTE) B);
            break;
            }
#endif

         case KVT_DBL:   
            {
            *(DOUBLE *) val = to_double(value_string);
            break;
            }

         case KVT_NUM64: 
            {
            *(S64 *) val = 0;
            sscanf(value_string,"%I64d", (S64 *) val);
            break;
            }

         case KVT_BOOL:  
            {
            *(bool *) val = to_bool(value_string);
            break;
            }

         default: 
            {
            break;
            }
         }
      }

   bool from_var(const void *varaddr)    // Update internal string representation from type-specific variable, if any
      {                                  // Typically (but not necessarily) this is the user valaddr or defaddr member
      if (varaddr == NULL)       
         {                          
         return FALSE;
         }

      assert(varaddr != value_string);

      bool result = FALSE;

      switch (type)     
         {
         case KVT_STR:   result = setstr  ( (const C8 *)     varaddr); break;
         case KVT_NUM:   result = setnum  (*(const S32 *)    varaddr); break;
         case KVT_HEX:   result = sethex  (*(const U32 *)    varaddr); break;
         case KVT_DBL:   result = setdbl  (*(const DOUBLE *) varaddr); break;
         case KVT_NUM64: result = setnum64(*(const S64 *)    varaddr); break;
         case KVT_BOOL:  result = setbool (*(const bool *)   varaddr); break;
#ifdef GDIPVER
         case KVT_COLOR: result = setcolor(*(const Color *)  varaddr); break;
#endif
         default: break;
         }

      return result;
      }

   bool user_var_to_string(void)
      {
      return from_var(valaddr);
      }

   bool set_default(void)
      {
      return from_var(defaddr);
      }

   bool name_match(C8 *match_name)
      {
      return !lex_compare(match_name);
      }

   const C8 *str(void)           // Value of any type can be retrieved as a string representation
      {
      return value_string;
      }

   bool setstr(const C8 *val)    // Set a value of any type, given a string initializer 
      {                          // (If initializer is NULL, set value to empty string)
      release_owned_value();
      value_string = (val == NULL) ? _strdup("") : _strdup(val);

      if (value_string == NULL)
         {
         return FALSE;
         }

      to_var(valaddr);
      return TRUE;
      }

   bool is_blank(void)
      {
      return (value_string == NULL) || (value_string[0] == 0);
      }

   bool sprintf(const C8 *fmt, ...)
      {
      va_list ap;

      va_start(ap, 
               fmt);

      bool result = vsprintf(fmt, ap);

      va_end(ap);
      return result;
      }

   bool vsprintf(const C8 *fmt, va_list ap)
      {
      release_owned_value();
      value_string = (C8 *) calloc(1,VALSTR_LEN+1);

      if (value_string == NULL)
         {
         return FALSE;
         }

      _vsnprintf(value_string,
                 VALSTR_LEN,
                 fmt,
                 ap);

      to_var(valaddr);
      return TRUE;
      }

   bool scatf(const C8 *fmt, ...)      // like sprintf(), but concatenates to existing contents if any
      {
      va_list ap;

      va_start(ap, 
               fmt);

      bool result = FALSE;

      if (is_blank())
         {
         release_owned_value();
         value_string = (C8 *) calloc(1,VALSTR_LEN+1);

         if (value_string != NULL)
            {
            _vsnprintf(value_string,
                       VALSTR_LEN,
                       fmt,
                       ap);

            to_var(valaddr);
            result = TRUE;
            }
         }
      else
         {
         C8 *old_string = _strdup(value_string);

         if (old_string != NULL)
            {
            release_owned_value();
            value_string = (C8 *) calloc(1,VALSTR_LEN+1);

            if (value_string != NULL)
               {
               strcpy(value_string, old_string);

               S32 len = strlen(value_string);

               _vsnprintf(&value_string[len],
                           max(0,VALSTR_LEN - len),
                           fmt,
                           ap);

               to_var(valaddr);
               result = TRUE;
               }

            free(old_string);
            old_string = NULL;
            }
         }
         
      va_end(ap);
      return result;
      }

   bool sgl(SINGLE *result)
      {
      if (type != KVT_DBL) return FALSE;

      DOUBLE v = 0.0;
      to_var(&v);
      *result = (SINGLE) v;
      return TRUE;
      }

   SINGLE sgl(void)
      {
      SINGLE result = 0.0;
      assert(sgl(&result));
      return result;
      }

   bool dbl(DOUBLE *result)
      {
      if (type != KVT_DBL) return FALSE;

      to_var(result);
      return TRUE;
      }

   DOUBLE dbl(void)
      {
      DOUBLE result = 0.0;
      assert(dbl(&result));
      return result;
      }

   bool setdbl(DOUBLE val)
      {
      release_owned_value();
      value_string = (C8 *) calloc(1, 32); 

      if (value_string == NULL)
         {
         return FALSE;
         }

      _snprintf(value_string, 31, "%.16lG", val);

      to_var(valaddr);
      return TRUE;
      }

   bool num(S32 *result)
      {
      if (type != KVT_NUM) return FALSE;

      to_var(result);
      return TRUE;
      }

   S32 num(void)
      {
      S32 result = 0;
      assert(num(&result));
      return result;
      }

   bool setnum(S32 val)
      {
      release_owned_value();
      value_string = (C8 *) calloc(1, 16); 

      if (value_string == NULL)
         {
         return FALSE;
         }

      _snprintf(value_string, 15, "%d", val);

      to_var(valaddr);
      return TRUE;
      }

   bool num64(S64 *result)
      {
      if (type != KVT_NUM64) return FALSE;

      to_var(result);
      return TRUE;
      }

   S64 num64(void)
      {
      S64 result = 0;
      assert(num64(&result));
      return result;
      }

   bool setnum64(S64 val)
      {
      release_owned_value();
      value_string = (C8 *) calloc(1, 32); 

      if (value_string == NULL)
         {
         return FALSE;
         }

      _snprintf(value_string, 31, "%I64d", val);

      to_var(valaddr);
      return TRUE;
      }

   bool hex(U32 *result)
      {
      if (type != KVT_HEX) return FALSE;

      to_var(result);
      return TRUE;
      }

   U32 hex(void)
      {
      U32 result = 0;
      assert(hex(&result));
      return result;
      }

   bool sethex(U32 val)
      {
      release_owned_value();
      value_string = (C8 *) calloc(1, 16); 

      if (value_string == NULL)
         {
         return FALSE;
         }

      _snprintf(value_string, 15, "0x%X", val);

      to_var(valaddr);
      return TRUE;
      }

#ifdef GDIPVER
   bool color(Color *result)
      {
      if (type != KVT_COLOR) return FALSE;

      to_var(result);
      return TRUE;
      }

   Color color(void)
      {
      Color result(255,255,255,0);
      assert(color(&result));
      return result;
      }

   bool setcolor(Color val)
      {
      release_owned_value();
      value_string = (C8 *) calloc(1, 32);

      if (value_string == NULL)
         {
         return FALSE;
         }

      _snprintf(value_string, 31, "RGB(%d,%d,%d)", val.GetR(), val.GetG(), val.GetB());

      to_var(valaddr);
      return TRUE;
      }
#endif

   bool boolean(bool *result)
      {
      if (type != KVT_BOOL) return FALSE;

      to_var(result);
      return TRUE;
      }

   bool boolean(void)
      {
      bool result = FALSE;
      assert(boolean(&result));
      return result;
      }

   bool setbool(bool val)
      {
      release_owned_value();
      value_string = (C8 *) calloc(1, 8);

      if (value_string == NULL)
         {
         return FALSE;
         }

      _snprintf(value_string, 7, "%s", val ? "True" : "False");

      to_var(valaddr);
      return TRUE;
      }
};

//
// Macros used to declare global or local variables backed by KVSTORE entries
// Typically used for .INI file maintenance and command-line parsing
// 

#define INI_BOOL(database,varname,defval)       bool   varname               = defval; struct varname##_INI_init_ { varname##_INI_init_() { database.addbool (#varname,        &varname); }} varname##_INI_container_;
#define INI_S32(database,varname,defval)        S32    varname               = defval; struct varname##_INI_init_ { varname##_INI_init_() { database.addnum  (#varname,        &varname); }} varname##_INI_container_;
#define INI_S64(database,varname,defval)        S64    varname               = defval; struct varname##_INI_init_ { varname##_INI_init_() { database.addnum64(#varname,        &varname); }} varname##_INI_container_;
#define INI_DOUBLE(database,varname,defval)     DOUBLE varname               = defval; struct varname##_INI_init_ { varname##_INI_init_() { database.adddbl  (#varname,        &varname); }} varname##_INI_container_;
#define INI_U32(database,varname,defval)        U32    varname               = defval; struct varname##_INI_init_ { varname##_INI_init_() { database.addhex  (#varname,        &varname); }} varname##_INI_container_;
#define INI_HEX(database,varname,defval)        U32    varname               = defval; struct varname##_INI_init_ { varname##_INI_init_() { database.addhex  (#varname,        &varname); }} varname##_INI_container_;
#define INI_STRING(database,varname,defval)     C8     varname[VALSTR_LEN+1] = defval; struct varname##_INI_init_ { varname##_INI_init_() { database.addstr  (#varname,         varname); }} varname##_INI_container_;
#define INI_ENUM(database,type,varname,defval)  type   varname               = defval; struct varname##_INI_init_ { varname##_INI_init_() { database.addhex  (#varname,(U32 *) &varname); }} varname##_INI_container_;

#define ARG_S32(database,varname,defval,KV_flags)    S32    ARG_##varname               = defval; struct ARG_##varname##_init_ { ARG_##varname##_init_() { database.addnum  (#varname,        &ARG_##varname, NULL, 0, KV_flags); }} ARG_##varname##_container_;
#define ARG_S64(database,varname,defval,KV_flags)    S64    ARG_##varname               = defval; struct ARG_##varname##_init_ { ARG_##varname##_init_() { database.addnum64(#varname,        &ARG_##varname, NULL, 0, KV_flags); }} ARG_##varname##_container_;
#define ARG_DOUBLE(database,varname,defval,KV_flags) DOUBLE ARG_##varname               = defval; struct ARG_##varname##_init_ { ARG_##varname##_init_() { database.adddbl  (#varname,        &ARG_##varname, NULL, 0, KV_flags); }} ARG_##varname##_container_;
#define ARG_HEX(database,varname,defval,KV_flags)    U32    ARG_##varname               = defval; struct ARG_##varname##_init_ { ARG_##varname##_init_() { database.addhex  (#varname,        &ARG_##varname, NULL, 0, KV_flags); }} ARG_##varname##_container_;
#define ARG_STRING(database,varname,defval,KV_flags) C8     ARG_##varname[VALSTR_LEN+1] = defval; struct ARG_##varname##_init_ { ARG_##varname##_init_() { database.addstr  (#varname,         ARG_##varname, NULL, 0, KV_flags); }} ARG_##varname##_container_;
#define ARG_BOOL(database,varname,defval,KV_flags)   bool   ARG_##varname               = defval; struct ARG_##varname##_init_ { ARG_##varname##_init_() { database.addbool (#varname,        &ARG_##varname, NULL, 0, KV_flags); }} ARG_##varname##_container_;

template<class KVCONTENTS> class KVSTORE
{
   void init(void)
      {
      contents.clear();
      clear_error();
      }

public:   
   HashList<KVCONTENTS> contents;      
   C8                   error[1024];

   KVSTORE(void)
      {
      }

   virtual ~KVSTORE()
      {
      }

   KVCONTENTS *operator [] (const C8 *key)    // returns NULL if lookup fails
      {
      return contents[key];
      }

   KVCONTENTS &operator [] (S32 index)        // O(1) if walking in order
      {
      return contents[index];
      }

   //
   // Parse key/definition string of form <text> <0x00000000> "key" definition
   // into separate strings
   //
   // <text> is skipped if present, *key points to the string inside the first pair of
   // quotes (excluding the quotes themselves), and *def points to the remainder
   // of the string after the second quote in the pair and any following whitespace.
   //
   // The input string is modified to zero-terminate the key at the second quote.  
   //
   // If a substring beginning with 0x appears prior to the first quote, it's
   // parsed as a hex constant and written to *flags
   //
   // Special case: if the string does not contain either a single or a double quote, 
   // it's assumed to be a standalone key with no <text>, <0x> flags, or definition.
   //
   // keydef strings are useful as an alternate interface to create key/value entries
   // based on single lines from a file (which themselves might begin with a tag)
   // 

   bool keydef(C8 *text, U32 *flags, C8 **key, C8 **def)
      {
      C8 *q1 = strchr(text,'\'');
      C8 *q2 = strchr(text,'\"');

      if ((q1 == NULL) && (q2 == NULL))
         {
         if (key   != NULL) *key = text;
         if (def   != NULL) *def = &text[strlen(text)];
         if (flags != NULL) *flags = 0;
         return TRUE;
         }

      C8 *k = NULL;

      if ((q1 != NULL) && (q2 != NULL))
         {
         k = (q1 < q2) ? q1 : q2;      // if keydef contains both ' and ", the earlier one is the key delimiter
         }
      else
         {
         k = (q1 == NULL) ? q2 : q1;   // otherwise, key delimiter is whichever one was found

         if (k == NULL)
            {
            message_printf(KVAL_ERROR,"KVAL error: malformed keydef (%s)", text);
            return FALSE;
            }
         }

      C8 *quote = k++;

      C8 *d = strchr(k, *quote);

      if (d == NULL)
         {
         message_printf(KVAL_ERROR,"KVAL error: malformed keydef (%s)", text);
         return FALSE;
         }

      *d++ = 0;

      while (isspace((U8) *d)) d++;

      if (key != NULL) *key = k;
      if (def != NULL) *def = d;

      if (flags != NULL)
         {
         C8 *f = strstr(text, "0x");

         if ((f != NULL) && (f < quote))
            sscanf(f,"0x%X",flags);
         else
            *flags = 0;
         }

      return TRUE;
      }

   //
   // Clear last-reported-error state
   // Useful before performing a series of related retrievals
   // 

   void clear_error(void)
      {
      error[0] = 0;
      }

   //
   // Error/status message sink
   //

   virtual void message_sink(KVMSGLVL level,   
                             C8      *text)
      {
      ::printf("%s\n",text);
      }

   virtual void message_printf(KVMSGLVL level,
                               C8 *fmt,
                               ...)
      {
      C8 buffer[16384] = { 0 };

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

      //
      // Keep a copy of the last error message for ease of retrieval
      // by the caller after an operation fails
      //

      if (level == KVAL_ERROR)
         {
         memset(error, 0, sizeof(error));
         strncpy(error, buffer, sizeof(error)-1);
         }

      message_sink(level,
                   buffer);
      }

   //
   // Delete the first entry (in the hash table) with specified key name
   // 

   void delete_entry(const C8 *key)
      {
      KVCONTENTS *V = contents[key];

      if (V != NULL)
         {
         contents.unlink(V);
         }
      }

   //
   // Delete the specified entry
   //

   void delete_entry(KVCONTENTS *V)
      {
      contents.unlink(V);
      }

   //
   // Delete any/all entries with specified key name
   //

   void delete_all_by_key(C8 *key)
      {
      for (;;)
         {
         KVCONTENTS *V = contents[key];

         if (V == NULL)
            {
            break;
            }

         contents.unlink(V);
         }
      }

   //
   // Delete all entries
   //

   void clear(void)
      {
      contents.clear();
      }

   //
   // Get # of entries
   //

   S32 count(void)
      {
      return contents.count();
      }

   //
   // Get # of entries with specified key name
   //

   S32 count_by_key(C8 *key)
      {
      S32 n=0;

      for (S32 i=0; i < contents.count(); i++)
         {
         if (contents[i].name_match(key))
            {
            ++n;
            }
         }

      return n;
      }

   //
   // Sort contents by name
   // 

   void sort(S32 polarity=1)
      {
      contents.sort_simple_contents(polarity);
      }

   //
   // Retrieve a name/value pair directly
   //

   KVCONTENTS *lookup(C8 *key)
      {
      return contents[key];
      }

   bool has(C8 *key)
      {
      return (contents[key] != NULL);
      }

   //
   // Retrieve a value as a string, bool, U32, S32, S64, or DOUBLE, with choice of methods
   // based on the desired error-checking level
   //

   const C8 *str(const C8 *key)
      {
      KVCONTENTS *V = contents[key];
      
      if (V == NULL)
         {
         return NULL;
         }

      return V->str();
      }

   bool dbl(const C8 *key, DOUBLE *result)
      {
      KVCONTENTS *V = contents[key];

      if (V == NULL)
         {
         return FALSE;
         }

      if (!V->dbl(result))
         {
         message_printf(KVAL_ERROR,"KVAL error: dbl(%s) incompatible type", key);
         return FALSE;
         }

      return TRUE;
      }

   DOUBLE dbl(const C8 *key, DOUBLE fallback = 0.0, bool *outcome = NULL)    
      {
      KVCONTENTS *V = contents[key];

      if (V == NULL)
         {
         if (outcome == NULL)
            {
            message_printf(KVAL_WARNING, "KVAL warning: dbl(%s) not found", key);
            return fallback;
            }

         *outcome = FALSE;
         return fallback;
         }

      DOUBLE value = fallback;
      bool result = V->dbl(&value);

      if (outcome != NULL)
         {
         *outcome = result;
         }
      else
         {
         if (!result)
            {
            message_printf(KVAL_ERROR,"KVAL error: dbl(%s) incompatible type", key);
            }
         }

      return value;
      }

   bool sgl(const C8 *key, SINGLE *result)
      {
      KVCONTENTS *V = contents[key];

      if (V == NULL)
         {
         return FALSE;
         }

      if (!V->sgl(result))
         {
         message_printf(KVAL_ERROR,"KVAL error: sgl(%s) incompatible type", key);
         return FALSE;
         }

      return TRUE;
      }

   SINGLE sgl(const C8 *key, SINGLE fallback = 0.0, bool *outcome = NULL)    
      {
      KVCONTENTS *V = contents[key];

      if (V == NULL)
         {
         if (outcome == NULL)
            {
            message_printf(KVAL_WARNING, "KVAL warning: sgl(%s) not found", key);
            return fallback;
            }

         *outcome = FALSE;
         return fallback;
         }

      SINGLE value = fallback;
      bool result = V->sgl(&value);

      if (outcome != NULL)
         {
         *outcome = result;
         }
      else
         {
         if (!result)
            {
            message_printf(KVAL_ERROR,"KVAL error: sgl(%s) incompatible type", key);
            }
         }

      return value;
      }

   bool num(const C8 *key, S32 *result)
      {
      KVCONTENTS *V = contents[key];

      if (V == NULL)
         {
         return FALSE;
         }

      if (!V->num(result))
         {
         message_printf(KVAL_ERROR,"KVAL error: num(%s) incompatible type", key);
         return FALSE;
         }

      return TRUE;
      }

   S32 num(const C8 *key, S32 fallback = 0, bool *outcome = NULL)    
      {
      KVCONTENTS *V = contents[key];

      if (V == NULL)
         {
         if (outcome == NULL)
            {
            message_printf(KVAL_WARNING, "KVAL warning: num(%s) not found", key);
            return fallback;
            }

         *outcome = FALSE;
         return fallback;
         }

      S32 value = fallback;
      bool result = V->num(&value);

      if (outcome != NULL)
         {
         *outcome = result;
         }
      else
         {
         if (!result)
            {
            message_printf(KVAL_ERROR,"KVAL error: num(%s) incompatible type", key);
            }
         }

      return value;
      }

   bool hex(const C8 *key, U32 *result)
      {
      KVCONTENTS *V = contents[key];

      if (V == NULL)
         {
         return FALSE;
         }

      if (!V->hex(result))
         {
         message_printf(KVAL_ERROR,"KVAL error: hex(%s) incompatible type", key);
         return FALSE;
         }

      return TRUE;
      }

   U32 hex(const C8 *key, U32 fallback = 0, bool *outcome = NULL)    
      {
      KVCONTENTS *V = contents[key];

      if (V == NULL)
         {
         if (outcome == NULL)
            {
            message_printf(KVAL_WARNING, "KVAL warning: hex(%s) not found", key);
            return fallback;
            }

         *outcome = FALSE;
         return fallback;
         }

      U32 value = fallback;
      bool result = V->hex(&value);

      if (outcome != NULL)
         {
         *outcome = result;
         }
      else
         {
         if (!result)
            {
            message_printf(KVAL_ERROR,"KVAL error: hex(%s) incompatible type", key);
            }
         }

      return value;
      }

   bool num64(const C8 *key, S64 *result)
      {
      KVCONTENTS *V = contents[key];

      if (V == NULL)
         {
         return FALSE;
         }

      if (!V->num64(result))
         {
         message_printf(KVAL_ERROR,"KVAL error: num64(%s) incompatible type", key);
         return FALSE;
         }

      return TRUE;
      }

   S64 num64(const C8 *key, S64 fallback = 0, bool *outcome = NULL)    
      {
      KVCONTENTS *V = contents[key];

      if (V == NULL)
         {
         if (outcome == NULL)
            {
            message_printf(KVAL_WARNING, "KVAL warning: num64(%s) not found", key);
            return fallback;
            }

         *outcome = FALSE;
         return fallback;
         }

      S64 value = fallback;
      bool result = V->num64(&value);

      if (outcome != NULL)
         {
         *outcome = result;
         }
      else
         {
         if (!result)
            {
            message_printf(KVAL_ERROR,"KVAL error: num64(%s) incompatible type", key);
            }
         }

      return value;
      }

   bool boolean(const C8 *key, bool *result)
      {
      KVCONTENTS *V = contents[key];

      if (V == NULL)
         {
         return FALSE;
         }

      if (!V->boolean(result))
         {
         message_printf(KVAL_ERROR,"KVAL error: bool(%s) incompatible type", key);
         return FALSE;
         }

      return TRUE;
      }

   bool boolean(const C8 *key, bool fallback = FALSE, bool *outcome = NULL)    
      {
      KVCONTENTS *V = contents[key];

      if (V == NULL)
         {
         if (outcome == NULL)
            {
            message_printf(KVAL_WARNING, "KVAL warning: bool(%s) not found", key);
            return fallback;
            }

         *outcome = FALSE;
         return fallback;
         }

      bool value = fallback;
      bool result = V->boolean(&value);

      if (outcome != NULL)
         {
         *outcome = result;
         }
      else
         {
         if (!result)
            {
            message_printf(KVAL_ERROR,"KVAL error: bool(%s) incompatible type", key);
            }
         }

      return value;
      }

   //
   // Create a new entry of the given type, using one of three alternative methods for each:
   //
   //    - Specify the key name and the address of a backing variable (if not NULL), plus an optional 
   //      initializer string, user flag word, and storage flag word.  If the initializer string is NULL
   //      and a backing variable is supplied, the value will be initialized from the backing variable.
   //      If both valaddr and initializer are NULL, the key will be created with an appropriate empty/zero
   //      value
   //
   //    - Specify a keydef string that encodes the key name, initial value string, and user flags, 
   //      plus optional storage flag word.  The entry is created with no backing variable.
   //      See comments for KVSTORE::keydef() for information on keydef formatting
   //
   //    - Specify a pointer to a value of the appropriate (non-string) type, plus optional storage flag word.
   //      The entry is created with no backing variable and no specific initialization string
   //
   // The operation will fail if an attempt is made to add a key that already exists, unless the existing
   // entry has the KVAL_MULTIPLE attribute
   //

   KVCONTENTS *addstr(const C8 *key, C8 *valaddr, const C8 *initializer=NULL, U32 user_flags=0, U32 KV_flags=0)
      {
      KVCONTENTS *V = NULL;

      if (!(KV_flags & KVAL_MULTIPLE))
         {
         V = contents[key];

         if (V != NULL)
            {
            message_printf(KVAL_ERROR,"KVAL error: addstr(%s) already present", key);
            return NULL;
            }
         }

      V = contents.allocate(key);

      if (V == NULL)
         {
         message_printf(KVAL_ERROR,"KVAL error: addstr(%s) out of memory", key);
         return NULL;
         }

      V->type       = KVT_STR;
      V->KV_flags   = KV_flags;
      V->user_flags = user_flags;
      V->valaddr    = valaddr;
      
      bool result = (initializer != NULL) ? V->setstr(initializer) : V->setstr(valaddr);

      if (!result)
         {
         message_printf(KVAL_ERROR,"KVAL error: addstr(%s) out of memory", key);
         return NULL;
         }
   
      return V;
      }

   KVCONTENTS *addstr(const C8 *key_def, U32 KV_flags=0)    
      {
      C8 *text = _strdup(key_def);

      if (text == NULL)
         {
         message_printf(KVAL_ERROR,"KVAL error: addstr(%s) out of memory", key_def);
         return NULL;
         }

      C8 *key=NULL, *def=NULL;
      U32 flags = 0;

      if (!keydef(text, &flags, &key, &def))
         {
         free(text);
         return NULL;
         }

      KVCONTENTS *result = addstr(key, NULL, def, flags, KV_flags);

      free(text);
      return result;
      }

   KVCONTENTS *adddbl(const C8 *key, DOUBLE val, U32 user_flags=0, U32 KV_flags=0)
      {
      KVCONTENTS *V = NULL;

      if (!(KV_flags & KVAL_MULTIPLE))
         {
         V = contents[key];

         if (V != NULL)
            {
            message_printf(KVAL_ERROR,"KVAL error: adddbl(%s) already present", key);
            return NULL;
            }
         }

      V = contents.allocate(key);

      if (V == NULL)
         {
         message_printf(KVAL_ERROR,"KVAL error: adddbl(%s) out of memory", key);
         return NULL;
         }

      V->type       = KVT_DBL;
      V->KV_flags   = KV_flags;
      V->user_flags = user_flags;
      V->valaddr    = NULL;

      bool result = V->setdbl(val);

      if (!result)
         {
         message_printf(KVAL_ERROR,"KVAL error: adddbl(%s) out of memory", key);
         return NULL;
         }

      return V;
      }

   KVCONTENTS *adddbl(const C8 *key, DOUBLE *valaddr, const C8 *initializer=NULL, U32 user_flags=0, U32 KV_flags=0)
      {
      KVCONTENTS *V = NULL;

      if (!(KV_flags & KVAL_MULTIPLE))
         {
         V = contents[key];

         if (V != NULL)
            {
            message_printf(KVAL_ERROR,"KVAL error: adddbl(%s) already present", key);
            return NULL;
            }
         }

      V = contents.allocate(key);

      if (V == NULL)
         {
         message_printf(KVAL_ERROR,"KVAL error: adddbl(%s) out of memory", key);
         return NULL;
         }

      V->type       = KVT_DBL;
      V->KV_flags   = KV_flags;
      V->user_flags = user_flags;
      V->valaddr    = valaddr;

      bool result = ((initializer != NULL) || (valaddr == NULL)) ? V->setstr(initializer) : V->setdbl(*valaddr);

      if (!result)
         {
         message_printf(KVAL_ERROR,"KVAL error: adddbl(%s) out of memory", key);
         return NULL;
         }

      return V;
      }

   KVCONTENTS *adddbl(const C8 *key_def, U32 KV_flags=0)
      {
      C8 *text = _strdup(key_def);

      if (text == NULL)
         {
         message_printf(KVAL_ERROR,"KVAL error: adddbl(%s) out of memory", key_def);
         return NULL;
         }

      C8 *key=NULL, *def=NULL;
      U32 flags = 0;

      if (!keydef(text, &flags, &key, &def))
         {
         free(text);
         return NULL;
         }

      KVCONTENTS *result = adddbl(key, NULL, def, flags, KV_flags);

      free(text);
      return result;
      }

   KVCONTENTS *addnum(const C8 *key, S32 val, U32 user_flags=0, U32 KV_flags=0)
      {
      KVCONTENTS *V = NULL;

      if (!(KV_flags & KVAL_MULTIPLE))
         {
         V = contents[key];

         if (V != NULL)
            {
            message_printf(KVAL_ERROR,"KVAL error: addnum(%s) already present", key);
            return NULL;
            }
         }

      V = contents.allocate(key);

      if (V == NULL)
         {
         message_printf(KVAL_ERROR,"KVAL error: addnum(%s) out of memory", key);
         return NULL;
         }

      V->type       = KVT_NUM;
      V->KV_flags   = KV_flags;
      V->user_flags = user_flags;
      V->valaddr    = NULL;

      bool result = V->setnum(val);

      if (!result)
         {
         message_printf(KVAL_ERROR,"KVAL error: addnum(%s) out of memory", key);
         return NULL;
         }

      return V;
      }

   KVCONTENTS *addnum(const C8 *key, S32 *valaddr, const C8 *initializer=NULL, U32 user_flags=0, U32 KV_flags=0)
      {
      KVCONTENTS *V = NULL;

      if (!(KV_flags & KVAL_MULTIPLE))
         {
         V = contents[key];

         if (V != NULL)
            {
            message_printf(KVAL_ERROR,"KVAL error: addnum(%s) already present", key);
            return NULL;
            }
         }

      V = contents.allocate(key);

      if (V == NULL)
         {
         message_printf(KVAL_ERROR,"KVAL error: addnum(%s) out of memory", key);
         return NULL;
         }

      V->type       = KVT_NUM;
      V->KV_flags   = KV_flags;
      V->user_flags = user_flags;
      V->valaddr    = valaddr;
      
      bool result = ((initializer != NULL) || (valaddr == NULL)) ? V->setstr(initializer) : V->setnum(*valaddr);

      if (!result)
         {
         message_printf(KVAL_ERROR,"KVAL error: addnum(%s) out of memory", key);
         return NULL;
         }

      return V;
      }
   
   KVCONTENTS *addnum(const C8 *key_def, U32 KV_flags=0) 
      {
      C8 *text = _strdup(key_def);

      if (text == NULL)
         {
         message_printf(KVAL_ERROR,"KVAL error: addnum(%s) out of memory", key_def);
         return NULL;
         }

      C8 *key=NULL, *def=NULL;
      U32 flags = 0;

      if (!keydef(text, &flags, &key, &def))
         {
         free(text);
         return NULL;
         }

      KVCONTENTS *result = addnum(key, NULL, def, flags, KV_flags);

      free(text);
      return result;
      }

   KVCONTENTS *addhex(const C8 *key, U32 val, U32 user_flags=0, U32 KV_flags=0)
      {
      KVCONTENTS *V = NULL;

      if (!(KV_flags & KVAL_MULTIPLE))
         {
         V = contents[key];

         if (V != NULL)
            {
            message_printf(KVAL_ERROR,"KVAL error: addhex(%s) already present", key);
            return NULL;
            }
         }

      V = contents.allocate(key);

      if (V == NULL)
         {
         message_printf(KVAL_ERROR,"KVAL error: addhex(%s) out of memory", key);
         return NULL;
         }

      V->type       = KVT_HEX;
      V->KV_flags   = KV_flags;
      V->user_flags = user_flags;
      V->valaddr    = NULL;

      bool result = V->sethex(val);

      if (!result)
         {
         message_printf(KVAL_ERROR,"KVAL error: addhex(%s) out of memory", key);
         return NULL;
         }

      return V;
      }

   KVCONTENTS *addhex(const C8 *key, U32 *valaddr, const C8 *initializer=NULL, U32 user_flags=0, U32 KV_flags=0)
      {
      KVCONTENTS *V = NULL;

      if (!(KV_flags & KVAL_MULTIPLE))
         {
         V = contents[key];

         if (V != NULL)
            {
            message_printf(KVAL_ERROR,"KVAL error: addhex(%s) already present", key);
            return NULL;
            }
         }

      V = contents.allocate(key);

      if (V == NULL)
         {
         message_printf(KVAL_ERROR,"KVAL error: addhex(%s) out of memory", key);
         return NULL;
         }

      V->type       = KVT_HEX;
      V->KV_flags   = KV_flags;
      V->user_flags = user_flags;
      V->valaddr    = valaddr;
      
      bool result = ((initializer != NULL) || (valaddr == NULL)) ? V->setstr(initializer) : V->sethex(*valaddr);

      if (!result)
         {
         message_printf(KVAL_ERROR,"KVAL error: addhex(%s) out of memory", key);
         return NULL;
         }

      return V;
      }

   KVCONTENTS *addhex(const C8 *key_def, U32 KV_flags=0)  
      {
      C8 *text = _strdup(key_def);

      if (text == NULL)
         {
         message_printf(KVAL_ERROR,"KVAL error: addhex(%s) out of memory", key_def);
         return NULL;
         }

      C8 *key=NULL, *def=NULL;
      U32 flags = 0;

      if (!keydef(text, &flags, &key, &def))
         {
         free(text);
         return NULL;
         }

      KVCONTENTS *result = addhex(key, NULL, def, flags, KV_flags);

      free(text);
      return result;
      }

   KVCONTENTS *addnum64(const C8 *key, S64 val, U32 user_flags=0, U32 KV_flags=0)
      {
      KVCONTENTS *V = NULL;

      if (!(KV_flags & KVAL_MULTIPLE))
         {
         V = contents[key];

         if (V != NULL)
            {
            message_printf(KVAL_ERROR,"KVAL error: addnum64(%s) already present", key);
            return NULL;
            }
         }

      V = contents.allocate(key);

      if (V == NULL)
         {
         message_printf(KVAL_ERROR,"KVAL error: addnum64(%s) out of memory", key);
         return NULL;
         }

      V->type       = KVT_NUM64;
      V->KV_flags   = KV_flags;
      V->user_flags = user_flags;
      V->valaddr    = NULL;

      bool result = V->setnum64(val);

      if (!result)
         {
         message_printf(KVAL_ERROR,"KVAL error: addnum64(%s) out of memory", key);
         return NULL;
         }

      return V;
      }

   KVCONTENTS *addnum64(const C8 *key, S64 *valaddr, const C8 *initializer=NULL, U32 user_flags=0, U32 KV_flags=0)
      {
      KVCONTENTS *V = NULL;

      if (!(KV_flags & KVAL_MULTIPLE))
         {
         V = contents[key];

         if (V != NULL)
            {
            message_printf(KVAL_ERROR,"KVAL error: addnum64(%s) already present", key);
            return NULL;
            }
         }

      V = contents.allocate(key);

      if (V == NULL)
         {
         message_printf(KVAL_ERROR,"KVAL error: addnum64(%s) out of memory", key);
         return NULL;
         }

      V->type       = KVT_NUM64;
      V->KV_flags   = KV_flags;
      V->user_flags = user_flags;
      V->valaddr    = valaddr;

      bool result = ((initializer != NULL) || (valaddr == NULL)) ? V->setstr(initializer) : V->setnum64(*valaddr);

      if (!result)
         {
         message_printf(KVAL_ERROR,"KVAL error: addnum64(%s) out of memory", key);
         return NULL;
         }

      return V;
      }

   KVCONTENTS *addnum64(const C8 *key_def, U32 KV_flags=0)    
      {
      C8 *text = _strdup(key_def);

      if (text == NULL)
         {
         message_printf(KVAL_ERROR,"KVAL error: addnum64(%s) out of memory", key_def);
         return NULL;
         }

      C8 *key=NULL, *def=NULL;
      U32 flags = 0;

      if (!keydef(text, &flags, &key, &def))
         {
         free(text);
         return NULL;
         }

      KVCONTENTS *result = addnum64(key, NULL, def, flags, KV_flags);

      free(text);
      return result;
      }

   KVCONTENTS *addbool(const C8 *key, bool val, U32 user_flags=0, U32 KV_flags=0)
      {
      KVCONTENTS *V = NULL;

      if (!(KV_flags & KVAL_MULTIPLE))
         {
         V = contents[key];

         if (V != NULL)
            {
            message_printf(KVAL_ERROR,"KVAL error: addbool(%s) already present", key);
            return NULL;
            }
         }

      V = contents.allocate(key);

      if (V == NULL)
         {
         message_printf(KVAL_ERROR,"KVAL error: addbool(%s) out of memory", key);
         return NULL;
         }

      V->type       = KVT_BOOL;
      V->KV_flags   = KV_flags;
      V->user_flags = user_flags;
      V->valaddr    = NULL;

      bool result = V->setbool(val);

      if (!result)
         {
         message_printf(KVAL_ERROR,"KVAL error: addbool(%s) out of memory", key);
         return NULL;
         }

      return V;
      }

   KVCONTENTS *addbool(const C8 *key, bool *valaddr, const C8 *initializer=NULL, U32 user_flags=0, U32 KV_flags=0)
      {
      KVCONTENTS *V = NULL;

      if (!(KV_flags & KVAL_MULTIPLE))
         {
         V = contents[key];

         if (V != NULL)
            {
            message_printf(KVAL_ERROR,"KVAL error: addbool(%s) already present", key);
            return NULL;
            }
         }

      V = contents.allocate(key);

      if (V == NULL)
         {
         message_printf(KVAL_ERROR,"KVAL error: addbool(%s) out of memory", key);
         return NULL;
         }

      V->type       = KVT_BOOL;
      V->KV_flags   = KV_flags;
      V->user_flags = user_flags;
      V->valaddr    = valaddr;

      bool result = ((initializer != NULL) || (valaddr == NULL)) ? V->setbool(V->to_bool(initializer)) : V->setbool(*valaddr);

      if (!result)
         {
         message_printf(KVAL_ERROR,"KVAL error: addbool(%s) out of memory", key);
         return NULL;
         }

      return V;
      }

   KVCONTENTS *addbool(const C8 *key_def, U32 KV_flags=0)   
      {
      C8 *text = _strdup(key_def);

      if (text == NULL)
         {
         message_printf(KVAL_ERROR,"KVAL error: addbool(%s) out of memory", key_def);
         return NULL;
         }

      C8 *key=NULL, *def=NULL;
      U32 flags = 0;

      if (!keydef(text, &flags, &key, &def))
         {
         free(text);
         return NULL;
         }

      KVCONTENTS *result = addbool(key, NULL, def, flags, KV_flags);

      free(text);
      return result;
      }

#ifdef GDIPVER
   KVCONTENTS *addcolor(const C8 *key, Color val, U32 user_flags=0, U32 KV_flags=0)
      {
      KVCONTENTS *V = NULL;

      if (!(KV_flags & KVAL_MULTIPLE))
         {
         V = contents[key];

         if (V != NULL)
            {
            message_printf(KVAL_ERROR,"KVAL error: addcolor(%s) already present", key);
            return NULL;
            }
         }

      V = contents.allocate(key);

      if (V == NULL)
         {
         message_printf(KVAL_ERROR,"KVAL error: addcolor(%s) out of memory", key);
         return NULL;
         }

      V->type       = KVT_COLOR;
      V->KV_flags   = KV_flags;
      V->user_flags = user_flags;
      V->valaddr    = NULL;

      bool result = V->setcolor(val);

      if (!result)
         {
         message_printf(KVAL_ERROR,"KVAL error: addcolor(%s) out of memory", key);
         return NULL;
         }

      return V;
      }

   KVCONTENTS *addcolor(const C8 *key, Color *valaddr, const C8 *initializer=NULL, U32 user_flags=0, U32 KV_flags=0)
      {
      KVCONTENTS *V = NULL;

      if (!(KV_flags & KVAL_MULTIPLE))
         {
         V = contents[key];

         if (V != NULL)
            {
            message_printf(KVAL_ERROR,"KVAL error: addcolor(%s) already present", key);
            return NULL;
            }
         }

      V = contents.allocate(key);

      if (V == NULL)
         {
         message_printf(KVAL_ERROR,"KVAL error: addcolor(%s) out of memory", key);
         return NULL;
         }

      V->type       = KVT_COLOR;
      V->KV_flags   = KV_flags;
      V->user_flags = user_flags;
      V->valaddr    = valaddr;
      
      bool result = ((initializer != NULL) || (valaddr == NULL)) ? V->setstr(initializer) : V->setcolor(*valaddr);

      if (!result)
         {
         message_printf(KVAL_ERROR,"KVAL error: addcolor(%s) out of memory", key);
         return NULL;
         }

      return V;
      }

   KVCONTENTS *addcolor(const C8 *key, Color *valaddr, const Color *default_initializer, U32 user_flags=0, U32 KV_flags=0)
      {
      KVCONTENTS *V = NULL;

      if (!(KV_flags & KVAL_MULTIPLE))
         {
         V = contents[key];

         if (V != NULL)
            {
            message_printf(KVAL_ERROR,"KVAL error: addcolor(%s) already present", key);
            return NULL;
            }
         }

      V = contents.allocate(key);

      if (V == NULL)
         {
         message_printf(KVAL_ERROR,"KVAL error: addcolor(%s) out of memory", key);
         return NULL;
         }

      V->type       = KVT_COLOR;
      V->KV_flags   = KV_flags;
      V->user_flags = user_flags;
      V->valaddr    = valaddr;
      V->defaddr    = default_initializer;
      
      bool result = V->setcolor(*default_initializer);

      if (!result)
         {
         message_printf(KVAL_ERROR,"KVAL error: addcolor(%s) out of memory", key);
         return NULL;
         }

      return V;
      }

   KVCONTENTS *addcolor(const C8 *key_def, U32 KV_flags=0)  
      {
      C8 *text = _strdup(key_def);

      if (text == NULL)
         {
         message_printf(KVAL_ERROR,"KVAL error: addcolor(%s) out of memory", key_def);
         return NULL;
         }

      C8 *key=NULL, *def=NULL;
      U32 flags = 0;

      if (!keydef(text, &flags, &key, &def))
         {
         free(text);
         return NULL;
         }

      KVCONTENTS *result = addcolor(key, NULL, def, flags, KV_flags);

      free(text);
      return result;
      }
#endif

   //
   // Create or update a named entry with the specified string value, optionally
   // returning whether or not an existing value was changed
   //
   // If an entry of the same name already exists and isn't flagged as KVAL_MULTIPLE, 
   // replace the existing value, keeping its type
   //
   // If the entry does not exist, we create it as a string entry, using the KVAL_MULTIPLE flag in 
   // case it's the first entry in a list
   //
   // Note that entries backed by user variables, or which need user flags, must be initially 
   // created by calling the corresponding 'add' method!
   //

   bool update_value(const C8 *key, const C8 *string, bool *changed = NULL)
      {
      KVCONTENTS *V = contents[key];

      if ((V == NULL) || (V->KV_flags & KVAL_MULTIPLE))
         {
         if (changed != NULL)
            {
            *changed = TRUE;
            }

         return (addstr(key, NULL, string, 0, KVAL_MULTIPLE) != NULL);  
         }

      return update_value(V, string, changed);
      }

   //
   // Update an existing entry of any type with a new string representation
   // Also updates the contents of the backed user variable, if any
   //

   bool update_value(KVCONTENTS *V, const C8 *string, bool *changed = NULL)
      {
      if (V->KV_flags & KVAL_READ_ONLY)
         {
         message_printf(KVAL_ERROR,"KVAL error: can't update read-only value %s", V->name);
         return FALSE;
         }

      if (changed != NULL)
         {
         if (strcmp(string, V->str())) 
            {
            *changed = TRUE;
            }
         }

      if (!V->setstr(string))    
         {
         message_printf(KVAL_ERROR,"KVAL error: update() out of memory, key %s", V->name);
         return FALSE;
         }

      return TRUE;
      }

   //
   // Copy user variables to string representations
   //
   // Typically called before saving INI variables to a file, to ensure that
   // the string database is up to date.  Applications that depend on the strings to
   // contain user-specified representations of DOUBLEs, etc. must maintain the string
   // values themselves, rather than calling this function
   //

   void vars_to_strings(void)
      {
      for (S32 i=0; i < contents.count(); i++)
         {
         contents[i].user_var_to_string();
         }
      }

   //---------------------------------------------------------------
   //
   // Various utility functions
   //
   //---------------------------------------------------------------

   //
   // Read database entries from file
   //

   bool read(const C8 *filename, U32 KVF_flags = 0)
      {
      if ((strchr(filename,':') != NULL) || (strstr(filename,"\\\\") != NULL))
         return read("", filename, KVF_flags);
      else
         return read(".\\", filename, KVF_flags);
      }

   bool read(const C8 *directory_name, const C8 *filename, U32 KVF_flags = 0)
      {
      C8 pathname[MAX_PATH] = "";

      memset(pathname, 0, sizeof(pathname));
      _snprintf(pathname, sizeof(pathname)-1, "%s%s", directory_name, filename);

      FILE *infile = fopen(pathname,"rt");

      if (infile == NULL)
         {
         if (KVF_flags & KVF_FILE_MUST_EXIST)
            {
            message_printf(KVAL_ERROR,"File '%s' not found", pathname);
            }

         return FALSE;
         }

      bool result = read(infile, KVF_flags);
      fclose(infile);

      return result;
      }

   bool read(FILE *infile, U32 KVF_flags = 0)
      {
      for (;;)
         {
         //
         // Read input line
         //

         C8 linbuf[2048] = { 0 };

         C8 *result = fgets(linbuf, 
                            sizeof(linbuf) - 1, 
                            infile);

         if (result == NULL)
            {
            break;
            }

         //
         // Skip commented-out lines, and kill trailing and leading spaces
         //

         S32 l = strlen(linbuf);

         if ((!l) || (linbuf[0] == ';'))
            {
            continue;
            }

         C8 *key  = linbuf;
         C8 *end  = linbuf;
         bool leading = TRUE;

         for (S32 i=0; i < l; i++)
            {
            if (!isspace((U8) linbuf[i]))
               {
               if (leading)
                  {
                  key = &linbuf[i];
                  leading = FALSE;
                  }

               end = &linbuf[i];
               }
            }

         end[1] = 0;

         if (leading || (!strlen(key)))
            {
            continue;
            }

         //
         // If first character of key is a quote (which will always be the case if we wrote the file), 
         // the key ends at the first occurrence of the identical delimiter
         //
         // Otherwise the key ends at the first space
         //

         C8 EOK_char = ' ';

         if ((key[0] == '\'') || (key[0] == '\"'))
            {
            EOK_char = key[0];
            key++;
            }

         //
         // Value begins at first nonspace character after end of key
         // Lines with no trailing delimiter are assumed to have empty values
         //

         C8 *value = strchr(key, EOK_char);

         if (value == NULL)
            {
            value = "";
            }
         else
            {
            *value++ = 0;
            while (isspace((U8) (*value))) value++;
            }

         //
         // Add or replace entry
         //

         if (KVF_flags & KVF_DO_NOT_ADD)
            {
            KVCONTENTS *V = contents[key];

            if (V == NULL)
               {
               continue;
               }
            }

         if (!update_value(key, value))
            {
            fclose(infile);
            return FALSE;
            }
         }

      fclose(infile);
      return TRUE;
      }

   //
   // Write database entries to file
   //

   bool write(const C8 *filename)
      {
      if ((strchr(filename,':') != NULL) || (strstr(filename,"\\\\") != NULL))
         return write("", filename);
      else
         return write(".\\", filename);
      }

   bool write(const C8 *directory_name, const C8 *filename, bool keep_existing_entries=FALSE)
      {
      C8 destname[MAX_PATH] = "";

      memset(destname, 0, sizeof(destname));
      _snprintf(destname, sizeof(destname)-1, "%s%s", directory_name, filename);

      FILE *outfile = fopen(destname,"wt");

      if (outfile == NULL)
         {
         message_printf(KVAL_ERROR,"File '%s' could not be opened for writing", destname);
         return FALSE;
         }

      //
      // Keep any existing entries that don't have corresponding entries in our database
      //

      if (keep_existing_entries)
         {
         KVSTORE existing;
         existing.read(destname);

         for (S32 i=0; i < existing.contents.count(); i++)
            {
            KVCONTENTS *E = &existing.contents[i];
            KVCONTENTS *V = contents[E->name];

            if (V == NULL)
               {
               if (!write(outfile, E))
                  {
                  fclose(outfile);
                  return FALSE;
                  }
               }
            }
         }

      //
      // Add our own entries
      //

      for (S32 i=0; i < contents.count(); i++)
         {
         KVCONTENTS *V = &contents[i];

         if (V->KV_flags & KVAL_TEMP)
            {
            continue;
            }

         if (!write(outfile, V))
            {
            fclose(outfile);
            return FALSE;
            }
         }

      fclose(outfile);
      return TRUE;
      }

   bool write(FILE *outfile, KVCONTENTS *V)
      {
      //
      // Enclose name in quotes (double quotes preferred, falling back to single 
      // quotes if name contains double quotes)
      //

      if (V->KV_flags & KVAL_TEMP)
         {
         return FALSE;
         }

      if (strchr(V->name, '\"') == NULL)
         fprintf(outfile,"\"%s\" %s\n", V->name, V->str());
      else
         fprintf(outfile,"'%s' %s\n", V->name, V->str());

      if (ferror(outfile))
         {
         message_printf(KVAL_ERROR,"Write failure -- disk full?");
         return FALSE;
         }

      return TRUE;
      }

   //
   // Support routines to parse console arglist, splitting arguments beginning with '/' or '-' and containing 
   // '=' or ':' into key/value pairs
   //
   // Anonymous argument values are saved under numeric keys corresponding to their order of appearance ("1", "2", etc.)
   //
   // Named arguments must correspond to keys already in the database, and can appear only once per key
   // Each entry's user_flags member will be set to TRUE if it was encountered
   //
   // Usage text may use '$' as a placeholder to display the current (i.e., default) value for each key
   //

   void show_usage(const C8 *usage, bool leading_CRLF)
      {
      C8 work[8192] = { 0 };

      const C8 *s = usage;
      C8       *d = work;

      C8 cur_key[256] = { 0 };

      bool has_default_placeholders = (strchr(usage,'$') != NULL);

      while ((*s) && (d < &work[sizeof(work)-1]))
         {
         if (*s == '\n') 
            {
            cur_key[0] = 0;
            }

         if (has_default_placeholders    &&        // Log first occurrence of "/<key>:" on line 
            ((*s == '/') || (*s == '-')) && 
            (cur_key[0] == 0))    
            {
            C8 *t = cur_key;

            const C8 *c = s;
            c++;

            while ((*c) && (!isspace((U8) *c)) && (*c != ':') && (*c != '=') && (t < &cur_key[sizeof(cur_key)-1]))
               {
               *t++ = *c++;
               }

            *t++ = 0;
            }

         if ((*s == '$') && (cur_key[0] != 0))    // Replace next occurrence of '$' with logged key value
            {
            s++;

            KVCONTENTS *V = contents[cur_key];
            cur_key[0] = 0;

            if (V != NULL)
               {
               const C8 *c = V->str();

               while ((*c) && (d < &work[sizeof(work)-1]))
                  {
                  *d++ = *c++;
                  }
               }
            }
         else
            {
            *d++ = *s++;
            }
         }

      *d++ = 0;

      if (leading_CRLF)
         message_printf(KVAL_NOTICE, "\n%s", work);
      else
         message_printf(KVAL_NOTICE, "%s", work);
      }

   bool from_args(S32               argc, 
                  const C8 * const *argv, 
                  const C8         *usage, 
                  S32               max_anon_values      = 0, 
                  S32               required_anon_values = 0)
      {
      if ((argc > 1) && 
         ((argv[1][0] == '-') || (argv[1][0] == '/')) &&
         ((argv[1][1] == '?') ))
         {
         show_usage(usage, FALSE);
         return FALSE;
         }

      S32 anon_key = 1;
      C8  key[MAX_PATH] = { 0 };
      C8  val[MAX_PATH] = { 0 };

      for (S32 i=1; i < argc; i++)
         {
         memset(key, 0, sizeof(key));
         memset(val, 0, sizeof(val));

         const C8 *A = argv[i];

         if ((A[0] != '-') && (A[0] != '/'))
            {
            //
            // Do we already have the max # of permitted anonymous values?
            //

            if (anon_key > max_anon_values)
               {
               show_usage(usage, TRUE);
               message_printf(KVAL_ERROR,"\nUnrecognized argument (\"%s\")", A);
               return FALSE;
               }

            sprintf(key,"%d",anon_key++);
            strncpy(val, A, sizeof(val)-1);

            if (addstr(key, NULL, val, 0, 0) == NULL)
               {
               return FALSE;
               }
            }
         else
            {
            //
            // Separate key and value portions of argument
            //

            const C8 *v = strchr(A, ':');
            if (v == NULL) v = strchr(A, '=');

            if (v == NULL) 
               {
               strncpy(key, &A[1], sizeof(key)-1);
               }
            else
               {
               strncpy(key, &A[1], min(max(0,v-A-1), sizeof(key)-1));
               strncpy(val, v+1, sizeof(val)-1);
               }
            }

         //
         // If database doesn't contain an entry for this key, report error
         // Otherwise, set its 'present' flag and update the value
         //

         KVCONTENTS *V = contents[key];

         if (V == NULL)
            {
            show_usage(usage, TRUE);
            message_printf(KVAL_ERROR,"\nUnrecognized argument (\"%s\")", key);
            return FALSE;
            }

         if (V->user_flags)
            {
            show_usage(usage, TRUE);
            message_printf(KVAL_ERROR,"\nDuplicate arguments (\"%s\")", key);
            return FALSE;
            }

         if ((!val[0]) && (V->type != KVT_BOOL))
            {
            show_usage(usage, TRUE);
            message_printf(KVAL_ERROR,"\nValue must be supplied for argument \"%s\"", key);
            return FALSE;
            }

         if (!update_value(V, val))
            {
            return FALSE;
            }

         V->user_flags = TRUE;
         }

      //
      // Ensure that any KVAL_REQUIRED keys have their 'present' flag set
      //

      C8 missing_args[8192] = { 0 };
      C8 *d = missing_args;

      S32 n_missing = 0;

      for (S32 i=0; i < contents.count(); i++)
         {
         KVCONTENTS *V = &contents[i];

         if ((V->KV_flags & KVAL_REQUIRED) && (!V->user_flags))
            {
            if (missing_args[0])
               {
               d += _snprintf(d, sizeof(missing_args)-(d-missing_args+1), ", ");
               }

            d += _snprintf(d, sizeof(missing_args)-(d-missing_args+1), "\"%s\"", V->name);
            ++n_missing;
            }
         }

      if (n_missing)
         {
         show_usage(usage, TRUE);

         if (n_missing > 1)
            message_printf(KVAL_ERROR,"\nMissing arguments (%s)", missing_args);
         else
            message_printf(KVAL_ERROR,"\nMissing argument (%s)", missing_args);
            
         return FALSE;
         }

      if (required_anon_values >= anon_key)
         {
         show_usage(usage, TRUE);
         message_printf(KVAL_ERROR,"\nMissing argument");
         return FALSE;
         }

      return TRUE;
      }

   //
   // Add this database's contents to another existing database
   //

   bool copy_to(KVSTORE *D, bool include_addresses=FALSE)
      {
      for (S32 i=0; i < contents.count(); i++)
         {
         KVAL *SV = &contents[i];
         KVAL *DV = D->lookup(SV->name); 

         if (DV == NULL)
            {
            DV = D->contents.allocate(SV->name);

            if (DV == NULL)
               {
               message_printf(KVAL_ERROR,"Out of memory");
               return FALSE;
               }

            DV->type       = SV->type;
            DV->user_flags = SV->user_flags;
            DV->user_ptr   = SV->user_ptr;
            DV->KV_flags   = SV->KV_flags;

            if (include_addresses)
               {
               DV->valaddr = SV->valaddr;
               }
            }

         if (!DV->setstr(SV->str()))
            {
            message_printf(KVAL_ERROR,"Out of memory");
            return FALSE;
            }
         }

      return TRUE;
      }

   //
   // Copy assignment and constructor
   //

   KVSTORE & operator= (KVSTORE &src)
      {
      if (&src == this)
         {
         return *this;
         }

      clear();
      src.copy_to(this);

      return *this;
      }

   KVSTORE(KVSTORE &src)
      {
      clear();
      src.copy_to(this);
      }
};

void ____________________________KVDLGFIELD_KVDLGSTORE_______________________________________________(){}

//
// .INI-backed dialog-field database
//

#ifdef Button_GetCheck     // (available if commdlg.h included)

enum KVDC
{  
   KVDC_TEXT,
   KVDC_DOUBLE,
   KVDC_S32,
   KVDC_RADIOBTN,
   KVDC_CHECKBOX,
   KVDC_LISTBOX
};

struct KVDLGFIELD : public KVAL
{
   KVDC ctrl_type;
   S32  ctrl_ID;
         
   C8   ctrl_name [512];
   C8   ctrl_init [4096];

   C8   min_str[512];      // enclose min/max in '|' bars to accept either neg or pos values
   C8   max_str[512];
   C8   def_str[512];

   bool ctrl_enabled;      // use this to disable controls in to_dialog(), and keep from reading them in from_dialog() ...
                           // otherwise, from_dialog() will read from disabled controls
   void initialize(const void *V) 
      {
      KVAL::initialize(V);
      ctrl_type      = KVDC_TEXT;
      ctrl_ID        = 0;
      ctrl_name[0]   = 0;
      ctrl_init[0]   = 0;
      min_str[0]     = 0;
      max_str[0]     = 0;
      def_str[0]     = 0;
      ctrl_enabled   = TRUE;
      }

   //
   // Return string representing clamped out-of-range entry value, or NULL
   // if the value was in range
   //
   // Also handles simple rational expressions (a/b)
   //

   C8 *clamp(C8 *test_value = NULL)
      {
      if ((ctrl_type != KVDC_DOUBLE) && 
          (ctrl_type != KVDC_S32))
         {
         return NULL;
         }

      if (test_value == NULL)
         {
         test_value = value_string;
         }

      if ((test_value == NULL) || (!test_value[0]))
         {
         return def_str;       // if value was blank, return its default
         }

      DOUBLE dbl_val = KVAL::to_double(test_value);
      
      C8 *mx = max_str;
      C8 *mn = min_str;

      bool max_abs = (mx[0] == '|'); if (max_abs) { ++mx; dbl_val = fabs(dbl_val); }    // force abs comparison if either max or min value is
      bool min_abs = (mn[0] == '|'); if (min_abs) { ++mn; dbl_val = fabs(dbl_val); }    // enclosed with '|' characters

      DOUBLE max_val = KVAL::to_double(mx);
      DOUBLE min_val = KVAL::to_double(mn);

      if (ctrl_type == KVDC_S32)
         {
         max_val += 0.01;      // guard against rounding error when comparing ints
         min_val -= 0.01;
         }  
      
      if (dbl_val < min_val) 
         return min_str;
      else if (dbl_val > max_val)
         return max_str;

      return NULL;
      }
};

struct KVDLGSTORE : public KVSTORE<KVDLGFIELD>
{
   //
   // Copy database values to associated dialog controls
   //
   // Skips any fields associated with a different tab control or other child window,
   // or with no window at all
   //

   void to_dialog(HWND hDlg, bool set_enable_states=TRUE)
      {
      for (S32 i=0; i < contents.count(); i++)
         {
         KVDLGFIELD *E = &contents[i];

         if (E->ctrl_ID == 0) 
            {
            continue;
            }

         HWND hDT = GetDlgItem(hDlg, E->ctrl_ID);

         if (hDT == NULL)
            {
            continue;      
            }              

         switch (E->ctrl_type)
            {
            case KVDC_TEXT:
            case KVDC_DOUBLE:
            case KVDC_S32:
               {
               C8 *clamped_value = E->clamp();      // ensure we never use an out-of-range value as a dialog default

               if (clamped_value != NULL)
                  {
                  update_value(E->name, clamped_value);
                  }

               assert(SetDlgItemText(hDlg, E->ctrl_ID, E->str()));

               if (set_enable_states) EnableWindow(GetDlgItem(hDlg, E->ctrl_ID), E->ctrl_enabled);
               break;
               }

            case KVDC_RADIOBTN:
            case KVDC_CHECKBOX:
               {
               if (E->boolean())
                  Button_SetCheck(hDT, TRUE);
               else
                  Button_SetCheck(hDT, FALSE);

               if (set_enable_states) EnableWindow(GetDlgItem(hDlg, E->ctrl_ID), E->ctrl_enabled);
               break;
               }

            case KVDC_LISTBOX:
               {
               SendMessage(hDT, CB_RESETCONTENT, 0, 0);

               C8 *src = E->ctrl_init;

               while (*src)
                  {
                  C8 buffer[512];
                  C8 *dest = buffer;

                  while ((*src != '\n') && (*src)) *dest++ = *src++;
                  while  (*src == '\n')                       src++;
                  *dest++ = 0;

                  SendMessage(hDT, CB_ADDSTRING, 0, (LPARAM) buffer);
                  }
               
               SendMessage(hDT, CB_SETCURSEL, E->num(), 0);

               if (set_enable_states) EnableWindow(GetDlgItem(hDlg, E->ctrl_ID), E->ctrl_enabled);
               break;
               }
            }
         }
      }

   //
   // Update database entries from their associated dialog controls
   //
   // Skips any fields associated with a different tab control or other child window,
   // or with no window at all
   // 
   // Returns FALSE if database update failed for any reason.  Basic value clamping and
   // validation takes place before any entries are updated, so there's ordinarily no 
   // need to restore from a backup copy of the database if the user hits Cancel after 
   // an unsuccessful attempt to submit new values
   //
   // If *changed is used, it should be initialized to FALSE
   //

   bool from_dialog(HWND hDlg, bool *changed = NULL)
      {
      //
      // Pass 1: Clamp/validate fields, returning FALSE if any out of range
      //

      for (S32 i=0; i < contents.count(); i++)
         {
         C8 buffer[512] = { 0 };

         KVDLGFIELD *E = &contents[i];

         if (E->ctrl_ID == 0) 
            {
            continue;
            }

         HWND hDT = GetDlgItem(hDlg, E->ctrl_ID);

         if ((hDT == NULL) || (!E->ctrl_enabled))
            {
            continue;      
            }              

         switch (E->ctrl_type)
            {
            case KVDC_TEXT:
            case KVDC_DOUBLE:
            case KVDC_S32:
               {
               GetDlgItemText(hDlg, E->ctrl_ID, buffer, sizeof(buffer)-1);

               if ((buffer[0] == 0) && (E->ctrl_type != KVDC_TEXT))  // Text fields can be blank but numeric fields can't be, 
                  {                                                  // unless the input window itself is disabled or there are no valid range limits
                  if ((!IsWindowEnabled(hDT)) ||
                      (E->min_str[0] == 0)    ||                     // NB: this doesn't guarantee the field is a valid KVDC_DOUBLE, just that it's not blank.
                      (E->max_str[0] == 0))                          // If the min_str-max_str range includes 0, an invalid value should be interpreted as 0.  
                     {                                               // Otherwise it will be rejected by the range check below
                     continue;
                     }

                  if (E->def_str[0])
                     message_printf(KVAL_ERROR,"%s invalid: enter a value from %s to %s (default = %s)", E->ctrl_name, E->min_str, E->max_str, E->def_str);
                  else
                     message_printf(KVAL_ERROR,"%s invalid: enter a value from %s to %s", E->ctrl_name, E->min_str, E->max_str);

                  SetFocus(hDT);
                  return FALSE;
                  }

               if (E->ctrl_type == KVDC_S32)    // OK to use scientific notation for ints, but not decimal or rational values
                  {
                  for (S32 j=0; j < (S32) strlen(buffer); j++)
                     {
                     if ((!isdigit((U8) buffer[j])) && (buffer[j] != '-') && (buffer[j] != '+'))
                        {
                        if (E->def_str[0])
                           message_printf(KVAL_ERROR,"%s must be an integer value from %s to %s (default = %s)", E->ctrl_name, E->min_str, E->max_str, E->def_str);
                        else
                           message_printf(KVAL_ERROR,"%s must be an integer value from %s to %s", E->ctrl_name, E->min_str, E->max_str);

                        SetFocus(hDT);
                        return FALSE;                     
                        }
                     }

                  DOUBLE val = KVAL::to_double(buffer);

                  if (val < 0.0) val -= 0.5;
                  if (val > 0.0) val += 0.5;

                  sprintf(buffer,"%d", (S32) val);
                  }

               if (E->clamp(buffer) != NULL)
                  {
                  if (E->def_str[0])
                     message_printf(KVAL_ERROR,"%s out of range (%s): enter a value from %s to %s (default = %s)", E->ctrl_name, buffer, E->min_str, E->max_str, E->def_str);
                  else
                     message_printf(KVAL_ERROR,"%s out of range (%s): enter a value from %s to %s", E->ctrl_name, buffer, E->min_str, E->max_str);

                  SetFocus(hDT);
                  return FALSE;
                  }

               break;
               }
            }
         }

      //
      // Pass 2: Read fields into database (which will not normally fail)
      //

      for (S32 i=0; i < contents.count(); i++)
         {
         C8 buffer[512];
         memset(buffer, 0, sizeof(buffer));

         KVDLGFIELD *E = &contents[i];

         if (E->ctrl_ID == 0) 
            {
            continue;
            }

         HWND hDT = GetDlgItem(hDlg, E->ctrl_ID);

         if ((hDT == NULL) || (!E->ctrl_enabled))
            {
            continue;      
            }

         switch (E->ctrl_type)
            {
            case KVDC_TEXT:
            case KVDC_DOUBLE:
            case KVDC_S32:
               {
               GetDlgItemText(hDlg, E->ctrl_ID, buffer, sizeof(buffer)-1);

               if (E->ctrl_type == KVDC_S32)    
                  {
                  DOUBLE val = KVAL::to_double(buffer);

                  if (val < 0.0) val -= 0.5;
                  if (val > 0.0) val += 0.5;

                  sprintf(buffer,"%d", (S32) val);
                  }

               if (!update_value(E->name, buffer, changed))
                  {
                  return FALSE;
                  }

               break;
               }

            case KVDC_RADIOBTN:
            case KVDC_CHECKBOX:
               {
               C8 *v = Button_GetCheck(hDT) ? "True" : "False";

               if (!update_value(E->name, v, changed))
                  {
                  return FALSE;
                  }
               
               break;
               }

            case KVDC_LISTBOX:
               {
               LRESULT sel = SendMessage(hDT, CB_GETCURSEL, 0, 0);

               if (changed != NULL)
                  {
                  if (E->num() != (S32) sel)
                     {
                     *changed = TRUE;
                     }
                  }

               if (!E->setnum((S32) sel))
                  {
                  return FALSE;
                  }

               break;
               }
            }
         }

      return TRUE;
      }
};

//
// Macros to declare database entries for both dialog and .INI representations
//
// Note that the default values for DOUBLE entries are passed as strings in order to allow the
// user to specify exactly how they should be written.  (Don't call vars_to_strings() on
// these, or the user's formatting will be lost.) 
//
// S32 defaults are integers to allow them to be initialized with enums and character constants
//

#define INI_DLG_STRING(ctrlname,ctrlID,varname,userflags,defval) \
   struct varname##_container_ \
      { \
      C8 value[VALSTR_LEN+1]; \
      KVDLGFIELD *F; \
      KVDLGSTORE *D; \
      void store(KVDLGSTORE *database) \
         { \
         D = database; \
         F = D->addstr(ctrlname, value, defval, userflags); \
         if (F == NULL) return; \
         F->ctrl_type = KVDC_TEXT; \
         F->ctrl_ID = ctrlID; \
         strcpy(F->ctrl_name, ctrlname); \
         } \
      varname##_container_() \
         { \
         D = NULL; \
         F = NULL; \
         value[0] = 0; \
         } \
      ~varname##_container_() \
         { \
         if (F != NULL) \
            { \
            D->delete_entry(F); \
            F = NULL; \
            } \
         } \
      C8 * operator = (const C8 *v) \
         { \
         F->setstr(v); \
         return value; \
         } \
      } \
   varname;

#define INI_DLG_DOUBLE(ctrlname,ctrlID,varname,userflags,defval,minval,maxval) \
   struct varname##_container_ \
      { \
      DOUBLE value; \
      KVDLGFIELD *F; \
      KVDLGSTORE *D; \
      void store(KVDLGSTORE *database) \
         { \
         D = database; \
         F = D->adddbl(ctrlname, &value, defval, userflags); \
         if (F == NULL) return; \
         F->ctrl_type = KVDC_DOUBLE; \
         F->ctrl_ID = ctrlID; \
         strcpy(F->ctrl_name, ctrlname); \
         strcpy(F->max_str, maxval); \
         strcpy(F->min_str, minval); \
         strcpy(F->def_str, defval); \
         } \
      varname##_container_() \
         { \
         D = NULL; \
         F = NULL; \
         value = 0.0; \
         } \
      ~varname##_container_() \
         { \
         if (F != NULL) \
            { \
            D->delete_entry(F); \
            F = NULL; \
            } \
         } \
      DOUBLE operator = (DOUBLE v) \
         { \
         F->setdbl(v); \
         return v; \
         } \
      } \
   varname;

#define INI_DLG_S32(ctrlname,ctrlID,varname,userflags,defval,minval,maxval) \
   struct varname##_container_ \
      { \
      S32 value; \
      KVDLGFIELD *F; \
      KVDLGSTORE *D; \
      void store(KVDLGSTORE *database) \
         { \
         D = database; \
         F = D->addnum(ctrlname, &value, NULL, userflags); \
         if (F == NULL) return; \
         F->ctrl_type = KVDC_S32; \
         F->ctrl_ID = ctrlID; \
         strcpy(F->ctrl_name, ctrlname); \
         strcpy(F->max_str, maxval); \
         strcpy(F->min_str, minval); \
         sprintf(F->def_str, "%d", defval); \
         } \
      varname##_container_() \
         { \
         D = NULL; \
         F = NULL; \
         value = defval; \
         } \
      ~varname##_container_() \
         { \
         if (F != NULL) \
            { \
            D->delete_entry(F); \
            F = NULL; \
            } \
         } \
      S32 operator = (S32 v) \
         { \
         F->setnum(v); \
         return v; \
         } \
      } \
   varname;

#define INI_DLG_RADIOBTN(ctrlname,ctrlID,varname,userflags,defval) \
   struct varname##_container_ \
      { \
      bool value; \
      KVDLGFIELD *F; \
      KVDLGSTORE *D; \
      void store(KVDLGSTORE *database) \
         { \
         D = database; \
         F = D->addbool(ctrlname, &value, NULL, userflags); \
         if (F == NULL) return; \
         F->ctrl_type = KVDC_RADIOBTN; \
         F->ctrl_ID = ctrlID; \
         strcpy(F->ctrl_name, ctrlname); \
         } \
      varname##_container_() \
         { \
         D = NULL; \
         F = NULL; \
         value = defval; \
         } \
      ~varname##_container_() \
         { \
         if (F != NULL) \
            { \
            D->delete_entry(F); \
            F = NULL; \
            } \
         } \
      bool operator = (bool v) \
         { \
         F->setbool(v); \
         return v; \
         } \
      } \
   varname;

#define INI_DLG_CHECKBOX(ctrlname,ctrlID,varname,userflags,defval) \
   struct varname##_container_ \
      { \
      bool value; \
      KVDLGFIELD *F; \
      KVDLGSTORE *D; \
      void store(KVDLGSTORE *database) \
         { \
         D = database; \
         F = D->addbool(ctrlname, &value, NULL, userflags); \
         if (F == NULL) return; \
         F->ctrl_type = KVDC_CHECKBOX; \
         F->ctrl_ID = ctrlID; \
         strcpy(F->ctrl_name, ctrlname); \
         } \
      varname##_container_() \
         { \
         D = NULL; \
         F = NULL; \
         value = defval; \
         } \
      ~varname##_container_() \
         { \
         if (F != NULL) \
            { \
            D->delete_entry(F); \
            F = NULL; \
            } \
         } \
      bool operator = (bool v) \
         { \
         F->setbool(v); \
         return v; \
         } \
      } \
   varname;

#define INI_DLG_LISTBOX(ctrlname,ctrlID,varname,userflags,defval,initstr) \
   struct varname##_container_ \
      { \
      S32 value; \
      KVDLGFIELD *F; \
      KVDLGSTORE *D; \
      void store(KVDLGSTORE *database) \
         { \
         D = database; \
         F = D->addnum(ctrlname, &value, NULL, userflags); \
         if (F == NULL) return; \
         F->ctrl_type = KVDC_LISTBOX; \
         F->ctrl_ID = ctrlID; \
         strcpy(F->ctrl_name, ctrlname); \
         strcpy(F->ctrl_init, initstr); \
         } \
      varname##_container_() \
         { \
         D = NULL; \
         F = NULL; \
         value = defval; \
         } \
      ~varname##_container_() \
         { \
         if (F != NULL) \
            { \
            D->delete_entry(F); \
            F = NULL; \
            } \
         } \
      S32 operator = (S32 v) \
         { \
         F->setnum(v); \
         return v; \
         } \
      } \
   varname;

#endif   // Button_GetCheck

void ________________________________________________________________________________________________(){}

#ifdef GDIPVER

//
// GDI+ Color object
//

#define INI_RGB(database,varname,desc_text,def_R,def_G,def_B) \
   Color varname (def_R, def_G, def_B); \
   const Color varname##default(def_R, def_G, def_B); \
   struct varname##_INI_init_ \
   { \
      varname##_INI_init_() \
         { \
         KVAL *K = database.addcolor(#varname, &varname, &varname##default); \
         if (K != NULL) K->user_ptr = desc_text;   \
         } \
   } varname##_INI_container;

#endif   // GDIPVER
#endif   // STDTPL_H
