//
// GNSS.CPP: Read fixes from various GNSS sources (GPS NMEA, etc.)
//
// Author: John Miles, KE5FX (jmiles@pop.net)
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
#include <process.h>

#include "typedefs.h"

#define SERIAL_CONTROL_MODE 0
#include "gpiblib/comblock.h"

#define BUILDING_GNSS
#include "gnss.h"

enum TCMD
{
   TC_NONE,
   TC_FORCE_EXIT,
   TC_FORCE_EXIT_ERROR
};

enum TSTATE
{
   TS_PRESTART, 
   TS_STARTUP,
   TS_WAITING,
   TS_RUNNING,
   TS_EXIT_OK,    
   TS_EXIT_WARNING, 
   TS_EXIT_FAULT,   
   TS_EXIT_ERROR    
};

// ----------------------------------------------------------------------------------
// Abstract base class manages common resources for all GNSS receiver types 
// ----------------------------------------------------------------------------------

#define MAX_LINE_LEN 1024

UINT WINAPI background_proc(LPVOID pParam);

struct GNSS_CONTEXT                        
{
   GNSS_STATE *S;
   bool        connected;

   HANDLE       hThread;
   volatile S32 thread_state;
   volatile S32 thread_command;

   CRITICAL_SECTION csLock;
   C8               line_buffer[MAX_LINE_LEN];
   S32              line_len;

   S32    line_UTC_hours;
   S32    line_UTC_mins;
   S32    line_UTC_secs;
   DOUBLE line_latitude;
   DOUBLE line_longitude;
   DOUBLE line_altitude_m;
   DOUBLE line_geoid_sep_m;
   S64    line_fix_num;

   GNSS_CONTEXT(GNSS_STATE *_S)
      {
      S = _S;
      connected = FALSE;

      InitializeCriticalSection(&csLock);

      hThread = NULL;
      thread_state = TS_PRESTART;
      thread_command = TC_NONE;

      memset(line_buffer, 0, sizeof(line_buffer));
      line_len = 0;

      line_UTC_hours    = 0;
      line_UTC_mins     = 0;
      line_UTC_secs     = 0;
      line_latitude     = 0.0;
      line_longitude    = 0.0;
      line_altitude_m   = 0.0;
      line_geoid_sep_m  = 0.0;
      line_fix_num      = 0;
      }

   virtual ~GNSS_CONTEXT()
      {
      DeleteCriticalSection(&csLock);
      }

   virtual bool start_thread(void)
      {
      thread_command = TC_NONE;

      unsigned int stupId;
      hThread =  (HANDLE) _beginthreadex(NULL,                   
                                         0,                              
                                         background_proc,
                                         this,
                                         0,                              
                                        &stupId);
      if (hThread == NULL)
         {
         return FALSE;
         }

      return TRUE;
      }

   virtual void stop_thread(void)
      {
      if (hThread == NULL)
         {
         return;
         }

      thread_command = TC_FORCE_EXIT;                     
      DWORD result = WaitForSingleObject(hThread, 5000);  

      if (result != WAIT_OBJECT_0)
         {
         TerminateThread(hThread, 0);
         }

      CloseHandle(hThread);
      hThread = NULL;
      }

   void inline lock(void)
      {
      EnterCriticalSection(&csLock);
      }

   void inline unlock(void)
      {
      LeaveCriticalSection(&csLock);
      }

   virtual UINT thread_proc(void) = 0;

   virtual bool xmit(C8 *text) = 0;

   virtual bool xmit_GPGGA(C8 *text) = 0;

   virtual bool update(void) = 0;
};

// ----------------------------------------------------------------------------------
// Private implementation for NMEA receivers connected via COM or TCP/IP port  
// (Can also read NMEA data from ASCII text file)
// ----------------------------------------------------------------------------------

#define NMEA_MAX_LEN    512
#define NMEA_EOS_CHAR   10
#define NMEA_TIMEOUT_MS 100
#define NMEA_DROPOUT_MS 100

struct NMEA_SOURCE : public GNSS_CONTEXT      
{
   COMBLOCK *COM;
   C8       *COM_err;
   FILE     *in;
   C8        in_buf[NMEA_MAX_LEN];

