//
// Include after winvfx.h and sal.h
//

#ifndef XY_H
#define XY_H

//
// Reserved amplitude value to represent out-of-threshold value
//

#define BLANK_VALUE (-999999.0F)

//
// X/Y graph style
//

enum XY_GRAPHSTYLE
{
   XY_DOTS,            // Discrete dots (if oversampling) or horizontal lines (if undersampling)
   XY_BARS,            // Vertical bars 
   XY_CONNECTED_LINES  // Continuous connected lines (oscilloscope style)
};

//
// Channel entry for optional band display
//

struct CHANNEL
{
   DOUBLE center_MHz;
   DOUBLE plus_or_minus_MHz;
   C8     label[64];
   S32    R;
   S32    G;
   S32    B;
};

struct BAND
{
   CHANNEL *channels;
   S32      n_channels;
   S32      label_height;
};

//
// Statistical operation codes for X/Y accumulation and multipoint combining
//
// For pixel combining, only one of these codes should be used
// For accumulation, any combination of AVERAGE/MINIMUM/MAXIMUM may be used together
//

#define XY_AVERAGE  1   // Display / accumulate average value of points in column
#define XY_MINIMUM  2   // Display / accumulate minimum value of points in column
#define XY_MAXIMUM  4   // Display / accumulate maximum value of points in column

#define XY_PTSAMPLE 8   // Downsample using point-sampling
#define XY_SPLINE   16  // Downsample using cubic resampler

//
// X/Y output data request types
//

enum XY_DATAREQ        
{
   XY_COMBINED_ARRAY,  // Data has been combined but not yet subjected to threshold or max-hold constraints
   XY_MIN_ARRAY,       // Minimum combined array values encountered so far
   XY_MAX_ARRAY,       // Maximum combined array values encountered so far
   XY_THRESHOLD_ARRAY, // Combined data outside threshold bounds is set to BLANK_VALUE, but max-hold has not been applied
   XY_DISPLAYED_ARRAY  // Combined data has been subjected to all processing, including thresholds and max-hold
};

//
// XY.H
//
// X-Y real-time display class
//

class XY
{
   PANE *graph;
   PANE *outer;
   S32   display_width;
   S32   display_height;
   S32   n_points;
   S32   ratio;

   S32           merge_op;
   S32           accum_op;
   XY_GRAPHSTYLE style;
   S32           max_hold;
   S32           background_color;
   S32           graph_color;
   S32           scale_color;
   S32           major_graticule_color;
   S32           minor_graticule_color;
   S32           cursor_color;
   S32           running_overlay_color;
   S32           running_overlay;
   void         *font_palette;

   SINGLE top_val;
   SINGLE bottom_val;
   C8     Y_format[256];
   SINGLE Y_major_tic_increment;
   SINGLE Y_minor_tic_increment;

   DOUBLE left_val;
   DOUBLE right_val;
   C8     X_format[256];
   DOUBLE X_major_tic_increment;
   DOUBLE X_minor_tic_increment;

   SINGLE *input_array;      // [n_points]
   SINGLE *combined_array;   // [display_width]
   SINGLE *threshold_array;  // [display_width]
   SINGLE *display_array;    // [display_width]

   SINGLE *sum_array;        // [display_width]
   SINGLE *min_array;        // [display_width]
   SINGLE *max_array;        // [display_width]
   S32     n_cycles;         // # of cycles recorded, for average calculation

   DOUBLE *X_array;          // Turns pane X into frequency, with integral values at tick locations
   SINGLE *Y_array;          // Turns pane Y into amplitude, with integral values at tick locations

   S32 active_cursor;
   S32 zoom_mode;
   S32 enable_zoom_cursors;

   S32 thresh_y0;
   S32 thresh_y1;
   S32 zoom_x0;
   S32 zoom_x1;

   S32 include(SINGLE t);

public:

   //
   // Constructor accepts pointer to destination panes
   //
   // Two panes are specified -- the graph pane defines the boundaries 
   // of the rendered X/Y graph itself, while the outer pane is used
   // to render the tick marks, scale values, and other peripheral 
   // information.
   //
   // By default, # of input pixels is set to width of graph pane
   //

   XY(PANE *graph_pane, PANE *outer_pane);

  ~XY();

   //
   // Reset values in running min/max/avg arrays
   //

   void reset_running_values(void);

   //
   // Clear max-hold state
   //

   void clear_max_hold(void);

   //
   // Set # of points in X dimension
   //
   // Returns 0 (failure) if width of destination pane is not evenly divisible
   // by # of points
   //

   S32 set_input_width(S32 n_points);

   //
   // Set Y value range for vertical scale
   //
   // Range will be applied over graph pane height
   // 
   // Labels will be placed at major tic intervals 
   //

