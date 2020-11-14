#include <stdio.h>
#include <inttypes.h>
#include <malloc.h>
#include <float.h>
#include <windows.h>

#include "gpiblib.h"
#include <cassert>

#define FTOI(x) (S32((x)+0.5))

static int cmdline_i = 0;
static int cmdline_a = 16;
static int cmdline_s11 = 0;
static int cmdline_s21 = 0;
static int cmdline_s12 = 0;
static int cmdline_s22 = 0;

static volatile int ready_to_receive_command = 1;
static volatile int running = 1;
static volatile int autosweep = 0;
static char *save_file_name = 0;

enum input_mode { mode_menu, mode_cmd, mode_input_blocked };

static volatile input_mode current_input_mode = mode_menu;

static char ln[52];

enum action_type {
    action_none, action_connect, action_disconnect, action_sweep, action_getstate, action_setstate,
    action_getcalib, action_setcalib, action_cmd_puts, action_cmd_query, action_cmd_status, action_cmd_read_asc,
    action_cmd_read_bin, action_exit, action_reset, action_freset
};

static volatile action_type action;


HANDLE action_event, end_of_input_event;


void WINAPI GPIB_error(C8* msg, S32 ibsta, S32 iberr, S32 ibcntl)
{
    printf("GPIB Error %s, ibsta=%d, iberr=%d, ibcntl=%d\n", msg, ibsta, iberr, ibcntl);
    exit(1);
}

S32 S16_BE(C8* s)
{
    U8* p = (U8*)s;
    return (((S32)p[0]) << 8) + (S32)p[1];
}

C8* time_text(S64 time_uS)
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

    GetTimeFormatA(LOCALE_SYSTEM_DEFAULT,
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

C8* date_text(S64 time_uS)
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

    GetDateFormatA(LOCALE_SYSTEM_DEFAULT,
        0,
        &stime,
        NULL,
        text,
        sizeof(text));

    return text;
}

C8* timestamp(void)
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

int DC_entry = 0;
DOUBLE R_ohms = 50;
C8 instrument_name[512];

bool is_8753(void)
{
    return (strstr(instrument_name, "8753") != NULL) ||
           (strstr(instrument_name, "8702") != NULL) ||
           (strstr(instrument_name, "8703") != NULL);
}

bool is_8752(void)
{
    return (strstr(instrument_name, "8752") != NULL);
}

bool is_8720(void)
{
    return (strstr(instrument_name, "8719") != NULL) ||
        (strstr(instrument_name, "8720") != NULL) ||
        (strstr(instrument_name, "8722") != NULL);
}

bool is_8510(void)
{
    return (strstr(instrument_name, "8510") != NULL);
}

bool is_8510C(void)
{
    return (strstr(instrument_name, "8510C") != NULL);
}

void instrument_setup(bool debug_mode = TRUE)
{
    if (debug_mode)
    {
        GPIB_printf("DEBUON;");
    }

    C8* data = GPIB_query("OUTPIDEN");
    _snprintf(instrument_name, sizeof(instrument_name) - 1, "%s", data);
    //printf("instrument name: %s\n", data);   
    GPIB_printf("HOLD;");
}

bool read_complex_trace(const C8* param,
    const C8* query,
    COMPLEX_DOUBLE* dest,
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

        U8 result = GPIB_serial_poll();

        if (result & mask)
        {
            printf("! %s sweep finished in %d ms\n", param, timeGetTime() - st);
            break;
        }
    }

    if (is_8753()) { GPIB_printf("CLES;SRE 0;"); }
    if (is_8752()) { GPIB_printf("CLES;SRE 0;"); }
    if (is_8720()) { GPIB_printf("CLES;SRE 0;"); }
    if (is_8510()) { GPIB_printf("CLES;"); }

    GPIB_printf("%s;", query);

    for (S32 i = 0; i < cnt; i++)
    {
        C8* data = GPIB_read_ASC(-1, FALSE);

        DOUBLE I = DBL_MIN;
        DOUBLE Q = DBL_MIN;

        sscanf(data, "%lf, %lf", &I, &Q);

        if ((I == DBL_MIN) || (Q == DBL_MIN))
        {
            printf("Error VNA read timed out reading %s (point %d of %d points)", param, i, cnt);
            return FALSE;
        }

        dest[i].real = I;
        dest[i].imag = Q;
    }

    return 1;
}

