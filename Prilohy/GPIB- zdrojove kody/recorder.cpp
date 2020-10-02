//
// RECORDER.CPP
//

#include <stdio.h>
#include <io.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <io.h>
#include <fcntl.h>
#include <assert.h>
#include <limits.h>
#include <float.h>
#include <time.h>
#include <assert.h>
#include <fcntl.h>
#include <io.h>
#include <sys\stat.h>

#include "typedefs.h"

//#include "w32sal.h"
//#include "winvfx.h"

//#include "gnss.h"
#include "recorder.h"

#define SSM_HEADER_SIZE           65536
#define HEADER_BYTES_USED         48
#define OFFSET_TO_Y_CURSOR_RECORD 40
#define OFFSET_TO_PALETTE         1024
#define CURRENT_FILE_VERSION      2

#define RRT_HEADER_SIZE           36

//
// File version 2 row structure:
//
//    32-bit time_t
//    SINGLE amplitude[record width]
//    BYTE [V2_EXTRA_BYTES]
//       BYTE block_tag
//       BYTE block_nbytes (Not including 2-byte block descriptor)
//

#define V2_EXTRA_BYTES            128

#define TAG_EOD                   0    // End-of-data tag type, does not need to be followed by length
#define TAG_LATLONGALT            1    // Lat/long/alt tag type, len is normally 24
#define TAG_CAPTION               2    // Caption tag type, len is # of bytes in ASCIIZ string
#define TAG_FREQ                  3    // Start/stop freq tag type, len is normally 16

//############################################################################
//
// update_file()
// 
//############################################################################

static S32 update_file (int      write_file, //)
                        SINGLE   top_cursor_dBm,
                        SINGLE   bottom_cursor_dBm,
                        S32      n_palette_entries,
                        VFX_RGB *palette)
{
   if (top_cursor_dBm != -10000.0F)
      {
      if (_lseek(write_file,
                 OFFSET_TO_Y_CURSOR_RECORD,
                 SEEK_SET) != OFFSET_TO_Y_CURSOR_RECORD) return FALSE;

      if (_write(write_file,
                &top_cursor_dBm,
                 sizeof(SINGLE)) != sizeof(SINGLE)) return FALSE;

      if (_write(write_file,
                &bottom_cursor_dBm,
                 sizeof(SINGLE)) != sizeof(SINGLE)) return FALSE;
      }

   if (n_palette_entries != 0)
      {
      if (_lseek(write_file,
                 OFFSET_TO_PALETTE,
                 SEEK_SET) != OFFSET_TO_PALETTE) return FALSE;

      if (_write(write_file,
                &n_palette_entries,
                 sizeof(S32)) != sizeof(S32)) return FALSE;

      if (_write(write_file,
                 palette,
                 sizeof(VFX_RGB) * n_palette_entries) != (S32) (sizeof(VFX_RGB) * n_palette_entries)) return FALSE;
      }

   return TRUE;
}

//############################################################################
//
// update_physical_file()
//
//############################################################################

S32 update_physical_file (C8      *filename, //)
                          SINGLE   top_cursor_dBm,
                          SINGLE   bottom_cursor_dBm,
                          S32      n_palette_entries,
                          VFX_RGB *palette)
{
   int write_file = _open(filename, O_RDWR | O_BINARY, S_IREAD | S_IWRITE);

   if (write_file == -1)
      {
      return FALSE;
      }

   S32 result = update_file(write_file,
                            top_cursor_dBm,    
                            bottom_cursor_dBm, 
                            n_palette_entries, 
                            palette);
   _close(write_file);

   return result;
}

//############################################################################
//
// RECORDER()
//
//############################################################################

RECORDER::RECORDER()
{
   assert(sizeof(time_t) == 4);        // Must be compiled with /D_USE_32BIT_TIME_T in VS8

   read_file = -1;
   write_file = -1;
   read_header_size = 0;

   file_version = 0;
   read_input_width = 0;
   record_extra_bytes = V2_EXTRA_BYTES;
   write_input_width = 0;

   write_data_array = NULL;
}

//############################################################################
//
// ~RECORDER()
//
//############################################################################

RECORDER::~RECORDER()
{
   close_readable_file();
   close_writable_file();
}

//############################################################################
//
// open_writable_file()
//
//############################################################################