   ~NMEA_SOURCE()
      {
      stop_thread();

      if (COM != NULL)
         {
         delete COM;
         COM = NULL;
         }

      if (in != NULL)
         {
         fclose(in);
         in = NULL;
         }

      connected = FALSE;
      }

   NMEA_SOURCE(GNSS_STATE *_S) : GNSS_CONTEXT(_S)
      {
      connected = FALSE;
      in  = NULL;
      COM = NULL;
      COM_err = NULL;

      memset(in_buf, 0, sizeof(in_buf));

      switch (S->type)
         {
         case GPS_NMEA_FILE:
            {
            in = fopen(S->address,"rt");

            if (in == NULL)
               {
               _snprintf(S->error_text,sizeof(S->error_text)-1,"Couldn't open %s", S->address);
               return;
               }

            break;
            }

         case GPS_NMEA_COM:
            {
            COM = new SERBLOCK(S->address, &COM_err);
            break;
            }

         case GPS_NMEA_TCPIP:
            {
            COM = new TCPBLOCK(S->address, &COM_err);
            break;
            }

         default:
            {
            sprintf(S->error_text,"Unknown connection type (%d)", S->type);
            return;
            }
         }

      if (COM_err != NULL)
         {
         strcpy(S->error_text, COM_err);

         delete COM;
         COM = NULL;
         return;
         }

      connected = TRUE;
      }

   DOUBLE ddmm2degrees(DOUBLE dm, C8 dir)
      {
      DOUBLE degs = (DOUBLE) ((S32) dm / 100);
      DOUBLE mins = dm - (100.0 * degs);

      degs = degs + (mins / 60.0);

      if ((dir == 'S') || (dir == 'W') || (dir == 's') || (dir == 'w'))
         {
         degs *= -1.0;
         }

      return degs;
      }

   DOUBLE degrees2ddmm(DOUBLE degs, bool *neg)
      {
      DOUBLE ddmm = degs;

      *neg = (degs <= 0.0);

      DOUBLE sgn = *neg ? -1.0 : 1.0;
      degs *= sgn;

      ddmm = floor(degs);
      DOUBLE frac = degs - ddmm;
      ddmm = (ddmm * 100.0) + (60.0 * frac);

      return ddmm;
      }
                
   virtual bool xmit(C8 *text)
      {
      C8 buf[256] = { 0 };
      _snprintf(buf, sizeof(buf)-1, "%s\r\n", text);

      lock();
      bool result = COM->send(buf);
      unlock();

      return result;
      }

   virtual bool xmit_sentence(C8 *text)
      {
      C8 buf[256] = { 0 };

      U8 chksum = 0;

      for (S32 i=1; i < strlen(text); i++)
         {
         chksum ^= (U8) text[i];
         }

      _snprintf(buf, sizeof(buf)-1, "%s*%.02X\r\n", text, chksum);

      _snprintf(S->error_text, sizeof(S->error_text)-1, "%s", buf);

      lock();
      bool result = COM->send(buf);
      unlock();

      return result;
      }

   virtual bool xmit_GPGGA(C8 *text)
      {
      //
      // Parse input of form lat,long,alt,h,m,s
      //

      DOUBLE lat=0.0;
      DOUBLE lon=0.0;
      DOUBLE alt=0.0;
      S32    hrs=0;
      S32    min=0;
      S32    sec=0;

      sscanf(text,"%lf,%lf,%lf,%d,%d,%d",
         &lat, &lon, &alt, &hrs, &min, &sec);

      C8 buf[256] = { 0 };
      bool neg = FALSE;

      S32    UTC              = (hrs*10000) + (min*100) + sec;
      DOUBLE latitude         = degrees2ddmm(lat, &neg);
      C8     lat_sign         = neg ? 'S':'N';
      DOUBLE longitude        = degrees2ddmm(lon, &neg);
      C8     long_sign        = neg ? 'W':'E';
      S32    fix_quality      = 1;
      S32    n_SV             = 8;
      DOUBLE HDOP             = 0.9;
      DOUBLE altitude         = alt;
      C8     alt_units        = 'M';
      DOUBLE geoid_sep        = 46.9;
      C8     geoid_sep_units  = 'M';

      _snprintf(buf, sizeof(buf)-1, "$GPGGA,%d,%4.3lf,%c,%5.3lf,%c,%d,%d,%.2lf,%.3lf,%c,%.1lf,%c,,",
            UTC,
            latitude,
            lat_sign,
            longitude,
            long_sign,
            fix_quality,
            n_SV,
            HDOP,
            altitude,
            alt_units,
            geoid_sep,
            geoid_sep_units);
      
      bool result = xmit_sentence(buf);

      return result;
      }