static int connected = 0;

static S32 n_AC_points;
static S32 first_AC_point;
static bool include_DC;
static S32 n_alloc_points;
static DOUBLE  *freq_Hz;
COMPLEX_DOUBLE *S11, *S21, *S12, *S22;
static double start_Hz, stop_Hz;

int connect()
{
    if (connected) return 1;
    S32 addr = cmdline_a;
    
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
    instrument_setup();

    connected = 1;
    return 1;
}

int sweep()
{
    if (!connected)
    {
        printf("!not connected\n");
        return 0;
    }
    C8* data = GPIB_query("FORM4;STAR;OUTPACTI;");
    sscanf(data, "%lf", &start_Hz);
    data = GPIB_query("STOP;OUTPACTI;");
    sscanf(data, "%lf", &stop_Hz);

    data = GPIB_query("POIN;OUTPACTI;");
    DOUBLE fn = 0.0;
    sscanf(data, "%lf", &fn);
    S32 n = (S32)(fn + 0.5);

    if ((n < 1) || (n > 1000000))
    {
        GPIB_disconnect();
        printf("Error n_points = %d\n", n);
        return 0;
    }

    //
    // Reserve space for DC term if requested
    // 
    include_DC = (DC_entry != 0);

    n_alloc_points = n;
    n_AC_points = n;
    first_AC_point = 0;

    if (include_DC)
    {
        n_alloc_points++;
        first_AC_point = 1;
    }

    freq_Hz = (DOUBLE*)malloc(n_alloc_points * sizeof(freq_Hz[0])); memset(freq_Hz, 0, n_alloc_points * sizeof(freq_Hz[0]));

    S11 = (COMPLEX_DOUBLE*)malloc(n_alloc_points * sizeof(S11[0])); memset(S11, 0, n_alloc_points * sizeof(S11[0]));
    S21 = (COMPLEX_DOUBLE*)malloc(n_alloc_points * sizeof(S21[0])); memset(S21, 0, n_alloc_points * sizeof(S21[0]));
    S12 = (COMPLEX_DOUBLE*)malloc(n_alloc_points * sizeof(S12[0])); memset(S12, 0, n_alloc_points * sizeof(S12[0]));
    S22 = (COMPLEX_DOUBLE*)malloc(n_alloc_points * sizeof(S22[0])); memset(S22, 0, n_alloc_points * sizeof(S22[0]));

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

    if (!is_8510())
    {
        lin_sweep = (GPIB_query("LINFREQ?;")[0] == '1');
    }

    if (lin_sweep)
    {
        for (S32 i = 0; i < n_AC_points; i++)
        {
            freq_Hz[i + first_AC_point] = start_Hz + (((stop_Hz - start_Hz) * i) / (n_AC_points - 1));
        }
    }
    else
    {
        GPIB_printf("OUTPLIML;");

        for (S32 i = 0; i < n_AC_points; i++)
        {
            C8* data = GPIB_read_ASC(-1, FALSE);

            DOUBLE f = DBL_MIN;
            sscanf(data, "%lf", &f);

            if (f == DBL_MIN)
            {
                printf("Error, VNA read timed out reading OUTPLIML (point %d of %d points)", i, n_AC_points);
                GPIB_disconnect();
                return 0;
            }

            freq_Hz[i + first_AC_point] = f;
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
        for (active_param = 0; active_param < 4; active_param++)
        {
            C8 text[512] = { 0 };
            _snprintf(text, sizeof(text) - 1, "%s?", param_names[active_param]);

            if (GPIB_query(text)[0] == '1')
            {
                break;
            }
        }
    }

    //
    // Read data from VNA
    //

    int sweep_iteration = 0;

    do {
        bool result = FALSE;

        if (cmdline_s11) result = read_complex_trace("S11", "OUTPDATA", &S11[first_AC_point], n_AC_points, 20);
        if (cmdline_s21) result = result && read_complex_trace("S21", "OUTPDATA", &S21[first_AC_point], n_AC_points, 40);
        if (cmdline_s12) result = result && read_complex_trace("S12", "OUTPDATA", &S12[first_AC_point], n_AC_points, 60);
        if (cmdline_s22) result = result && read_complex_trace("S22", "OUTPDATA", &S22[first_AC_point], n_AC_points, 80);

        if (result)
        {
            double min_Hz = include_DC ? 0.0 : start_Hz;
            double max_Hz = stop_Hz;
            double Zo = R_ohms;

            char* real_file_name = 0;
            FILE* sf = 0;

            if (save_file_name)
            {
                real_file_name = (char*)malloc(strlen(save_file_name) + 15);
                if (autosweep) sprintf(real_file_name, "%04d_%s", sweep_iteration, save_file_name);
                else strcpy(real_file_name, save_file_name);
                sf = fopen(real_file_name, "w+");
            }

            printf("! Touchstone 1.1 file saved by HPCTRL.EXE\n"
                "! %s\n"
                "!\n"
                "!    Source: %s\n", timestamp(), instrument_name);
            for (S32 i = 0; i < n_alloc_points; i++)
            {
                printf("%.6lf", freq_Hz[i]);
                if (cmdline_s11) printf(" %.6lf %.6lf", S11[i].real, S11[i].imag);
                if (cmdline_s21) printf(" %.6lf %.6lf", S21[i].real, S21[i].imag);
                if (cmdline_s12) printf(" %.6lf %.6lf", S12[i].real, S12[i].imag);
                if (cmdline_s22) printf(" %.6lf %.6lf", S22[i].real, S22[i].imag);
                printf("\n");
            }

            if (sf)
            {
                fprintf(sf, "! Touchstone 1.1 file saved by HPCTRL.EXE\n"
                    "! %s\n"
                    "!\n"
                    "!    Source: %s\n", timestamp(), instrument_name);
                for (S32 i = 0; i < n_alloc_points; i++)
                {
                    fprintf(sf, "%.6lf", freq_Hz[i]);
                    if (cmdline_s11) fprintf(sf, " %.6lf %.6lf", S11[i].real, S11[i].imag);
                    if (cmdline_s21) fprintf(sf, " %.6lf %.6lf", S21[i].real, S21[i].imag);
                    if (cmdline_s12) fprintf(sf, " %.6lf %.6lf", S12[i].real, S12[i].imag);
                    if (cmdline_s22) fprintf(sf, " %.6lf %.6lf", S22[i].real, S22[i].imag);
                    fprintf(sf, "\n");
                }
                fclose(sf);
                free(real_file_name);
            }
        }
        sweep_iteration++;

    }  while (autosweep && running);

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

    GPIB_printf("DEBUOFF;CONT;");

    free(freq_Hz);
    free(S11);
    free(S21);
    free(S12);
    free(S22);

    printf("\n");
    fflush(stdout);
    if (save_file_name) free(save_file_name);
    save_file_name = 0;
    return 1;
}

void disconnect()
{
    Sleep(500);
    GPIB_disconnect();
    connected = 0;
}

void getstate(int include_cal)
{
    int CALIONE2 = 1;
    if (!connected)
    {
        printf("!not connected\n");
        return;
    }
    
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
    uint8_t *data = (uint8_t *)GPIB_read_BIN(2, TRUE, FALSE, &actual);

    if (actual != 2)
    {
        printf("Error: OUTPLEAS header query returned %d bytes", actual);
        return;
    }

    if ((data[0] != '#') || (data[1] != 'A'))
    {
        printf("Error: OUTPLEAS FORM1 block header was 0x%.2X 0x%.2X", data[0], data[1]);
        return;
    }

    learn_H = *(U16*)data;

    printf("%02x%02x", data[0], data[1]);

    //
    // Get length in bytes of learn string
    //

    actual = 0;
    data = (uint8_t *)GPIB_read_BIN(2, TRUE, FALSE, &actual);

    S32 len = S16_BE((C8 *)data);

    if (actual != 2)
    {
        printf("Error: OUTPLEAS string len returned %d bytes", actual);
        return;
    }

    learn_N = *(U16*)data;

    printf("%02x%02x\n", data[0], data[1]);

    //
    // Get learn string data
    //

    data = (uint8_t *)GPIB_read_BIN(len, TRUE, FALSE, &learn_string_size);

    if (learn_string_size != len)
    {
        printf("Error: OUTPLEAS string len = %d, received %d", len, learn_string_size);
        return;
    }

    for (int i = 0; i < learn_string_size; i++)
    {
        printf("%02x", data[i]);
        if ((i % 40 == 39) && (i < learn_string_size - 1)) printf("\n");
    }
    printf("\n");

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

            const C8* types[] = { "RESPONSE", "RESPONSE & ISOL'N", "S11 1-PORT", "S22 1-PORT", "2-PORT" };
            const C8* names[] = { "CALIRESP", "CALIRAI",           "CALIS111",   "CALIS221",   "CALIFUL2" };
            const S32 cnts[] = { 1,          2,                   3,            3,            12 };

            S32 active_cal_type = -1;
            S32 active_cal_set = -1;

            S32 n_types = ARY_CNT(cnts);

            C8* result = GPIB_query("CALI?;");
            C8* term = strrchr(result, '"');                          // Remove leading and trailing quotes returned by 8510

            if (term != NULL)
            {
                *term = 0;
                result++;
            }

            for (S32 i = 0; i < n_types; i++)
            {
                if (!_stricmp(result, types[i]))
                {
                    active_cal_type = i;
                    active_cal_set = atoi(GPIB_query("CALS?;"));
                    break;
                }
            }

            if ((active_cal_type != -1) && (active_cal_set > 0))
            {
                const C8* active_cal_name = names[active_cal_type];   // Get name of active calibration type
                S32       n_arrays = cnts[active_cal_type];    // Get # of data arrays for active calibration type

                data = (uint8_t *)GPIB_query("POIN;OUTPACTI;");                  // Get # of points in calibrated trace 
                DOUBLE fnpts = 0.0;
                sscanf((C8 *)data, "%lf", &fnpts);
                S32 trace_points = FTOI(fnpts);

                if ((trace_points < 1) || (trace_points > 1000000))
                {
                    printf("Error: trace_points = %d\n", trace_points);
                    return;
                }

                S32 array_bytes = trace_points * 6;
                S32 total_bytes = array_bytes * n_arrays;

                printf("CORROFF;HOLD;FORM1;%s;", active_cal_name);

                //
                // For each array....
                // 

                S32 n = 0;

                for (S32 j = 0; j < n_arrays; j++)
                {
                    n += array_bytes;

                    GPIB_printf("FORM1;OUTPCALC%d%d;", (j + 1) / 10, (j + 1) % 10);

                    data = (uint8_t *)GPIB_read_BIN(2);

                    if ((data[0] != '#') || (data[1] != 'A'))
                    {
                        printf("Error: OUTPCALC FORM1 block header was 0x%.2X 0x%.2X", data[0], data[1]);
                        return;
                    }

                    U16 H = *(U16*)data;

                    data = (uint8_t *)GPIB_read_BIN(2);

                    len = S16_BE((C8 *)data);

                    if (len != array_bytes)
                    {
                        printf("Error: OUTPCALC returned %d bytes, expected %d", len, array_bytes);
                        return;
                    }

                    U16 N = *(U16*)data;

                    printf("%d\n", H);
                    printf("%d\n", N);
                    printf("INPUCALC%d%d;", (j + 1) / 10, (j + 1) % 10);

                    C8* IQ = GPIB_read_BIN(array_bytes, TRUE, FALSE, &actual);

                    if (actual != array_bytes)
                    {
                        printf("Error: OUTPCALC%d%d returned %d bytes, expected %d", (j + 1) / 10, (j + 1) % 10, actual, array_bytes);
                        return;
                    }

                    printf("%d\n", array_bytes);
                    for (int i = 0; i < array_bytes; i++)
                    {
                        printf("%02x", IQ[i]);
                        if ((i % 40 == 39) && (i < array_bytes - 1)) printf("\n");
                    }
                    printf("\n");
                }

                assert(n == total_bytes);

                printf("SAVC; CALS%d; CORROFF;", active_cal_set);

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

            const C8* types[] = { "CALIRESP", "CALIRAI", "CALIS111", "CALIS221", "CALIFUL2", "CALIONE2" };
            const S32 cnts[] = { 1,          2,         3,          3,          12,         12 };

            S32 active_cal_type = -1;
            S32 n_types = ARY_CNT(cnts);

            if (!CALIONE2)
            {
                n_types--;
            }

            for (S32 i = 0; i < n_types; i++)
            {
                GPIB_printf("%s?;", types[i]);
                C8* result = GPIB_read_ASC();

                if (result[0] == '1')
                {
                    active_cal_type = i;
                    break;
                }
            }

            if (active_cal_type != -1)
            {
                const C8* active_cal_name = types[active_cal_type];   // Get name of active calibration type
                S32       n_arrays = cnts[active_cal_type];    // Get # of data arrays for active calibration type

                data = (uint8_t *)GPIB_query("FORM3;POIN?;");                    // Get # of points in calibrated trace
                DOUBLE fnpts = 0.0;
                sscanf((C8 *)data, "%lf", &fnpts);
                S32 trace_points = FTOI(fnpts);

                S32 array_bytes = trace_points * 6;
                S32 total_bytes = array_bytes * n_arrays;

                S32 cmdlen = 15 + strlen(active_cal_name);             // Write command to file that will select this calibration type
                    
                //printf("CORROFF;FORM1;%s;", active_cal_name);
                printf("%s\n", active_cal_name);
                printf("%d\n", n_arrays);
                //
                // For each array....
                // 

                S32 n = 0;

                for (S32 j = 0; j < n_arrays; j++)
                {
                    n += array_bytes;

                    GPIB_printf("FORM1;OUTPCALC%d%d;", (j + 1) / 10, (j + 1) % 10);
                    
                    data = (uint8_t *)GPIB_read_BIN(2);

                    if ((data[0] != '#') || (data[1] != 'A'))
                    {
                        printf("Error: OUTPCALC FORM1 block header was 0x%.2X 0x%.2X", data[0], data[1]);
                        return;
                    }

                    U16 H = *(U16*)data;

                    data = (uint8_t *)GPIB_read_BIN(2);

                    len = S16_BE((C8 *)data);

                    if (len != array_bytes)
                    {
                        printf("Error: OUTPCALC returned %d bytes, expected %d", len, array_bytes);
                        return;
                    }

                    U16 N = *(U16*)data;

                    //printf("INPUCALC%d%d;", (j + 1) / 10, (j + 1) % 10);
                    
                    uint8_t* IQ = (uint8_t *)GPIB_read_BIN(array_bytes, TRUE, FALSE, &actual);

                    if (actual != array_bytes)
                    {
                        printf("Error: OUTPCALC%d%d returned %d bytes, expected %d", (j + 1) / 10, (j + 1) % 10, actual, array_bytes);
                        return;
                    }

                    printf("%d\n", array_bytes);
                    for (int i = 0; i < array_bytes; i++)
                    {
                        printf("%02x", IQ[i]);
                        if ((i % 40 == 39) && (i < array_bytes - 1)) printf("\n");
                    }
                    printf("\n");
                }

                assert(n == total_bytes);

               // printf("OPC?;SAVC;\n");
            }
        }
    }

    printf("\n");
    fflush(stdout);

    Sleep(500);
    GPIB_printf("DEBUOFF;CONT;");
}