   void set_Y_range(SINGLE top_val, 
                    SINGLE bottom_val, 
                    C8    *format,
                    SINGLE major_tic_increment,
                    SINGLE minor_tic_increment);

   void set_Y_cursors(SINGLE top_cursor_val, 
                      SINGLE bottom_cursor_val);

   //
   // Set X value range for horizontal scale
   // 
   // Range will be applied over graph pane width
   //
   // Labels will be placed at major tic intervals 
   //

   void set_X_range(DOUBLE left_val, 
                    DOUBLE right_val, 
                    C8    *format,
                    DOUBLE major_tic_increment,
                    DOUBLE minor_tic_increment);

   void set_X_cursors(DOUBLE left_cursor_val, 
                      DOUBLE right_cursor_val);
   
   //
   // Set points in graph
   //

   void set_single_point  (SINGLE val, 
                           S32    X);

   void set_arrayed_points(SINGLE *val, 
                           S32     first_X = 0, 
                           S32     last_X  = -1, 
                           S32    *explicit_X = NULL);

   void set_arrayed_points(DOUBLE *val, 
                           S32     first_X = 0, 
                           S32     last_X  = -1, 
                           S32    *explicit_X = NULL);

   //
   // Set graph attributes
   //

   void set_graph_attributes(XY_GRAPHSTYLE style                 = XY_CONNECTED_LINES,
                             S32           merge_op              = XY_AVERAGE,
                             S32           accum_op              = XY_AVERAGE,
                             S32           max_hold              = 0,
                             S32           background_color      = RGB_TRIPLET(6,0,90),
                             S32           graph_color           = RGB_TRIPLET(0,255,0),
                             S32           scale_color           = RGB_TRIPLET(255,255,255),
                             S32           major_graticule_color = RGB_TRIPLET(16,110,130),
                             S32           minor_graticule_color = RGB_TRIPLET(10,80,100),
                             S32           cursor_color          = RGB_TRIPLET(192,0,192),
                             S32           running_overlay_color = RGB_TRIPLET(255,255,0),
                             void         *font_palette          = NULL);

   //
   // Draw graph background
   //

   void draw_background(void);

   //
   // Process input data into various XY_DATAREQ arrays
   //
   // We separate the processing/combining phase from the rendering pass 
   // to make it easier to process file data for the rising-raster display
   // (otherwise we must render the X/Y view numerous times when preparing 
   // the initial rising-raster page view after a file is loaded, and when
   // scrolling through a saved file faster than one row)
   //

   void process_input_data(void);

   //
   // Draw graph
   //

   void draw_graph(void);

   //
   // Draw threshold and zoom cursors atop graph
   //

   void draw_cursors(void);

   //
   // Return values at cursors
   //

   SINGLE top_cursor_val(void);
   SINGLE bottom_cursor_val(void);
   DOUBLE left_cursor_val(void);
   DOUBLE right_cursor_val(void);

   //
   // Enable/disable running overlay
   //

   void enable_running_overlay(S32 enable);

   //
   // Enable/disable zoom mode and cursors
   //

   void set_zoom_mode(S32 zoom_enable, 
                      S32 enable_cursors);

   //
   // Update cursor positions and highlighting in response to mouse input
   //

   void update_cursors(S32 mouse_x, 
                       S32 mouse_y, 
                       S32 button_state);

   //
   // Draw scale (tick marks), labels, and optional graticule (in scale color)
   //

   void draw_scale(S32   graticule_x = 0,
                   S32   graticule_y = 0);

   //
   // Draw band boundaries
   //

   void draw_band_markers(BAND *band);

   //
   // Grant caller access to internal graph data
   //
   // This array contains the values plotted to the X/Y graph, subject to
   // min/max/avg merging, max-hold, thresholds, and other constraints based
   // on the specified XY_DATAREQ type.  It can be used to feed the 
   // rising-raster display, among other things.
   //
   // The size of the array is equal to the width of the graph_pane as 
   // supplied to the XY class constructor.
   //

   SINGLE *get_graph_data(XY_DATAREQ stage);

   //
   // Get single data point from an internal graph, based on (x,y) 
   // coordinate in screen space
   //
   // Used for mouse-pointer queries
   //
   // Also returns the interpolated horizontal scale value corresponding
   // to the specified x-coordinate, even if the y-coordinate is not 
   // within the X/Y graph pane (useful when querying the rising-raster 
   // graph)
   //
   // Returns 0 if (x,y) is not within X/Y graph pane, 1 otherwise
   //

   SINGLE query_graph(S32        x,
                      S32        y,
                      XY_DATAREQ stage,
                      SINGLE    *vertical_value,
                      SINGLE    *vertical_scale_value,
                      DOUBLE    *horizontal_scale_value);
};

#endif
