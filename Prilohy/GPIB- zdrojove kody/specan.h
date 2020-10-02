//
// SPECAN.CPP: Basic spectrum-analyzer trace acquisition library
// (no error handling, multiple instruments, or other distractions)
//
// Author: John Miles, KE5FX (john@miles.io)
//

#ifndef SPECAN_H
#define SPECAN_H

#include "typedefs.h"

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef BUILDING_SPECAN
   #define SADEF __declspec(dllexport)
#else
   #define SADEF __declspec(dllimport)
#endif

enum SA_TYPE
{
   SATYPE_INVALID,
   SATYPE_TEK_490P,
   SATYPE_TEK_271X,
   SATYPE_TEK_278X,
   SATYPE_HP8566A_8568A,
   SATYPE_HP8566B_8568B,
   SATYPE_HP8560,
   SATYPE_HP8590,
   SATYPE_HP8569B,
   SATYPE_HP3585,
   SATYPE_HP358XA,
   SATYPE_HP70000,
   SATYPE_R3267_SERIES,
   SATYPE_R3465_SERIES,
   SATYPE_R3131,
   SATYPE_R3132_SERIES,
   SATYPE_R3261_R3361,
   SATYPE_R3265_R3271,
   SATYPE_TR4172_TR4173,
   SATYPE_MS8604A,
   SATYPE_MS265X_MS266X,
   SATYPE_E4406A,
   SATYPE_FSE,
   SATYPE_FSP,
   SATYPE_FSU,
   SATYPE_N9900,
   SATYPE_SCPI,
   SATYPE_SA44
};

struct SA_STATE
{
   //
   // Following members are initialized by SA_startup(), updated by
   // SA_parse_command_line(), and/or may be modified by app 
   // while analyzer is disconnected
   //

   DOUBLE  F_offset;
   DOUBLE  A_offset;

   S32     hi_speed_acq;         // If TRUE, favor speed over resolution, CRT display updates, etc. where possible

   S32     HP856XA_mode;
   S32     HP8569B_mode;
   S32     HP3585_mode;
   S32     HP358XA_mode;
   S32     SCPI_mode;
   S32     Advantest_mode;
   S32     R3261_mode;
   S32     TR417X_mode;
   S32     SA44_mode;

   bool    addr_specified;       // Address from -addr option, if present
   C8      addr[512];

   bool    interface_name_specified;
   C8      interface_name[512];  // Interface name from -interface option, if present

   //
   // Error state
   //

   bool    error;                // TRUE if fatal error occurred in a previous API call
   C8      error_text[4096];

   //
   // Always valid after SA_connect()
   //

   SA_TYPE type;
   C8      ID_string[512];

   DOUBLE  CF_Hz;                // normally (max_Hz + min_Hz) / 2, but may differ, e.g. in Tek 49x series full-band modes
   DOUBLE  min_Hz;
   DOUBLE  max_Hz;
   DOUBLE  min_dBm;
   DOUBLE  max_dBm;
   S32     amplitude_levels;
   S32     dB_division;
   S32     n_trace_points;

   //
   // Valid after SA_connect() (but not all parameters are reported
   // on all analyzers)
   //
   // Note that a nonzero vid_avgs value does not mean that video
   // averaging is enabled (e.g., HP 8561E reports 100 after PRESET).
   // There appears to be no way to tell if video averaging is turned on
   // on the 8561E
   //

   S32     FFT_size;    // Valid if > 0
   DOUBLE  RBW_Hz;      // Valid if >= 0.0
   DOUBLE  VBW_Hz;      // Valid if >= 0.0
   S32     vid_avgs;    // Valid if >= 0, known to be turned off if 0
   DOUBLE  sweep_secs;  // Valid if >= 0
   DOUBLE  RFATT_dB;    // Valid if > -10000.0

   //
   // Valid after SA_fetch_trace() 
   //

   DOUBLE *dBm_values;       

   //
   // Instrument-specific private data
   // 

   DOUBLE CTRL_RL_dBm;   
   DOUBLE CTRL_start_Hz; 
   DOUBLE CTRL_stop_Hz;  
   bool   CTRL_freq_specified;
   DOUBLE CTRL_CF_Hz;  
   DOUBLE CTRL_span_Hz;  
   S32    CTRL_sens;     // (for Signal Hound)
   DOUBLE CTRL_atten_dB; 
   S32    CTRL_FFT_size; 
   S32    CTRL_n_divs;
   S32    CTRL_dB_div;

   S32   SH_initialized;

   S32   VSA_display_state;
   S32   VSA_orig_avg_state;
   S32   VSA_orig_display_state;

   S32   FSP_display_state;
   S32   FSP_orig_display_state;

   void *blind_data;
};

SADEF SA_STATE * WINAPI  SA_startup            (void);
SADEF void       WINAPI  SA_parse_command_line (C8 *command_line);
SADEF bool       WINAPI  SA_connect            (S32 address);
SADEF void       __cdecl SA_printf             (C8 *format, ...);
SADEF C8 *       __cdecl SA_query              (C8 *format, ...);
SADEF void       __cdecl SA_query_printf       (C8 *format, ...);
SADEF void       WINAPI  SA_fetch_trace        (void);
SADEF void       WINAPI  SA_disconnect         (bool final_exit = FALSE);
SADEF void       WINAPI  SA_shutdown           (void);

//
// Trace resampler
//

enum RESAMPLE_OP
{
   RT_POINT,
   RT_SPLINE,
   RT_AVG,
   RT_MIN,
   RT_MAX,
};

SADEF void WINAPI SA_resample_data(DOUBLE *src,  S32 ns, 
                                   DOUBLE *dest, S32 nd,
                                   RESAMPLE_OP operation);

#ifdef __cplusplus
}
#endif

#endif