   virtual bool update(void)
      {
      lock();

      if (S->fix_num == line_fix_num)
         {
         unlock();
         return FALSE;     // no new fix available (but any old one is still valid)
         }

      S->longitude       = line_longitude;
      S->latitude        = line_latitude;
      S->lat_long_valid  = TRUE;
      S->altitude_m      = line_altitude_m;
      S->alt_valid       = TRUE;
      S->geoid_sep_m     = line_geoid_sep_m;
      S->geoid_sep_valid = TRUE;
      S->UTC_hours       = line_UTC_hours;
      S->UTC_mins        = line_UTC_mins;
      S->UTC_secs        = line_UTC_secs;
      S->UTC_valid       = TRUE;
      S->fix_num         = line_fix_num;

      unlock();
      return TRUE;         // new fix available
      }

   virtual UINT thread_proc(void)
      {
      thread_state = TS_RUNNING;

      for (;;)
         {
         if (thread_command == TC_FORCE_EXIT) 
            {
            thread_state = TS_EXIT_OK;
            break;
            }

         //
         // Read line from COM port or text file
         //

         C8 *result = NULL;
         S32 cnt = 0;

         if (COM != NULL)
            {
            TERM_REASON reason = SR_IN_PROGRESS;

            result = (C8 *) COM->receive(NMEA_MAX_LEN,
                                         NMEA_EOS_CHAR,
                                         NMEA_TIMEOUT_MS,
                                         NMEA_DROPOUT_MS,
                                         NULL,
                                        &cnt,
                                        &reason);
            if (cnt == 0)
               {
               continue;
               }
            }
         else if (in != NULL)
            {
            Sleep(100);    // give the foreground thread a chance to see the data...

            memset(in_buf, 0, sizeof(in_buf));
            result = fgets(in_buf, sizeof(in_buf)-2, in);

            if (result == NULL)
               {
               thread_state = TS_EXIT_OK;
               break;
               }

            cnt = strlen(result);
            }

         //
         // NMEA lines begin with $ and end with LF or CR/LF
         //

         for (S32 i=0; i < cnt; i++)
            {
            C8 ch = result[i];

//            printf("%c", ch);

            if (ch == 10)
               {
               //
               // Reject any malformed/incomplete lines, and those with bad checksums
               //

               if (line_buffer[0] != '$') 
                  {
                  continue;
                  }

               U8 *chk = (U8 *) strrchr(line_buffer,'*');
               if (chk == NULL) continue;

               U32 chksum = 0;
               sscanf((C8 *) &chk[1],"%2X",&chksum);

               U8 *ptr = (U8 *) &line_buffer[1];

               while (ptr < chk)
                  {
                  chksum ^= *ptr++;
                  }

               if (chksum != 0) continue;

               //
               // If line begins with $GPGGA, parse it and update the fix
               //

               if (!_strnicmp(line_buffer,"$GPGGA,",7))
                  {
                  DOUBLE UTC              = 0.0;
                  DOUBLE latitude         = 0.0;
                  C8     lat_sign         = 0;
                  DOUBLE longitude        = 0.0;
                  C8     long_sign        = 0;
                  S32    fix_quality      = 0;
                  S32    n_SV             = 0;
                  DOUBLE HDOP             = 0.0;
                  DOUBLE altitude         = 0.0;
                  C8     alt_units        = 0;
                  DOUBLE geoid_sep        = 0.0;
                  C8     geoid_sep_units  = 0;

	               sscanf(line_buffer,"$GPGGA,%lf,%lf,%c,%lf,%c,%d,%d,%lf,%lf,%c,%lf,%c",
		                  &UTC,
                        &latitude,
                        &lat_sign,
		                  &longitude,
                        &long_sign,
                        &fix_quality,
                        &n_SV,
                        &HDOP,
                        &altitude,
		                  &alt_units,
                        &geoid_sep,
                        &geoid_sep_units);

                  if (fix_quality == 0) continue;

                  lock();
                  S32 hms = (S32) UTC;    // drop fractional seconds rather than rounding to nearest

                  line_UTC_secs    = hms % 100; hms /= 100;
                  line_UTC_mins    = hms % 100; hms /= 100;
                  line_UTC_hours   = hms % 100;

                  line_latitude    = ddmm2degrees(latitude,  lat_sign);
                  line_longitude   = ddmm2degrees(longitude, long_sign);

                  line_altitude_m  = altitude;
                  line_geoid_sep_m = geoid_sep;

                  line_fix_num++;
                  unlock();
                  }

               continue;
               }

            if (ch == '$')
               {
               line_len = 0;
               }

            if (line_len == MAX_LINE_LEN)
               {
               continue;
               }

            line_buffer[line_len++] = ch;
            line_buffer[line_len] = 0;
            }
         }

      return TRUE;
      }
};

