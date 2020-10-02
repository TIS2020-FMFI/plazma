#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>

#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <time.h>

#include "typedefs.h"
#include "recorder.h"

RECORDER R;
 
void main(int argc, char **argv)
{
   if (argc < 2) 
      {
      printf("Usage: ssmdump filename.ssm\n");
      exit(1);
      }

   S32 file_version = 0;
   DOUBLE freq_start = 0.0;
   DOUBLE freq_end = 0.0;
   SINGLE min_dBm = 0.0F;
   SINGLE max_dBm = 0.0F;
   S32    n_amplitude_levels = 0;
   S32    dB_per_division = 0;
   SINGLE top_cursor_dBm = 0.0F;
   SINGLE bottom_cursor_dBm = 0.0F;
   VFX_RGB palette[256] = { 0 };

   S32 width = R.open_readable_file(argv[1],
                                  &file_version,
                                  &freq_start,
                                  &freq_end,              
                                  &min_dBm,               
                                  &max_dBm,               
                                  &n_amplitude_levels,    
                                  &dB_per_division,       
                                  &top_cursor_dBm,        
                                  &bottom_cursor_dBm,     
                                  palette,               
                                  256);

   printf("\n");
   printf("Filename:           %s\n", argv[1]);
   printf("File version:       %d\n", file_version);
   printf("Row width:          %d bins\n", width);
   printf("Hz/bin:             %.1lf\n", (freq_end - freq_start) / width);
   printf("Start Hz:           %lG\n", freq_start);
   printf("End Hz:             %lG\n", freq_end);
   printf("Min dBm:            %f\n", min_dBm);
   printf("Max dBm:            %f\n", max_dBm);
   printf("# amplitude levels: %d\n", n_amplitude_levels);
   printf("dB/div:             %d\n", dB_per_division);
   printf("Top cursor dBm:     %f\n", top_cursor_dBm);
   printf("Bottom cursor dBm:  %f\n", bottom_cursor_dBm);

   S32 n_records = R.n_readable_records();

   printf("Records: %d\n", n_records);

   SINGLE dBm_bins[65536] = { 0 };
   C8     caption[128] = { 0 };
   DOUBLE latitude      = 0.0;
   DOUBLE longitude     = 0.0; 
   DOUBLE altitude_m    = 0.0; 
   DOUBLE start_Hz      = 0.0;
   DOUBLE stop_Hz       = 0.0;

   printf("First five records:\n");

   for (S32 i=0; i < min(5, n_records); i++)
      {
      C8 caption[128] = { 0 };

      time_t acquisition_time = R.read_record(dBm_bins,
                                              i,
                                             &latitude,
                                             &longitude,
                                             &altitude_m,
                                             &start_Hz,
                                             &stop_Hz,
                                              caption,
                                              sizeof(caption));

      printf("\nRecord %d/%d of %s", i+1, n_records, ctime(&acquisition_time));
      printf("                 Lat: %lf\n", latitude);
      printf("                 Lon: %lf\n", longitude);
      printf("                 Alt: %lf\n", altitude_m);
      printf("               Start: %lf Hz\n", start_Hz == INVALID_FREQ ? freq_start : start_Hz);
      printf("                Stop: %lf Hz\n", stop_Hz == INVALID_FREQ ? freq_end : stop_Hz);
      printf("     First five bins: ");

      for (S32 i=0; i < min(5, width); i++)
         {
         printf("%.1lf ", dBm_bins[i]);
         }

      printf("\n");
      }




}