S32 RECORDER::open_writable_file(C8      *filename, //)
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
                                 S32      n_palette_entries)
{
   if (read_file != -1)
      {
      //
      // Can't open for reading and writing at the same time
      //

      return 0;
      }

   if (write_file != -1)
      {
      //
      // File already open for writing
      // 

      return 0;
      }

   _unlink(filename);

   write_file = _open(filename, O_RDWR | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);

   if (write_file == -1)
      {
      return 0;
      }

   //
   // First 4 bytes = version ID
   //

   file_version = CURRENT_FILE_VERSION;

   if (_write(write_file,
             &file_version,
              sizeof(S32)) != sizeof(S32))
      {
      close_writable_file();
      return 0;
      }

   //
   // Next 4 bytes = input width
   //
   // This allows us to set up the required input width when reading the
   // file back
   //

   if (_write(write_file,
             &input_width,
              sizeof(S32)) != sizeof(S32))
      {
      close_writable_file();
      return 0;
      }

   write_input_width = input_width;

   //
   // Next 40 bytes = source properties
   //

   if (_write(write_file,
             &freq_start,
              sizeof(DOUBLE)) != sizeof(DOUBLE))
      {
      close_writable_file();
      return 0;
      }

   if (_write(write_file,
             &freq_end,
              sizeof(DOUBLE)) != sizeof(DOUBLE))
      {
      close_writable_file();
      return 0;
      }

   if (_write(write_file,
             &min_dBm,
              sizeof(SINGLE)) != sizeof(SINGLE))
      {
      close_writable_file();
      return 0;
      }

   if (_write(write_file,
             &max_dBm,
              sizeof(SINGLE)) != sizeof(SINGLE))
      {
      close_writable_file();
      return 0;
      }

   if (_write(write_file,
             &n_amplitude_levels,
              sizeof(S32)) != sizeof(S32))
      {
      close_writable_file();
      return 0;
      }

   if (_write(write_file,
             &dB_per_division,
              sizeof(S32)) != sizeof(S32))
      {
      close_writable_file();
      return 0;
      }

   S32 loc = _tell(write_file);
   assert(loc == OFFSET_TO_Y_CURSOR_RECORD);

   if (_write(write_file,
             &initial_top_cursor_dBm,
              sizeof(SINGLE)) != sizeof(SINGLE))
      {
      close_writable_file();
      return 0;
      }

   if (_write(write_file,
             &initial_bottom_cursor_dBm,
              sizeof(SINGLE)) != sizeof(SINGLE))
      {
      close_writable_file();
      return 0;
      }

   //
   // Write zeroes until beginning of palette area
   //

   static C8 reserved1[OFFSET_TO_PALETTE - HEADER_BYTES_USED];
   memset(reserved1, 0, sizeof(reserved1));

   loc = _tell(write_file);
   assert(loc == HEADER_BYTES_USED);

   if (_write(write_file,
              reserved1,
              OFFSET_TO_PALETTE - HEADER_BYTES_USED) != (OFFSET_TO_PALETTE - HEADER_BYTES_USED))
      {
      close_writable_file();
      return 0;
      }

   //
   // Write palette entry count, followed by VFX_RGB array
   //

   if (_write(write_file,
             &n_palette_entries,
              sizeof(S32)) != sizeof(S32))
      {
      close_writable_file();
      return 0;
      }

   if (_write(write_file,
              palette,
              sizeof(VFX_RGB) * n_palette_entries) != (S32) (sizeof(VFX_RGB) * n_palette_entries))
      {
      close_writable_file();
      return 0;
      }

   //
   // Rest of header is reserved space
   //

   static C8 reserved2[SSM_HEADER_SIZE];
   memset(reserved2, 0, sizeof(reserved2));

   loc = _tell(write_file);

   if (_write(write_file,
              reserved2,
              SSM_HEADER_SIZE - loc) != (SSM_HEADER_SIZE - loc))
      {
      close_writable_file();
      return 0;
      }

   //
   // Allocate and clear internal array
   //

   write_data_array = (SINGLE *) malloc(write_input_width * sizeof(SINGLE));

   if (write_data_array == NULL)
      {
      close_writable_file();
      return 0;
      }

   memset(write_data_array,
          0,
          write_input_width * sizeof(SINGLE));

   return file_version;
}

//############################################################################
//
// open_readable_file()
//
//############################################################################

