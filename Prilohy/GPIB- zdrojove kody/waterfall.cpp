//
// WATERFALL.CPP
//

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <io.h>
#include <fcntl.h>
#include <assert.h>
#include <limits.h>
#include <float.h>
#include <time.h>

#include "typedefs.h"

#include "w32sal.h"
#include "winvfx.h"

#include "waterfall.h"

//
// Ensure string can be rendered by VFX by replacing any negative ASCII
// characters with spaces
//

static C8 *sanitize_VFX(C8 *input)
{
   static C8 output[MAX_PATH];

   C8 *ptr = output;

   S32 len = strlen(input);

   if (len >= sizeof(output))
      {
      len = sizeof(output)-1;
      }

   for (S32 i=0; i < len; i++)
      {
      C8 ch = input[i];

      if (((U8) ch) >= 0x80) ch = ' ';

      *ptr++ = ch;
      }

   *ptr++ = 0;

   return output;
}

//
// Float to string, with precision specifier
// Rounds to nearest (like printf %#.#f specifier)
//

static C8 *float_to_string(DOUBLE val, S32 precision)
{
   static C8 string[256];

   if (precision > 0)
      {
      S32 exp = S32(pow(10.0F, precision) + 0.5F);

      if (val >= 0.0)
         val += (0.5 / exp);
      else
         val -= (0.5 / exp);

      S32 decimal = S32(fabs(val * exp)) % exp;

      C8 dec[256];
      sprintf(dec,"%d",decimal);

      while (((S32) strlen(dec)) < precision)
         {
         memmove(&dec[1],&dec[0],strlen(dec)+1);
         dec[0] = '0';
         }

      if (val < 0.0)
         {
         sprintf(string,"-%d.%s",
            S32(fabs(val)),
            dec);
         }
      else
         {
         sprintf(string,"%d.%s",
            S32(fabs(val)),
            dec);
         }
      }
   else
      {
      sprintf(string,"%d", S32(val));
      }

   return string;
}

//
// Return width of printed string in pixels
// 

static S32 string_width(C8 *string, VFX_FONT *font)
{
   S32 w = 0;

   for (U32 i=0; i < strlen(string); i++)
      {
      w += VFX_character_width(font,
                               string[i]);
      }

   return w;
}

//############################################################################
//
// WATERFALL()
//
//############################################################################

WATERFALL::WATERFALL(PANE *_graph, PANE *_outer)
{
   graph = _graph;
   outer = _outer;
                   
   display_width    = (graph->x1 - graph->x0) + 1;
   display_height   = (graph->y1 - graph->y0) + 1;

   color_array      = NULL;
   value_array      = NULL;
   time_array       = NULL;
   row_list         = NULL;
                   
   visible_rows     = 0;
   visible_y0       = 0;
   n_entries        = 0;
   color_base       = 0;
   row_record_valid = 0;
   rows_in_list     = 0;

   blank_color      = RGB_TRIPLET(0,0,0);
   background_color = RGB_TRIPLET(0,0,0);

   cell_height = 1;
   step_CLUT = NULL;
   smooth_CLUT = NULL;

#if WF_SPLINE_SMOOTHING
   smooth_R = NULL;
   smooth_G = NULL;
   smooth_B = NULL;
   R_color_array    = NULL;
   G_color_array    = NULL;
   B_color_array    = NULL;
#endif
}

//############################################################################
//
// ~WATERFALL()
//
//############################################################################

WATERFALL::~WATERFALL()
{
   if (step_CLUT != NULL)
      {
      free(step_CLUT);
      step_CLUT = NULL;
      }

   if (smooth_CLUT != NULL)
      {
      free(smooth_CLUT);
      smooth_CLUT = NULL;
      }

   if (color_array != NULL)
      {
      free(color_array);
      color_array = NULL;
      }

#if WF_SPLINE_SMOOTHING
   if (smooth_R != NULL)
      {
      free(smooth_R);
      smooth_R = NULL;
      }

   if (smooth_G != NULL)
      {
      free(smooth_G);
      smooth_G = NULL;
      }

   if (smooth_B != NULL)
      {
      free(smooth_B);
      smooth_B = NULL;
      }

   if (R_color_array != NULL)
      {
      free(R_color_array);
      R_color_array = NULL;
      }

   if (G_color_array != NULL)
      {
      free(G_color_array);
      G_color_array = NULL;
      }

   if (B_color_array != NULL)
      {
      free(B_color_array);
      B_color_array = NULL;
      }
#endif

   if (value_array != NULL)
      {
      free(value_array);
      value_array = NULL;
      }

   if (time_array != NULL)
      {
      free(time_array);
      time_array = NULL;
      }

   if (row_list != NULL)
      {
      for (S32 i=0; i < visible_rows; i++)
         {
         if (row_list[i].data != NULL)
            {
            free(row_list[i].data);
            row_list[i].data = NULL;
            }
         }

      free(row_list);
      row_list = NULL;
      }
}