void setstate(int include_cal)
{
    if (!connected)
    {
        printf("!not connected\n");
        return;
    }
    int val, val2;
    uint8_t header[2], len[2];

    scanf("%02x%02x", &val, &val2);
    header[0] = (uint8_t)val;
    header[1] = (uint8_t)val2;
    if ((header[0] != '#') || (header[1] != 'A'))
    {
        printf("!setstate: header does not match\n");
        return;
    }
    scanf("%02x%02x\n", &val, &val2);
    len[0] = (uint8_t)val;
    len[1] = (uint8_t)val2;
    uint16_t length = (len[0] << 8) + len[1];  // 16-bit, big endian
    uint8_t *data = (uint8_t *)malloc(22 + length);
    sprintf((char *)data, "FORM1;INPULEAS;");
    data[15] = header[0];
    data[16] = header[1];
    data[17] = len[0];
    data[18] = len[1];
    for (int i = 0; i < length; i++)
    {        
        if (scanf("%02x", &val) != 1)
        {
            printf("!setstate: state data broken\n");
            return;
        }
        data[i + 19] = (uint8_t)val;
        //if ((i % 40 == 39) && (i < length - 1)) scanf("\n");
    }
    //scanf("\n");
    printf("!going to send...\n");
        
    GPIB_write_BIN(data, length + 19);    
    Sleep(1500);
    if (include_cal)
    {
        char active_cal_name[20];
        int n_arrays;
        scanf("%20s\n", active_cal_name);
        GPIB_printf("CORROFF;FORM1;%s;", active_cal_name);        
        scanf("%d\n", &n_arrays);
        for (int i = 0; i < n_arrays; i++)
        {
            int array_bytes, val;

            scanf("%d\n", &array_bytes);
            uint8_t* data = (uint8_t*)malloc(15 + array_bytes);

            sprintf((char *)data, "INPUCALC%d%d;", (i + 1) / 10, (i + 1) % 10);
            data[11] = header[0];
            data[12] = header[1];
            data[13] = (array_bytes >> 8);
            data[14] = (array_bytes & 255);
            
            for (int i = 0; i < array_bytes; i++)
            {
                scanf("%02x", &val);
                data[15 + i] = (uint8_t)val;
                if ((i % 40 == 39) && (i < array_bytes - 1)) scanf("\n");
            }
            scanf("\n");
            GPIB_write_BIN(data, 15 + array_bytes);
            Sleep(250);

            GPIB_printf("CLES;SRE 32;ESE 1;OPC;SAVC;");

            S32 st = timeGetTime();
            while (1)
            {
                Sleep(100);
                U8 result = GPIB_serial_poll();
                if (result & 0x40)
                {
                    printf("!Complete in %d ms\n", timeGetTime() - st);
                    break;
                }
            }
            GPIB_printf("CLES;SRE 0;");            
        }
        Sleep(1500);
    }    
    GPIB_printf("DEBUOFF;CONT;");
    free(data);
}