UINT WINAPI background_proc(LPVOID pParam)
{
   GNSS_CONTEXT *C = (NMEA_SOURCE *) pParam;
   return C->thread_proc();
}

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
      MessageBox(NULL, 
                 work_string,
                "Error", 
                 MB_OK);
      }
   else
      {
      MessageBox(NULL,
                 work_string,
                 caption,
                 MB_OK);
      }
}

// -------------------------------------------------------------------------
// GNSS_startup()
// -------------------------------------------------------------------------

GNSS_STATE * WINAPI GNSS_startup(void)
{
   //
   // Initialize global state
   //

   GNSS_STATE *S = new GNSS_STATE;

   if (S == NULL)
      {
      return NULL;
      }

   S->latitude   = INVALID_LATLONGALT;
   S->longitude  = INVALID_LATLONGALT; 
   S->altitude_m = INVALID_LATLONGALT; 

   return S;
}

// -------------------------------------------------------------------------
// GNSS_shutdown()
// -------------------------------------------------------------------------

void WINAPI GNSS_shutdown(GNSS_STATE *S)
{
   if (S != NULL)
      {
      GNSS_disconnect(S);
      delete S;      
      }
}

// -------------------------------------------------------------------------
// GNSS_setup()
// -------------------------------------------------------------------------

bool WINAPI GNSS_parse_command_line(GNSS_STATE *S, C8 *command_line)
{
   if (S == NULL)
      {
      return FALSE;
      }

   //
   // Parse command line
   //

   C8 lpCmdLine[MAX_PATH];
   C8 lpUCmdLine[MAX_PATH];

   memset(lpCmdLine, 0, sizeof(lpCmdLine));
   strncpy(lpCmdLine,  command_line, sizeof(lpCmdLine)-1);
   strncpy(lpUCmdLine, command_line, sizeof(lpCmdLine)-1);
   _strupr(lpUCmdLine);

   //
   // Check for CLI options
   //

   S->address[0] = 0;
   S->setup_text[0] = 0;
   S->GPGGA_text[0] = 0;
   S->type = GNSS_NONE;

   C8 *option = NULL;
   C8 *end    = NULL;
   C8 *beg    = NULL;
   C8 *ptr    = NULL;

   while (1)
      {
      //
      // -NMEA: Specify COM port for NMEA receiver
      //                                  

      option = strstr(lpUCmdLine, "-NMEA:");

      if (option != NULL)
         {
         beg = &option[6];
         end = beg;
         ptr = S->address;

         C8 term = ' ';
         
         if (*end == '[')
            {
            term = ']';
            end++;
            } 

         while ((*end) && (*end != term))
            {
            *ptr++ = *end;
            *ptr = 0;

            end++;
            }

         while (*end == term) ++end;

         memmove(option, end, strlen(end)+1);

         S->type = GPS_NMEA_COM;
         continue;
         }

      //
      // -NMFILE: Specify name of NMEA input text file
      //                                  

      option = strstr(lpUCmdLine, "-NMFILE:");

      if (option != NULL)
         {
         beg = &option[8];
         end = beg;
         ptr = S->address;

         C8 term = ' ';
         
         if (*end == '[')
            {
            term = ']';
            end++;
            } 

         while ((*end) && (*end != term))
            {
            *ptr++ = *end;
            *ptr = 0;

            end++;
            }

         while (*end == term) ++end;

         memmove(option, end, strlen(end)+1);

         S->type = GPS_NMEA_FILE;
         continue;
         }

      //
      // -NMIP: Specify IP address of NMEA stream
      //

      option = strstr(lpUCmdLine, "-NMIP:");

      if (option != NULL)
         {
         beg = &option[6];
         end = beg;
         ptr = S->address;

         C8 term = ' ';
         
         if (*end == '[')
            {
            term = ']';
            end++;
            } 

         while ((*end) && (*end != term))
            {
            *ptr++ = *end;
            *ptr = 0;

            end++;
            }

         while (*end == term) ++end;

         memmove(option, end, strlen(end)+1);

         S->type = GPS_NMEA_TCPIP;
         continue;
         }

      //
      // -NMOUT: Specify text to send after connection
      //

      option = strstr(lpCmdLine, "-nmout:");

      if (option != NULL)
         {
         beg = &option[7];
         end = beg;
         ptr = S->setup_text;

         C8 term = ' ';
         
         if (*end == '[')
            {
            term = ']';
            end++;
            } 

         while ((*end) && (*end != term))
            {
            *ptr++ = *end;
            *ptr = 0;

            end++;
            }

         while (*end == term) ++end;

         memmove(option, end, strlen(end)+1);
         continue;
         }

      //
      // -GPGGA: Send lat/long sentence
      //

      option = strstr(lpCmdLine, "-gpgga:");

      if (option != NULL)
         {
         beg = &option[7];
         end = beg;
         ptr = S->GPGGA_text;

         C8 term = ' ';
         
         if (*end == '[')
            {
            term = ']';
            end++;
            } 

         while ((*end) && (*end != term))
            {
            *ptr++ = *end;
            *ptr = 0;

            end++;
            }

         while (*end == term) ++end;

         memmove(option, end, strlen(end)+1);
         continue;
         }

      //
      // Exit loop when no more CLI options are recognizable
      //

      break;
      }

   return TRUE;
}