void WATERFALL::set_graph_colors    (VFX_RGB *_color_array,
                                     S32       underrange_color,
                                     S32       overrange_color,
                                     S32       blank_color,
                                     S32      _scale_color)
{
   S32 i,s;
   assert(n_entries != 0);    // must call set_graph_attributes() first; we can't change n_entries here!

   scale_color = _scale_color;

   if (color_array != NULL)
      {
      free(color_array);
      color_array = NULL;
      }

   if (step_CLUT != NULL)
      {
      free(step_CLUT);
      step_CLUT = NULL;
      }

   if (smooth_CLUT != NULL)
      {
      free(smooth_CLUT);
      smooth_CLUT = NULL;
      }

#if WF_SPLINE_SMOOTHING
   if (R_color_array != NULL)
      {
      free(R_color_array);
      R_color_array = NULL;
      }

   if (G_color_array != NULL)
      {
      free(G_color_array);
      G_color_array = NULL;
      }

   if (B_color_array != NULL)
      {
      free(B_color_array);
      B_color_array = NULL;
      }

   if (smooth_R != NULL)
      {
      free(smooth_R);
      smooth_R = NULL;
      }

   if (smooth_G != NULL)
      {
      free(smooth_G);
      smooth_G = NULL;
      }

   if (smooth_B != NULL)
      {
      free(smooth_B);
      smooth_B = NULL;
      }
#endif

   //
   // Keep a copy of the palette
   //

   color_array = (S32 *) malloc(n_entries * sizeof(S32));

   if (color_array == NULL)
      {
      return;
      }

   for (i=0; i < n_entries; i++)
      {
      color_array[i] = VFX_pixel_value(&_color_array[i]) | 0x80000000;
      }

   //
   // Build discrete CLUT, filling in overrange/underrange entries for both CLUTs
   //

   S32 n = (color_limit - color_base) + 2;

   step_CLUT = (S32 *) malloc(n * sizeof(S32));

   if (step_CLUT == NULL)
      {
      return;
      }

   smooth_CLUT = (S32 *) malloc(n * sizeof(S32));

   if (smooth_CLUT == NULL)
      {
      return;
      }

   step_CLUT[0]   = blank_color;
   smooth_CLUT[0] = blank_color;

   S32 bin_base = 1;

   for (i=color_base; i < value_array[0]; i++)
      {
      assert(bin_base < n);

      step_CLUT   [bin_base] = underrange_color;
      smooth_CLUT [bin_base] = underrange_color;
      bin_base++;
      }

   S32 first_smoothed_bin = bin_base;

   for (s=0; s < n_entries-1; s++)
      {
      assert(bin_base < n);

      S32 n_bins = value_array[s+1] - value_array[s];

#if 1
      i = 0;
#else
      S32 half = n_bins/2;

      for (i=0; i < half; i++)
         {
         step_CLUT[bin_base+i] = color_array[s];
         }
#endif

      for (;i < n_bins; i++)
         {
         step_CLUT[bin_base+i] = color_array[s+1];
         }

      bin_base += n_bins;
      }

   S32 last_smoothed_bin = bin_base-1;

   while (bin_base < n)
      {
      step_CLUT   [bin_base] = overrange_color;
      smooth_CLUT [bin_base] = overrange_color;
      bin_base++;
      }

   //
   // Build smoothed CLUT
   //

#if WF_SPLINE_SMOOTHING

   //
   // This doesn't work as well as it could because ispline() assumes the control
   // points are evenly distributed... which they aren't, at the endpoints
   //

   R_color_array = (SINGLE *) malloc(n_entries * sizeof(SINGLE));
   G_color_array = (SINGLE *) malloc(n_entries * sizeof(SINGLE));
   B_color_array = (SINGLE *) malloc(n_entries * sizeof(SINGLE));

   if ((R_color_array == NULL) || (G_color_array == NULL) || (B_color_array == NULL)) 
      {
      return;
      }
   
   for (i=0; i < n_entries; i++)
      {
      R_color_array[i] = _color_array[i].r;
      G_color_array[i] = _color_array[i].g;
      B_color_array[i] = _color_array[i].b;
      }

   smooth_R = (SINGLE *) malloc(n * sizeof(SINGLE));
   smooth_G = (SINGLE *) malloc(n * sizeof(SINGLE));
   smooth_B = (SINGLE *) malloc(n * sizeof(SINGLE));

   if ((smooth_R == NULL) || (smooth_G == NULL) || (smooth_B == NULL)) 
      {
      return;
      }

   S32 len1 = n_entries;
   S32 len2 = last_smoothed_bin - first_smoothed_bin + 1;

   ispline(R_color_array, len1, smooth_R, len2);
   ispline(G_color_array, len1, smooth_G, len2);
   ispline(B_color_array, len1, smooth_B, len2);

   SINGLE *SR = smooth_R;
   SINGLE *SG = smooth_G;
   SINGLE *SB = smooth_B;
#endif

   bin_base = first_smoothed_bin;

   for (s=0; s < n_entries-1; s++)
      {
      assert(bin_base < n);

      S32 n_bins = value_array[s+1] - value_array[s];

      if (!n_bins)
         {
         continue;
         }

#if WF_SPLINE_SMOOTHING
      for (i=0; i < n_bins; i++)
         {
         smooth_CLUT[bin_base+i] = RGB_NATIVE(U8(*SR++ + 0.5F), 
                                              U8(*SG++ + 0.5F), 
                                              U8(*SB++ + 0.5F)); 
         }
#else
      VFX_RGB AR = *VFX_RGB_value(color_array[s]);
      VFX_RGB BR = *VFX_RGB_value(color_array[s+1]);

      DOUBLE ar = AR.r;
      DOUBLE ag = AR.g;
      DOUBLE ab = AR.b;
      DOUBLE br = BR.r;
      DOUBLE bg = BR.g;
      DOUBLE bb = BR.b;

      S32 d = n_bins;

      DOUBLE dr = (br-ar) / d;
      DOUBLE dg = (bg-ag) / d;
      DOUBLE db = (bb-ab) / d;

      for (i=0; i < n_bins; i++)
         {
         smooth_CLUT[bin_base+i] = RGB_NATIVE(U8(ar+0.5F), 
                                              U8(ag+0.5F), 
                                              U8(ab+0.5F)); 
         ar += dr;
         ag += dg;
         ab += db;
         }
#endif

      bin_base += n_bins;
      }
}

