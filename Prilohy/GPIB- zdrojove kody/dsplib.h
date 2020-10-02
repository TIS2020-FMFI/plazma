//
// DSPLIB: Collection of various DSP routines from public-domain and open-source packages
//
// 7/10 jmiles@pop.net
// 

#ifndef DSPLIB_H
#define DSPLIB_H

#include "typedefs.h"

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef BUILDING_DSPLIB
   #define DSPDEF __declspec(dllexport)
#else
   #define DSPDEF __declspec(dllimport)
#endif

#ifndef PI_DEFINED
#define PI_DEFINED
  static const DOUBLE PI          = 3.1415926535897932;
  static const DOUBLE TWO_PI      = 6.2831853071795865;
  static const DOUBLE PI_OVER_TWO = 1.5707963263;
#endif

//
// FIR functions
// 

static const S32 FIR_DEFAULT_GRID_DENSITY   = 16;
static const S32 FIR_DEFAULT_MAX_ITERATIONS = 40;

struct FIR_FILTER
{
   DOUBLE *C;
   S32     len;
};

DSPDEF FIR_FILTER *  WINAPI DSP_FIR_create           (void);
DSPDEF void          WINAPI DSP_FIR_destroy          (FIR_FILTER *F);

DSPDEF S32           WINAPI DSP_FIR_PM_estimate      (DOUBLE      sample_rate_Hz,       
                                                      DOUBLE      transition_BW_Hz,
                                                      DOUBLE      stopband_atten_dB,            
                                                      DOUBLE      passband_ripple_dB);          
                                                   
DSPDEF S32           WINAPI DSP_FIR_Kaiser_estimate  (DOUBLE      sample_rate_Hz,   
                                                      DOUBLE      transition_BW_Hz,     
                                                      DOUBLE      stopband_atten_dB,
                                                      DOUBLE     *beta);                        

DSPDEF void          WINAPI DSP_FIR_response_dB    (FIR_FILTER *F,
                                                    DOUBLE     *output,
                                                    S32         n_points,
                                                    BOOL        normalize = FALSE);
                                                                       
DSPDEF void          WINAPI DSP_FIR_delay_design   (FIR_FILTER *F,
                                                    S32         order);

DSPDEF S32           WINAPI DSP_FIR_PM_LPF_design  (FIR_FILTER *F,
                                                    DOUBLE      sample_rate_Hz,             
                                                    DOUBLE      passband_HF_Hz,          
                                                    DOUBLE      stopband_LF_Hz,          
                                                    DOUBLE      stopband_atten_dB,         
                                                    DOUBLE      passband_ripple_dB,
                                                    S32         order          = -1,
                                                    S32         max_iterations = FIR_DEFAULT_MAX_ITERATIONS,
                                                    S32         grid_density   = FIR_DEFAULT_GRID_DENSITY);

DSPDEF S32           WINAPI DSP_FIR_PM_HPF_design  (FIR_FILTER *F,
                                                    DOUBLE      sample_rate_Hz,             
                                                    DOUBLE      stopband_HF_Hz,          
                                                    DOUBLE      passband_LF_Hz,          
                                                    DOUBLE      stopband_atten_dB,         
                                                    DOUBLE      passband_ripple_dB,
                                                    S32         order          = -1,
                                                    S32         max_iterations = FIR_DEFAULT_MAX_ITERATIONS,
                                                    S32         grid_density   = FIR_DEFAULT_GRID_DENSITY);

DSPDEF S32           WINAPI DSP_FIR_PM_BPF_design  (FIR_FILTER *F,
                                                    DOUBLE      sample_rate_Hz,             
                                                    DOUBLE      passband_LF_Hz,          
                                                    DOUBLE      passband_HF_Hz,          
                                                    DOUBLE      transition_BW_Hz,
                                                    DOUBLE      stopband_atten_dB,         
                                                    DOUBLE      passband_ripple_dB,
                                                    S32         order          = -1,
                                                    S32         max_iterations = FIR_DEFAULT_MAX_ITERATIONS,
                                                    S32         grid_density   = FIR_DEFAULT_GRID_DENSITY);