// -------------------------------------------------------------------------
// GNSS_connect()
// -------------------------------------------------------------------------

bool WINAPI GNSS_connect(GNSS_STATE *S)
{
   if (S == NULL)
      {
      return FALSE;
      }

   if (S->C != NULL)
      {
      GNSS_disconnect(S);
      }

   S->C = new NMEA_SOURCE(S); 

   GNSS_CONTEXT *C = (GNSS_CONTEXT *) S->C;

   if (C == NULL)
      {
      sprintf(S->error_text, "Out of memory");
      return FALSE;
      }

   if (!C->connected)
      {
      return FALSE;
      }

   if (S->setup_text[0])
      {
      if (!C->xmit(S->setup_text))
         {
         GNSS_disconnect(S);
         sprintf(S->error_text, "Transmit failure");
         return FALSE;
         }
      }

   if (S->GPGGA_text[0])
      {
      if (!C->xmit_GPGGA(S->GPGGA_text))
         {
         GNSS_disconnect(S);
         sprintf(S->error_text, "Transmit failure");
         return FALSE;
         }

      GNSS_disconnect(S);
      return FALSE;
      }

   if (!C->start_thread())
      {
      GNSS_disconnect(S);
      return FALSE;
      }

   S->fix_num = 0;   
   return TRUE;
}

void WINAPI GNSS_disconnect(GNSS_STATE *S)
{
  if (S == NULL)
      {
      return;
      }

   GNSS_CONTEXT *C = (GNSS_CONTEXT *) S->C;

   if (C == NULL)
      {
      return;
      }

   delete C;
   S->C = NULL;
}

// -------------------------------------------------------------------------
// GNSS_update()
// -------------------------------------------------------------------------

bool WINAPI GNSS_update(GNSS_STATE *S)
{  
   if ((S == NULL) || (S->C == NULL))
      {
      return FALSE;
      }

   GNSS_CONTEXT *C = (GNSS_CONTEXT *) S->C;
   
   return C->update();
}

// -------------------------------------------------------------------------
// GNSS_send()
// -------------------------------------------------------------------------

bool WINAPI GNSS_send(GNSS_STATE *S, C8 *text)
{
   if ((S == NULL) || (S->C == NULL))
      {
      return FALSE;
      }

   GNSS_CONTEXT *C = (GNSS_CONTEXT *) S->C;

   return C->xmit(text);
}