S32 RECORDER::open_readable_file(C8      *filename, //)
                                 S32     *version,
                                 DOUBLE  *freq_start,
                                 DOUBLE  *freq_stop,
                                 SINGLE  *min_dBm,
                                 SINGLE  *max_dBm,
                                 S32     *n_amplitude_levels,
                                 S32     *dB_per_division,
                                 SINGLE  *top_cursor_dBm,
                                 SINGLE  *bottom_cursor_dBm,
                                 VFX_RGB *palette,
                                 S32      max_palette_entries)
{
   if (write_file != -1)
      {
      //
      // Can't open for reading and writing at the same time
      //

      return 0;
      }

   if (read_file != -1)
      {
      close_readable_file();
      }

   read_file = _open(filename,O_RDONLY | O_BINARY);

   if (read_file == -1)
      {
      return 0;
      }

   //
   // Get 4-byte file version
   //

   if (_read(read_file,
            &file_version,
             sizeof(S32)) != sizeof(S32))
      {
      close_readable_file();
      return 0;
      }

   //
   // If the "version" field is a valid n_points value, this is a legacy .RRT file
   // (version 0)
   //
   // We can still open these files, but we can't save them
   //

   if ((file_version == 4)   ||
       (file_version == 16)  || 
       (file_version == 32)  || 
       (file_version == 64)  || 
       (file_version == 128) || 
       (file_version == 320) || 
       (file_version == 640))
      {
      read_input_width = file_version;
      record_extra_bytes = 0;

      file_version = 0;
      *version = file_version;

      if (_read(read_file,
                freq_start,
                sizeof(DOUBLE)) != sizeof(DOUBLE))
         {
         close_readable_file();
         return 0;
         }

      if (_read(read_file,
                freq_stop,
                sizeof(DOUBLE)) != sizeof(DOUBLE))
         {
         close_readable_file();
         return 0;
         }

      if (_read(read_file,
                min_dBm,
                sizeof(SINGLE)) != sizeof(SINGLE))
         {
         close_readable_file();
         return 0;
         }

      if (_read(read_file,
                max_dBm,
                sizeof(SINGLE)) != sizeof(SINGLE))
         {
         close_readable_file();
         return 0;
         }

      if (_read(read_file,
                top_cursor_dBm,
                sizeof(SINGLE)) != sizeof(SINGLE))
         {
         close_readable_file();
         return 0;
         }

      if (_read(read_file,
                bottom_cursor_dBm,
                sizeof(SINGLE)) != sizeof(SINGLE))
         {
         close_readable_file();
         return 0;
         }

      *version         = 0;                                    // .RRT files are version 0
      *dB_per_division = 10;                                   // Always 10 dB/div for .RRT files

      S32 dBm_range = (S32) ((*max_dBm - *min_dBm) + 0.5F);    // Always either 80 dB or 100 dB for .RRT files

      *n_amplitude_levels = dBm_range / 10;
 
      read_header_size = RRT_HEADER_SIZE;

      return read_input_width;
      }

   //
   // This is an .SSM file
   //
   // Store file version and get 4-byte record width
   //

   *version = file_version;

   if (_read(read_file,
            &read_input_width,
             sizeof(S32)) != sizeof(S32))
      {
      close_readable_file();
      return 0;
      }

   //
   // Version 2 files have 128 extra bytes at the end of each record for GPS data, etc.
   //

   if (file_version >= 2)
      record_extra_bytes = V2_EXTRA_BYTES;
   else
      record_extra_bytes = 0;

   //
   // Next 40 bytes = source descriptor
   //

   if (_read(read_file,
             freq_start,
             sizeof(DOUBLE)) != sizeof(DOUBLE))
      {
      close_readable_file();
      return 0;
      }

   if (_read(read_file,
             freq_stop,
             sizeof(DOUBLE)) != sizeof(DOUBLE))
      {
      close_readable_file();
      return 0;
      }

   if (_read(read_file,
             min_dBm,
             sizeof(SINGLE)) != sizeof(SINGLE))
      {
      close_readable_file();
      return 0;
      }

   if (_read(read_file,
             max_dBm,
             sizeof(SINGLE)) != sizeof(SINGLE))
      {
      close_readable_file();
      return 0;
      }

   if (_read(read_file,
             n_amplitude_levels,
             sizeof(S32)) != sizeof(S32))
      {
      close_readable_file();
      return 0;
      }

   if (_read(read_file,
             dB_per_division,
             sizeof(S32)) != sizeof(S32))
      {
      close_readable_file();
      return 0;
      }

   if (_read(read_file,
             top_cursor_dBm,
             sizeof(SINGLE)) != sizeof(SINGLE))
      {
      close_readable_file();
      return 0;
      }

   if (_read(read_file,
             bottom_cursor_dBm,
             sizeof(SINGLE)) != sizeof(SINGLE))
      {
      close_readable_file();
      return 0;
      }

   //
   // Generate default grayscale palette, then seek to palette offset
   // and get # of palette entries actually present in this file
   //

   if (max_palette_entries > 0)
      {
      S32 dc_di = 255 / max_palette_entries;
      S32 c     = 0;

      for (S32 i=0; i < max_palette_entries; i++)
         {
         palette[i].r = (U8) c;
         palette[i].g = (U8) c;
         palette[i].b = (U8) c;

         c += dc_di;
         }

      _lseek(read_file,
             OFFSET_TO_PALETTE,
             SEEK_SET);
      
      S32 actual_palette_entry_cnt = 0;

      if (_read(read_file,
               &actual_palette_entry_cnt,
                sizeof(S32)) != sizeof(S32))
         {
         close_readable_file();
         return 0;
         }

      //
      // Read palette entries
      //

      S32 n_entries = min(actual_palette_entry_cnt, 
                          max_palette_entries);

      if (_read(read_file,
                palette,
                sizeof(VFX_RGB) * n_entries) != (S32) (sizeof(VFX_RGB) * n_entries))
         {
         close_readable_file();
         return 0;
         }
      }

   //
   // Rest of header is reserved space
   //

   read_header_size = SSM_HEADER_SIZE;

   return read_input_width;
}

