//
// Include after winvfx.h and sal.h
//

#ifndef WATERFALL_H
#define WATERFALL_H

#define WF_SPLINE_SMOOTHING 0

//
// ROW_TAG structure for internal use
//

struct ROW_TAG
{
   S32      row;
   SINGLE  *data;

   DOUBLE   latitude;
   DOUBLE   longitude;
   DOUBLE   altitude_m;

   DOUBLE   start_Hz;
   DOUBLE   stop_Hz;

   C8       caption[128];

   time_t   time;
   S32      in_use;
};

//
// WATERFALL.H
//
// Rising-raster real-time display class
//

class WATERFALL
{
   PANE *graph;
   PANE *outer;
   S32   display_width;
   S32   display_height;
   S32   cell_height;
   S32   blank_color;
   S32   background_color;
   S32   scale_color;

   S32  *step_CLUT;
   S32  *smooth_CLUT;
   S32   color_base;
   S32   color_limit;
   U16   transparent_font_CLUT[256];

   S32     *value_array;
   S32     *color_array;
   S32      n_entries;

   S32     visible_rows;
   S32     visible_y0;
   time_t *time_array;

#if WF_SPLINE_SMOOTHING
   SINGLE *smooth_R;
   SINGLE *smooth_G;
   SINGLE *smooth_B;
   SINGLE *R_color_array;
   SINGLE *G_color_array;
   SINGLE *B_color_array;
#endif

   ROW_TAG *row_list; 
   S32      rows_in_list;
   S32      row_record_valid;

public:
   
   //
   // Constructor accepts pointer to destination panes
   //
   // Two panes are specified -- the graph pane defines the boundaries 
   // of the raster graph itself, while the outer pane is used
   // to render the time stamps, color key, and other peripheral 
   // information.
   //

   WATERFALL(PANE *graph_pane, PANE *outer_pane);

  ~WATERFALL();

   //
   // Set color/value range and cell height
   //
   // Each value entry is the threshold above which its corresponding
   // color will be plotted.  The overrange, underrange, and "blank" colors
   // are used when the values exceed or fall below the overrange/underrange
   // values, or lie outside the amplitude-plot limits, respectively.
   //
   // The absolute minimum and maximum out-of-range values must be supplied
   // as well, to aid in construction of an internal data structure used
   // for fast value-to-color translation.  Passing values outside of this
   // range may result in a crash.
   //
   // The cell height specifies the number of scanlines occupied by each
   // new data set, and the number of scanlines the graph is to be scrolled
   // by scroll_graph().
   //
   // Must be called before ANY other WATERFALL methods
   //

   void set_graph_attributes(S32      cell_height,
                             SINGLE  *value_array,
                             VFX_RGB *color_array,  
                             S32      n_entries,
                             SINGLE   underrange_limit,
                             SINGLE   overrange_limit,
                             S32      underrange_color,
                             S32      overrange_color,
                             S32      blank_color,
                             S32      scale_color = RGB_TRIPLET(255,255,255));

   //
   // Set colors only, without invalidating other attributes
   // Can be called only after set_graph_attributes()
   //

   void set_graph_colors    (VFX_RGB *color_array,   
                             S32      underrange_color,
                             S32      overrange_color,
                             S32      blank_color,
                             S32      scale_color = RGB_TRIPLET(255,255,255));

   //
   // Wipe contents of graph and time scale
   //

   void wipe_graph(S32 background_color);

   //
   // Return # of visible rows in graph
   //
   // This is equivalent to the graph pane height divided by the # of 
   // visible cells
   //

   S32 n_visible_rows(void);

   //
   // Query value being displayed at x,y (in screen/mouse-cursor space)
   //
   // Returns 0 if point not within raster pane -- otherwise,
   // returns a non-zero value with valid *acquisition_time and *value
   //
   // *acquisition_time   = acquisition time of row under Y
   // *value              = amplitude of spot under X,Y
   // *horizontal_percent = X as percentage of horizontal display width (0-100%)
   // Etc...
   //
   // Any or all of the output pointers may be set to NULL if their value(s)
   // are not desired
   //

   S32 query_graph(S32      x,
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
                   S32      caption_bytes);

   //
   // Scroll graph up or down n lines ("line" = cell height specified in last
   // call to set_graph_attributes())
   //
   // Positive scroll offset = downward scrolling
   // Negative scroll offset = upward scrolling
   //

   void scroll_graph(S32 scroll_offset);

   //
   // Render specified data into specified row of graph pane
   // (Alternate version which receives 24.8 fixed-point input format used internally)
   //
   // Row may be -1 to indicate bottom of graph pane
   //

   void draw_fixed_point_graph_row(time_t acquisition_time,
                                   S32   *data,
                                   S32    row,
                                   BOOL   smooth_mode,
                                   DOUBLE latitude,
                                   DOUBLE longitude,
                                   DOUBLE altitude_m,
                                   DOUBLE start_Hz,
                                   DOUBLE stop_Hz);

   //
   // Render specified single-precision float data into specified row of graph pane
   //
   // Row may be -1 to indicate bottom of graph pane
   //

   void draw_graph_row(time_t  acquisition_time,
                       SINGLE *data,
                       S32     row,
                       BOOL    smooth_mode,
                       DOUBLE  latitude,
                       DOUBLE  longitude,
                       DOUBLE  altitude_m,
                       DOUBLE  start_Hz,
                       DOUBLE  stop_Hz,
                       C8     *caption);

   //
   // Draw scale and labels
   //
   // Returns -1 if specified hit point is within scale index n
   //

   S32 draw_scale(S32 hit_x, S32 hit_y);
};

#endif
