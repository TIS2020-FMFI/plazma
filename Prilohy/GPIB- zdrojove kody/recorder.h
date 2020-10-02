//
// Simple file-based record/playback class for streaming input data
//

#ifndef RECORDER_H
#define RECORDER_H

#ifndef VFX_RGB_DEFINED
#define VFX_RGB_DEFINED

typedef struct
{
   U8 r;
   U8 g;
   U8 b;
}
VFX_RGB;

#endif

#ifndef GNSS_H
static const DOUBLE INVALID_LATLONGALT = -1E6;
#endif

static const DOUBLE INVALID_FREQ = -1E30;

//
// Update an existing file without opening it as a recording
//

S32 update_physical_file (C8      *pathname,
                          SINGLE   top_cursor_dBm    = -10000.0F,
                          SINGLE   bottom_cursor_dBm = -10000.0F,
                          S32      n_palette_entries = 0,
                          VFX_RGB *palette           = NULL);

class RECORDER
{
public:
   int      read_file;           // File handles for reading and/or writing
   int      write_file;

   S32      file_version;        // Version header word

   S32      read_input_width;    // Record width for file being read
   S32      write_input_width;   // Record width for file being written

   S32      read_header_size;    // Header size for readable file (which can be .SSM or .RRT)
   S32      record_extra_bytes;  // 128 bytes for file versions >= 2, else 0

   SINGLE  *write_data_array;          

   //
   // Constructor/destructor
   //

   RECORDER();

  ~RECORDER();

   //
   // Open file for writing
   //
   // input_width = # of points per input frame to record
   //
   // Returns 0 on failure, else nonzero
   //

   S32 open_writable_file(C8      *filename,
                          S32      input_width,
                          DOUBLE   freq_start,
                          DOUBLE   freq_end,
                          SINGLE   min_dBm,
                          SINGLE   max_dBm,
                          S32      n_amplitude_levels,
                          S32      dB_per_division,
                          SINGLE   initial_top_cursor_dBm,
                          SINGLE   initial_bottom_cursor_dBm,
                          VFX_RGB *palette,
                          S32      n_palette_entries);

   //
   // Open file for reading
   //
   // Returns input width (saved as first 4 bytes in file), or 0 on error
   //

   S32 open_readable_file(C8      *filename, 
                          S32     *file_version,
                          DOUBLE  *freq_start, 
                          DOUBLE  *freq_end,
                          SINGLE  *min_dBm,
                          SINGLE  *max_dBm,
                          S32     *n_amplitude_levels,
                          S32     *dB_per_division,
                          SINGLE  *top_cursor_dBm,
                          SINGLE  *bottom_cursor_dBm,
                          VFX_RGB *palette,
                          S32      max_palette_entries);

   //
   // Close file being written
   //

   void close_writable_file(SINGLE   top_cursor_dBm    = -10000.0F,
                            SINGLE   bottom_cursor_dBm = -10000.0F,
                            S32      n_palette_entries = 0,
                            VFX_RGB *palette           = NULL);

   //
   // Close file being read
   //

   void close_readable_file(void);

   //
   // Get # of bytes per record stored in file, given the file's record width
   //

   S32 bytes_per_record(S32 record_width);

   //
   // Get # of frames recorded in file
   //
   // Playback position passed to read_record() must range from 0 to 
   // this value-1
   //

   S32 n_readable_records(void);

   //
   // Store values into recorded entry
   //
   // File must be open for writing
   //

   void set_single_point  (SINGLE val, 
                           S32    X);

   void set_arrayed_points(SINGLE *val, 
                           S32     first_X    = 0, 
                           S32     last_X     = -1, 
                           S32    *explicit_X = NULL);

   //
   // Save record to disk at end of file
   //
   // File must be open for writing
   // 
   // Returns 0 on error, else nonzero
   // 

   S32 write_record   (time_t time,
                       DOUBLE latitude    = INVALID_LATLONGALT,
                       DOUBLE longitude   = INVALID_LATLONGALT,
                       DOUBLE altitude_m  = INVALID_LATLONGALT,
                       DOUBLE freq_start  = INVALID_FREQ, 
                       DOUBLE freq_end    = INVALID_FREQ,   
                       C8    *caption     = NULL);

   //
   // Retrieve record at specified playback position
   //
   // Returns timestamp
   //
   // File must be open for reading
   //

   time_t read_record   (SINGLE *dest,
                         S32     read_position,
                         DOUBLE *latitude,
                         DOUBLE *longitude,
                         DOUBLE *altitude_m,
                         DOUBLE *start_Hz,
                         DOUBLE *stop_Hz,
                         C8     *caption_dest,
                         S32     caption_dest_size);

};

#endif
