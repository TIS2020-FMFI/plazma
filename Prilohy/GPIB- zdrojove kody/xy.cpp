//
// XY.CPP
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

#include "typedefs.h"

#include "w32sal.h"
#include "winvfx.h"

#include "xy.h"

#define XY_UNITY       0
#define XY_OVERSAMPLE  1
#define XY_UNDERSAMPLE 2

#define CUR_L 0
#define CUR_R 1
#define CUR_T 2
#define CUR_B 3

#define EPSILON 0.0001F    // Tolerance for floating-point compares

static U16 transparent_font_CLUT[256];

//
// Round float to nearest int
//

static S32 round_to_nearest(SINGLE f)
{
   if (f >= 0.0F) 
      f += 0.5F;
   else
      f -= 0.5F;

   return S32(f);
}

static S32 round_to_nearest(DOUBLE f)
{
   if (f >= 0.0) 
      f += 0.5;
   else
      f -= 0.5;

   return S32(f);
}

//
// Float to string with format/precision specifier
//

static C8 *float_to_string(DOUBLE val, C8 *format)
{
   static C8 string[256];

   memset(string, 0, sizeof(string));
   _snprintf(string, sizeof(string)-1, format, val);

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
// xy() private methods
//
//############################################################################

//
// Handle Y-exclusion region testing
//

S32 inline XY::include(SINGLE t)
{
   if (bottom_cursor_val() <= top_cursor_val())
      {
      return (t >= bottom_cursor_val()) && (t <= top_cursor_val());
      }
   else
      {
      return (t >= bottom_cursor_val()) || (t <= top_cursor_val());
      }
}

//############################################################################
//
// xy() public methods
//
//############################################################################

XY::XY(PANE *_graph, PANE *_outer)
{
   graph = _graph;
   outer = _outer;

   display_width  = (graph->x1 - graph->x0) + 1;
   display_height = (graph->y1 - graph->y0) + 1;

   set_graph_attributes();

   X_array = NULL;
   Y_array = NULL;
   input_array   = NULL;
   display_array = NULL;
   combined_array = NULL;
   threshold_array = NULL;
   min_array = NULL;
   max_array = NULL;
   sum_array = NULL;

   set_input_width(display_width);
   set_X_range(0.0F, 1800.0F, "%.1lf", 360.0F, 180.0F);
   set_Y_range(0.0F, -100.0F, "%.1lf", 25.0F,  12.5F);

   zoom_x0   = 0;
   zoom_x1   = display_width-1;
   thresh_y0 = 0;
   thresh_y1 = display_height-1;

   active_cursor = -1;
   zoom_mode = 0;
   enable_zoom_cursors = 1;
   n_cycles = 0;
   running_overlay = 0;
   font_palette = VFX_default_font_color_table(VFC_WHITE_ON_BLACK);

   transparent_font_CLUT[0] = (U16) RGB_TRANSPARENT;
}

//############################################################################
//
// ~XY()
//
//############################################################################

XY::~XY()
{
   if (X_array != NULL)
      {
      free(X_array);
      X_array = NULL;
      }

   if (Y_array != NULL)
      {
      free(Y_array);
      Y_array = NULL;
      }

   if (input_array != NULL)
      {
      free(input_array);
      input_array = NULL;
      }

   if (display_array != NULL)
      {
      free(display_array);
      display_array = NULL;
      }

   if (threshold_array != NULL)
      {
      free(threshold_array);
      threshold_array = NULL;
      }

   if (combined_array != NULL)
      {
      free(combined_array);
      combined_array = NULL;
      }

   if (min_array != NULL)
      {
      free(min_array);
      min_array = NULL;
      }

   if (max_array != NULL)
      {
      free(max_array);
      max_array = NULL;
      }

   if (sum_array != NULL)
      {
      free(sum_array);
      sum_array = NULL;
      }
}

//############################################################################
//
// reset_running_values()
//
//############################################################################

void XY::reset_running_values(void)
{
   if ((max_array != NULL) && 
       (min_array != NULL) && 
       (sum_array != NULL))
      {
      S32 i;

      for (i=0; i < display_width; i++)
         {
         max_array[i] = bottom_val;
         min_array[i] = top_val;
         sum_array[i] = 0.0F;
         }
      }

   n_cycles = 0;
}

//############################################################################
//
// clear_max_hold()
//
//############################################################################

void XY::clear_max_hold(void)
{
   for (S32 i=0; i < display_width; i++)
      {
      display_array[i] = combined_array[i];
      }
}

//############################################################################
//
// set_input_width()
//
//############################################################################

S32 XY::set_input_width(S32 _n_points)
{
   if (!_n_points)
      {
      return 0;
      }

   if (display_width > _n_points)
      {
      //
      // "Oversampling" the input -- ensure display_width is evenly divisible
      // by _n_points
      //

      if (((display_width / _n_points) * _n_points) != display_width)
         {
         return 0;
         }

      ratio = XY_OVERSAMPLE;
      }
   else if (display_width < _n_points)
      {
      //
      // "Undersampling" the input -- ensure _n_points is evenly divisible
      // by display_width
      //

      if (((_n_points / display_width) * display_width) != _n_points)
         {
         return 0;
         }

      ratio = XY_UNDERSAMPLE;
      }
   else
      {
      //
      // Unity sampling -- display_width is equal to _n_points
      //

      ratio = XY_UNITY;
      }

   n_points = _n_points;

   //
   // Allocate input array (reflects values passed from app)
   //

   if (input_array != NULL)
      {
      free(input_array);
      input_array = NULL;
      }

   input_array = (SINGLE *) malloc(n_points * sizeof(SINGLE));

   if (input_array == NULL)
      {
      return 0;
      }

   memset(input_array, 0, n_points * sizeof(SINGLE));

   //
   // Allocate combined array (reflects input data merged into displayable
   // columns)
   //

   if (combined_array != NULL)
      {
      free(combined_array);
      combined_array = NULL;
      }

   combined_array = (SINGLE *) malloc(display_width * sizeof(SINGLE));

   if (combined_array == NULL)
      {
      free(input_array);
      return 0;
      }

   for (S32 i=0; i < display_width; i++)
      {
      combined_array[i] = (SINGLE) -LONG_MAX;
      }

   //
   // Allocate threshold array (reflects input data with threshold-detection
   // applied, but before max-hold applied)
   //

   if (threshold_array != NULL)
      {
      free(threshold_array);
      threshold_array = NULL;
      }

   threshold_array = (SINGLE *) malloc(display_width * sizeof(SINGLE));

   if (threshold_array == NULL)
      {
      free(input_array);
      return 0;
      }

   memset(threshold_array, 0, display_width * sizeof(SINGLE));

   //
   // Allocate display array (reflects current values actually graphed)
   //
   // We must initialize the display array to a large negative value, or max-hold
   // won't work!
   //

   if (display_array != NULL)
      {
      free(display_array);
      display_array = NULL;
      }

   display_array = (SINGLE *) malloc(display_width * sizeof(SINGLE));

   if (display_array == NULL)
      {
      free(input_array);
      free(combined_array);
      return 0;
      }

   clear_max_hold();

   //
   // Allocate min/max/sum running arrays
   //

   if (min_array != NULL)
      {
      free(min_array);
      min_array = NULL;
      }

   min_array = (SINGLE *) malloc(display_width * sizeof(SINGLE));

   if (min_array == NULL)
      {
      free(display_array);
      free(input_array);
      free(combined_array);
      return 0;
      }

   if (max_array != NULL)
      {
      free(max_array);
      max_array = NULL;
      }

   max_array = (SINGLE *) malloc(display_width * sizeof(SINGLE));

   if (max_array == NULL)
      {
      free(display_array);
      free(input_array);
      free(combined_array);
      free(min_array);
      return 0;
      }

   if (sum_array != NULL)
      {
      free(sum_array);
      sum_array = NULL;
      }

   sum_array = (SINGLE *) malloc(display_width * sizeof(SINGLE));

   if (sum_array == NULL)
      {
      free(display_array);
      free(input_array);
      free(combined_array);
      free(min_array);
      free(max_array);
      return 0;
      }

   //
   // Init max/min/sum arrays
   //

   reset_running_values();

   return 1;
}


//############################################################################
//
// set_Y_range()
//
//############################################################################

void XY::set_Y_range(SINGLE _top_val,  //)
                     SINGLE _bottom_val, 
                     C8    *_format,
                     SINGLE _major_tic_increment,
                     SINGLE _minor_tic_increment)
{
   S32 i;

   top_val               = _top_val;
   bottom_val            = _bottom_val;
   Y_major_tic_increment = _major_tic_increment;
   Y_minor_tic_increment = _minor_tic_increment;

   strcpy(Y_format, _format);

   reset_running_values();

   if (Y_array != NULL)
      {
      free(Y_array);
      Y_array = NULL;
      }

   Y_array = (SINGLE *) malloc(display_height * sizeof(SINGLE));

   if (Y_array == NULL)
      {
      return;
      }

   SINGLE h = bottom_val - top_val;

   SINGLE dv = h / (SINGLE) (display_height-1);

   for (i=0; i < display_height; i++)
      {
      Y_array[i] = top_val + (((SINGLE) i) * dv);
      }

   //
   // Find closest Y_array entries to all minor tic points, and ensure they are 
   // exact
   //
   // We assume that major tic marks are coincident with minor ones, tic intervals are positive,
   // and amplitude values decrease down the Y axis
   //

   i = 0;

   while (1)
      {
      SINGLE v = -Y_minor_tic_increment * (SINGLE) i++;

      if ((top_val + v) <= bottom_val)
         {
         break;
         }

      S32 j = round_to_nearest(v / dv);
      assert(j <= display_height-1);

      Y_array[j] = top_val + v;
      }

   Y_array[display_height-1] = bottom_val;
}

//############################################################################
//
// set_Y_cursors()
//
//############################################################################

void XY::set_Y_cursors(SINGLE _top_cursor_val,  //)
                       SINGLE _bottom_cursor_val)
{
   thresh_y0 = 0;
   thresh_y1 = 0;

   SINGLE y0_min_diff = (SINGLE) LONG_MAX;
   SINGLE y1_min_diff = (SINGLE) LONG_MAX;

   for (S32 i=0; i < display_height; i++)
      {
      SINGLE dt = (SINGLE) fabs(_top_cursor_val - Y_array[i]);

      if (dt < y0_min_diff) 
         {
         y0_min_diff = dt;
         thresh_y0 = i;
         }

      dt = (SINGLE) fabs(_bottom_cursor_val - Y_array[i]);

      if (dt < y1_min_diff)
         {
         y1_min_diff = dt;
         thresh_y1 = i;
         }
      }
}

//############################################################################
//
// set_X_range()
//
//############################################################################

void XY::set_X_range(DOUBLE _left_val,  //)
                     DOUBLE _right_val, 
                     C8    *_format,
                     DOUBLE _major_tic_increment,
                     DOUBLE _minor_tic_increment)
{
   S32 i;

   left_val              = _left_val;
   right_val             = _right_val;
   X_major_tic_increment = _major_tic_increment;
   X_minor_tic_increment = _minor_tic_increment;

   strcpy(X_format, _format);

   if (X_array != NULL)
      {
      free(X_array);
      X_array = NULL;
      }

   X_array = (DOUBLE *) malloc(display_width * sizeof(DOUBLE));

   if (X_array == NULL)
      {
      return;
      }

   DOUBLE w = right_val - left_val;

   DOUBLE dv = w / (DOUBLE) (display_width-1);

   for (i=0; i < display_width; i++)
      {
      X_array[i] = left_val + (((DOUBLE) i) * dv);
      }

   //
   // Find closest X_array entries to all minor tic points, and ensure they are 
   // exact
   //
   // We assume that major tic marks are coincident with minor ones, tic intervals are positive,
   // and amplitude values increase along the X axis
   //

   i = 0;

   while (1)
      {
      DOUBLE v = X_minor_tic_increment * (DOUBLE) i++;

      if ((left_val + v) >= right_val)
         {
         break;
         }

      S32 j = round_to_nearest(v / dv);
      assert(j <= display_width-1);

      X_array[j] = left_val + v;
      }

   X_array[display_width-1] = right_val;
}
   
//############################################################################
//
// set_X_cursors()
//
//############################################################################

void XY::set_X_cursors(DOUBLE _left_cursor_val, //)
                       DOUBLE _right_cursor_val)
{
   zoom_x0 = 0;
   zoom_x1 = 0;

   SINGLE x0_min_diff = (SINGLE) LONG_MAX;
   SINGLE x1_min_diff = (SINGLE) LONG_MAX;

   for (S32 i=0; i < display_width; i++)
      {
      SINGLE dt = (SINGLE) fabs(_left_cursor_val - X_array[i]);

      if (dt < x0_min_diff) 
         {
         x0_min_diff = dt;
         zoom_x0 = i;
         }

      dt = (SINGLE) fabs(_right_cursor_val - X_array[i]);

      if (dt < x1_min_diff)
         {
         x1_min_diff = dt;
         zoom_x1 = i;
         }
      }

   zoom_x0 += graph->x0;
   zoom_x1 += graph->x0;
}

//############################################################################
//
// set_single_point()
//
//############################################################################

void XY::set_single_point  (SINGLE val, //)
                            S32    X)
{
   input_array[X] = val;
}

//############################################################################
//
// set_arrayed_points()
//
//############################################################################

void XY::set_arrayed_points(DOUBLE *val,
                            S32     first_X,
                            S32     last_X,
                            S32    *explicit_X)
{
   if (explicit_X == NULL)
      {
      S32 start = first_X;
      S32 end   = (last_X == -1) ? n_points-1 : last_X;

      if ((start < 0) || (end > n_points-1))
         {
         return;
         }

      for (S32 i=start; i <= end; i++)
         {
         input_array[i] = (SINGLE) *val++;
         }
      }
   else
      {
      while (*explicit_X != -1)
         {
         S32 x = *explicit_X++;

         input_array[x] = (SINGLE) *val++;
         }
      }
}

void XY::set_arrayed_points(SINGLE *val,
                            S32     first_X, 
                            S32     last_X, 
                            S32    *explicit_X)
{
   if (explicit_X == NULL)
      {
      S32 start = first_X;
      S32 end   = (last_X == -1) ? n_points-1 : last_X;

      if ((start < 0) || (end > n_points-1))
         {
         return;
         }

      for (S32 i=start; i <= end; i++)
         {
         input_array[i] = *val++;
         }
      }
   else
      {
      while (*explicit_X != -1)
         {
         S32 x = *explicit_X++;

         input_array[x] = *val++;
         }
      }
}

//############################################################################
//
// set_graph_attributes()
//
//############################################################################

void XY::set_graph_attributes(XY_GRAPHSTYLE _style, //)
                              S32           _merge_op,
                              S32           _accum_op,
                              S32           _max_hold,
                              S32           _background_color,
                              S32           _graph_color,
                              S32           _scale_color,
                              S32           _major_graticule_color,
                              S32           _minor_graticule_color,
                              S32           _cursor_color,
                              S32           _running_overlay_color,
                              void         *_font_palette)
{
   style                 = _style;
   merge_op              = _merge_op;
   accum_op              = _accum_op;
   max_hold              = _max_hold;
   background_color      = _background_color;
   graph_color           = _graph_color;
   scale_color           = _scale_color;
   major_graticule_color = _major_graticule_color;
   minor_graticule_color = _minor_graticule_color;
   cursor_color          = _cursor_color;
   running_overlay_color = _running_overlay_color;
   font_palette          = _font_palette;

   if (font_palette == NULL)
      {
      font_palette = VFX_default_font_color_table(VFC_WHITE_ON_BLACK);
      }
}

//############################################################################
//
// draw_background()
//
//############################################################################

void XY::draw_background(void)
{
   VFX_pane_wipe(graph, background_color);
}

//############################################################################
//
// process_input_data()
//
//############################################################################

void XY::process_input_data(void)
{
   S32    i,j,m,n;
   SINGLE k,x,dx;

   //
   // Resample input array into intermediate array
   //

   switch (ratio)
      {
      //
      // Unity case: simply copy points from input to display
      //

      case XY_UNITY:

         for (i=0; i < display_width; i++)
            {
            combined_array[i] = input_array[i];
            }
         break;

      //
      // Oversampling: more displayed points than input points
      //
      // Each input point must be replicated across one or more
      // display points
      //

      case XY_OVERSAMPLE:

         dx = SINGLE(n_points) / SINGLE(display_width);

         x = 0.0F;

         for (i=0; i < display_width; i++)
            {
            combined_array[i] = input_array[S32(x)];
            x += dx;
            }
         break;

      //
      // Undersampling: more input points than displayed points
      //
      // Each integral cluster of input points must be combined 
      // to yield one display point
      //

      case XY_UNDERSAMPLE:

         n = n_points / display_width;

         i = m = 0;

         for (i=0; i < display_width; i++)
            {
            switch (merge_op)
               {
               default:
                  assert(0);
                  break;

               case XY_AVERAGE:

                  k = 0.0F;

                  for (j=0; j < n; j++)
                     {
                     k += input_array[m++];
                     }

                  combined_array[i] = k / SINGLE(n);
                  break;

               case XY_MINIMUM:

                  k = FLT_MAX;

                  for (j=0; j < n; j++)
                     {
                     if (input_array[m] < k)
                        {
                        k = input_array[m];
                        }

                     ++m;
                     }

                  combined_array[i] = k;
                  break;

               case XY_MAXIMUM:

                  k = -FLT_MAX;

                  for (j=0; j < n; j++)
                     {
                     if (input_array[m] > k)
                        {
                        k = input_array[m];
                        }

                     ++m;
                     }

                  combined_array[i] = k;
                  break;
               }
            }
         break;
      }

   //
   // Update running max/min/sum arrays with combined array data
   //

   for (i=0; i < display_width; i++)
      {
      SINGLE c = combined_array[i];

      sum_array[i] += c;

      if (c > max_array[i])
         {
         max_array[i] = c;
         }

      if (c < min_array[i])
         {
         min_array[i] = c;
         }
      }

   ++n_cycles;

   //
   // Transfer contents of combined array to display array, performing
   // zoom and max-hold processing
   //
   // Before applying max-hold, copy the same data to the threshold array 
   // with threshold processing (any values > or < the min/max thresholds 
   // are set to BLANK_VALUE).  This array is normally the one which is
   // sent to the complementary rising-raster display.
   //

   if (zoom_mode)
      {
      //
      // z=index of display array entry at left cursor
      //

      DOUBLE z = DOUBLE(zoom_x0);
      DOUBLE dz = DOUBLE(zoom_x1 - zoom_x0) / DOUBLE(display_width-1);

      //
      // Transfer array contents with zoom function
      //

      if (!max_hold)
         {
         for (i=0; i < display_width; i++)
            {
            SINGLE t = combined_array[S32(z)];

            display_array[i] = t;

            if (include(t))
               {
               threshold_array[i] = t;
               }
            else
               {
               threshold_array[i] = BLANK_VALUE;
               }

            z += dz;
            }
         }
      else
         {
         for (i=0; i < display_width; i++)
            {
            SINGLE t = combined_array[S32(z)];

            if (t > display_array[i])
               {
               display_array[i] = t;
               }
                          
            if (include(t))
               {
               threshold_array[i] = t;
               }
            else
               {
               threshold_array[i] = BLANK_VALUE;
               }

            z += dz;
            }
         }
      }
   else
      {
      if (!max_hold)
         {
         for (i=0; i < display_width; i++)
            {
            SINGLE t = combined_array[i];

            if (include(t))
               {
               threshold_array[i] = t;
               }
            else
               {
               threshold_array[i] = BLANK_VALUE;
               }

            display_array[i] = t;
            }
         }
      else
         {
         for (i=0; i < display_width; i++)
            {
            SINGLE t = combined_array[i];

            if (include(t))
               {
               threshold_array[i] = t;
               }
            else
               {
               threshold_array[i] = BLANK_VALUE;
               }

            if (t > display_array[i])
               {
               display_array[i] = t;
               }
            }
         }
      }
}

//############################################################################
//
// draw_graph()
//
//############################################################################

void XY::draw_graph(void)
{
   S32 i;

   //
   // Render the display array
   //

   S32 y,y1,y2;

   SINGLE y_factor = SINGLE(display_height - 1) / (bottom_val - top_val);

   switch (style)
      {
      case XY_DOTS:

         for (i=0; i < display_width; i++)
            {
            if (display_array[i] == BLANK_VALUE) continue;

            y = round_to_nearest(((display_array[i] - top_val) * y_factor));

            VFX_pixel_write(graph, i, y, graph_color);
            }
         break;

      case XY_BARS:

         for (i=0; i < display_width; i++)
            {
            if (display_array[i] == BLANK_VALUE) continue;

            y = round_to_nearest(((display_array[i] - top_val) * y_factor));

            VFX_line_draw(graph, 
                        i, 
                        display_height-1, 
                        i,
                        y,
                        LD_DRAW,
                        graph_color);
            }
         break;

      case XY_CONNECTED_LINES:

         for (i=0; i < display_width-1; i++)
            {
            if (display_array[i]   == BLANK_VALUE) continue;
            if (display_array[i+1] == BLANK_VALUE) continue;

            y1 = round_to_nearest(((display_array[i]   - top_val) * y_factor));
            y2 = round_to_nearest(((display_array[i+1] - top_val) * y_factor));

            VFX_line_draw(graph, 
                        i, 
                        y1,
                        i,
                        y2,
                        LD_DRAW,
                        graph_color);
            }
         break;
      }

   //
   // Draw running-accumulation overlay if not in zoom mode
   //

   if (running_overlay && (!zoom_mode))
      {
      //
      // Show average accumulation
      //

      if (accum_op & XY_AVERAGE)
         {
         for (i=0; i < display_width; i++)
            {
            SINGLE r = sum_array[i] / SINGLE(n_cycles);

            y = round_to_nearest(((r - top_val) * y_factor));

            VFX_pixel_write(graph, i, y, running_overlay_color);
            }
         }

      if (accum_op & XY_MAXIMUM)
         {
         for (i=0; i < display_width; i++)
            {
            y = round_to_nearest(((max_array[i] - top_val) * y_factor));

            VFX_pixel_write(graph, i, y, running_overlay_color);
            }
         }

      if (accum_op & XY_MINIMUM)
         {
         for (i=0; i < display_width; i++)
            {
            y = round_to_nearest(((min_array[i] - top_val) * y_factor));

            VFX_pixel_write(graph, i, y, running_overlay_color);
            }
         }

      }

   //
   // Show max-hold status
   //

   if (max_hold)
      {
      transparent_font_CLUT[1] = (U16) RGB_NATIVE(255,255,0);

      VFX_string_draw(graph,
                      8,
                      8,
                      VFX_default_system_font(),
                      "Max Hold",
                      transparent_font_CLUT);
      }
}

//############################################################################
//
// ..._cursor_val()
//
//############################################################################

SINGLE XY::top_cursor_val(void)
{
   return Y_array[thresh_y0];
}

SINGLE XY::bottom_cursor_val(void)
{
   return Y_array[thresh_y1];
}

DOUBLE XY::left_cursor_val(void)
{
   return X_array[zoom_x0];
}

DOUBLE XY::right_cursor_val(void)
{
   return X_array[zoom_x1];
}

//############################################################################
//
// update_cursors()
//
//############################################################################

void XY::update_cursors(S32 mouse_x, //)
                        S32 mouse_y, 
                        S32 button_state)
{
   //
   // Normalize (x,y) to graph display pane
   //

   mouse_x -= graph->x0;
   mouse_y -= graph->y0;

   S32 out_of_pane = (mouse_x < 0)              ||
                     (mouse_x >= display_width) ||
                     (mouse_y < 0)              ||
                     (mouse_y >= display_height);

   //
   // Exit if mouse cursor outside graph pane with button up
   // 

   if (out_of_pane && (!button_state))
      {
      if (active_cursor != -1)
         {
         active_cursor = -1;
         draw_cursors();
         }

      return;
      }

   S32 dl = abs(mouse_x - zoom_x0);
   S32 dr = abs(mouse_x - zoom_x1);
   S32 dt = abs(mouse_y - thresh_y0);
   S32 db = abs(mouse_y - thresh_y1);

   S32 prev_cursor = active_cursor;

   if (!button_state)
      {
      active_cursor = -1;

      if ((dl < 8) && (dl <= dr) && (dl <= dt) && (dl <= db) /* && (!zoom_mode) */)
         {
         if (enable_zoom_cursors)
            active_cursor = CUR_L;
         }

      if ((dr < 8) && (dr <= dl) && (dr <= dt) && (dr <= db) /* && (!zoom_mode) */)
         {
         if (enable_zoom_cursors)
            active_cursor = CUR_R;
         }

      if ((dt < 8) && ((dt <= dr) || zoom_mode) && ((dt <= dl) /* || zoom_mode */) && (dt <= db))
         {
         active_cursor = CUR_T;
         }

      if ((db < 8) && ((db <= dl) || zoom_mode) && ((db <= dr) /* || zoom_mode */) && (db <= dt))
         {
         active_cursor = CUR_B;
         }
      }

   S32 refresh_cursors = 0;
   S32 refresh_graph   = 0;

   if (active_cursor != prev_cursor)
      {
      refresh_cursors = 1;
      }

   //
   // If cursor is being dragged, make it follow the mouse
   // 

   if ((active_cursor != -1) && (button_state))
      {
      switch (active_cursor)
         {
         case CUR_L: zoom_x0   = mouse_x; break;
         case CUR_R: zoom_x1   = mouse_x; break;
         case CUR_T: thresh_y0 = mouse_y; break;
         case CUR_B: thresh_y1 = mouse_y; break;
         }

      refresh_graph = 1;
      }

   //
   // Handle necessary refresh operations
   //

   if (refresh_graph)
      {
      //
      // Active cursor has moved -- recalculate cursor values and redraw
      // graph
      //
      // First, clamp cursor bounds
      //

      if (zoom_x0 >= zoom_x1) 
         { 
         if ((zoom_x1 == display_width-1) || (active_cursor == CUR_R)) 
            {
            zoom_x0 = zoom_x1-1; 
            }
         else
            {
            ++zoom_x1; 
            }
         }

#if 0    // allow to cross for Y exclusion
      if (thresh_y0 >= thresh_y1) 
         { 
         if ((thresh_y1 == display_height-1) || (active_cursor == CUR_B)) 
            {
            thresh_y0 = thresh_y1-1; 
            }
         else
            {
            ++thresh_y1; 
            }
         }
#endif

      if (zoom_x0 < 0)                  zoom_x0    = 0;
      if (zoom_x0 > display_width-2)    zoom_x0    = display_width-2;

      if (thresh_y0 < 0)                thresh_y0  = 0;
      if (thresh_y0 > display_height-1) thresh_y0  = display_height-1;

      if (zoom_x1 <= zoom_x0)           zoom_x1    = zoom_x0+1;
      if (zoom_x1 < 1)                  zoom_x1    = 1;
      if (zoom_x1 > display_width-1)    zoom_x1    = display_width-1;

#if 0    // allow to cross for Y exclusion
      if (thresh_y1 <= thresh_y0)       thresh_y1  = thresh_y0+1;
#endif
      if (thresh_y1 < 0)                thresh_y1  = 0;
      if (thresh_y1 > display_height-1) thresh_y1  = display_height-1;

      draw_graph();

      refresh_cursors = 1;
      }

   if (refresh_cursors)
      {
      //
      // Active cursor has moved or changed -- refresh all cursors
      //

      draw_cursors();
      }
}

//############################################################################
//
// draw_cursors()
//
//############################################################################

//
// Draw broken cursor line
//

static U32   solid_cursor_line_color;
static U32   broken_cursor_line_color;
static PANE *broken_cursor_line_pane;

void __cdecl horiz_cursor_shader(S32 x, S32 y)
{
   if (x & 8)
      {
      VFX_pixel_write(broken_cursor_line_pane,
                      x,
                      y,
                      broken_cursor_line_color);
      }
}

void __cdecl vert_cursor_shader(S32 x, S32 y)
{
   if (y & 8)
      {
      VFX_pixel_write(broken_cursor_line_pane,
                      x,
                      y,
                      broken_cursor_line_color);
      }
}


void XY::draw_cursors(void)
{
   //
   // Calculate "bright" version of cursor color, to indicate active
   // cursor (the one nearest the mouse pointer)
   //

#if 0

   VFX_RGB RGB = *VFX_color_to_RGB(cursor_color);

   RGB.r = (RGB.r > 127) ? 255 : (RGB.r * 2);
   RGB.g = (RGB.g > 127) ? 255 : (RGB.g * 2);
   RGB.b = (RGB.b > 127) ? 255 : (RGB.b * 2);

   U32 bright = RGB_TRIPLET(RGB.r, RGB.g, RGB.b);

#else

   U32 bright = RGB_TRIPLET(255,255,255);

#endif

   //
   // Draw top threshold cursor
   //

   solid_cursor_line_color = (active_cursor == CUR_T) ? bright : cursor_color; 

   VFX_line_draw(graph,
                 0,
                 thresh_y0,
                 display_width-1,
                 thresh_y0,
                 LD_DRAW,
                 solid_cursor_line_color);

   //
   // Draw bottom threshold cursor as dashed line
   //

   broken_cursor_line_color = (active_cursor == CUR_B) ? bright : cursor_color;
   broken_cursor_line_pane  = graph;

   VFX_line_draw(graph,
                 0,
                 thresh_y1,
                 display_width-1,
                 thresh_y1,
                 LD_EXECUTE,
           (U32) horiz_cursor_shader);

#if 0
   if (!zoom_mode)
#endif
   if (enable_zoom_cursors)
      {
      //
      // Draw left zoom cursor
      //

      VFX_line_draw(graph,
                    zoom_x0,
                    0,
                    zoom_x0,
                    display_height-1,
                    LD_DRAW,
                    (active_cursor == CUR_L) ? bright : cursor_color);

      //
      // Draw right zoom cursor as dashed line
      //

      broken_cursor_line_color = (active_cursor == CUR_R) ? bright : cursor_color;
      broken_cursor_line_pane  = graph;

      VFX_line_draw(graph,
                    zoom_x1,
                    0,
                    zoom_x1,
                    display_height-1,
                    LD_EXECUTE,
              (U32) vert_cursor_shader);
      }

   //
   // Print top/bottom Y cursor values
   //

   C8 *s;
   S32 l;

   S32 outer_right = graph->x1 - outer->x0;

   S32 fh = VFX_font_height(VFX_default_system_font());

   S32 y0 = thresh_y0 - (fh / 2);
   S32 y1 = thresh_y1 - (fh / 2);

   y0 += (graph->y0 - outer->y0);
   y1 += (graph->y0 - outer->y0);

   //
   // Keep cursor labels from overwriting each other...
   //

   while (abs(y1-y0) < fh)
      {
      if (y1 > y0)
         {
         y0--;
         y1++;
         }
      else
         {
         y0++;
         y1--;
         }
      }

   s = float_to_string(top_cursor_val(),
                       Y_format);

   l = string_width(s, 
                    VFX_default_system_font());

   VFX_RGB color = *VFX_color_to_RGB(solid_cursor_line_color);
   transparent_font_CLUT[1] = (U16) RGB_NATIVE(color.r, color.g, color.b);

   VFX_string_draw(outer,
                   outer_right + 2,
                   y0,
                   VFX_default_system_font(),
                   s,
                   transparent_font_CLUT);

   s = float_to_string(bottom_cursor_val(), 
                       Y_format);

   l = string_width(s, 
                    VFX_default_system_font());

   color = *VFX_color_to_RGB(broken_cursor_line_color);
   transparent_font_CLUT[1] = (U16) RGB_NATIVE(color.r, color.g, color.b);

   VFX_string_draw(outer,
                   outer_right + 2,
                   y1 + 1,
                   VFX_default_system_font(),
                   s,
                   transparent_font_CLUT);
}

//############################################################################
//
// enable_running_overlay()
//
//############################################################################

void XY::enable_running_overlay(S32 enable)
{
   running_overlay = enable;
}

//############################################################################
//
// set_zoom_mode()
//
//############################################################################

void XY::set_zoom_mode(S32 zoom_enable, S32 enable_cursors)
{
   zoom_mode           = zoom_enable;
   enable_zoom_cursors = enable_cursors;
}

//############################################################################
//
// draw_scale()
//
//############################################################################

static PANE *RS_outer;
static PANE *RS_inner;
static S32   RS_scale_color;
static S32   RS_major_graticule_color;
static S32   RS_minor_graticule_color;
static S32   RS_pixel_count;
static S32   RS_is_major;

static void __cdecl scale_shader(S32 x, S32 y)
{
   S32 color = RS_scale_color;

   S32 sx = RS_outer->x0 + x + 2;
   S32 sy = RS_outer->y0 + y - 2;

   static S32 last_sx = -1;
   static S32 last_sy = -1;

   if ((sy < RS_inner->y1) && 
       (sx > RS_inner->x0))
      {
      if (RS_is_major)
         {
         if ((sx & 1) && (last_sy == sy))
            {
            last_sx = sx;
            return;
            }

         if ((sy & 1) && (last_sx == sx))
            {
            last_sy = sy;
            return;
            }

         color = RS_major_graticule_color;
         }
      else
         {
         if ((sx & 1) && (last_sy == sy))
            {
            last_sx = sx;
            return;
            }

         if ((sy & 1) && (last_sx == sx))
            {
            last_sy = sy;
            return;
            }

         color = RS_minor_graticule_color;
         }
      }

   VFX_pixel_write(RS_outer, x, y, color);

   last_sx = sx;
   last_sy = sy;
}

void XY::draw_scale(S32   grat_x,  //)
                    S32   grat_y)
{
   C8 *s;
   S32 i,l;

   //
   // Height of minor tic marks = 1/60 the inner pane's window height
   // Height of major tic marks = minor height * 2
   //

   S32 minor_X_height = graph->window->y_max / 60;

   if (minor_X_height < 2)
      {
      minor_X_height = 2;
      }

   S32 major_X_height = minor_X_height * 2;

   //
   // Width of minor tic marks = 1/120 the inner pane's window width
   // Width of major tic marks = minor width * 2
   //

   S32 minor_Y_width = graph->window->x_max / 120;

   if (minor_Y_width < 2)
      {
      minor_Y_width = 2;
      }

   S32 major_Y_width = minor_Y_width * 2;

   //
   // Translate graph left/right edges into outer pane space
   //
   // (We assume the graph pane is within the outer pane, and the two 
   // panes share a common window buffer)
   //

   S32 graph_height = graph->y1 - graph->y0 + 1;

   S32 outer_left   = graph->x0 - outer->x0;
   S32 outer_right  = graph->x1 - outer->x0;
   S32 outer_top    = graph->y0 - outer->y0;
   S32 outer_bottom = graph->y1 - outer->y0;

   S32 tic_top      = outer_top + display_height + 2;
   S32 tic_right    = outer_left - 2;

   S32 minor_X_bottom = tic_top + minor_X_height;
   S32 major_X_bottom = tic_top + major_X_height;

   S32 minor_Y_left = tic_right - minor_Y_width;
   S32 major_Y_left = tic_right - major_Y_width;

   if (grat_x)
      {
      tic_top = outer_top;
      }

   if (grat_y)
      {
      tic_right = outer_right;
      }

   //
   // Set up parameters used by "pixel shader" function
   //

   RS_outer = outer;
   RS_inner = graph;
   RS_scale_color = scale_color;
   RS_major_graticule_color = major_graticule_color;
   RS_minor_graticule_color = minor_graticule_color;
   RS_is_major = FALSE;

   //
   // Draw X and Y baselines
   //

   VFX_line_draw(outer,
                 outer_left,
                 outer_bottom+2,
                 outer_right,
                 outer_bottom+2,
                 LD_EXECUTE,
           (S32) scale_shader);

   VFX_line_draw(outer,
                 outer_left-2,
                 outer_top,
                 outer_left-2,
                 outer_bottom,
                 LD_EXECUTE,
           (S32) scale_shader);

   //
   // Starting at left edge of graph, draw series of minor tic marks
   //

   DOUBLE w  = right_val - left_val;
   DOUBLE dx = w / (DOUBLE) (display_width-1);

   i = 0;

   while (1)
      {
      DOUBLE v = X_minor_tic_increment * (DOUBLE) i++;

      if ((left_val + v) >= right_val)
         {
         break;
         }

      S32 x = round_to_nearest(v / dx) + outer_left;

      VFX_line_draw(outer,
                    x,
                    tic_top,
                    x,
                    minor_X_bottom,
                    LD_EXECUTE,
              (S32) scale_shader);
      }

   //
   // Starting at top edge of graph, draw series of minor tic marks
   //

   SINGLE h  = bottom_val - top_val;
   SINGLE dy = h / (SINGLE) (display_height-1);

   i = 0;

   while (1)
      {
      SINGLE v = -Y_minor_tic_increment * (SINGLE) i++;

      if ((top_val + v) <= bottom_val)
         {
         break;
         }

      S32 y = round_to_nearest(v / dy) + outer_top;

      VFX_line_draw(outer,
                    minor_Y_left,
                    y,
                    tic_right,
                    y,
                    LD_EXECUTE,
              (S32) scale_shader);
      }

   //
   // Starting at left edge of graph, draw series of major tic marks and 
   // labels
   //

   RS_is_major = TRUE;

   i = 0;

   while (1)
      {
      DOUBLE v = X_major_tic_increment * (DOUBLE) i++;

      if ((left_val + v) >= right_val)
         {
         break;
         }

      S32 x = round_to_nearest(v / dx) + outer_left;

      VFX_line_draw(outer,
                    x,
                    tic_top,
                    x,
                    major_X_bottom,
                    LD_EXECUTE,
              (S32) scale_shader);

      s = float_to_string((DOUBLE) (left_val + v), 
                          X_format);

      l = string_width(s, 
                       VFX_default_system_font());

      VFX_string_draw(outer,
                      x - (l / 2),
                      major_X_bottom + 2,
                      VFX_default_system_font(),
                      s,
                      font_palette);
      }

   VFX_line_draw(outer,
                 outer_right,
                 tic_top,
                 outer_right,
                 major_X_bottom,
                 LD_EXECUTE,
           (S32) scale_shader);

   s = float_to_string(right_val, 
                       X_format);

   l = string_width(s, 
                    VFX_default_system_font());

   VFX_string_draw(outer,
                   outer_right - (l / 2),
                   major_X_bottom + 2,
                   VFX_default_system_font(),
                   s,
                   font_palette);

   //
   // Starting at top edge of graph, draw series of major tic marks and labels
   //
   // The labels will collapse as the graph shrinks vertically, to keep the 
   // scale readable
   //

   RS_is_major = TRUE;

   S32 fh = VFX_font_height(VFX_default_system_font());
   S32 ty = 0, last_ty = INT_MIN;

   i = 0;

   while (1)
      {
      SINGLE v = -Y_major_tic_increment * (SINGLE) i++;

      if ((top_val + v) <= bottom_val)
         {
         break;
         }

      S32 y = round_to_nearest(v / dy) + outer_top;

      VFX_line_draw(outer,
                    major_Y_left,
                    y,
                    tic_right,
                    y,
                    LD_EXECUTE,
              (S32) scale_shader);

      ty = y - (fh / 2);

      if ((ty >= last_ty + (fh + 4)) || (last_ty == INT_MIN))
         {
         s = float_to_string((DOUBLE) (top_val + v), 
                             Y_format);
      
         l = string_width(s, 
                          VFX_default_system_font());

         VFX_string_draw(outer,
                        (major_Y_left - l) - 2,
                         ty,
                         VFX_default_system_font(),
                         s,
                         font_palette);
         last_ty = ty;
         }
      }

   VFX_line_draw(outer,
                 major_Y_left,
                 outer_bottom,
                 tic_right,
                 outer_bottom,
                 LD_EXECUTE,
           (S32) scale_shader);

   ty = outer_bottom - (fh / 2);

   if ((ty >= last_ty + (fh + 4)) || (last_ty == INT_MIN))
      {
      s = float_to_string(bottom_val, 
                          Y_format);

      l = string_width(s, 
                       VFX_default_system_font());

      VFX_string_draw(outer,
                     (major_Y_left - l) - 2,
                      ty,
                      VFX_default_system_font(),
                      s,
                      font_palette);
      }
}

//############################################################################
//
// draw_band_markers()
//
//############################################################################

void XY::draw_band_markers(BAND *band)
{
   S32 l;

   //
   // Translate graph left/right edges into outer pane space
   //
   // (We assume the graph pane is within the outer pane, and the two 
   // panes share a common window buffer)
   //

   S32 graph_height = graph->y1 - graph->y0 + 1;

   DOUBLE w  = right_val - left_val;
   DOUBLE dx = w / (DOUBLE) (display_width-1);

   for (S32 c=0; c < band->n_channels; c++)
      {
      CHANNEL *C = &band->channels[c];

      DOUBLE LV = C->center_MHz - C->plus_or_minus_MHz;
      DOUBLE RV = C->center_MHz + C->plus_or_minus_MHz;

      if ((LV > right_val) || (RV < left_val))
         {
         continue;
         }

      S32 x     = round_to_nearest(((C->center_MHz - left_val) / dx));
      S32 left  = round_to_nearest(((LV            - left_val) / dx));
      S32 right = round_to_nearest(((RV            - left_val) / dx));

      S32 i;
      for (i=band->label_height; i < graph_height; i += 8)
         {
         VFX_line_draw(graph,
                       left,
                       i,
                       left,
                       i + 4,
                       LD_DRAW,
                       RGB_TRIPLET(C->R, C->G, C->B));
         }

      for (i=band->label_height + 4; i < graph_height; i += 8)
         {
         VFX_line_draw(graph,
                       right,
                       i,
                       right,
                       i + 4,
                       LD_DRAW,
                       RGB_TRIPLET(C->R, C->G, C->B));
         }

      transparent_font_CLUT[1] = (U16) RGB_NATIVE(C->R, C->G, C->B);

      l = string_width(C->label, 
                       VFX_default_system_font());

      VFX_string_draw(graph,
                      x - (l / 2),
                      band->label_height + 4,
                      VFX_default_system_font(),
                      C->label,
                      transparent_font_CLUT);
      }
}

//############################################################################
//
// get_graph_data()
//
//############################################################################

SINGLE *XY::get_graph_data(XY_DATAREQ stage)
{
   switch (stage)
      {
      case XY_COMBINED_ARRAY:  return combined_array;
      case XY_THRESHOLD_ARRAY: return threshold_array;
      case XY_DISPLAYED_ARRAY: return display_array;
      case XY_MIN_ARRAY:       return min_array;
      case XY_MAX_ARRAY:       return max_array;
      }

   return NULL;
}

//############################################################################
//
// query_graph()
//
//############################################################################

SINGLE XY::query_graph(S32        x, //)
                       S32        y,
                       XY_DATAREQ stage,
                       SINGLE    *vertical_value,
                       SINGLE    *vertical_scale_value,
                       DOUBLE    *horizontal_scale_value)
{
   //
   // Normalize (x,y) to graph display pane
   //

   x -= graph->x0;
   y -= graph->y0;

   //
   // Exit if x is to left or right of X/Y display
   //

   if ((x < 0) || 
       (x >= display_width))
      {
      return 0;
      }

   //
   // Determine horizontal (typically "frequency") value at x
   //

   if (horizontal_scale_value != NULL)
      {
      *horizontal_scale_value = X_array[x];
      }

   //
   // Exit if y is above or below X/Y dislpay
   //

   if ((y < 0) || 
       (y >= display_height))
      {
      return 0;
      }

   //
   // Determine graph-scale amplitude at Y
   //
    
   if (vertical_scale_value != NULL)
      {
      *vertical_scale_value = Y_array[y];
      }

   //
   // Return selected array entry
   //

   if (vertical_value != NULL)
      {
      switch (stage)
         {
         case XY_COMBINED_ARRAY:  *vertical_value = combined_array  [x]; break;
         case XY_THRESHOLD_ARRAY: *vertical_value = threshold_array [x]; break;
         case XY_DISPLAYED_ARRAY: *vertical_value = display_array   [x]; break;
         case XY_MIN_ARRAY:       *vertical_value = min_array       [x]; break;
         case XY_MAX_ARRAY:       *vertical_value = max_array       [x]; break;
         }                                               
      }

   return 1;
}