DSPDEF S32           WINAPI DSP_FIR_PM_BSF_design  (FIR_FILTER *F,
                                                    DOUBLE      sample_rate_Hz,             
                                                    DOUBLE      stopband_LF_Hz,          
                                                    DOUBLE      stopband_HF_Hz,          
                                                    DOUBLE      transition_BW_Hz,
                                                    DOUBLE      stopband_atten_dB,         
                                                    DOUBLE      passband_ripple_dB,
                                                    S32         order          = -1,
                                                    S32         max_iterations = FIR_DEFAULT_MAX_ITERATIONS,
                                                    S32         grid_density   = FIR_DEFAULT_GRID_DENSITY);

DSPDEF S32           WINAPI DSP_FIR_PM_Hilbert_transformer_design   
                                                   (FIR_FILTER *F,
                                                    S32         order            = 100,
                                                    DOUBLE      transition_BW_Hz = 0.05,
                                                    S32         max_iterations   = FIR_DEFAULT_MAX_ITERATIONS,
                                                    S32         grid_density     = FIR_DEFAULT_GRID_DENSITY);

DSPDEF S32           WINAPI DSP_FIR_PM_differentiator_design   
                                                   (FIR_FILTER *F,
                                                    DOUBLE      sample_rate_Hz,             
                                                    S32         order,
                                                    S32         max_iterations   = FIR_DEFAULT_MAX_ITERATIONS,
                                                    S32         grid_density     = FIR_DEFAULT_GRID_DENSITY);

DSPDEF void          WINAPI DSP_FIR_Kaiser_LPF_design 
                                                   (FIR_FILTER *F,
                                                    DOUBLE      sample_rate_Hz,             
                                                    DOUBLE      passband_HF_Hz,          
                                                    DOUBLE      stopband_LF_Hz,          
                                                    DOUBLE      stopband_atten_dB,         
                                                    S32         order = -1,
                                                    DOUBLE      beta  = 0.0);

DSPDEF void          WINAPI DSP_FIR_Kaiser_HPF_design 
                                                   (FIR_FILTER *F,
                                                    DOUBLE      sample_rate_Hz,             
                                                    DOUBLE      stopband_HF_Hz,          
                                                    DOUBLE      passband_LF_Hz,          
                                                    DOUBLE      stopband_atten_dB,         
                                                    S32         order = -1,
                                                    DOUBLE      beta  = 0.0);

DSPDEF void          WINAPI DSP_FIR_Kaiser_BPF_design 
                                                   (FIR_FILTER *F,
                                                    DOUBLE      sample_rate_Hz,             
                                                    DOUBLE      passband_LF_Hz,          
                                                    DOUBLE      passband_HF_Hz,          
                                                    DOUBLE      transition_BW_Hz,
                                                    DOUBLE      stopband_atten_dB,         
                                                    S32         order = -1,
                                                    DOUBLE      beta  = 0.0);

//
// FIR instance functions
//

struct FIR_INSTANCE
{
   FIR_FILTER *F;
   DOUBLE     *history;
   S32         len;
   S32         head;
};

DSPDEF FIR_INSTANCE * 
                     WINAPI DSP_FIR_instance_create  (FIR_FILTER    *F);

DSPDEF void          WINAPI DSP_FIR_instance_destroy (FIR_INSTANCE  *FI);
                                                                   
DSPDEF void          WINAPI DSP_FIR_instance_input   (FIR_INSTANCE  *FI,
                                                      DOUBLE         val);

DSPDEF DOUBLE        WINAPI DSP_FIR_instance_output  (FIR_INSTANCE  *FI);


//
// IIR functions
//

DSPDEF S32           WINAPI DSP_IIR_Butterworth_estimate
                                                    (DOUBLE  sample_rate_Hz,
                                                     DOUBLE  passband_Hz,
                                                     DOUBLE  stopband_Hz,
                                                     DOUBLE  max_passband_ripple_dB,
                                                     DOUBLE  min_stopband_atten_dB,
                                                     DOUBLE *actual_cutoff_3dB_Hz);