void WATERFALL::set_graph_attributes(S32      _cell_height, //)
                                     SINGLE  *_value_array,
                                     VFX_RGB *_color_array,
                                     S32      _n_entries,
                                     SINGLE    underrange_limit,
                                     SINGLE    overrange_limit,
                                     S32       underrange_color,
                                     S32       overrange_color,
                                     S32       blank_color,
                                     S32      _scale_color)
{                                          
   S32 i;

   cell_height = _cell_height;
   n_entries   = _n_entries;
   color_base  = S32(underrange_limit * 256.0F);
   color_limit = S32(overrange_limit * 256.0F);

   if (value_array != NULL)
      {
      free(value_array);
      value_array = NULL;
      }

   if (time_array != NULL)
      {
      free(time_array);
      time_array = NULL;
      }

   //
   // Allocate arrays to hold values and colors
   //

   value_array = (S32 *) malloc(n_entries * sizeof(S32));

   if (value_array == NULL)
      {
      return;
      }

   for (i=0; i < n_entries; i++)
      {
      value_array[i] = S32(_value_array[i] * 256.0F);
      }

   //
   // Calculate integral # of visible rows, and allocate time array
   //

   visible_rows = display_height / cell_height;

   visible_y0 = (graph->y1 - (visible_rows * cell_height)) + 1;

   time_array = (time_t *) malloc(visible_rows * sizeof(time_t));

   if (time_array == NULL)
      {
      return;
      }

   memset(time_array, 
          0, 
          visible_rows * sizeof(time_t));

   //
   // Allocate row descriptors, freeing any old list first
   //

   if (row_list != NULL)
      {
      for (i=0; i < rows_in_list; i++)
         {
         if (row_list[i].data != NULL)
            {
            free(row_list[i].data);
            row_list[i].data = NULL;
            }
         }

      free(row_list);
      row_list = NULL;
      }

   rows_in_list = visible_rows;

   row_list = (ROW_TAG *) malloc(visible_rows * sizeof(ROW_TAG));

   if (row_list == NULL)
      {
      return;
      }

   memset(row_list, 
          0, 
          visible_rows * sizeof(ROW_TAG));

   //
   // Allocate row data records, used for mouse-pointer querying 
   //

   for (i=0; i < visible_rows; i++)
      {
      row_list[i].data = (SINGLE *) malloc(display_width * sizeof(SINGLE));

      if (row_list[i].data == NULL)
         {
         return;
         }
      }

   //
   // Configure colors
   //

   set_graph_colors(_color_array,
                     underrange_color,
                     overrange_color,
                     blank_color,
                    _scale_color);
}