void reset()
{
    if (!connected)
    {
        printf("!not connected\n");
        return;
    }
    GPIB_puts("PRES;");
}

void factory_reset()
{
    if (!connected)
    {
        printf("!not connected\n");
        return;
    }
    if (!is_8510())
    {
        GPIB_puts("RST;");
    }
    else
    {
        if (is_8510C())
            GPIB_puts("FACTPRES;");
        else
            GPIB_puts("PRES;");
    }
}

void direct_command()
{
    // s string   - send string using gpib_puts()
    // q query    - send string using gpib_query() and print result
    // a          - read and print ascii response
    // b          - read and print binary response
    // ?          - print status
    // .          - exit direct command mode
    uint8_t* data;
    int len;
    char* response = 0;

    switch (action)
    {
    case action_cmd_puts:
        GPIB_puts(ln + 2);
        break;
    case action_cmd_query:
        response = GPIB_query(ln + 2);
        printf("%s", response);
        fflush(stdout);
        break;
    case action_cmd_read_asc:
        data = (uint8_t *)GPIB_read_ASC();
        if ((data[0] != '#') || (data[1] != 'A'))
            printf("!header not received\n");
        len = (data[2] << 8) + data[3];
        for (int i = 0; i < len; i++)
            printf("%c", data[i + 4]);
        printf("\n");
        fflush(stdout);
        break;
    case action_cmd_read_bin:
        data = (uint8_t*)GPIB_read_BIN();
        if ((data[0] != '#') || (data[1] != 'A'))
            printf("!header not received\n");
        len = (data[2] << 8) + data[3];
        for (int i = 0; i < len + 4; i++)
            printf("%02x", data[i]);
        printf("\n");
        fflush(stdout);
        break;
    case action_cmd_status:
        S32 status = GPIB_status();            
        printf("%d\n", status);
        fflush(stdout);
        break;
    } 
}