//
// Wrapper for FIDLIB filter-design library
//

struct _FID_FILTER
{
   DOUBLE rate_Hz;
   void *F_list;
};

typedef _FID_FILTER *FID_FILTER;

DSPDEF FID_FILTER    WINAPI DSP_FID_create           (DOUBLE      rate, 
                                                      C8        **pp, 
                                                      C8        **error_result = NULL);

DSPDEF FID_FILTER    WINAPI DSP_FID_create_from_array
                                                     (DOUBLE     *arr);   
                                                         
DSPDEF FID_FILTER    WINAPI DSP_FID_create_from_list (BOOL        free_input, 
                                                      ...);

DSPDEF void          WINAPI DSP_FID_destroy          (FID_FILTER  F);

DSPDEF DOUBLE        WINAPI DSP_FID_phase_response   (FID_FILTER  F, 
                                                      DOUBLE      freq, 
                                                      DOUBLE     *phase);

DSPDEF DOUBLE        WINAPI DSP_FID_mag_response     (FID_FILTER  F, 
                                                      DOUBLE      freq);

DSPDEF S32           WINAPI DSP_FID_sample_delay     (FID_FILTER  F);

struct _FID_INSTANCE
{
   void *run;
   void *buf;
};

typedef _FID_INSTANCE *FID_INSTANCE;

DSPDEF FID_INSTANCE  WINAPI DSP_FID_instance_create  (FID_FILTER     F);

DSPDEF void          WINAPI DSP_FID_instance_destroy (FID_INSTANCE   FI);

DSPDEF void          WINAPI DSP_FID_instance_flush   (FID_INSTANCE   FI);

DSPDEF DOUBLE        WINAPI DSP_FID_instance_process (FID_INSTANCE   FI,
                                                      DOUBLE         val);

//
// Wrapper for FFTSS or other FFTW-style library
//                                                    

#define DSP_FFT_FORWARD         -1
#define DSP_FFT_BACKWARD         1

#define DSP_FFT_MEASURE          0
#define DSP_FFT_DESTROY_INPUT    (1<<0)
#define DSP_FFT_UNALIGNED        (1<<1)
#define DSP_FFT_CONSERVE_MEMORY  (1<<2)
#define DSP_FFT_EXHAUSTIVE       (1<<3)
#define DSP_FFT_PRESERVE_INPUT   (1<<4)
#define DSP_FFT_PATIENT          (1<<5)
#define DSP_FFT_ESTIMATE         (1<<6)

#define DSP_FFT_NO_SIMD          (1<<17)

#define DSP_FFT_VERBOSE          (1<<20)
#define DSP_FFT_INOUT            (1<<21)

typedef void *DSP_FFT_PLAN;

DSPDEF void * WINAPI       DSP_malloc              (S32             bytes);      // para-aligned malloc/free functions 
DSPDEF void   WINAPI       DSP_free                (void           *mem);

DSPDEF DSP_FFT_PLAN WINAPI DSP_FFT_plan_DFT_1D     (S32             len, 
                                                    COMPLEX_DOUBLE *in, 
                                                    COMPLEX_DOUBLE *out, 
                                                    S32             sign, 
                                                    S32             flags);

DSPDEF DSP_FFT_PLAN WINAPI DSP_FFT_plan_DFT_2D     (S32             len_x, 
                                                    S32             len_y, 
                                                    S32             stride_y, 
                                                     COMPLEX_DOUBLE *in, 
                                                    COMPLEX_DOUBLE *out, 
                                                    S32             sign, 
                                                    S32             flags);

DSPDEF DSP_FFT_PLAN WINAPI DSP_FFT_plan_DFT_3D     (S32             len_x, 
                                                    S32             len_y, 
                                                    S32             len_z, 
                                                    S32             stride_y, 
                                                    S32             stride_z, 
                                                     COMPLEX_DOUBLE *in, 
                                                    COMPLEX_DOUBLE *out, 
                                                    S32             sign, 
                                                    S32             flags);