//############################################################################
//
// wipe_graph()
//
//############################################################################

void WATERFALL::wipe_graph(S32 _background_color)
{
   background_color = _background_color;

   //
   // Wipe rising-raster graph
   //

   VFX_pane_wipe(graph, 
                 background_color);

   //
   // Wipe time scale
   // 

   S32 outer_left   = graph->x0 - outer->x0;
   S32 outer_right  = graph->x1 - outer->x0;
   S32 outer_top    = graph->y0 - outer->y0;
   S32 outer_bottom = graph->y1 - outer->y0;

   S32 left = outer_left - 44;

   VFX_rectangle_fill(outer,
                      left,
                      outer_top,
                      outer_left - 1,
                      outer_bottom,
                      LD_DRAW,
                      background_color);

   //
   // Clear time array
   //

   memset(time_array, 
          0, 
          visible_rows * sizeof(time_t));

   //
   // Mark all recorded rows as invalid
   //

   for (S32 i=0; i < visible_rows; i++)
      {
      row_list[i].in_use = 0;
      }
}

//############################################################################
//
// n_visible_rows()
//
//############################################################################

S32 WATERFALL::n_visible_rows(void)
{
   return visible_rows;
}

//############################################################################
//
// scroll_graph()
//
//############################################################################

void WATERFALL::scroll_graph(S32 scroll_offset)
{
   //
   // No need to waste time scrolling the graph beyond the screen limits
   //

   if (scroll_offset > display_height)
      scroll_offset = display_height;

   if (scroll_offset < -display_height)
      scroll_offset = -display_height;

   //
   // Scroll subpane which represents integral # of visible rows -- so we can
   // avoid showing fractional rows, which would cause problems for
   // bidirectional scrolling
   //

   PANE visible = *graph;
   visible.y0 = visible_y0;

   VFX_pane_scroll(&visible, 
                    0,
                    cell_height * scroll_offset,
                    PS_NOWRAP,
                    RGB_TRIPLET(0,0,0));

   //
   // Scroll time array in parallel with graph rows
   //

   S32 n,i;

   if (scroll_offset > 0)
      {
      for (n=0; n < scroll_offset; n++)
         {
         //
         // Scrolling downward
         //

         for (i=visible_rows-2; i >= 0; i--)
            {
            time_array[i+1] = time_array[i];
            }

         time_array[i+1] = 0;
         }
      }
   else
      {
      for (n=0; n < (-scroll_offset); n++)
         {
         //
         // Scrolling upward
         //

         for (i=1; i < visible_rows; i++)
            {
            time_array[i-1] = time_array[i];
            }

         time_array[i-1] = 0;
         }
      }

   //
   // Update row #s for stored records used for mouse queries
   //
   // If scrolling up, mark all row records < -scroll_offset invalid
   // If scrolling down, mark all row records >= (visible_rows-scroll_offset) invalid
   //

   if (scroll_offset < 0)
      {
      S32 c = -scroll_offset;

      for (i=0; i < visible_rows; i++)
         {
         if (row_list[i].row < c)
            {
            row_list[i].in_use = 0;
            }
         else
            {
            row_list[i].row += scroll_offset;
            }
         }
      }
   else
      {
      for (i=0; i < visible_rows; i++)
         {
         S32 c = visible_rows - scroll_offset;

         if (row_list[i].row >= c)
            {
            row_list[i].in_use = 0;
            }
         else
            {
            row_list[i].row += scroll_offset;
            }
         }
      }
}

