//
// GNSS.H: Read fix from various GNSS sources (GPS NMEA, etc.)
//
// Author: John Miles, KE5FX (jmiles@pop.net)
//

#ifndef GNSS_H
#define GNSS_H

#include "typedefs.h"

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef BUILDING_GNSS
   #define GNSSDEF __declspec(dllexport)
#else
   #define GNSSDEF __declspec(dllimport)
#endif

enum GNSS_TYPE
{
   GNSS_NONE,
   GPS_NMEA_FILE,
   GPS_NMEA_COM,
   GPS_NMEA_TCPIP
};

const DOUBLE INVALID_LATLONGALT = -1E6;

struct GNSS_STATE
{
   C8 error_text[512];

   //
   // Public info
   // 
   // Must be set manually or by calling GNSS_parse_command_line() prior to calling GNSS_connect()
   //

   GNSS_TYPE type;
   C8        address[512];
   C8        setup_text[16384];
   C8        GPGGA_text[16384];

   //
   // Valid after GNSS_update()
   //

   S64    fix_num;            // # of valid fixes since connection initiated

   DOUBLE latitude;
   DOUBLE longitude;
   bool   lat_long_valid;

   DOUBLE altitude_m;
   bool   alt_valid;

   DOUBLE geoid_sep_m;
   bool   geoid_sep_valid;

   S32    UTC_hours;
   S32    UTC_mins;
   S32    UTC_secs;
   bool   UTC_valid;

   //
   // Context-specific private data
   // (NULL if GNSS_connect() has not been called)
   //
   
   void *C;

   GNSS_STATE()
      {
      type           = GNSS_NONE;
      error_text[0]  = 0;
      address[0]     = 0;
      setup_text[0]  = 0;
      fix_num        = 0;
      latitude       = 0.0;
      longitude      = 0.0;
      lat_long_valid = FALSE;
      altitude_m     = 0.0;
      alt_valid      = FALSE;
      geoid_sep_m    = 0.0;
      geoid_sep_m    = FALSE;
      UTC_hours      = 0;
      UTC_mins       = 0;
      UTC_secs       = 0;
      UTC_valid      = FALSE;
      C              = NULL;
      }
};

GNSSDEF GNSS_STATE * WINAPI  GNSS_startup            (void);
GNSSDEF void         WINAPI  GNSS_shutdown           (GNSS_STATE *S);
GNSSDEF bool         WINAPI  GNSS_parse_command_line (GNSS_STATE *S, C8 *command_line);
GNSSDEF bool         WINAPI  GNSS_connect            (GNSS_STATE *S);
GNSSDEF bool         WINAPI  GNSS_update             (GNSS_STATE *S);
GNSSDEF bool         WINAPI  GNSS_send               (GNSS_STATE *S, C8 *text);
GNSSDEF void         WINAPI  GNSS_disconnect         (GNSS_STATE *S);

#ifdef __cplusplus
}
#endif

#endif