//############################################################################
//
// close_writable_file()
//
//############################################################################

void RECORDER::close_writable_file(SINGLE   top_cursor_dBm, //)
                                   SINGLE   bottom_cursor_dBm,
                                   S32      n_palette_entries,
                                   VFX_RGB *palette)
{
   if (write_file != -1)
      {
      //
      // Cursors and palette can be edited manually while recording a file.  
      // Before we close the file, we'll log the final cursor and palette
      // values so they can be restored properly when reopening the file
      //
      // These values don't really need to be written when the file is opened...
      //

      update_file(write_file,
                  top_cursor_dBm,
                  bottom_cursor_dBm,
                  n_palette_entries,
                  palette);

      _close(write_file);

      write_file = -1;
      }

   if (write_data_array != NULL)
      {
      free(write_data_array);
      write_data_array = NULL;
      }
}

//############################################################################
//
// close_readable_file()
//
//############################################################################

void RECORDER::close_readable_file(void)
{
   if (read_file != -1)
      {
      _close(read_file);
      read_file = -1;
      }
}

//############################################################################
//
// write_record()
//
//############################################################################

S32 RECORDER::write_record       (time_t timestamp,
                                  DOUBLE latitude,    // Valid if not INVALID_LATLONGALT
                                  DOUBLE longitude,   // Valid if not INVALID_LATLONGALT 
                                  DOUBLE altitude_m,  // Valid if not INVALID_LATLONGALT 
                                  DOUBLE start_Hz,    // Valid if not INVALID_FREQ
                                  DOUBLE stop_Hz,     // Valid if not INVALID_FREQ
                                  C8    *caption)     // NULL if no caption for this record
{
   if (write_file == -1)
      {
      return 0;
      }

   //
   // Write timestamp header at beginning of record
   //

   if (_write(write_file,
             &timestamp,
              sizeof(time_t)) == -1)
      {       
      return 0;
      }

   //
   // Write record
   //

   if (_write(write_file,
              write_data_array,
              write_input_width * sizeof(SINGLE)) == -1)
      {
      return 0;
      }

   //
   // Write 128-byte block of extra data (file version 2)
   //
   // Lat/long/alt is always written, even if invalid
   // Caption and start/stop freq written only if present
   //

   C8 extra[V2_EXTRA_BYTES] = { 0 };
   
   C8 *D = extra;

   *D++ = TAG_LATLONGALT;
   *D++ = 24;

   memcpy(D, &latitude,   8); D += 8;
   memcpy(D, &longitude,  8); D += 8;
   memcpy(D, &altitude_m, 8); D += 8;

   if (caption != NULL)
      {
      S32 nbytes = strlen(caption)+1;
      S32 avail = V2_EXTRA_BYTES - (D-extra) - 2 - 1;    // copy only as much of caption string as will fit, leaving room for following EOD tag

      if (nbytes > avail) nbytes = avail;

      if (nbytes > 0)
         {
         *D++ = TAG_CAPTION;
         *D++ = (C8) nbytes;

         memcpy(D, caption, nbytes);
         D[nbytes-1] = 0;
         D += nbytes;
         }
      }

   if ((start_Hz != INVALID_FREQ) && (stop_Hz != INVALID_FREQ))
      {
      *D++ = TAG_FREQ;
      *D++ = 16;

      memcpy(D, &start_Hz, 8); D += 8;
      memcpy(D, &stop_Hz,  8); D += 8;
      }

   *D++ = TAG_EOD;
   assert(D-extra <= V2_EXTRA_BYTES);

   if (_write(write_file,
              extra,
              V2_EXTRA_BYTES) == -1)
      {
      return 0;
      }

   //
   // Clear record 
   //

   memset(write_data_array,
          0,
          write_input_width * sizeof(SINGLE));

   //
   // Return success
   //

   return 1;
}