int measure()
{
    if (connect())
        if (sweep())
        {
            disconnect();
            return 1;
        }
    return 0;
}

void help()
{
    printf("         CONNECT    ... connect to the device\n");
    printf("         DISCONNECT ... disconnect the device\n");
    printf("         S11 .. S22 ... configure (add) a channel for measurement\n");
    printf("         ALL        ... configure measurement of all 4 channels\n");
    printf("         CLEAR      ... reset measurement config to no channels\n");
    printf("         MEASURE    ... perform configured measurement\n");
    printf("         M+         ... perform repeated configured measurements\n");
    printf("         M-         ... stop the repetitions of the measurements\n");
    printf("         FILE path  ... configure file to save the next measurement\n");
    printf("                        for continuous, they are prefixed with XXXX_\n");
    printf("         GETSTATE   ... dump the device state\n");
    printf("         SETSTATE   ... set the device state (followed in next line)\n");
    printf("         GETCALIB   ... get the device calibration\n");
    printf("         SETCALIB   ... set the device calibration (followed in next line)\n");
    printf("         RESET      ... reset instrument\n");
    printf("         FACTRESET  ... factory reset instrument\n");
    printf("         CMD        ... enter direct command mode:\n");
    printf("             s str  ... send the string using gpib_puts()\n");
    printf("             q str  ... send a query and read a string\n");
    printf("                        response using gpib_query()\n");
    printf("             a      ... retrieve response with gpib_read_ASC()\n");
    printf("             b      ... retrieve response with gpib_read_BIN()\n");
    printf("             ?      ... read and print status\n");
    printf("             .      ... leave direct command mode\n");
    printf("         HELP       ... print this help\n");
    printf("         EXIT       ... terminate the application\n");
    fflush(stdout);
}