DSPDEF void         WINAPI DSP_FFT_destroy_plan    (DSP_FFT_PLAN    plan);

DSPDEF void         WINAPI DSP_FFT_set             (DSP_FFT_PLAN    plan, 
                                                    COMPLEX_DOUBLE *in, 
                                                    COMPLEX_DOUBLE *out);

DSPDEF void         WINAPI DSP_FFT_execute         (DSP_FFT_PLAN    plan);

DSPDEF void         WINAPI DSP_FFT_execute_dft     (DSP_FFT_PLAN    plan, 
                                                    COMPLEX_DOUBLE *in, 
                                                    COMPLEX_DOUBLE *out);

DSPDEF DOUBLE       WINAPI DSP_FFT_get_wtime       (void);

DSPDEF S32          WINAPI DSP_FFT_init_threads    (void);

DSPDEF void         WINAPI DSP_FFT_cleanup_threads (void);

DSPDEF void         WINAPI DSP_FFT_plan_with_nthreads
                                                   (S32             nthreads);

DSPDEF S32          WINAPI DSP_FFT_test_DFT_1D     (S32             len, 
                                                    COMPLEX_DOUBLE *in, 
                                                    COMPLEX_DOUBLE *out,
                                                    S32             sign, 
                                                    S32             flags);

DSPDEF void         WINAPI DSP_FFT_shift_1D        (S32             len,
                                                    COMPLEX_DOUBLE *array);

//
// HLAPI for FFTs
//

enum FFTWNDTYPE
{
   WND_HFT95_FLATTOP,
   WND_HFT248D_FLATTOP,
   WND_HFT70_FLATTOP,
   WND_HAMMING,
   WND_HANN,
   WND_BH,
};

DSPDEF void         WINAPI DSP_FFT_make_window     (FFTWNDTYPE type,
                                                    S32        len,
                                                    DOUBLE    *dest,
                                                    DOUBLE    *NEQBW_bins,
                                                    DOUBLE    *coherent_gain_dB);

//
// Adaptive comb filter typically used for AF hum reduction
//
// Derived from public-domain humfilt.c by Paul Nicholson 
// (http://abelian.org/humfilt/)
//

class HUM_FILTER
{
   BOOL    active;
   DOUBLE  sample_rate_Hz;
   DOUBLE *comb_buffer;
   S32     max_comb_buffer_bytes;
   S32     n_comb_stages;

   S32 comb_len_samples;
   U32 comb_mask;
   S32 comb_lp;

   DOUBLE comb_delay_samples;
   DOUBLE max_delay_samples;
   DOUBLE min_delay_samples;
   DOUBLE comb_delay_adj;

   DOUBLE track_low;
   DOUBLE track_mid;
   DOUBLE track_high;
   DOUBLE tracking_width;
   DOUBLE tracking_cycle_secs;

   DOUBLE slew_rate;
   DOUBLE max_track_samples;

   DOUBLE p_low;
   DOUBLE p_mid;
   DOUBLE p_high;
   DOUBLE sum_square;
   DOUBLE peak;
   S32    count;
   DOUBLE comb_smooth;

   virtual void init()
      {
      active = FALSE;
      sample_rate_Hz = 0.0;
      comb_buffer = NULL;
      max_comb_buffer_bytes = 0;
      n_comb_stages = 0;
      comb_len_samples = 0;
      comb_mask = 0;
      comb_lp = 0;
      comb_delay_samples = 0.0;
      max_delay_samples = 0.0;
      min_delay_samples = 0.0;
      comb_delay_adj = 0.0;
      track_low = 0.0;
      track_mid = 0.0;
      track_high = 0.0;
      tracking_width = 0.0;
      tracking_cycle_secs = 0.0;
      slew_rate = 0.0;
      max_track_samples = 0.0;
      p_low = 0.0;
      p_high = 0.0;
      p_mid = 0.0;
      sum_square = 0.0;
      peak = 0.0;
      count = 0;
      comb_smooth = 0.0;
      }