//############################################################################
//
// query_graph()
//
//############################################################################

S32 WATERFALL::query_graph(S32      x,  //)
                           S32      y,
                           time_t  *acquisition_time,
                           SINGLE  *value,
                           SINGLE  *horizontal_percent,
                           SINGLE **row_record,
                           DOUBLE  *latitude,
                           DOUBLE  *longitude,
                           DOUBLE  *altitude_m,
                           DOUBLE  *start_Hz,
                           DOUBLE  *stop_Hz,
                           C8      *caption,
                           S32      caption_bytes)
{
   //
   // Normalize (x,y) to graph display pane
   //

   x -= graph->x0;
   y -= graph->y0;

   if ((x < 0) || 
       (y < 0) || 
       (x >= display_width) || 
       (y >= display_height))
      {
      return 0;
      }

   //
   // Determine row # for cursor Y
   // 
   // Graph is bottom-justified
   // 

   S32 graph_top = display_height - (visible_rows * cell_height);

   y -= graph_top;

   if (y < 0)
      {
      return 0;
      }

   S32 row = y / cell_height;

   //
   // Search for valid record for this row, and look up requested value
   //

   for (S32 i=0; i < visible_rows; i++)
      {
      if ((row_list[i].in_use) && 
          (row_list[i].row == row))
         {
         if (acquisition_time != NULL)
            {
            *acquisition_time = row_list[i].time;
            }

         if (value != NULL)
            {
            *value = row_list[i].data[x];
            }

         if (horizontal_percent != NULL)
            {
            *horizontal_percent = SINGLE(x) / SINGLE(display_width-1);
            }

         if (row_record != NULL)
            {
            *row_record = row_list[i].data;
            }

         if (start_Hz != NULL)
            {
            *start_Hz = row_list[i].start_Hz;
            }

         if (stop_Hz != NULL)
            {
            *stop_Hz = row_list[i].stop_Hz;
            }

         if (latitude != NULL)
            {
            *latitude = row_list[i].latitude;
            }

         if (longitude != NULL)
            {
            *longitude = row_list[i].longitude;
            }

         if (altitude_m != NULL)
            {
            *altitude_m = row_list[i].altitude_m;
            }

         if ((caption != NULL) && (caption_bytes > 0))
            {
            memset(caption, 0, caption_bytes);
            _snprintf(caption, caption_bytes-1, "%s", row_list[i].caption);
            }

         //
         // Return success
         //

         return 1;
         }
      }

   //
   // Row lookup failed -- we don't have any valid data for this row
   //

   return 0;
}

//############################################################################
//
// draw_fixed_point_graph_row()
//
//############################################################################