void print_usage()
{
    printf("usage: hpctrl [-a n] [-i | [-S11][-S21][-S12][-S22]]\n\n");
    printf(" -a n   specify device address, default=16\n");
    printf(" -Sxy   retrieve measurement from channel xy\n");
    printf(" -i     interactive mode, accepted commands:\n");
    help();
}

void parse_cmdline(int argc, const char** argv)
{
    for (int i = 1; i < argc; i++)
        if (_stricmp(argv[i], "-i") == 0) cmdline_i = 1;
        else if (_stricmp(argv[i], "-a") == 0) { if (++i < argc) sscanf(argv[i], "%d", &cmdline_a); }
        else if (_stricmp(argv[i], "-s11") == 0) cmdline_s11 = 1;
        else if (_stricmp(argv[i], "-s21") == 0) cmdline_s21 = 1;
        else if (_stricmp(argv[i], "-s12") == 0) cmdline_s12 = 1;
        else if (_stricmp(argv[i], "-s22") == 0) cmdline_s22 = 1;
}

void set_file()
{
    if (strlen(ln) < 6) return;
    if (save_file_name) free(save_file_name);
    save_file_name = (char*)malloc(strlen(ln + 5) + 1);
    strcpy(save_file_name, ln + 5);
}

DWORD WINAPI interactive_thread(LPVOID arg)
{
    do {
        if (current_input_mode == mode_input_blocked)
        {
            DWORD event = WaitForSingleObject(
                end_of_input_event, // event handle
                INFINITE);    // indefinite wait

            if (event != WAIT_OBJECT_0)
            {
                printf("Wait error (%d)\n", GetLastError());
                exit(1);
            }
            current_input_mode = mode_menu;
        }
        fgets(ln, 50, stdin);
        ln[strlen(ln) - 1] = 0; //strip EOL
        if (strlen(ln) == 0) continue;

        action = action_none;

        if (current_input_mode == mode_menu)
        { 
            if (_stricmp(ln, "HELP") == 0) help();
            else if (_stricmp(ln, "USAGE") == 0) print_usage();
            else if (_stricmp(ln, "EXIT") == 0) action = action_exit;
            else if (_stricmp(ln, "M-") == 0) autosweep = 0;
            else if (!ready_to_receive_command) printf("!not ready, try again later (%s)\n", ln);
            else if (_stricmp(ln, "CONNECT") == 0) action = action_connect;
            else if (_stricmp(ln, "DISCONNECT") == 0) action = action_disconnect;
            else if (_stricmp(ln, "S11") == 0) cmdline_s11 = 1;
            else if (_stricmp(ln, "S21") == 0) cmdline_s21 = 1;
            else if (_stricmp(ln, "S12") == 0) cmdline_s12 = 1;
            else if (_stricmp(ln, "S22") == 0) cmdline_s22 = 1;
            else if (_stricmp(ln, "ALL") == 0) cmdline_s11 = cmdline_s21 = cmdline_s12 = cmdline_s22 = 1;
            else if (_stricmp(ln, "CLEAR") == 0) cmdline_s11 = cmdline_s21 = cmdline_s12 = cmdline_s22 = 0;
            else if (_strnicmp(ln, "FILE", 4) == 0) set_file();
            else if (_stricmp(ln, "MEASURE") == 0) action = action_sweep;
            else if (_stricmp(ln, "M+") == 0) { autosweep = 1; action = action_sweep; }
            else if (_stricmp(ln, "GETSTATE") == 0) action = action_getstate;
            else if (_stricmp(ln, "SETSTATE") == 0) { action = action_setstate; current_input_mode = mode_input_blocked; }
            else if (_stricmp(ln, "GETCALIB") == 0) action = action_getcalib;
            else if (_stricmp(ln, "SETCALIB") == 0) { action = action_setcalib; current_input_mode = mode_input_blocked; }
            else if (_stricmp(ln, "RESET") == 0) action = action_reset;
            else if (_stricmp(ln, "FACTRESET") == 0) action = action_freset;
            else if (_stricmp(ln, "CMD") == 0) current_input_mode = mode_cmd;
            else printf("!unknown command %s\n", ln);            
        }
        else if (current_input_mode == mode_cmd)
        {
            if (ln[0] == '.') current_input_mode = mode_menu;
            else if ((ln[0] == 's') && (strlen(ln) > 2)) action = action_cmd_puts;
            else if ((ln[0] == 'q') && (strlen(ln) > 2)) action = action_cmd_query;
            else if (ln[0] == 'a') action = action_cmd_read_asc;
            else if (ln[0] == 'b') action = action_cmd_read_bin;
            else if (ln[0] == '?') action = action_cmd_status;
        }

        if (action != action_none)
        {
            ready_to_receive_command = 0;
            if (!SetEvent(action_event))
            {
                printf("SetEvent failed (%d)\n", GetLastError());
                exit(1);
            }
        }

    } while (action != action_exit);
    if (connected)
    {
        printf("!auto disconnect before exit\n");
        disconnect();
    }
    return 0;
}