   virtual void shutdown()
      {
      active = FALSE;

      if (comb_buffer != NULL)
         {
         DSP_free(comb_buffer);
         comb_buffer = NULL;
         }
      }


   //
   // Comb summing function.  Sum over the given number of delay stages, each
   // of length delay_steps samples.  Use linear interpolation between samples.
   //
   // At high sample rates, adequate results might be obtained without the linear
   // interpolation, which can save quite a bit of CPU time
   //

   inline DOUBLE comb_sum(DOUBLE delay_steps)
      {
      DOUBLE f = 0;
      DOUBLE g = delay_steps;

      for (S32 ne=1; ne <= n_comb_stages; ne++, g += delay_steps)
         {
         S32 gf = (S32) g;

         S32 p = comb_lp - gf; 
         DOUBLE f1 = comb_buffer[p-- & comb_mask]; 
         DOUBLE f2 = comb_buffer[p   & comb_mask]; 
         f += f1 + ((f2-f1) * (g-gf));
         }

      return f / n_comb_stages;
      }

   inline void smooth_accumulate(DOUBLE *a, DOUBLE val)
      {
      *a = (*a * comb_smooth) + (val * (1.0-comb_smooth));
      }

   inline void leaky_integrate(DOUBLE *a, DOUBLE val)
      {
      *a = ((*a + val) * 0.999);
      }

   //
   // Revise the filter delay.  Compare the mean square amplitudes of the high
   // and low tracking signals and adjust the delay to bring them nearer to 
   // equality
   //

   virtual void adjust_delay(void)
      {
      DOUBLE balance = track_high / track_low;   // Equals 1.0 when in tune
      DOUBLE mean    = track_high * track_low / track_mid / track_mid;
   
      if (balance > 1) 
         {
         comb_delay_adj = 1.0;      
         }
      else
         {
         comb_delay_adj = -1.0;
         balance = 1.0 / balance;
         }
   
      DOUBLE f = pow(balance, 10.0);  
      
      if (f > 10.0) 
         {
         f = 10.0;
         }

      comb_delay_adj *= (f * slew_rate / n_comb_stages);      
   
      if (mean < 0.5) 
         {
         tracking_width /= 2;
         }
      else
         {
         if (mean > 0.8) 
            {
            tracking_width *= 2;
            }
         }
   
      if (tracking_width < 0.1) 
         {
         tracking_width = 0.1;
         }
      else
         {
         if (tracking_width > max_track_samples)
            {
            tracking_width = max_track_samples;
            }
         }
   
      //
      // Revise the delay, and constrain within the end stops
      //
   
      comb_delay_samples += comb_delay_adj * sample_rate_Hz / 1000.0;  

      if (comb_delay_samples > max_delay_samples) comb_delay_samples = max_delay_samples;
      if (comb_delay_samples < min_delay_samples) comb_delay_samples = min_delay_samples;
      }

public:

   HUM_FILTER(S32 _max_comb_buffer_bytes)
      {
      init();

      max_comb_buffer_bytes = _max_comb_buffer_bytes;

      comb_buffer = (DOUBLE *) DSP_malloc(max_comb_buffer_bytes);

      if (comb_buffer == NULL)
         {
         return;
         }
         
      active = TRUE;
      }

   virtual ~HUM_FILTER() 
      {
      shutdown();
      }

   virtual BOOL status()
      {
      return active;
      }

   //
   // Initialize the comb filter, ensuring buffer is long enough for the 
   // max possible delay plus an extra allowance for the high period tracking.  
   // We then round up to the next power of two for efficiency
   //