void WATERFALL::draw_fixed_point_graph_row(time_t acquisition_time, //)
                                           S32   *data,
                                           S32    row,
                                           BOOL   smooth_mode,
                                           DOUBLE latitude,
                                           DOUBLE longitude,
                                           DOUBLE altitude_m,
                                           DOUBLE start_Hz,
                                           DOUBLE stop_Hz)
{
   S32 *CLUT = smooth_mode ? smooth_CLUT : step_CLUT;

   if ((CLUT == NULL) || (display_width > 4096))
      {
      return;
      }

   if (!row_record_valid)
      {
      //
      // Find a free row record (at least one record should have been freed by a 
      // previous scroll operation)
      //

      S32 i;
      S32 record = -1;

      for (i=0; i < visible_rows; i++)
         {
         if (!row_list[i].in_use)
            {
            record = i;
            break;
            }

         if (row_list[i].row == row)
            {
            record = i;
            }
         }

      //
      // If no row records were free, fail call (shouldn't happen) 
      //

      if (record == -1)
         {
         return;
         }

      //
      // Mark row record in use, and log timestamp
      //

      row_list[record].in_use     = 1;
      row_list[record].row        = row;
      row_list[record].time       = acquisition_time;
      row_list[record].latitude   = latitude;
      row_list[record].longitude  = longitude;
      row_list[record].altitude_m = altitude_m;
      row_list[record].start_Hz   = start_Hz;
      row_list[record].stop_Hz    = stop_Hz;

      //
      // Copy fixed-point data to row record, translating it back to floating-point
      // for retrieval by mouse-pointer query function
      //

      SINGLE *row_data = row_list[record].data;

      for (i=0; i < display_width; i++)
         {
         row_data[i] = SINGLE(data[i] >> 8);
         }
      }

   //
   // Draw the graph
   // Overrange/underrange colors are rendered in "blank color"
   //

   S32 colors[4096];

   for (S32 i=0; i < display_width; i++)
      {
      if ((data[i] < color_base) || (data[i] > color_limit))
         {
         colors[i] = CLUT[0];
         }
      else
         {
         colors[i] = CLUT[(data[i] - color_base) + 1];
         }
      }

   if (row < 0)
      {
      row = visible_rows-1;
      }

   if ((row < 0) || (row > (visible_rows-1)))
      {
      return;
      }

   S32 row_base = (visible_rows - row - 1) * cell_height;

   for (S32 h=1; h <= cell_height; h++)
      {
      S32 y = (display_height - h) - row_base;

      U16 *basl = (U16 *) (((U8 *) graph->window->buffer) +
                                 ((y+graph->y0) * ((graph->window->x_max+1) * 
                                   graph->window->pixel_pitch)));
      basl += graph->x0;

      for (S32 x=0; x < display_width; x++)
         {
         *basl++ = U16(colors[x]);
         }
      }

   //
   // Log the time
   //

   time_array[row] = acquisition_time;

   //
   // Indicate need to fill row record for next call
   //

   row_record_valid = 0;
}

//############################################################################
//
// draw_graph_row()
//
//############################################################################

void WATERFALL::draw_graph_row(time_t  acquisition_time, //)
                               SINGLE *data,
                               S32     row,
                               BOOL    smooth_mode,
                               DOUBLE  latitude,
                               DOUBLE  longitude,
                               DOUBLE  altitude_m,
                               DOUBLE  start_Hz,
                               DOUBLE  stop_Hz,
                               C8     *caption)
{
   if (step_CLUT == NULL)
      {
      return;
      }

   //
   // Find a free row record (at least one record should have been freed by a 
   // previous scroll operation)
   //

   S32 i;
   S32 record = -1;

   for (i=0; i < visible_rows; i++)
      {
      if (!row_list[i].in_use)
         {
         record = i;
         break;
         }

      if (row_list[i].row == row)
         {
         record = i;
         }
      }

   //
   // If no row records were free, fail call (shouldn't happen) 
   //

   if (record == -1)
      {
      return;
      }

   //
   // Mark row record in use, and log timestamp
   //

   row_list[record].in_use     = 1;
   row_list[record].row        = row;
   row_list[record].time       = acquisition_time;
   row_list[record].start_Hz   = start_Hz;
   row_list[record].stop_Hz    = stop_Hz;
   row_list[record].latitude   = latitude;
   row_list[record].longitude  = longitude;
   row_list[record].altitude_m = altitude_m;

   if (caption != NULL)
      _snprintf(row_list[record].caption, sizeof(row_list[record].caption)-1, "%s", caption);
   else
      row_list[record].caption[0] = 0;

   //
   // Convert floating-point input data to 24.8 fixed-point (also storing it in row
   // record for mouse queries)
   //

   SINGLE *row_data = row_list[record].data;

   S32 temp_array[4096];
   SINGLE x256 = 256.0F;

   for (i=0; i < display_width; i++)
      {
      _asm
         {
         mov ebx,i
         mov eax,data
         mov ecx,row_data
         fld [eax][ebx*4]
         fst DWORD PTR [ecx][ebx*4]
         fmul x256
         fistp temp_array[ebx*4]
         }
      }

   //
   // Indicate row record already filled (so integer version of routine won't
   // try to fill it as well)
   //

   row_record_valid = 1;

   //
   // Call fixed-point integer version
   //

   draw_fixed_point_graph_row(acquisition_time, 
                              temp_array,
                              row,
                              smooth_mode,
                              latitude,
                              longitude,
                              altitude_m,
                              start_Hz,
                              stop_Hz);
}

//############################################################################
//
// draw_scale()
//
//############################################################################