void create_event_and_thread()
{
    action_event = CreateEvent(
        NULL,               // default security attributes
        FALSE,               // auto-reset event
        FALSE,              // initial state is nonsignaled
        0                   // no event object name
    );

    if (action_event == NULL)
    {
        printf("CreateEvent failed (%d)\n", GetLastError());
        exit(1);
    }

    end_of_input_event = CreateEvent(
        NULL,               // default security attributes
        FALSE,              // auto-reset event
        FALSE,              // initial state is nonsignaled
        0                   // no event object name
    );

    if (end_of_input_event == NULL)
    {
        printf("CreateEvent failed (%d)\n", GetLastError());
        exit(1);
    }

    HANDLE t = CreateThread(
        NULL,                   // default security attributes
        0,                      // use default stack size  
        interactive_thread,       // thread function name
        0,                       // argument to thread function 
        0,                      // use default creation flags 
        0);                    // does not return the thread identifier 

    if (t == NULL)
    {
        printf("CreateThread failed (%d)\n", GetLastError());
        exit(1);
    }
}

void main_action_loop()
{
    while (running)
    {
        DWORD event = WaitForSingleObject(
            action_event, // event handle
            INFINITE);    // indefinite wait
        
        if (event != WAIT_OBJECT_0)
        {
            printf("Wait error (%d)\n", GetLastError());
            exit(1);
        }

        switch (action)
        {
        case action_connect: connect();
            break;
        case action_disconnect: disconnect();
            break;
        case action_sweep: sweep();
            break;
        case action_getstate: getstate(0);
            break;
        case action_setstate: setstate(0);
            if (!SetEvent(end_of_input_event))
            {
                printf("SetEvent failed (%d)\n", GetLastError());
                exit(1);
            }
            break;
        case action_getcalib: getstate(1);
            break;
        case action_setcalib: setstate(1);
            if (!SetEvent(end_of_input_event))
            {
                printf("SetEvent failed (%d)\n", GetLastError());
                exit(1);
            }
            break;
        case action_cmd_puts: 
        case action_cmd_status: 
        case action_cmd_query:
        case action_cmd_read_asc:
        case action_cmd_read_bin:
            direct_command();
            break;
        case action_exit: running = 0;
            break;
        case action_reset: reset();
            break;
        case action_freset: factory_reset();
            break;
        }
        //printf("!ok\n");
        ready_to_receive_command = 1;
    }
}

void interactive()
{
    create_event_and_thread();
    main_action_loop();
    CloseHandle(action_event);
    CloseHandle(end_of_input_event);
}

const char* test1argv[] = { "hpctrl", "-a", "16", "-s11", "-s12", "-s21", "-s22" };
const char* test2argv[] = { "hpctrl", "-a", "16", "-i" };

int test1argc = 7;
int test2argc = 4;

//int runtest = 1;
int runtest = 2;

int main(int argc, char** argv)
{
    const char** av = (const char**)argv;

    if (runtest == 1)
    {
        av = test1argv;
        argc = test1argc;
    }
    else if (runtest == 2)
    {
        av = test2argv;
        argc = test2argc;
    }
    
    if (argc < 2)
        print_usage();
    else
    {
        parse_cmdline(argc, av);
        if (cmdline_i) interactive();
        else measure();
    }
}