//############################################################################
//
// set_single_point()
//
//############################################################################

void RECORDER::set_single_point  (SINGLE val,  //)
                                  S32    X)
{                  
   if (write_file == -1)
      {
      return;
      }

   write_data_array[X] = val;
}

//############################################################################
//
// set_arrayed_points()
//
//############################################################################

void RECORDER::set_arrayed_points(SINGLE *val,  //)
                                  S32     first_X, 
                                  S32     last_X, 
                                  S32    *explicit_X)
{
   if (write_file == -1)
      {
      return;
      }

   if (explicit_X == NULL)
      {
      S32 start = first_X;
      S32 end   = (last_X == -1) ? write_input_width-1 : last_X;

      if ((start < 0) || (end > write_input_width-1))
         {
         return;
         }

      for (S32 i=start; i <= end; i++)
         {
         write_data_array[i] = *val++;
         }
      }
   else
      {
      while (*explicit_X != -1)
         {
         S32 x = *explicit_X++;

         write_data_array[x] = *val++;
         }
      }
}

//############################################################################
//
// read_record()
//
//############################################################################

time_t RECORDER::read_record(SINGLE *dest, //)
                             S32     read_position,
                             DOUBLE *latitude,
                             DOUBLE *longitude,
                             DOUBLE *altitude_m,
                             DOUBLE *start_Hz,
                             DOUBLE *stop_Hz,
                             C8     *caption_dest,
                             S32     caption_dest_size)
{
   if ((read_file == -1) || (read_header_size == 0))
      {
      return 0;
      }

   //
   // Seek to specified record #, skipping header
   // at beginning of file and timestamp header at beginning of 
   // each record
   //

   S32 read_pos = read_header_size +                                                           
                 (read_position * (sizeof(time_t) + record_extra_bytes + (read_input_width * sizeof(SINGLE))));

   _lseek(read_file,
          read_pos,
          SEEK_SET);

   //
   // Read timestamp header at beginning of record
   //

   time_t timestamp;

   if (_read(read_file,
            &timestamp,
             sizeof(time_t)) == -1)
      {       
      return 0;
      }

   //
   // Read record
   //

   if (_read(read_file,
             dest,
             read_input_width * sizeof(SINGLE)) == -1)
      {
      return 0;
      }

   //
   // Read extra data
   //

   if (record_extra_bytes == 0)     // (pre-V2 file)
      {
      *latitude   = INVALID_LATLONGALT;
      *longitude  = INVALID_LATLONGALT; 
      *altitude_m = INVALID_LATLONGALT; 
      *start_Hz   = INVALID_FREQ;
      *stop_Hz    = INVALID_FREQ;
      caption_dest[0] = 0;
      }
   else
      {
      C8 extra[V2_EXTRA_BYTES] = { 0 };

      if (_read(read_file,
                extra,
                V2_EXTRA_BYTES) == -1)
         {       
         return 0;
         }

      C8 *S = extra;

      while (*S != TAG_EOD)
         {
         C8  tag = *S++;
         S32 len = *S++;

         switch (tag)
            {
            case TAG_LATLONGALT:
               {
               *latitude   = ((DOUBLE *) S)[0];
               *longitude  = ((DOUBLE *) S)[1]; 
               *altitude_m = ((DOUBLE *) S)[2];  
               break;
               }

            case TAG_CAPTION:
               {
               memcpy(caption_dest,    
                      S, 
                      max(caption_dest_size-1, len));
               break;
               }

            case TAG_FREQ:
               {
               *start_Hz = ((DOUBLE *) S)[0];
               *stop_Hz  = ((DOUBLE *) S)[1];
               break;
               }
            }

         S += len;
         }
      }

   //
   // Return timestamp
   //

   return timestamp;
}

//############################################################################
//
// bytes_per_record
//
//############################################################################

S32 RECORDER::bytes_per_record(S32 record_width)
{
   return (record_width * sizeof(SINGLE)) + 
           record_extra_bytes             + 
           sizeof(time_t);
}

//############################################################################
//
// n_readable_records()
//
//############################################################################

S32 RECORDER::n_readable_records(void)
{
   if ((read_file == -1) || (read_header_size == 0))
      {
      return 0;
      }

   S32 len = _filelength(read_file);

   //
   // Subtract header size at beginning of file
   // 

   len -= read_header_size;

   len /= bytes_per_record(read_input_width);

   return len;
}