S32 WATERFALL::draw_scale(S32 hit_x, //)
                          S32 hit_y)
{
   if (step_CLUT == NULL)
      {
      return -1;
      }

   //
   // Draw (and hit-test) color key at right of graph
   //

   S32 result = -1;

   S32 vh = VFX_font_height(VFX_default_system_font());

   S32 outer_left   = graph->x0 - outer->x0;
   S32 outer_right  = graph->x1 - outer->x0;
   S32 outer_top    = graph->y0 - outer->y0;
   S32 outer_bottom = graph->y1 - outer->y0;

   S32 x_offset = outer_right + 20; 
   S32 y_offset = outer_top + (2 * vh); 

   hit_x -= outer->x0;
   hit_y -= outer->y0;

   S32 left  = x_offset;
   S32 right = outer->x1 - 20;
   S32 width = (right - left) + 1;

   S32 y0 = y_offset;
   S32 i;

   for (i=n_entries-1; i >= 0; i--)
      {
      S32 y1 = y0 + vh + 2;

      C8 *s = float_to_string(SINGLE(value_array[i] >> 8),
                              0);

      S32 l = string_width(s, 
                           VFX_default_system_font());

      if ((hit_x >= left) && (hit_x <= right) && 
          (hit_y >= y0  ) && (hit_y <= y1))
         {
         result = i;
         }

      VFX_rectangle_fill(outer,
                         left,
                         y0,
                         right,
                         y1,
                         LD_DRAW,
                         color_array[i]);

      for (S32 oy=-1; oy <= 1; oy++)
         {
         for (S32 ox=-1; ox <= 1; ox++)
            {
            VFX_string_draw(outer,
                            left + ((width/2) - (l/2)) + ox,
                            y0+2 + oy,
                            VFX_default_system_font(),
                            s,
                            VFX_default_font_color_table(VFC_BLACK_ON_XP));
            }
         }

      transparent_font_CLUT[0] = (U16) RGB_TRANSPARENT;
      transparent_font_CLUT[1] = (U16) RGB_NATIVE(192,192,192);

      VFX_string_draw(outer,
                      left + ((width/2) - (l/2)),
                      y0+1,
                      VFX_default_system_font(),
                      s,
                      transparent_font_CLUT);
      y0 = y1 + 1;
      }

   //
   // Draw timestamp key at left of graph
   //
   // Timestamps are vertically centered in cell rows, and spaced at 
   // the first multiple of the cell height which is >= twice the font 
   // height
   //

   left = outer_left - 44;

   VFX_rectangle_fill(outer,
                      left,
                      outer_top,
                      outer_left - 1,
                      outer_bottom,
                      LD_DRAW,
                      background_color);

   S32 time_Y = (display_height - (cell_height / 2) - (vh / 2)) + outer_top;

   S32 time_Y_interval    = 0;
   S32 time_cell_interval = 0;

   while (time_Y_interval < (vh * 2))
      {
      time_Y_interval += cell_height;
      time_cell_interval++;
      }

   S32 n_time_cells = (visible_rows / time_cell_interval) + 1;
   S32 time_cell    = visible_rows - 1;

   for (i=0; i < n_time_cells; i++)
      {
      if (time_cell < 0)
         {
         break;
         }

      if (((time_Y + vh) <  outer_bottom) &&
           (time_Y       >= outer_top)    &&
           (time_array[time_cell] != 0)) 
         {
         C8 time_text[32];

         C8 *f = ctime(&time_array[time_cell]);

         if (f != NULL)
            {
            strcpy(time_text,f);
            }

         time_text[16] = 0;

         strcpy(time_text,sanitize_VFX(time_text));

         VFX_string_draw(outer,
                         left,
                         time_Y,
                         VFX_default_system_font(),
                        &time_text[11],
                         VFX_default_font_color_table(VFC_WHITE_ON_XP));
         }
      
      time_Y    -= time_Y_interval;
      time_cell -= time_cell_interval;
      }

   //
   // If cell height >= 3, draw tick marks at vertical cell boundaries
   //

   if (cell_height >= 3)
      {
      S32 tick_Y = (display_height - cell_height) + outer_top;

      for (i=0; i < visible_rows; i++)
         {
         if (time_array[(visible_rows - i) - 1])
            {
            VFX_line_draw(outer,
                          outer_left-6,
                          tick_Y,
                          outer_left-2,
                          tick_Y,
                          LD_DRAW,
                          scale_color);
            }

         tick_Y -= cell_height;
         }
      }

   return result;
}