   virtual BOOL set_params(DOUBLE _sample_rate_Hz,
                           DOUBLE _target_freq_Hz,    
                           S32    _n_comb_stages       = 4,       // Default=4
                           DOUBLE _tracking_cycle_secs = 1.0,     // How often to revise delay, default=1.0
                           S32    _max_track_samples   = 10,      // Max delay difference between low and high measurement periods and current delay period, default=10
                           DOUBLE _track_range_percent = 0.5,     // End-stop value; maximum +/- % deviation from nominal target frequency, default=0.5
                           DOUBLE _slew_rate           = 0.01,    // 0.01=stable locking under most conditions; higher value=less lag, lower=less probability of lock loss during noise bursts 
                           DOUBLE _comb_smooth         = 0.9999)  // Default=0.9999 smoothing for period tracker
      {
      DOUBLE nominal_delay_secs = 1.0 / _target_freq_Hz;
     
      n_comb_stages        = _n_comb_stages;
      tracking_cycle_secs  = _tracking_cycle_secs;
      slew_rate            = _slew_rate;
      max_track_samples    = _max_track_samples;
      sample_rate_Hz       = _sample_rate_Hz;
      comb_smooth          = _comb_smooth;

      comb_delay_samples = nominal_delay_secs * sample_rate_Hz;
      max_delay_samples  = comb_delay_samples * (1.0 + (_track_range_percent / 100.0));
      min_delay_samples  = comb_delay_samples * (1.0 - (_track_range_percent / 100.0));
     
      comb_len_samples = (S32) (n_comb_stages * (1 + max_track_samples + max_delay_samples));
     
      S32 i = 1;
     
      while (i < comb_len_samples)
         {
         i <<= 1;
         }
     
      comb_len_samples  = i;
      comb_mask         = comb_len_samples-1;
     
      if (comb_len_samples * (S32) sizeof(comb_buffer[0]) > max_comb_buffer_bytes)
         {
         return FALSE;
         }
     
      return TRUE;
      }

   //
   // Send sample through filter
   //

   virtual void print_info( DOUBLE peak, DOUBLE sum_square, int count)
      {
      printf("peak=%.3lf rms=%0.3lf del=%6.3lfms %+.4lf mS low=%.2lf high=%.2lf wid=%.2lf\n",
         peak,
         sqrt( sum_square/count), 
         1000.0 * comb_delay_samples/sample_rate_Hz,
         comb_delay_adj,
         track_low/track_mid, track_high/track_mid, tracking_width);
      }

   virtual DOUBLE comb_filter(DOUBLE s)
      {
      //
      // Insert the incoming sample into the buffer, and keep track of the mean
      // square and peak values 
      // 
    
      comb_buffer[comb_lp] = s;        
   
      sum_square += s*s;                

      if (s > peak) 
         {
         peak = s;
         }
      else
         {
         if (s < -peak) 
            {
            peak = -s;
            }
         }
   
      //
      // Run the comb summing function three times, for the low, high, and middle
      // periods.  Integrate each sum over time to give a 6db/octave roll-off. 
      // Then smooth the squared amplitudes of the integrated sums
      // 
   
      DOUBLE t = comb_sum(comb_delay_samples - tracking_width);
      leaky_integrate(&p_low, t);
      smooth_accumulate(&track_low, p_low * p_low);
    
      t = comb_sum(comb_delay_samples + tracking_width);
      leaky_integrate(&p_high, t);
      smooth_accumulate(&track_high, p_high * p_high);
   
      t = comb_sum(comb_delay_samples);
      leaky_integrate(&p_mid, t);
      smooth_accumulate(&track_mid, p_mid * p_mid);
   
      //
      // Revise the delay every tracking_cycle secs
      //
   
      if (++count > (sample_rate_Hz * tracking_cycle_secs))
         {
         if (track_mid != 0)      // Must have a non-zero input signal
            {
            adjust_delay();
//          print_info( peak, sum_square, count);
            }
   
         sum_square = peak = 0.0;
         count = 0;
         }
   
      comb_lp = (comb_lp + 1) & comb_mask;

      return s - t;
      }
};

#ifdef __cplusplus
}
#endif

#endif
