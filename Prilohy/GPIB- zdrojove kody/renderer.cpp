//
//
//   HPGL and simple PCL file drawer by Mark S. Sims
//   (C) Copyright 2008, 2013 by Mark S. Sims - all rights reserved.
//   Permission granted for free non-commercial use.
//
//

#define DEG_TO_RAD (3.14159265358970/180.0)

#define SCREEN_WIDTH  RES_X
#define SCREEN_HEIGHT RES_Y


#define COLS (SCREEN_WIDTH-SCREEN_WIDTH/20)    // display window size
#define ROWS (SCREEN_HEIGHT-SCREEN_HEIGHT/20)

#define SCREEN_SIZE_X SCREEN_WIDTH    // full screen size
#define SCREEN_SIZE_Y SCREEN_HEIGHT

#define LEFT_MARGIN (RES_X/40)  // X_MARGIN //((SCREEN_SIZE_X-COLS)/2)
#define TOP_MARGIN  (RES_Y/24)  // (Y_MARGIN+10) // ((SCREEN_SIZE_Y-ROWS)/2)

#define COLOR u08

COLOR color;

u08 gl_state = 0;     // parse command decode state
char gl_cmd[3];       // two char HPGL command code
u08 gl_saw_comma;     // used to fix HP5372 syntax bug
u08 gl_alt_font;      // alternate font selected

u08 gl_ct_mode;       
#define DEFAULT_CHORD 1.0  // default chord angle (standard says 5 degrees)
double gl_chord_angle;// tttt

#define GL_ARGLEN 32
char gl_arg1[GL_ARGLEN+1];   // first arg for HPGL command
char gl_arg2[GL_ARGLEN+1];   // second arg for HPGL command
u08 gl_argptr = 0;           // used to built the arg string
int gl_pair;                 // used to identify which arg pair we are parsing

#define QUOTE_CMT  0x80  // skipping CO" comment
#define SAW_SLASH  0x40  // used to handle /* and */ nested comments
#define SAW_STAR   0x20
u08 gl_cmt = 0;          // used to skip over commented out text

u08 gl_pen = 1;
u08 gl_pendown = 0;      // up or down
u08 uc_pendown = 0;      // cccc user defined character pen state
u08 gl_moveabs = 1;      // absolute or relative coordinates

u08 gl_dvx = 0;          // if string direction 0=right 1=down 2=left 3=up
u08 gl_dvy = 0;          // if 1 draw reverse line feed direction
u08 gl_rotate_chars = 0; // text character orientation
u08 gl_rotate_coords=0;  // coordinate system rotatuon (0,90,180,270 degrees = 0,1,2,3)

#define PEN_COUNT 8
u08 gl_symbol = 0;       // symbol mode character
u08 gl_pw[PEN_COUNT];    // pen widths (in pixels,  0=1 pixel wide)
u08 gl_pwu;              // pen width units type (1=rel, 0=mm)
u08 gl_charset = 0;      // current character set
u08 gl_term = 0x03;      // label terminator char
u08 gl_term_type = 1;    // printing/non-printing terminator
u08 gl_td = 0;           // transparent label text (prints control chars)

u08 gl_lt;               // line pattern type code
u08 gl_lt_rel;           // relative or absolute pattern type
double gl_lt_len;        // line pattern length

double gl_newx, gl_newy; // current command argument values
u08 gl_rel_cmd = 0;      // flag set if current command uses relative coordinates

double gl_char_sx=0.75/100.0, gl_char_sy=1.5/100.0;  // character size
u08 gl_rel_char = 1;                                // relative/abs char size


double gl_x, gl_y;              // current pen posiotion
double gl_cr_x, gl_cr_y;        // carriage return point

double gl_textx, gl_texty;      // text char size
double gl_spacex, gl_spacey;    // text char spacing
double gl_extra_x, gl_extra_y;  // text char spacing adjust
double gl_text_ofsx, gl_text_ofsy;      // text char offset tweaks

double gl_lo_x, gl_lo_y;        // label offset command params
int gl_lo_code;                // label offset type code

#define GL_TAB_STOP 8
int gl_char_num;               // used to calculate tabs

double gl_pos_tick = 0.5/100.0; // axis tick mark sizes
double gl_neg_tick = 0.5/100.0;

#define ENCODED_POLYLINES      // define this to enable the PE command


// vector char size
double VX = 8.0;
double VY = 8.0;


//
//  Video stuff
//
//  Most (if not all) of the system dependent code is here
//

void lcd_clear()
{
// erase_screen();
// redraw_screen();
}


#define set_color(x) color = (x)
void gl_set_color(u08 pen);

void invert_colors() 
{
   if(color == 0) gl_set_color(gl_pen);
   else           set_color(0);
}


/*
u08 gl_pe_test[] = {
  'S', 'C', '0',',','1','0',  ',',  '0', '1','0',';' ,
  'P', 'E',
  0x3A, 191+6*2, //0xC5,
  0x3E, 191+7*2, //0xC6,
  0x3C,
  0x3D, 0x3F, 0xD3, 0x3F, 0xD3,
  0x53, 0xE9, 0xBF,
  0x54, 0xD5, 0x6B, 0xE9,
  0x40, 0xD3, 0x6C, 0xE9,
  ';', 
  'S', 'C', ';',
  'P', 'U', ';',
  0
};
*/

//
//
//  These routines mimic routines from the standard MegaDonkey LCD controller
//  library (see mega-donkey.com)


#define ROT_CHAR_90      0x01
#define ROT_CHAR_VERT    0x02
#define ROT_CHAR_HORIZ   0x04
#define ROT_STRING_VERT  0x08
#define ROT_STRING_HORIZ 0x10
#define ROT_CHAR   (ROT_CHAR_90 | ROT_CHAR_VERT | ROT_CHAR_HORIZ)
#define ROT_STRING (ROT_STRING_VERT | ROT_STRING_HORIZ)
u08 rotate;   /* chars: 0x01=90 deg   0x02=bottom-top  0x04=right-left */
              /* strings: 0x08=verticl  0x10=right->left or bot->top */

#define set_rotate(x) rotate = (x)




//
//
//  Patterned line support
//
//
#define MAX_PATTERN_LENGTH 20
u08 pattern_list[MAX_PATTERN_LENGTH];  // list of pen down/up pairs
u08 pattern_length;            // number of values in the pattern list
u08 line_pattern = 0;          // where we are in the list plus 1 (if 0, pattern disabled)
int pattern_count = 0;         // where we are in the current run
u08 temp_line_pattern;
COLOR initial_line_color;
COLOR temp_pen_color;

void enable_line_pattern()
{
   line_pattern = 1;  // start the line pattern from the beginning
   pattern_count = pattern_list[line_pattern-1];
   initial_line_color = color;
}

void disable_line_pattern()
{
   line_pattern = 0;
}

void pause_line_pattern()
{
   temp_line_pattern = line_pattern;   // save current line pattern position
   temp_pen_color = color;             // save current line color (hardware color)
   if(line_pattern) {
      line_pattern = 0;                   // turn off line pattern
      set_color(initial_line_color);      // restore original pen color
   }
}

void resume_line_pattern()
{
   if(temp_line_pattern) {
      line_pattern = temp_line_pattern;    // restore current line pattern
      set_color(temp_pen_color);           // restore line pattern color
   }
}

void step_line_pattern()
{
  if(pattern_count <= 0) {  // time for the next run in the line pattern
     if(++line_pattern > pattern_length) line_pattern = 1;
     pattern_count = pattern_list[line_pattern-1];  // get line pattern run length
     if(pattern_length) {   // this is not special pattern 0
        invert_colors();    // swap foreground and background colors
     }
  }
  --pattern_count;
}


// Xiaolin Wu's line antialiasing implementation via Mike Abrash
// www.codeproject.com/Articles/13360/Antialiasing-Wu-Algorithm

short NumLevels = 256;
unsigned short IntensityBits = 8;

void WuDot(int x, int y, unsigned short shade)     // shade 0-16 = max to min contrast against background
{
   //
   // Background 0=white, 1=black
   // Shade 0=pen color, 15=background color
   //
if(color == 0) return;

   u08 fcolor = color;

   if (background) 
      {
      if (fcolor == 0)   
         fcolor = background;
      else 
         if (fcolor == background) 
            fcolor = 0;
      }

   VFX_RGB c = *VFX_color_to_RGB(pen_colors[fcolor]);
     
   DOUBLE r = c.r;
   DOUBLE g = c.g;
   DOUBLE b = c.b;

   c = *VFX_color_to_RGB(standard_pen_colors[background]);

   DOUBLE br = c.r;
   DOUBLE bg = c.g;
   DOUBLE bb = c.b;
   
   r = r + (((br - r) / NumLevels) * (DOUBLE) shade);    // TODO: would probably look better if gamma-corrected
   g = g + (((bg - g) / NumLevels) * (DOUBLE) shade);
   b = b + (((bb - b) / NumLevels) * (DOUBLE) shade);

   if (r < 0.0) r = 0.0; if (r > 255.0) r = 255.0;
   if (g < 0.0) g = 0.0; if (g > 255.0) g = 255.0;
   if (b < 0.0) b = 0.0; if (b > 255.0) b = 255.0;

   VFX_pixel_write(stage, x,y, RGB_TRIPLET((short) r, (short) g, (short) b));
}

void draw_Wu_line (short X0, short Y0, short X1, short Y1)
{
   unsigned short IntensityShift, ErrorAdj, ErrorAcc;
   unsigned short ErrorAccTemp, Weighting, WeightingComplementMask;
   short DeltaX, DeltaY, Temp, XDir;
   unsigned short BaseColor = 0;

   /* Make sure the line runs top to bottom */
   if (Y0 > Y1) {
      Temp = Y0; Y0 = Y1; Y1 = Temp;
      Temp = X0; X0 = X1; X1 = Temp;
   }
   /* Draw the initial pixel, which is always exactly intersected by
      the line and so needs no weighting */
   if(line_pattern) step_line_pattern();
   WuDot(X0, Y0, BaseColor);

   if ((DeltaX = X1 - X0) >= 0) {
      XDir = 1;
   } else {
      XDir = -1;
      DeltaX = -DeltaX; /* make DeltaX positive */
   }
   /* Special-case horizontal, vertical, and diagonal lines, which
      require no weighting because they go right through the center of
      every pixel */
   if ((DeltaY = Y1 - Y0) == 0) {
      /* Horizontal line */
      while (DeltaX-- != 0) {
         X0 += XDir;
         if(line_pattern) step_line_pattern();
         WuDot(X0, Y0, BaseColor);
      }
      return;
   }
   if (DeltaX == 0) {
      /* Vertical line */
      do {
         Y0++;
         if(line_pattern) step_line_pattern();
         WuDot(X0, Y0, BaseColor);
      } while (--DeltaY != 0);
      return;
   }
   if (DeltaX == DeltaY) {
      /* Diagonal line */
      do {
         X0 += XDir;
         Y0++;
         if(line_pattern) step_line_pattern();
         WuDot(X0, Y0, BaseColor);
      } while (--DeltaY != 0);
      return;
   }
   /* Line is not horizontal, diagonal, or vertical */
   ErrorAcc = 0;  /* initialize the line error accumulator to 0 */
   /* # of bits by which to shift ErrorAcc to get intensity level */
   IntensityShift = 16 - IntensityBits;
   /* Mask used to flip all bits in an intensity weighting, producing the
      result (1 - intensity weighting) */
   WeightingComplementMask = NumLevels - 1;
   /* Is this an X-major or Y-major line? */
   if (DeltaY > DeltaX) {
      /* Y-major line; calculate 16-bit fixed-point fractional part of a
         pixel that X advances each time Y advances 1 pixel, truncating the
         result so that we won't overrun the endpoint along the X axis */
      ErrorAdj = (unsigned short) (((unsigned long) DeltaX << 16) / (unsigned long) DeltaY);
      /* Draw all pixels other than the first and last */
      while (--DeltaY) {
         ErrorAccTemp = ErrorAcc;   /* remember currrent accumulated error */
         ErrorAcc += ErrorAdj;      /* calculate error for next pixel */
         if (ErrorAcc <= ErrorAccTemp) {
            /* The error accumulator turned over, so advance the X coord */
            X0 += XDir;
         }
         Y0++; /* Y-major, so always advance Y */
         /* The IntensityBits most significant bits of ErrorAcc give us the
            intensity weighting for this pixel, and the complement of the
            weighting for the paired pixel */
         Weighting = ErrorAcc >> IntensityShift;
         if(line_pattern) step_line_pattern();
         WuDot(X0, Y0, BaseColor + Weighting);
         if(line_pattern) step_line_pattern();
         WuDot(X0 + XDir, Y0,
               BaseColor + (Weighting ^ WeightingComplementMask));
      }
      /* Draw the final pixel, which is 
         always exactly intersected by the line
         and so needs no weighting */
      if(line_pattern) step_line_pattern();
      WuDot(X1, Y1, BaseColor);
      return;
   }
   /* It's an X-major line; calculate 16-bit fixed-point fractional part of a
      pixel that Y advances each time X advances 1 pixel, truncating the
      result to avoid overrunning the endpoint along the X axis */
   ErrorAdj = (unsigned short) (((unsigned long) DeltaY << 16) / (unsigned long) DeltaX);
   /* Draw all pixels other than the first and last */
   while (--DeltaX) {
      ErrorAccTemp = ErrorAcc;   /* remember currrent accumulated error */
      ErrorAcc += ErrorAdj;      /* calculate error for next pixel */
      if (ErrorAcc <= ErrorAccTemp) {
         /* The error accumulator turned over, so advance the Y coord */
         Y0++;
      }
      X0 += XDir; /* X-major, so always advance X */
      /* The IntensityBits most significant bits of ErrorAcc give us the
         intensity weighting for this pixel, and the complement of the
         weighting for the paired pixel */
      Weighting = ErrorAcc >> IntensityShift;
      if(line_pattern) step_line_pattern();
      WuDot(X0, Y0, BaseColor + Weighting);
      if(line_pattern) step_line_pattern();
      WuDot(X0, Y0 + 1,
            BaseColor + (Weighting ^ WeightingComplementMask));
   }
   /* Draw the final pixel, which is always exactly intersected by the line
      and so needs no weighting */
   if(line_pattern) step_line_pattern();
   WuDot(X1, Y1, BaseColor);
}


void dot(int x, int y, u08 color)
{
   if(color == 0) return;  // so patterned lines draw correctly

   if(background) {
      if(color == 0) color = background;
      else if(color == background) color = 0;
   }
   VFX_pixel_write(stage, x,y, pen_colors[color]);
}

void draw_norm_line(COORD x1,COORD y1, COORD x2,COORD y2)
{
int i1, i2;
int d1, d2, d;

   i1 = i2 = 1;

   d1 = (int) x2 - (int) x1;
   if(d1 < 0) {
      i1 = (-1);
      d1 = 0 - d1;
   }

   d2 = (int) y2 - (int) y1;
   if(d2 < 0) {
      i2 = (-1);
      d2 = 0 - d2;
   }

   if(d1 > d2) {
      d = d2 + d2 - d1;
      while(1) {
         if(line_pattern) step_line_pattern();

         dot(x1,y1, color);

         if(x1 == x2) break;
         if(d >= 0) {
            d = d - d1 - d1;
            y1 += i2;
         }
         d = d + d2 + d2;
         x1 += i1;
      }
   }
   else {
      d = d1 + d1 - d2;
      while (1) {
         if(line_pattern) step_line_pattern();

         dot(x1,y1, color);

         if(y1 == y2) break;
         if(d >= 0) {
            d = d - d2 - d2;
            x1 += i1;
         }
         d = d + d1 + d1;
         y1 += i2;
      }
   }
}

void draw_line(COORD x1,COORD y1, COORD x2,COORD y2)
{
   if (antialias && (background == 0))
      draw_Wu_line(x1, y1, x2, y2);
   else
      draw_norm_line(x1, y1, x2, y2);
}

// ------------------------------------------------------------------------ 
// Line Clipper - using Cohen & Sutherland's 4 bit Outcodes 
// Ron Grant 1987
// use clipping extents defined within viewport structure pointed to by pVP
typedef int int88;

typedef struct {
   int OrgX,OrgY;       // view center in world coordinates
   int Theta;           // viewport rotation with respect to world (0..359)
   int Width,Height;    // viewport width and height in world coordinates
                        // OR do we specify scale directly rather than doing   
                        // indirect calculations to map world viewport to screen viewport

   int88 ScaleX,ScaleY; // fixed point scale

   int   WinX1,WinY1,WinX2,WinY2; // viewport window screen coordinates 
                                  // Note: using unsigned values here caused problems with
                                  // clipper outcode function, that is, comparison of 
                                  // negative ordinate against window edge was failing --
                                  // e.g. x=-1  left=10   (x<left) was generating FALSE

   int EyeX,EyeY;
} viewport;


viewport *pVP;  // pointer to current viewport used by clipper...
viewport VP;    // Default Instance of viewport (more can be defined)
  
#define select_viewport(v) pVP=&v    /* point to current vieport */

void viewport_init(void)   // sets up default clipping to full screen
{
   VP.WinX1 = LEFT_MARGIN+0;
   VP.WinX2 = LEFT_MARGIN+COLS-1;
   VP.WinY1 = TOP_MARGIN+0;
   VP.WinY2 = TOP_MARGIN+ROWS-1;
   select_viewport(VP);  // inform viewing transforms and clipper about current viewport
}


#define CS_LEFT   1
#define CS_RIGHT  2
#define CS_BOTTOM 4
#define CS_TOP    8

u08 out_code(int x, int y)  // clipped line endpoint region test
{
u08 c = 0;
   
   if(x < pVP->WinX1)       c = CS_LEFT;
   else if(x > pVP->WinX2)  c = CS_RIGHT;

   if(y > pVP->WinY2)       c |= CS_BOTTOM;
   else if(y < pVP->WinY1)  c |= CS_TOP;

   return (c);
}    



u08 clipped_line(int x1,int y1, int x2,int y2)   // clipped line
{
u08 c,c1,c2;
int x,y;
long dx,dy;
u08 clip_count;

    // coordinates in window coordinates 
    x = 0;
    y = 0; 

    c1 = out_code(x1,y1);
    c2 = out_code(x2,y2);

    clip_count = 0;


    // while either endpoint outside window, we may need to clip provided there 
    // is a chance that the line is not completely outside.

    // Also, number of clips is limited to 4 -- in case a degenerate case 
    // locks algorithm into an infinite loop. 

    while((c1 | c2) && (clip_count++ < 4)) {
      if(c1 & c2) return(0);  // line completely outside window 
                              // e.g. both endpoints above,left,right,or below window
                              // -- other combos too, e.g. both above and left.. 

      dx = x2 - x1;
      dy = y2 - y1;

      if(c1)  c = c1;
      else    c = c2;

      // reason for this???
                                             // if p1 in or p2 out  
      //if((!c1) || (c2 & CS_TOP)) c = c2;   // on top then take p2 
      //else c = c1; 

      // clip line against one window edge
      // then update out code for that end and repeat

      // by nature of outcodes TOP and BOTTOM clips will never have a 0 dy, likewise
      // LEFT and RIGHT clips will never have a 0 dx, hence no testing for dx==0 or dy==0 is required
    
      if(c & CS_TOP) {    // clip to top FIRST 
         y = pVP->WinY1;
         x = x1 + ((dx * (long)(y-y1)) / dy);   // calc x for which y = VPminY
      }
      else if(c & CS_LEFT) {
         x = pVP->WinX1;
         y = y1 + ((dy * (long)(x-x1)) / dx);
      }
      else if(c & CS_RIGHT) {
         x = pVP->WinX2;
         y = y1 + ((dy * (long)(x-x1)) / dx);
      }
      else if(c & CS_BOTTOM) {
         y = pVP->WinY2;
         x = x1 + ((dx * (long)(y-y1)) / dy);
      }

      if(c == c1) {
          x1 = x;
          y1 = y;
          c1 = out_code(x,y);
       }
       else {
          x2 = x;
          y2 = y;
          c2 = out_code(x,y);
       }
    }

    draw_line(x1,y1, x2,y2);  // draw line
    return 1;
}

void clipped_box(int x1,int y1, int x2,int y2) 
{
   clipped_line(x1,y1, x2,y1);
   clipped_line(x2,y1, x2,y2);
   clipped_line(x2,y2, x1,y2);
   clipped_line(x1,y2, x1,y1);
}

void clipped_filled_box(int x1,int y1, int x2,int y2) 
{
int t;

   if(y1 > y2) {
      t = y1;
      y1 = y2;
      y2 = t;
   }

   while(y1 <= y2) {
      clipped_line(x1,y1, x2,y1);
      ++y1;
   }
}


//
//
// Vector Character Table
// this table remains in program memory
// requires special functions to access
//
// Each byte in a stroke list contains a pair of coordinate offsets 
// and two flag bits.
//   LINE:COL:EOL:ROW    (MOVE:1 bit  COL:3 bits    EOL:1 bit ROW:3 bits)
// If the LINE bit is 0,  then move to the new coordinate and draw a dot.
// If the LINE bit is 1,  draw a line from the current coordinate to the new
//                        coordinate.
// If the EOL bit is set, the byte is the last one in the stroke list
// A 0xFF byte indicates a null character.
#define PROGMEM
u08 vg_00[] PROGMEM = { 0xFF };
u08 vg_01[] PROGMEM = { 0x01,0x90,0xD0,0xE1,0xE6, 0x01,0x86,0x97,0xD7,0xE6, 0x22,
                         0x24,0xB5,0xC4, 0x4A };
u08 vg_02[] PROGMEM = { 0x01,0x90,0xC3, 0x01,0xE1,0xE6, 0x01,0x86,0x97,0xD7,0xE6,
                         0x02,0xA0,0xB1,0xC0,0xE2, 0x03,0xE3, 0x04,0xB7,0xE4, 0x05,0xA7,
                         0x05,0x95,0x97, 0x06,0xE6, 0x10,0xD0,0xE1, 0x10,0x91, 0x20,0xA1,0xB0,0xC1,
                         0x23,0xD0,0xD1, 0x26,0xA7, 0x30,0xB3, 0x36,0xB7, 0x40,0xC1,
                         0x46,0xC7,0xE5, 0x55,0xE5, 0x55,0xDF };
u08 vg_03[] PROGMEM = { 0x01,0x90,0xC3, 0x01,0xB4, 0x01,0xC1,0xC3, 0x01,0x83,0xA5,0xC3,
                         0x02,0xA4,0xC2, 0x02,0xC2, 0x03,0xB0,0xC1, 0x03,0xC3, 0x10,0x94,0xC1,
                         0x11,0xB3, 0x13,0xB1, 0x14,0xB4, 0x21,0xA6, 0x30,0xBC };
u08 vg_04[] PROGMEM = { 0x03,0xB6,0xE3, 0x03,0xB0,0xE3, 0x03,0xE3, 0x12,0xC5, 0x12,0xD2,0xD4,
                         0x12,0x94,0xC1,0xC5, 0x13,0xB5,0xD3, 0x13,0xB1,0xD3, 0x14,0xD4,
                         0x21,0xD4, 0x21,0xC1, 0x21,0xA5,0xD2, 0x22,0xC4, 0x24,0xC2,
                         0x25,0xC5, 0x30,0xBE };
u08 vg_05[] PROGMEM = { 0x03,0xC7, 0x03,0xD3,0xD4, 0x03,0x84,0xB7, 0x04,0xB1, 0x04,0xD4,
                         0x11,0xA0,0xB1, 0x11,0xC4, 0x11,0xC1, 0x13,0xB5, 0x13,0x95,0xB3,
                         0x14,0xC1, 0x15,0xC5, 0x17,0xD3, 0x17,0xC7, 0x20,0xB0,0xC1,
                         0x20,0xA7,0xD4, 0x21,0xB0,0xB7, 0x21,0xD4, 0x22,0xB2, 0x23,0xC5,
                         0x25,0xC3,0xC5, 0x26,0xBE };
u08 vg_06[] PROGMEM = { 0x03,0xC7, 0x03,0xB0,0xE3,0xE5, 0x03,0xE3, 0x03,0x85,0xC1,0xC5,
                         0x04,0xB1,0xE4, 0x04,0xE4, 0x12,0xC5, 0x12,0xD2,0xD4, 0x12,0x94,
                         0x13,0xB5,0xD3, 0x21,0xE5, 0x21,0xC1, 0x21,0xA5,0xD2, 0x22,0xC4,
                         0x24,0xC2, 0x25,0xC5, 0x27,0xE3, 0x27,0xC7, 0x30,0xBF };
u08 vg_07[] PROGMEM = { 0x23,0xB4,0xC3,0xC4, 0x23,0xB2,0xC3, 0x23,0xC3, 0x23,0xA4,0xB3,0xC4,
                         0x24,0xB5,0xC4, 0x24,0xC4, 0x32,0xBD };
u08 vg_08[] PROGMEM = { 0x00,0xE0,0xE7, 0x00,0x87,0xE7, 0x01,0x90,0x92, 0x01,0xE1,
                         0x02,0xA0,0xA1, 0x02,0x92, 0x03,0xB0,0xE3, 0x04,0xB7,0xE4,
                         0x05,0xA7, 0x05,0x95,0x97, 0x06,0x97, 0x06,0xE6, 0x26,0xA7,
                         0x30,0xB1, 0x36,0xB7, 0x40,0xE2, 0x40,0xC1, 0x46,0xC7,0xE5,
                         0x50,0xE1, 0x50,0xD2,0xE2, 0x55,0xE5, 0x55,0xD7,0xEE };
u08 vg_09[] PROGMEM = { 0x03,0xA1,0xC1,0xE3,0xE4, 0x03,0x84,0xA6,0xC6,0xEC };
u08 vg_0A[] PROGMEM = { 0x00,0xE0,0xE7, 0x00,0x87,0xE7, 0x23,0xB4,0xC3,0xC4, 0x23,0xB2,0xC3,
                         0x23,0xC3, 0x23,0xA4,0xB3,0xC4, 0x24,0xB5,0xC4, 0x24,0xC4,
                         0x32,0xBD };
u08 vg_0B[] PROGMEM = { 0x04,0x93,0xB3,0xC4,0xC6, 0x04,0x86,0x97,0xB7,0xC6, 0x33,0xE0,0xE2,
                         0x40,0xE8 };
u08 vg_0C[] PROGMEM = { 0x11,0xA0,0xC0,0xD1,0xD3, 0x11,0x93,0xA4,0xC4,0xD3, 0x16,0xD6,
                         0x34,0xBF };
u08 vg_0D[] PROGMEM = { 0x06,0x95,0xA5, 0x06,0x87,0x97,0xA6, 0x20,0xE0,0xE2, 0x20,0xA6,
                         0x22,0xEA };
u08 vg_0E[] PROGMEM = { 0x05,0x94,0xA4, 0x05,0x86,0x96,0xA5, 0x20,0xE0,0xE6, 0x20,0xA5,
                         0x22,0xE2, 0x46,0xD5,0xE5, 0x46,0xC7,0xD7,0xEE };
u08 vg_0F[] PROGMEM = { 0x00,0xA2, 0x03,0x93,0xB5,0xD3,0xE3, 0x06,0xA4, 0x13,0xB1,0xD3,
                         0x30,0xB1, 0x35,0xB6, 0x42,0xE0, 0x44,0xEE };
u08 vg_10[] PROGMEM = { 0x00,0xB3, 0x00,0x86,0xB3, 0x01,0xA3, 0x01,0x91,0x95, 0x02,0xA4,
                         0x02,0xA2,0xA4, 0x03,0xB3, 0x04,0xA2, 0x04,0xA4, 0x05,0xA3,
                         0x05,0x9D };
u08 vg_11[] PROGMEM = { 0x23,0xD6, 0x23,0xD0,0xD6, 0x23,0xD3, 0x32,0xD4, 0x32,0xD2,
                         0x32,0xB4,0xD2, 0x33,0xD5, 0x33,0xD1, 0x34,0xD4, 0x41,0xD1,
                         0x41,0xC5,0xDD };
u08 vg_12[] PROGMEM = { 0x12,0xB0,0xD2, 0x15,0xB7,0xD5, 0x30,0xBF };
u08 vg_13[] PROGMEM = { 0x10,0x94, 0x16, 0x50,0xD4, 0x5E };
u08 vg_14[] PROGMEM = { 0x01,0x90,0xE0,0xE6, 0x01,0x82,0x93,0xB3, 0x30,0xBE };
u08 vg_15[] PROGMEM = { 0x06,0x97,0xB7,0xC6, 0x11,0xA2,0xB2,0xC3,0xC4, 0x11,0xA0,0xC0,0xD1,
                         0x13,0xA2, 0x13,0x94,0xA5,0xB5,0xC6, 0x35,0xCC };
u08 vg_16[] PROGMEM = { 0x14,0xB6,0xD4,0xD6, 0x14,0xD4, 0x14,0x96,0xB4,0xD6, 0x15,0xA6,0xC4,0xD5,
                         0x15,0xA4,0xC6,0xD5, 0x15,0xD5, 0x16,0xD6, 0x24,0xA6, 0x34,0xB6,
                         0x44,0xCE };
u08 vg_17[] PROGMEM = { 0x12,0xB0,0xD2, 0x14,0xB6,0xD4, 0x17,0xD7, 0x30,0xBE };
u08 vg_18[] PROGMEM = { 0x12,0xB0,0xD2, 0x30,0xBE };
u08 vg_19[] PROGMEM = { 0x14,0xB6,0xD4, 0x30,0xBE };
u08 vg_1A[] PROGMEM = { 0x03,0xD3, 0x31,0xD3, 0x35,0xDB };
u08 vg_1B[] PROGMEM = { 0x03,0xA5, 0x03,0xA1, 0x03,0xDB };
u08 vg_1C[] PROGMEM = { 0x02,0x85,0xDD };
u08 vg_1D[] PROGMEM = { 0x03,0xA5, 0x03,0xA1, 0x03,0xE3, 0x41,0xE3, 0x45,0xEB };
u08 vg_1E[] PROGMEM = { 0x04,0xB1,0xE4, 0x04,0xE4, 0x13,0xD3,0xD4, 0x13,0x94,0xB2,0xD4,
                         0x22,0xC4, 0x22,0xC2,0xC4, 0x22,0xA4,0xC2, 0x31,0xBC };
u08 vg_1F[] PROGMEM = { 0x02,0xB5,0xE2, 0x02,0xE2, 0x12,0xB4,0xD2,0xD3, 0x12,0x93,0xD3,
                         0x22,0xC4, 0x22,0xA4,0xC2,0xC4, 0x24,0xC4, 0x32,0xBD };
u08 vg_20[] PROGMEM = { 0xFF };
u08 vg_21[] PROGMEM = { 0x11,0xA0,0xB1,0xB3, 0x11,0xB3, 0x11,0xB1, 0x11,0x93,0xA4,0xB3,
                         0x12,0xB2, 0x13,0xB1, 0x13,0xB3, 0x20,0xA4, 0x2E };
u08 vg_22[] PROGMEM = { 0x10,0x92, 0x40,0xCA };
u08 vg_23[] PROGMEM = { 0x02,0xD2, 0x04,0xD4, 0x10,0x96, 0x40,0xCE };
u08 vg_24[] PROGMEM = { 0x02,0x93,0xB3,0xC4, 0x02,0x91,0xC1, 0x05,0xB5,0xC4, 0x20,0xA1,
                         0x25,0xAE };
u08 vg_25[] PROGMEM = { 0x01,0x91,0x92, 0x01,0x82,0x92, 0x06,0xD1, 0x45,0xD5,0xD6,
                         0x45,0xC6,0xDE };
u08 vg_26[] PROGMEM = { 0x04,0xA2,0xB2,0xD4,0xE3, 0x04,0x85,0x96,0xC6,0xD5,0xE6,
                         0x11,0xA2, 0x11,0xA0,0xB0,0xC1, 0x32,0xC1, 0x54,0xDD };
u08 vg_27[] PROGMEM = { 0x02,0x91, 0x10,0x99 };
u08 vg_28[] PROGMEM = { 0x12,0xB0, 0x12,0x94,0xBE };
u08 vg_29[] PROGMEM = { 0x10,0xB2,0xB4, 0x16,0xBC };
u08 vg_2A[] PROGMEM = { 0x03,0xE3, 0x11,0xD5, 0x15,0xD1, 0x30,0xBE };
u08 vg_2B[] PROGMEM = { 0x03,0xC3, 0x21,0xAD };
u08 vg_2C[] PROGMEM = { 0x17,0xA6, 0x25,0xAE };
u08 vg_2D[] PROGMEM = { 0x03,0xCB };
u08 vg_2E[] PROGMEM = { 0x25,0xAE };
u08 vg_2F[] PROGMEM = { 0x05,0xD8 };
u08 vg_30[] PROGMEM = { 0x01,0x90,0xC0,0xD1,0xD5, 0x01,0x85,0x96,0xC6,0xD5, 0x05,0xD9 };
u08 vg_31[] PROGMEM = { 0x11,0xA0,0xA6, 0x16,0xBE };
u08 vg_32[] PROGMEM = { 0x01,0x90,0xB0,0xC1,0xC2, 0x05,0xA3,0xB3,0xC2, 0x05,0x86,0xCE };
u08 vg_33[] PROGMEM = { 0x01,0x90,0xB0,0xC1,0xC2, 0x05,0x96,0xB6,0xC5, 0x23,0xB3,0xC4,0xC5,
                         0x33,0xCA };
u08 vg_34[] PROGMEM = { 0x03,0xB0,0xC0,0xC6, 0x03,0x84,0xDC };
u08 vg_35[] PROGMEM = { 0x00,0xC0, 0x00,0x82,0xB2,0xC3,0xC5, 0x05,0x96,0xB6,0xCD };
u08 vg_36[] PROGMEM = { 0x02,0xA0,0xB0, 0x02,0x85,0x96,0xB6,0xC5, 0x03,0xB3,0xC4,0xCD };
u08 vg_37[] PROGMEM = { 0x00,0xC0,0xC2, 0x00,0x81, 0x24,0xC2, 0x24,0xAE };
u08 vg_38[] PROGMEM = { 0x01,0x90,0xB0,0xC1,0xC2, 0x01,0x82,0x93,0xB3,0xC4,0xC5,
                         0x04,0x93, 0x04,0x85,0x96,0xB6,0xC5, 0x33,0xCA };
u08 vg_39[] PROGMEM = { 0x01,0x90,0xB0,0xC1,0xC4, 0x01,0x82,0x93,0xC3, 0x16,0xA6,0xCC };
u08 vg_3A[] PROGMEM = { 0x21,0xA2, 0x25,0xAE };
u08 vg_3B[] PROGMEM = { 0x17,0xA6, 0x21,0xA2, 0x25,0xAE };
u08 vg_3C[] PROGMEM = { 0x03,0xB6, 0x03,0xB8 };
u08 vg_3D[] PROGMEM = { 0x02,0xC2, 0x05,0xCD };
u08 vg_3E[] PROGMEM = { 0x10,0xC3, 0x16,0xCB };
u08 vg_3F[] PROGMEM = { 0x01,0x90,0xB0,0xC1,0xC2, 0x24,0xC2, 0x24,0xA5, 0x2F };
u08 vg_40[] PROGMEM = { 0x01,0x90,0xC0,0xD1,0xD4, 0x01,0x85,0x96,0xB6, 0x32,0xB4,0xDC };
u08 vg_41[] PROGMEM = { 0x02,0xA0,0xC2,0xC6, 0x02,0x86, 0x04,0xCC };
u08 vg_42[] PROGMEM = { 0x00,0xB0,0xC1,0xC2, 0x00,0x86,0xB6,0xC5, 0x03,0xB3,0xC4,0xC5,
                         0x33,0xCA };
u08 vg_43[] PROGMEM = { 0x02,0xA0,0xC0,0xD1, 0x02,0x84,0xA6,0xC6,0xDD };
u08 vg_44[] PROGMEM = { 0x00,0xA0,0xC2,0xC4, 0x00,0x86,0xA6,0xCC };
u08 vg_45[] PROGMEM = { 0x10,0xD0, 0x10,0x96,0xD6, 0x13,0xBB };
u08 vg_46[] PROGMEM = { 0x10,0xD0, 0x10,0x96, 0x13,0xBB };
u08 vg_47[] PROGMEM = { 0x02,0xA0,0xC0,0xD1, 0x02,0x84,0xA6,0xD6, 0x44,0xD4,0xDE };
u08 vg_48[] PROGMEM = { 0x00,0x86, 0x03,0xC3, 0x40,0xCE };
u08 vg_49[] PROGMEM = { 0x10,0xB0, 0x16,0xB6, 0x20,0xAE };
u08 vg_4A[] PROGMEM = { 0x04,0x85,0x96,0xB6,0xC5, 0x30,0xD0, 0x40,0xCD };
u08 vg_4B[] PROGMEM = { 0x00,0x86, 0x03,0xA3,0xC5,0xC6, 0x23,0xC1, 0x40,0xC9 };
u08 vg_4C[] PROGMEM = { 0x10,0x96,0xDE };
u08 vg_4D[] PROGMEM = { 0x00,0xB3,0xE0,0xE6, 0x00,0x86, 0x33,0xBC };
u08 vg_4E[] PROGMEM = { 0x00,0xD5, 0x00,0x86, 0x50,0xDE };
u08 vg_4F[] PROGMEM = { 0x02,0xA0,0xB0,0xD2,0xD4, 0x02,0x84,0xA6,0xB6,0xDC };
u08 vg_50[] PROGMEM = { 0x10,0xC0,0xD1,0xD2, 0x10,0x96, 0x13,0xC3,0xDA };
u08 vg_51[] PROGMEM = { 0x01,0x90,0xB0,0xC1,0xC5, 0x01,0x85,0x96,0xB6,0xC5, 0x24,0xDF };
u08 vg_52[] PROGMEM = { 0x10,0xC0,0xD1,0xD2, 0x10,0x96, 0x13,0xC3,0xD2, 0x33,0xD5,0xDE };
u08 vg_53[] PROGMEM = { 0x01,0x90,0xB0,0xC1, 0x01,0x82,0x93,0xB3,0xC4,0xC5, 0x05,0x96,0xB6,0xCD };
u08 vg_54[] PROGMEM = { 0x00,0xC0, 0x20,0xAE };
u08 vg_55[] PROGMEM = { 0x00,0x85,0x96,0xB6,0xC5, 0x40,0xCD };
u08 vg_56[] PROGMEM = { 0x00,0x84,0xA6,0xC4, 0x40,0xCC };
u08 vg_57[] PROGMEM = { 0x00,0x86,0xB3,0xE6, 0x32,0xB3, 0x60,0xEE };
u08 vg_58[] PROGMEM = { 0x00,0x81,0xD6, 0x06,0xD1, 0x50,0xD9 };
u08 vg_59[] PROGMEM = { 0x00,0x82,0xA4,0xC2, 0x16,0xB6, 0x24,0xA6, 0x40,0xCA };
u08 vg_5A[] PROGMEM = { 0x10,0xD0,0xD1, 0x15,0xD1, 0x15,0x96,0xDE };
u08 vg_5B[] PROGMEM = { 0x10,0xB0, 0x10,0x96,0xBE };
u08 vg_5C[] PROGMEM = { 0x00,0xEE };
u08 vg_5D[] PROGMEM = { 0x10,0xB0,0xB6, 0x16,0xBE };
u08 vg_5E[] PROGMEM = { 0x03,0xB0,0xEB };
u08 vg_5F[] PROGMEM = { 0x07,0xEF };
u08 vg_60[] PROGMEM = { 0x20,0xA1,0xBA };
u08 vg_61[] PROGMEM = { 0x05,0x96,0xB6,0xC5,0xD6, 0x05,0x94,0xC4, 0x12,0xB2,0xC3,0xCD };
u08 vg_62[] PROGMEM = { 0x06,0x95,0xA6,0xC6,0xD5, 0x10,0x95, 0x13,0xC3,0xD4,0xDD };
u08 vg_63[] PROGMEM = { 0x03,0x92,0xB2,0xC3, 0x03,0x85,0x96,0xB6,0xCD };
u08 vg_64[] PROGMEM = { 0x04,0x93,0xC3, 0x04,0x85,0x96,0xB6,0xC5,0xD6, 0x40,0xCD };
u08 vg_65[] PROGMEM = { 0x03,0x92,0xB2,0xC3,0xC4, 0x03,0x85,0x96,0xB6, 0x04,0xCC };
u08 vg_66[] PROGMEM = { 0x03,0xA3, 0x06,0xA6, 0x11,0xA0,0xB0,0xC1, 0x11,0x9E };
u08 vg_67[] PROGMEM = { 0x03,0x92,0xB2,0xC3,0xD2, 0x03,0x84,0x95,0xC5, 0x07,0xB7,0xC6,
                         0x43,0xCE };
u08 vg_68[] PROGMEM = { 0x10,0x96, 0x14,0xB2,0xC2,0xD3,0xDE };
u08 vg_69[] PROGMEM = { 0x12,0xA2,0xA6, 0x16,0xB6, 0x28 };
u08 vg_6A[] PROGMEM = { 0x05,0x86,0x97,0xB7,0xC6, 0x40, 0x42,0xCE };
u08 vg_6B[] PROGMEM = { 0x10,0x96, 0x14,0xB4,0xD6, 0x34,0xDA };
u08 vg_6C[] PROGMEM = { 0x10,0xA0,0xA6, 0x16,0xBE };
u08 vg_6D[] PROGMEM = { 0x02,0x86, 0x04,0xA2,0xB3,0xC2,0xD3,0xD6, 0x33,0xBD };
u08 vg_6E[] PROGMEM = { 0x02,0x86, 0x04,0xA2,0xB2,0xC3,0xCE };
u08 vg_6F[] PROGMEM = { 0x03,0x92,0xB2,0xC3,0xC5, 0x03,0x85,0x96,0xB6,0xCD };
u08 vg_70[] PROGMEM = { 0x02,0x93,0xA2,0xC2,0xD3,0xD4, 0x07,0xA7, 0x13,0x97, 0x15,0xC5,0xDC };
u08 vg_71[] PROGMEM = { 0x03,0x92,0xB2,0xC3,0xD2, 0x03,0x84,0x95,0xC5, 0x37,0xD7,
                         0x43,0xCF };
u08 vg_72[] PROGMEM = { 0x02,0x93,0xA2,0xB2,0xC3, 0x06,0xA6, 0x13,0x9E };
u08 vg_73[] PROGMEM = { 0x03,0x94,0xB4,0xC5, 0x03,0x92,0xC2, 0x06,0xB6,0xCD };
u08 vg_74[] PROGMEM = { 0x12,0xC2, 0x20,0xA5,0xB6,0xCD };
u08 vg_75[] PROGMEM = { 0x02,0x85,0x96,0xB6,0xC5,0xD6, 0x42,0xCD };
u08 vg_76[] PROGMEM = { 0x02,0x84,0xA6,0xC4, 0x42,0xCC };
u08 vg_77[] PROGMEM = { 0x02,0x85,0x96,0xB4,0xD6,0xE5, 0x33,0xB4, 0x62,0xED };
u08 vg_78[] PROGMEM = { 0x02,0xA4,0xB4,0xD6, 0x06,0xA4, 0x34,0xDA };
u08 vg_79[] PROGMEM = { 0x02,0x84,0x95,0xC5, 0x07,0xB7,0xC6, 0x42,0xCE };
u08 vg_7A[] PROGMEM = { 0x02,0xC2, 0x06,0xC2, 0x06,0xCE };
u08 vg_7B[] PROGMEM = { 0x03,0x93,0xA4,0xA5,0xB6,0xC6, 0x13,0xA2, 0x21,0xB0,0xC0,
                         0x21,0xAA };
u08 vg_7C[] PROGMEM = { 0x30,0xB2, 0x34,0xBE };
u08 vg_7D[] PROGMEM = { 0x00,0x90,0xA1,0xA2,0xB3,0xC3, 0x06,0x96,0xA5, 0x24,0xB3,
                         0x24,0xAD };
u08 vg_7E[] PROGMEM = { 0x01,0x90,0xA0,0xB1,0xC1,0xD8 };
u08 vg_7F[] PROGMEM = { 0x04,0xB1,0xE4,0xE6, 0x04,0x86,0xEE };
u08 vg_80[] PROGMEM = { 0x01,0x90,0xC0,0xD1, 0x01,0x83,0x94,0xC4,0xD3, 0x27,0xC7,0xD6,
                         0x34,0xDE };
u08 vg_81[] PROGMEM = { 0x01, 0x03,0x85,0x96,0xB6,0xC5,0xD6, 0x41, 0x43,0xCD };
u08 vg_82[] PROGMEM = { 0x03,0x92,0xB2,0xC3,0xC4, 0x03,0x85,0x96,0xB6, 0x04,0xC4,
                         0x30,0xC8 };
u08 vg_83[] PROGMEM = { 0x01,0x90,0xD0,0xE1, 0x15,0xA6,0xC6,0xD5,0xE6, 0x15,0xA4,0xD4,
                         0x22,0xC2,0xD3,0xDD };
u08 vg_84[] PROGMEM = { 0x05,0x96,0xB6,0xC5,0xD6, 0x05,0x94,0xC4, 0x10, 0x12,0xB2,0xC3,0xC5, 0x48 };
u08 vg_85[] PROGMEM = { 0x00,0x90, 0x05,0x96,0xB6,0xC5,0xD6, 0x05,0x94,0xC4, 0x12,0xB2,0xC3,0xCD };
u08 vg_86[] PROGMEM = { 0x05,0x96,0xB6,0xC5,0xD6, 0x05,0x94,0xC4, 0x12,0xB2,0xC3,0xC5,
                         0x20,0xB0,0xB1, 0x20,0xA1,0xB9 };
u08 vg_87[] PROGMEM = { 0x03,0x92,0xC2,0xD3, 0x03,0x84,0x95,0xC5, 0x07,0x97,0xBD };
u08 vg_88[] PROGMEM = { 0x01,0x90,0xD0,0xE1, 0x13,0xA2,0xC2,0xD3,0xD4, 0x13,0x95,0xA6,0xC6,
                         0x14,0xDC };
u08 vg_89[] PROGMEM = { 0x00, 0x03,0x92,0xB2,0xC3,0xC4, 0x03,0x85,0x96,0xB6, 0x04,0xC4, 0x48 };
u08 vg_8A[] PROGMEM = { 0x00,0xA0, 0x03,0x92,0xB2,0xC3,0xC4, 0x03,0x85,0x96,0xB6,
                         0x04,0xCC };
u08 vg_8B[] PROGMEM = { 0x00, 0x12,0xA2,0xA6, 0x16,0xB6, 0x48 };
u08 vg_8C[] PROGMEM = { 0x01,0x90,0xC0,0xD1, 0x22,0xB2,0xB6, 0x26,0xCE };
u08 vg_8D[] PROGMEM = { 0x00,0xA0, 0x12,0xA2,0xA6, 0x16,0xBE };
u08 vg_8E[] PROGMEM = { 0x00, 0x03,0xA1,0xC3,0xC6, 0x03,0x86, 0x04,0xC4, 0x48 };
u08 vg_8F[] PROGMEM = { 0x04,0x93,0xB3,0xC4,0xC6, 0x04,0x86, 0x05,0xC5, 0x20,0xB0,0xB1,
                         0x20,0xA1,0xB9 };
u08 vg_90[] PROGMEM = { 0x12,0xC2, 0x12,0x96,0xC6, 0x14,0xB4, 0x20,0xC8 };
u08 vg_91[] PROGMEM = { 0x15,0xA4,0xE4, 0x15,0x96,0xE6, 0x22,0xE2, 0x42,0xCE };
u08 vg_92[] PROGMEM = { 0x02,0xA0,0xE0, 0x02,0x86, 0x03,0xD3, 0x30,0xB6,0xEE };
u08 vg_93[] PROGMEM = { 0x01,0x90,0xB0,0xC1, 0x04,0x93,0xB3,0xC4,0xC5, 0x04,0x85,0x96,0xB6,0xCD };
u08 vg_94[] PROGMEM = { 0x01, 0x04,0x93,0xB3,0xC4,0xC5, 0x04,0x85,0x96,0xB6,0xC5, 0x49 };
u08 vg_95[] PROGMEM = { 0x01,0xA1, 0x04,0x93,0xB3,0xC4,0xC5, 0x04,0x85,0x96,0xB6,0xCD };
u08 vg_96[] PROGMEM = { 0x01,0x90,0xB0,0xC1, 0x03,0x85,0x96,0xB6,0xC5,0xD6, 0x43,0xCD };
u08 vg_97[] PROGMEM = { 0x01,0xA1, 0x03,0x85,0x96,0xB6,0xC5,0xD6, 0x43,0xCD };
u08 vg_98[] PROGMEM = { 0x01, 0x03,0x84,0x95,0xC5, 0x07,0xB7,0xC6, 0x41, 0x43,0xCE };
u08 vg_99[] PROGMEM = { 0x00, 0x13,0xB1,0xD3,0xD4, 0x13,0x94,0xB6,0xD4, 0x68 };
u08 vg_9A[] PROGMEM = { 0x00, 0x02,0x85,0x96,0xB6,0xC5, 0x40, 0x42,0xCD };
u08 vg_9B[] PROGMEM = { 0x03,0x92,0xD2, 0x03,0x84,0x95,0xD5, 0x30,0xBF };
u08 vg_9C[] PROGMEM = { 0x03,0xA3, 0x06,0xC6,0xD5, 0x11,0xA0,0xB0,0xC1,0xC2, 0x11,0x9E };
u08 vg_9D[] PROGMEM = { 0x00,0x81,0xA3,0xC1, 0x04,0xC4, 0x06,0xC6, 0x12, 0x23,0xA7,
                         0x32, 0x40,0xC9 };
u08 vg_9E[] PROGMEM = { 0x00,0xB0,0xC1,0xC2, 0x00,0x87, 0x03,0xB3,0xC2, 0x54,0xF4,
                         0x63,0xE6,0xF7, 0xFF };
u08 vg_9F[] PROGMEM = { 0x06,0x97,0xA7,0xB6, 0x23,0xC3, 0x31,0xC0,0xD0,0xE1, 0x31,0xBE };
u08 vg_A0[] PROGMEM = { 0x05,0x96,0xB6,0xC5,0xD6, 0x05,0x94,0xC4, 0x12,0xB2,0xC3,0xC5,
                         0x20,0xC8 };
u08 vg_A1[] PROGMEM = { 0x12,0xA2,0xA6, 0x16,0xB6, 0x20,0xC8 };
u08 vg_A2[] PROGMEM = { 0x04,0x93,0xB3,0xC4,0xC5, 0x04,0x85,0x96,0xB6,0xC5, 0x21,0xC9 };
u08 vg_A3[] PROGMEM = { 0x03,0x85,0x96,0xB6,0xC5,0xD6, 0x21,0xC1, 0x43,0xCD };
u08 vg_A4[] PROGMEM = { 0x01,0x90,0xA0,0xB1,0xC1,0xD0, 0x03,0x86, 0x04,0x93,0xB3,0xC4,0xCE };
u08 vg_A5[] PROGMEM = { 0x00,0xC0, 0x02,0xC6, 0x02,0x86, 0x42,0xCE };
u08 vg_A6[] PROGMEM = { 0x11,0xA0,0xC0,0xC3, 0x11,0x92,0xA3,0xD3, 0x15,0xDD };
u08 vg_A7[] PROGMEM = { 0x11,0xA0,0xB0,0xC1,0xC2, 0x11,0x92,0xA3,0xB3,0xC2, 0x15,0xCD };
u08 vg_A8[] PROGMEM = { 0x15,0xA6,0xC6,0xD5, 0x15,0xB3, 0x30, 0x32,0xBB };
u08 vg_A9[] PROGMEM = { 0x03,0xC3, 0x03,0x8D };
u08 vg_AA[] PROGMEM = { 0x03,0xC3,0xCD };
u08 vg_AB[] PROGMEM = { 0x00,0x82, 0x05,0xD0, 0x34,0xC3,0xD3,0xE4, 0x46,0xE4, 0x46,0xC7,0xEF };
u08 vg_AC[] PROGMEM = { 0x00,0x82, 0x05,0xD0, 0x44,0xE2,0xE7, 0x44,0xC5,0xED };
u08 vg_AD[] PROGMEM = { 0x13,0xA2,0xB3,0xB5, 0x13,0xB5, 0x13,0xB3, 0x13,0x95,0xA6,0xB5,
                         0x14,0xB4, 0x15,0xB3, 0x15,0xB5, 0x20, 0x22,0xAE };
u08 vg_AE[] PROGMEM = { 0x03,0xA5, 0x03,0xA1, 0x43,0xE5, 0x43,0xE9 };
u08 vg_AF[] PROGMEM = { 0x01,0xA3, 0x05,0xA3, 0x41,0xE3, 0x45,0xEB };
u08 vg_B0[] PROGMEM = { 0x01, 0x03, 0x05, 0x07, 0x20, 0x22, 0x24, 0x26, 0x41, 0x43, 0x45,
                         0x47, 0x60, 0x62, 0x64, 0x6E };
u08 vg_B1[] PROGMEM = { 0x01,0xE7,0xF6, 0x01,0x90,0xF6, 0x03,0xC7,0xF4, 0x03,0xB0,0xF4,
                         0x05,0xA7,0xF2, 0x05,0xD0,0xF2, 0x07,0xF8 };
u08 vg_B2[] PROGMEM = { 0x01,0x91,0xA2,0xB2,0xC3,0xD3,0xE4,0xF4, 0x03,0x93,0xA4,0xB4,0xC5,0xD5,0xE6,0xF6,
                         0x05,0x95,0xA6,0xB6,0xC7,0xD7,0xE6, 0x07,0x97,0xA6, 0x11,0xA0,0xB0,0xC1,0xD1,0xE2,0xF2,
                         0x13,0xA2, 0x15,0xA4, 0x32,0xC1, 0x34,0xC3, 0x36,0xC5, 0x51,0xE0,0xF0,
                         0x53,0xE2, 0x55,0xEC };
u08 vg_B3[] PROGMEM = { 0x30,0xBF };
u08 vg_B4[] PROGMEM = { 0x04,0xB4, 0x30,0xBF };
u08 vg_B5[] PROGMEM = { 0x02,0xB2, 0x04,0xB4, 0x30,0xBF };
u08 vg_B6[] PROGMEM = { 0x04,0xB4, 0x30,0xB7, 0x60,0xEF };
u08 vg_B7[] PROGMEM = { 0x04,0xE4,0xE7, 0x34,0xBF };
u08 vg_B8[] PROGMEM = { 0x02,0xB2,0xB7, 0x04,0xBC };
u08 vg_B9[] PROGMEM = { 0x02,0xB2, 0x04,0xB4,0xB7, 0x30,0xB2, 0x60,0xEF };
u08 vg_BA[] PROGMEM = { 0x30,0xB7, 0x60,0xEF };
u08 vg_BB[] PROGMEM = { 0x02,0xE2,0xE7, 0x04,0xB4,0xBF };
u08 vg_BC[] PROGMEM = { 0x02,0xB2, 0x04,0xE4, 0x30,0xB2, 0x60,0xEC };
u08 vg_BD[] PROGMEM = { 0x04,0xE4, 0x30,0xB4, 0x60,0xEC };
u08 vg_BE[] PROGMEM = { 0x02,0xB2, 0x04,0xB4, 0x30,0xBC };
u08 vg_BF[] PROGMEM = { 0x04,0xB4,0xBF };
u08 vg_C0[] PROGMEM = { 0x30,0xB4,0xFC };
u08 vg_C1[] PROGMEM = { 0x04,0xF4, 0x30,0xBC };
u08 vg_C2[] PROGMEM = { 0x04,0xF4, 0x34,0xBF };
u08 vg_C3[] PROGMEM = { 0x30,0xB7, 0x34,0xFC };
u08 vg_C4[] PROGMEM = { 0x04,0xFC };
u08 vg_C5[] PROGMEM = { 0x04,0xF4, 0x30,0xBF };
u08 vg_C6[] PROGMEM = { 0x30,0xB7, 0x32,0xF2, 0x34,0xFC };
u08 vg_C7[] PROGMEM = { 0x30,0xB7, 0x60,0xE7, 0x64,0xFC };
u08 vg_C8[] PROGMEM = { 0x30,0xB4,0xF4, 0x60,0xE2,0xFA };
u08 vg_C9[] PROGMEM = { 0x32,0xF2, 0x32,0xB7, 0x64,0xF4, 0x64,0xEF };
u08 vg_CA[] PROGMEM = { 0x02,0xB2, 0x04,0xF4, 0x30,0xB2, 0x60,0xE2,0xFA };
u08 vg_CB[] PROGMEM = { 0x02,0xF2, 0x04,0xB4,0xB7, 0x64,0xF4, 0x64,0xEF };
u08 vg_CC[] PROGMEM = { 0x30,0xB7, 0x60,0xE2,0xF2, 0x64,0xF4, 0x64,0xEF };
u08 vg_CD[] PROGMEM = { 0x02,0xF2, 0x04,0xFC };
u08 vg_CE[] PROGMEM = { 0x02,0xB2, 0x04,0xB4,0xB7, 0x30,0xB2, 0x60,0xE2,0xF2, 0x64,0xF4,
                         0x64,0xEF };
u08 vg_CF[] PROGMEM = { 0x02,0xF2, 0x04,0xF4, 0x40,0xCA };
u08 vg_D0[] PROGMEM = { 0x04,0xF4, 0x30,0xB4, 0x60,0xEC };
u08 vg_D1[] PROGMEM = { 0x02,0xF2, 0x04,0xF4, 0x34,0xBF };
u08 vg_D2[] PROGMEM = { 0x04,0xF4, 0x34,0xB7, 0x64,0xEF };
u08 vg_D3[] PROGMEM = { 0x30,0xB4,0xF4, 0x60,0xEC };
u08 vg_D4[] PROGMEM = { 0x30,0xB4,0xF4, 0x32,0xFA };
u08 vg_D5[] PROGMEM = { 0x32,0xF2, 0x32,0xB7, 0x34,0xFC };
u08 vg_D6[] PROGMEM = { 0x34,0xF4, 0x34,0xB7, 0x64,0xEF };
u08 vg_D7[] PROGMEM = { 0x04,0xF4, 0x30,0xB7, 0x60,0xEF };
u08 vg_D8[] PROGMEM = { 0x02,0xF2, 0x04,0xF4, 0x30,0xBF };
u08 vg_D9[] PROGMEM = { 0x04,0xB4, 0x30,0xBC };
u08 vg_DA[] PROGMEM = { 0x34,0xF4, 0x34,0xBF };
u08 vg_DB[] PROGMEM = { 0x00,0xF0,0xF7, 0x00,0x87,0xF7, 0x01,0xF1, 0x02,0xF2, 0x03,0xF3,
                         0x04,0xF4, 0x05,0xF5, 0x06,0xF6, 0x10,0x97, 0x20,0xA7, 0x30,0xB7,
                         0x40,0xC7, 0x50,0xD7, 0x60,0xEF };
u08 vg_DC[] PROGMEM = { 0x04,0xF4,0xF7, 0x04,0x87,0xF7, 0x05,0xF5, 0x06,0xF6, 0x14,0x97,
                         0x24,0xA7, 0x34,0xB7, 0x44,0xC7, 0x54,0xD7, 0x64,0xEF };
u08 vg_DD[] PROGMEM = { 0x00,0xB0,0xB7, 0x00,0x87,0xB7, 0x01,0xB1, 0x02,0xB2, 0x03,0xB3,
                         0x04,0xB4, 0x05,0xB5, 0x06,0xB6, 0x10,0x97, 0x20,0xAF };
u08 vg_DE[] PROGMEM = { 0x40,0xF0,0xF7, 0x40,0xC7,0xF7, 0x41,0xF1, 0x42,0xF2, 0x43,0xF3,
                         0x44,0xF4, 0x45,0xF5, 0x46,0xF6, 0x50,0xD7, 0x60,0xEF };
u08 vg_DF[] PROGMEM = { 0x00,0xF0,0xF3, 0x00,0x83,0xF3, 0x01,0xF1, 0x02,0xF2, 0x10,0x93,
                         0x20,0xA3, 0x30,0xB3, 0x40,0xC3, 0x50,0xD3, 0x60,0xEB };
u08 vg_E0[] PROGMEM = { 0x03,0x92,0xA2,0xB3,0xC3,0xD2, 0x03,0x85,0x96,0xA6,0xB5,0xC5,0xD6,
                         0x33,0xBD };
u08 vg_E1[] PROGMEM = { 0x02,0x91,0xB1,0xC2, 0x02,0x87, 0x03,0xB3,0xC4, 0x05,0xB5,0xC4,
                         0x33,0xCA };
u08 vg_E2[] PROGMEM = { 0x01,0xC1,0xC2, 0x01,0x8E };
u08 vg_E3[] PROGMEM = { 0x02,0xD2, 0x12,0x96, 0x42,0xCE };
u08 vg_E4[] PROGMEM = { 0x00,0xC0,0xC1, 0x00,0x81,0xA3, 0x05,0xA3, 0x05,0x86,0xC6,
                         0x45,0xCE };
u08 vg_E5[] PROGMEM = { 0x03,0x92,0xD2, 0x03,0x85,0x96,0xA6,0xB5, 0x32,0xBD };
u08 vg_E6[] PROGMEM = { 0x07,0xA5,0xC5,0xD4, 0x11,0x94,0xA5, 0x51,0xDC };
u08 vg_E7[] PROGMEM = { 0x02,0x91,0xA1,0xB2,0xD2,0xE1, 0x32,0xBE };
u08 vg_E8[] PROGMEM = { 0x03,0x92,0xB2,0xC3,0xC4, 0x03,0x84,0x95,0xB5,0xC4, 0x10,0xB0,
                         0x17,0xB7, 0x20,0xAF };
u08 vg_E9[] PROGMEM = { 0x02,0xA0,0xB0,0xD2,0xD4, 0x02,0x84,0xA6,0xB6,0xD4, 0x03,0xDB };
u08 vg_EA[] PROGMEM = { 0x02,0xA0,0xB0,0xD2,0xD3, 0x02,0x83,0x94,0x96, 0x06,0x96,
                         0x44,0xD3, 0x44,0xC6,0xDE };
u08 vg_EB[] PROGMEM = { 0x04,0x93,0xC3, 0x04,0x85,0x96,0xB6,0xC5, 0x31,0xC2,0xC5,
                         0x31,0xC0,0xD8 };
u08 vg_EC[] PROGMEM = { 0x03,0x92,0xA2,0xB3,0xC2,0xD2,0xE3,0xE4, 0x03,0x84,0x95,0xA5,0xB4,0xC5,0xD5,0xE4,
                         0x33,0xBC };
u08 vg_ED[] PROGMEM = { 0x02,0x91,0xD1,0xE2,0xE4, 0x02,0x84,0x95,0xD5,0xE4, 0x06,0xE8 };
u08 vg_EE[] PROGMEM = { 0x02,0xA0,0xB0, 0x02,0x84,0xA6,0xB6, 0x03,0xBB };
u08 vg_EF[] PROGMEM = { 0x01,0x90,0xB0,0xC1,0xC6, 0x01,0x8E };
u08 vg_F0[] PROGMEM = { 0x11,0xD1, 0x13,0xD3, 0x15,0xDD };
u08 vg_F1[] PROGMEM = { 0x12,0xD2, 0x16,0xD6, 0x30,0xBC };
u08 vg_F2[] PROGMEM = { 0x06,0xC6, 0x10,0xB2, 0x14,0xBA };
u08 vg_F3[] PROGMEM = { 0x06,0xC6, 0x12,0xB4, 0x12,0xB8 };
u08 vg_F4[] PROGMEM = { 0x31,0xC0,0xD0,0xE1,0xE2, 0x31,0xBF };
u08 vg_F5[] PROGMEM = { 0x05,0x86,0x97,0xA7,0xB6, 0x30,0xBE };
u08 vg_F6[] PROGMEM = { 0x13,0xD3, 0x30,0xB1, 0x35,0xBE };
u08 vg_F7[] PROGMEM = { 0x02,0x91,0xA1,0xB2,0xC2,0xD1, 0x05,0x94,0xA4,0xB5,0xC5,0xDC };
u08 vg_F8[] PROGMEM = { 0x11,0xA0,0xB0,0xC1,0xC2, 0x11,0x92,0xA3,0xB3,0xCA };
u08 vg_F9[] PROGMEM = { 0x33,0xBC };
u08 vg_FA[] PROGMEM = { 0x3C };
u08 vg_FB[] PROGMEM = { 0x15,0xB7,0xC7, 0x40,0xE0, 0x40,0xCF };
u08 vg_FC[] PROGMEM = { 0x00,0x91,0xA0,0xB0,0xC1,0xC4, 0x11,0x9C };
u08 vg_FD[] PROGMEM = { 0x01,0x90,0xA0,0xB1, 0x04,0xB1, 0x04,0xBC };
u08 vg_FE[] PROGMEM = { 0x22,0xC4, 0x22,0xC2,0xC5, 0x22,0xA5,0xC3, 0x23,0xC5, 0x23,0xC3,
                         0x24,0xC2, 0x24,0xC4, 0x25,0xC5, 0x32,0xBD };
u08 vg_FF[] PROGMEM = { 0x01,0xD1,0xD6, 0x01,0x86,0xDE };

//
// Table of pointers to the character strokes
//
u08 *vgen[256] PROGMEM = {
   &vg_00[0],   &vg_01[0],   &vg_02[0],   &vg_03[0],   &vg_04[0],
   &vg_05[0],   &vg_06[0],   &vg_07[0],   &vg_08[0],   &vg_09[0],
   &vg_0A[0],   &vg_0B[0],   &vg_0C[0],   &vg_0D[0],   &vg_0E[0],
   &vg_0F[0],   &vg_10[0],   &vg_11[0],   &vg_12[0],   &vg_13[0],
   &vg_14[0],   &vg_15[0],   &vg_16[0],   &vg_17[0],   &vg_18[0],
   &vg_19[0],   &vg_1A[0],   &vg_1B[0],   &vg_1C[0],   &vg_1D[0],
   &vg_1E[0],   &vg_1F[0],   &vg_20[0],   &vg_21[0],   &vg_22[0],
   &vg_23[0],   &vg_24[0],   &vg_25[0],   &vg_26[0],   &vg_27[0],
   &vg_28[0],   &vg_29[0],   &vg_2A[0],   &vg_2B[0],   &vg_2C[0],
   &vg_2D[0],   &vg_2E[0],   &vg_2F[0],   &vg_30[0],   &vg_31[0],
   &vg_32[0],   &vg_33[0],   &vg_34[0],   &vg_35[0],   &vg_36[0],
   &vg_37[0],   &vg_38[0],   &vg_39[0],   &vg_3A[0],   &vg_3B[0],
   &vg_3C[0],   &vg_3D[0],   &vg_3E[0],   &vg_3F[0],   &vg_40[0],
   &vg_41[0],   &vg_42[0],   &vg_43[0],   &vg_44[0],   &vg_45[0],
   &vg_46[0],   &vg_47[0],   &vg_48[0],   &vg_49[0],   &vg_4A[0],
   &vg_4B[0],   &vg_4C[0],   &vg_4D[0],   &vg_4E[0],   &vg_4F[0],
   &vg_50[0],   &vg_51[0],   &vg_52[0],   &vg_53[0],   &vg_54[0],
   &vg_55[0],   &vg_56[0],   &vg_57[0],   &vg_58[0],   &vg_59[0],
   &vg_5A[0],   &vg_5B[0],   &vg_5C[0],   &vg_5D[0],   &vg_5E[0],
   &vg_5F[0],   &vg_60[0],   &vg_61[0],   &vg_62[0],   &vg_63[0],
   &vg_64[0],   &vg_65[0],   &vg_66[0],   &vg_67[0],   &vg_68[0],
   &vg_69[0],   &vg_6A[0],   &vg_6B[0],   &vg_6C[0],   &vg_6D[0],
   &vg_6E[0],   &vg_6F[0],   &vg_70[0],   &vg_71[0],   &vg_72[0],
   &vg_73[0],   &vg_74[0],   &vg_75[0],   &vg_76[0],   &vg_77[0],
   &vg_78[0],   &vg_79[0],   &vg_7A[0],   &vg_7B[0],   &vg_7C[0],
   &vg_7D[0],   &vg_7E[0],   &vg_7F[0],   &vg_80[0],   &vg_81[0],
   &vg_82[0],   &vg_83[0],   &vg_84[0],   &vg_85[0],   &vg_86[0],
   &vg_87[0],   &vg_88[0],   &vg_89[0],   &vg_8A[0],   &vg_8B[0],
   &vg_8C[0],   &vg_8D[0],   &vg_8E[0],   &vg_8F[0],   &vg_90[0],
   &vg_91[0],   &vg_92[0],   &vg_93[0],   &vg_94[0],   &vg_95[0],
   &vg_96[0],   &vg_97[0],   &vg_98[0],   &vg_99[0],   &vg_9A[0],
   &vg_9B[0],   &vg_9C[0],   &vg_9D[0],   &vg_9E[0],   &vg_9F[0],
   &vg_A0[0],   &vg_A1[0],   &vg_A2[0],   &vg_A3[0],   &vg_A4[0],
   &vg_A5[0],   &vg_A6[0],   &vg_A7[0],   &vg_A8[0],   &vg_A9[0],
   &vg_AA[0],   &vg_AB[0],   &vg_AC[0],   &vg_AD[0],   &vg_AE[0],
   &vg_AF[0],   &vg_B0[0],   &vg_B1[0],   &vg_B2[0],   &vg_B3[0],
   &vg_B4[0],   &vg_B5[0],   &vg_B6[0],   &vg_B7[0],   &vg_B8[0],
   &vg_B9[0],   &vg_BA[0],   &vg_BB[0],   &vg_BC[0],   &vg_BD[0],
   &vg_BE[0],   &vg_BF[0],   &vg_C0[0],   &vg_C1[0],   &vg_C2[0],
   &vg_C3[0],   &vg_C4[0],   &vg_C5[0],   &vg_C6[0],   &vg_C7[0],
   &vg_C8[0],   &vg_C9[0],   &vg_CA[0],   &vg_CB[0],   &vg_CC[0],
   &vg_CD[0],   &vg_CE[0],   &vg_CF[0],   &vg_D0[0],   &vg_D1[0],
   &vg_D2[0],   &vg_D3[0],   &vg_D4[0],   &vg_D5[0],   &vg_D6[0],
   &vg_D7[0],   &vg_D8[0],   &vg_D9[0],   &vg_DA[0],   &vg_DB[0],
   &vg_DC[0],   &vg_DD[0],   &vg_DE[0],   &vg_DF[0],   &vg_E0[0],
   &vg_E1[0],   &vg_E2[0],   &vg_E3[0],   &vg_E4[0],   &vg_E5[0],
   &vg_E6[0],   &vg_E7[0],   &vg_E8[0],   &vg_E9[0],   &vg_EA[0],
   &vg_EB[0],   &vg_EC[0],   &vg_ED[0],   &vg_EE[0],   &vg_EF[0],
   &vg_F0[0],   &vg_F1[0],   &vg_F2[0],   &vg_F3[0],   &vg_F4[0],
   &vg_F5[0],   &vg_F6[0],   &vg_F7[0],   &vg_F8[0],   &vg_F9[0],
   &vg_FA[0],   &vg_FB[0],   &vg_FC[0],   &vg_FD[0],   &vg_FE[0],
   &vg_FF[0]
};

#define VCHAR_W  8 // elemental width and height of character pattern 
#define VCHAR_H  8 
u08 vchar_inited;

// vector character attributes
// set generally by function calls

signed char VCharScaleX;
signed char VCharScaleY;

int VCharHeight;     // character height computed
int VCharWidth;      // character width  computed

int VCharTable;
int VCharThicknessX;
int VCharThicknessY;

int VCharSpaceX;
int VCharSpaceY;

void vchar_init(void);

void vchar_set_fontsize(u08 scaleX, u08 scaleY)
{
   if(vchar_inited == 0) vchar_init();  // a little idiot proofing

   VCharHeight = scaleY;
   VCharWidth  = scaleX;
}
   

void vchar_set_thickness(u08 thick_x, u08 thick_y)
{ 
   if(vchar_inited == 0) vchar_init();  // a little idiot proofing

   VCharThicknessX = thick_x;
   VCharThicknessY = thick_y;
}


void vchar_set_spacing(u08 space_x, u08 space_y)
{ 
   if(vchar_inited == 0) vchar_init();  // a little idiot proofing

   VCharSpaceX = space_x;
   VCharSpaceY = space_y;
}


void vchar_init(void) 
{
   vchar_inited = 1;

   viewport_init();
   vchar_set_fontsize(8,8);
   vchar_set_thickness(1,1);
   vchar_set_spacing(0,0);
}

signed char vchar_slant[VCHAR_H];


#define DL_LEN  256                 // uuuu max number of strokes in a DL char
#define UC_CHAR 256                 // UC char is DL font char 256
int gl_dl_font[2][256+1][DL_LEN+4]; // DL download font buffer
int gl_uc_ptr;                      // used to save UC use character strokes
void draw_user_char(double x,double y, int font, int set_xy, int c);

void vchar_char(int xoffset,int yoffset, int c)  // draw a vector character
{ 
int x1,y1, x2,y2, x,y;  
u08 VByte;
u08 *VIndex;  // char gen table offset
int xslant;
int dl;  //uuuu
int dlc;
int dl_stroke;
int font;
double old_glx,old_gly;
int lastx,lasty;
old_glx = gl_x;
old_gly = gl_y;


   font = gl_alt_font;

   if((c < 0) || (c > UC_CHAR)) return;
   if((font < 0) || (font > 1)) return;

   dlc = 0;
   if(gl_dl_font[font][c][0] != (-999999)) {
      dlc = 1;
   }
   else if(c == UC_CHAR) c = ' ';


   if(vchar_inited == 0) vchar_init();  // a little idiot proofing

   // loops allow overprinting of vector character with x,y
   // offsets, default is thickness 1 in x and y
   // result is character is drawn one time
   x2 = y2 = 0;
   x1 = y1 = 0;
   lastx = xoffset; lasty = yoffset;

   for(y=yoffset;y<yoffset+VCharThicknessY;y++) {
      for(x=xoffset;x<xoffset+VCharThicknessX;x++) {
         if(dlc == 0) VIndex = vgen[c];    // pointer to stroke for the char
         else         VIndex = vgen[' '];  // pointer to stroke for the char
  
         dl = 0;
         x1 = x2 = y1 = y2 = 0;
         lastx = xoffset; lasty = yoffset;
         while(1) {  // draw the character strokes
            if(dlc) {  //uuuu
               dl_stroke = gl_dl_font[font][c][dl++];
//printf("  stroke:%d\n", dl_stroke);
               if(dl_stroke == (-999999)) {  // end of char
                  break;
               }
               else if(dl_stroke >= 99) { // pen down
//printf("  pen down!\n");
                  continue;
               }
               else if(dl_stroke <= (-99)) { // pen up
//printf("  pen up!");
                  dl_stroke = gl_dl_font[font][c][dl++];
                  if(dl_stroke == (-999999)) break;
                  x1 = dl_stroke;
                  dl_stroke = gl_dl_font[font][c][dl++];
                  if(dl_stroke == (-999999)) break;
//if(c == UC_CHAR) ; else
                  dl_stroke = 31 - dl_stroke;
                  y1 = dl_stroke;
                  VByte = 0x00;
               }
               else {
                  x1 = dl_stroke;
                  dl_stroke = gl_dl_font[font][c][dl++];
                  if(dl_stroke == (-999999)) break;
//if(c == UC_CHAR) ; else                      
                  dl_stroke = 31 - dl_stroke;
                  y1 = dl_stroke;
//printf("  vector:%d,%d\n", x1,y1);
                  VByte = 0x80;
               }
            }
            else {
               VByte = *VIndex;
               if(VByte == 0xFF) break;
               ++VIndex;

               x1 = (VByte >> 4) & 0x07;
               y1 = (VByte & 0x07);
            }
            if(dlc) xslant = 0;  //!!!! we dont slant dl chars
            else    xslant = (((int) vchar_slant[y1]) * VCharWidth) / VCHAR_W;

            x1 *= VCharWidth;
            if((c >= 0xB0) && (c <= 0xDF) && (dlc == 0)) {  // uuuu make sure line drawing chars touch
               if(((VByte>>4) & 0x07) == (VCHAR_W-1)) x1 += (VCharWidth-1);
            }
            if(dlc) x1 /= 32;
            else    x1 /= VCHAR_W;

            y1 *= VCharHeight;
            if((c >= 0xB0) && (c <= 0xDF) && (dlc == 0)) { // uuuu make sure line drawing chars touch
               if((VByte & 0x07) == (VCHAR_H-1)) y1 += (VCharHeight-1);
            }
            if(dlc) y1 /= 32;
            else    y1 /= VCHAR_H;
   
            if(rotate & ROT_CHAR_HORIZ) {
               x1 = (VCharWidth-1) - x1;
               xslant = 0 - xslant;
            }
            if(rotate & ROT_CHAR_VERT) y1 = (VCharHeight-1) - y1;

            x1 += xslant;
//if(dlc) printf("  vector:%d,%d to %d,%d\n", x1,y1, x2,y2);

            if((VByte & 0x80) == 0x00) {  // move to point and draw a dot
               x2 = x1;   // we do dots as a single point line
               y2 = y1;
            }

            if(rotate & ROT_CHAR_90) {
               clipped_line(y1+x,x1+y, y2+x,x2+y);
               lastx = x+y2;
               lasty = y+x2;
            }
            else {
               clipped_line(x+x1,y+y1, x+x2,y+y2);
               lastx = x+x2;
               lasty = y+y2;
            }

            if(VByte & 0x08) break;  // end of list

            x2 = x1;  // prepare for next stroke in the character
            y2 = y1;
         }
      } // end for x
   } // end for y 
}  





double CurX,CurY;   // Current X,Y point as entity drawing progresses
              
double u,du,      // parameter u varried from 0 to 1
                 // du = delta u per point evaluation
     X1,Y1,      // 4 points defining bezier curve hull
     X2,Y2,      // also used by other drawing routines
     X3,Y3,      // e.g. X1,Y1 Circle Center
     X4,Y4;

u08 DrawCmdActive;

#define DC_BEZ       5


void bezier_init(long steps)
{       
  u = 0.0;      
  du = 1.0 / steps;
  DrawCmdActive = DC_BEZ;
}

// next_u: advance to next u value, if not right at end
// if u beyond end make = to end such that next call to nextpoint
// routine will gen point right at end of curve or line.

void next_u(void)
{       
  if(u == 1.0) {  // are we at the end of the parametric line/curve?
     DrawCmdActive = 0; 
  }
  else { 
    u += du;               // advance to next point
    if(u > 1.0) u = 1.0;   // if beyond end of curve, force end of curve to be next point
  }     
}       
        
void bezier_nextpoint(void) 
{
double u2,u3;
double a,b,c,d;

   // u squared and cubed
   u2 = u*u;
   u3 = u2*u;
   
   // cubic weight terms
   a = 1-u3 +3*(u2-u);  
   b = 3*(u+u3-2*u2);
   c = 3*(u2-u3);
   d = u3;

   // calc XY point on curve at current u 

   CurX = a * X1 + b * X2 + c * X3 + d * X4;
   CurY = a * Y1 + b * Y2 + c * Y3 + d * Y4;
   
   next_u();  // advance to next point
}



//
//   User interface routines
//


u08 new_lbterm;

int pcl_row, pcl_col;  // where to draw the scan line data
int pcl_encoding;
u08 pcl_state = 0;     // current parser state
int pcl_rep_count = 0;

void gl_set_defaults(u08 init);

void render_init()
{
   vchar_init();
   pcl_state = 0;  // current parser state
   pcl_row = pcl_col = 0;
   pcl_encoding = 0;
   pcl_rep_count = 0;
   gl_set_defaults(2);
   in_hpgl = 1;
   force_lb_term = 0;
}



#ifdef PCL_CODE
//
//
//   This is a PCL command parser.
//
//   It has some crude support for rendering uncompressed raster
//   graphics (just enough to render HP16500 and HP5371 screen prints).
//
//   There is some attempt to scale graphics to a (smaller) screen.
//   Horizontally the raster is scaled to the screen width by converting
//   runs of pixels to vectors and then scaling the vectors to the screen size.
//   Vertically the raster is scaled by dropping rows.
//
//
u08 pcl_next_state; // ... state to enter after processing a PCL command

u08 pcl_parm;       // the PCL parm command
u08 pcl_grp;        // the PCL group command
long pcl_value = 0; // the PCL command numeric argument  !!! this should be a double

int pcl_sign = 1;   // used to parse the numeric argument
u08 pcl_decimal;
long pcl_divisor;

#define PCL_SCALE 1
#define PCL_SCALE_VECTORS 1
int pcl_bytes;
int pcl_run_start;
u08 pcl_last_bit;

#define PCL_BUF_LEN 4096    // good for 32768 pixels
u08 pcl_buf[PCL_BUF_LEN+1];
int pcl_index;


void pcl_bit(u08 val)
{
int x1, x2;
int y;
int i;
int scale;
int pcl_top;
#define PCL_Y (COLS/(pcl_index*8))

   if(pcl_index == 0) return; // no scan line data 

   if(PCL_SCALE_VECTORS) {
      scale = (RES_X - LEFT_MARGIN) / (pcl_index * 8);
      if(scale <= 0) scale = 1;
      pcl_top = TOP_MARGIN+10;
   }
   else {
      scale = 1;
      pcl_top = TOP_MARGIN+10;
   }

   if(val == 0xFF) val = 2;  // end-of scan line,  dump final run
   else if(val) val = 1;
   else    val = 0;

   if(pcl_col == 0) {  // first pixel of a scan line, start a new run
      pcl_run_start = pcl_col;
   }
   else if(val != pcl_last_bit) {  // scale and render the run
      if(pcl_last_bit == 1) {
         for(i=0; i<scale; i++) {
            x1 = pcl_run_start*scale;
            x2 = pcl_col * scale - 1;
            y = (pcl_row * scale) + i;
if(0) {
            if(user_rotate & 0x02) {
               x1 = (pcl_index*8)-x1;
               x2 = (pcl_index*8)-x2;
            }
            if(user_rotate & 0x01) {  // !!!! at this point we don't know the height
               y = SCREEN_HEIGHT-y;   // !!!! of the pcl image, so we rotate to the screen height
            }
//          x1 += ((SCREEN_WIDTH-(pcl_index*8)) / 2);
//          x2 += ((SCREEN_WIDTH-(pcl_index*8)) / 2);
}
            draw_line(LEFT_MARGIN+x1,pcl_top+y,  LEFT_MARGIN+x2,pcl_top+y);
         }
      }
      pcl_run_start = pcl_col;   // start a new run
   }

   ++pcl_col;
   pcl_last_bit = val;
}

void pcl_byte(u08 data)
{
   pcl_bit(data & 0x80);
   pcl_bit(data & 0x40);
   pcl_bit(data & 0x20);
   pcl_bit(data & 0x10);
   pcl_bit(data & 0x08);
   pcl_bit(data & 0x04);
   pcl_bit(data & 0x02);
   pcl_bit(data & 0x01);
}

void pcl_rep(int i, u08 data)
{
   ++i;
   while(i--) pcl_byte(data);
}



void pcl_reset(u08 flag)
{
//printf("PCL reset %d\n", flag);
   in_hpgl = 1;
   pcl_state = 0;
   pcl_row = pcl_col = 0;
   pcl_encoding = 0;
   pcl_rep_count = 0;
}


void do_pcl_cmd(u08 pcl_cmd)
{
   pcl_cmd = toupper(pcl_cmd);
   pcl_state = pcl_next_state;   // where we go after this

   // this is the place where we decode and act upon the PCL command info
   if(pcl_parm == 0) {  // single byte commands
      if(pcl_cmd == 'E') pcl_reset(0);   // ESC E reset device
   }
   else if(pcl_parm == '*') {
      if((pcl_grp == 'b') && (pcl_cmd == 'W')) {  // ESC*b#W  raster data byte count
         ++pcl_row;
         pcl_col = 0;
         if(pcl_value >= 0) {  // number of bytes of raster data that will follow
            pcl_bytes = pcl_value;
            if(pcl_value > 0) {
               pcl_state = 10;   // next we will be getting the raster graphics data 
               pcl_index = 0;
            }
         }
      }
      else if((pcl_grp == 'b') && (pcl_cmd == 'M')) {  // ESC*b#M  pcl data encoding method
         pcl_encoding = pcl_value;
      }
      else if((pcl_grp == 'r') && (pcl_cmd == 'A')) {  // ESC*r#A  begin raster graphics
         // pcl_value is the graphics data origin code 0:left margin 1:cursor posn
         // pcl_origin = pcl_value;
         pcl_row = (-1);
//       lcd_clear();
      }
      else if((pcl_grp == 'r') && (pcl_cmd == 'B')) {  // ESC*r#B  end raster graphics
         // nothing much for us to do here
         in_hpgl = 1;
      }
      else if((pcl_grp == 'r') && (pcl_cmd == 'X')) {  // ESC*r#X 
         if(pcl_value == (-12345)) pcl_reset(1);
      }
      else if((pcl_grp == 't') && (pcl_cmd == 'R')) {  // ESC*t#R - data resolution in dots per inch
         // pcl_resolution = pcl_value;
      }
   }
   else if(pcl_parm == '%') {
      if(pcl_cmd == 'A')      in_hpgl = 0;  // enter PCL mode
      else if(pcl_cmd == 'B') in_hpgl = 1;  // enter HPGL mode
   }

// printf("ESC %c %c %ld %c\n", pcl_parm, pcl_grp, pcl_value, pcl_cmd);
}


void parse_pcl(u08 data)
{
int i;

   if(pcl_state == 0) {  // look for ESC or pass data through to the printer
      if(data == 0x1B) pcl_state = 1;
      else {  
         pass_through:
         // Here we should pass data through to the "printer"
         // Note that the HP5371A puts a 00 byte after each PCL command
         // Hopefully your printer ignores it
         pcl_state = 0;
      }
   }
   else if(pcl_state == 1) {  // get pcl parm code (usually *,%)
      pcl_parm = pcl_grp = 0;
      if((data >= 48) && (data <= 126)) {  // single char commands 0..~ 
         pcl_value = 0;
         pcl_next_state = 0; // no more info follows this command
         do_pcl_cmd(data);   // go decode and act on it
      }
      else if((data >= 33) && (data <= 47)) {  // parm code (!../) is followed by parameters 
         pcl_parm = data;
         pcl_state = 2;
      }
      else if(data == 0x1B) goto pass_through;  //ESC ESC - send ESC to the printer?
      else pcl_state = 99;   // unknown parm value, so skip over the command
   }
   else if(pcl_state == 2) {  // looking for pcl command group code
      pcl_value = 0;    // used to parse a number that might follow this 
      pcl_sign = (1);
      pcl_decimal = 0;
      pcl_divisor = 1;

      if((data >= 96) && (data <= 126)) {  // group codes are mostly lower case chars
         pcl_grp = data;
         pcl_encoding = 0;
         pcl_state = 3;  // got group code,  now get the optional numeric argument
      }
      else if((data == '+') || (data == '-') || (data == '.') || ((data >= '0') && (data <= '9'))) {
         // there is no group code,  but there is a numeric value
         pcl_state = 3;
         goto pcl_state_3;
      }
      else pcl_state = 99;   // invalid group code, so skip the command
   }
   else if(pcl_state == 3) {  // get (optional) numeric arg (could be doubleing point)
      pcl_state_3:
      if(data == '+')      pcl_sign = 1;     //!!! multiple decimals and signs should cause an error
      else if(data == '-') pcl_sign = (0-pcl_sign);
      else if(data == '.') pcl_decimal = 1;
      else if((data >= '0') && (data <= '9')) {
         pcl_value = (pcl_value*10) + (data-'0');
         if(pcl_decimal) pcl_divisor *= 10;
      }
      else if((data >= 64) && (data <= 94)) { // upper case (and some punctuation)
         pcl_value = (pcl_value * pcl_sign) / pcl_divisor;
         pcl_next_state = 0;   // this is the end of a PCL command sequence
         do_pcl_cmd(data);
      }
      else if((data >= 96) && (data <= 126)) { // lower case (and some punctuation)
         pcl_value = (pcl_value * pcl_sign) / pcl_divisor;
         pcl_next_state = 2;   // more commands in the same parm group follow this one
         do_pcl_cmd(data);
      }
      else pcl_state = 99;  // bad syntax, skip command
   }
   else if(pcl_state == 10) { // ESC*b#W seen - now we are getting raster data
      if(pcl_value > 0) {
         if(pcl_index < PCL_BUF_LEN) {
            pcl_buf[pcl_index++] = data;
         }
         --pcl_value;

         if(pcl_value == 0) {  // all bytes received, output the raster dats
            if(pcl_encoding == 0) {  // uncompressed
               for(i=0; i<pcl_index; i++) pcl_byte(pcl_buf[i]);
               pcl_bit(0xFF);  // output final run
            }
            else if(pcl_encoding == 1) {  // run length encoding
               for(i=0; i<pcl_index; i+=2) {
                  pcl_rep(pcl_buf[i], pcl_buf[i+1]);
               }
               pcl_bit(0xFF);  // output final run
            }
            else if(pcl_encoding == 2) {  // TIFF encoding
               for(i=0; i<pcl_index; ){
                  data = pcl_buf[i];
                  if(data == 0x80) {  // no-op
                  }
                  else if(data & 0x80) {  // repeated byte count
                     data = 0 - data;
                     ++i;
                     pcl_rep(data, pcl_buf[i]);
                  }
                  else { // uncompressed run
                     ++data;
                     while(data--) pcl_byte(pcl_buf[++i]);
                  }
                  ++i;
               }
               pcl_bit(0xFF);  // output final run
            }
            pcl_index = 0;
            pcl_state = 0;
         }
      }
   }
   else if(pcl_state == 99) {  // skip to end of PCL command (mostly an uppercase char)
      if((data >= 64) && (data <= 94)) {
         pcl_state = 0;
         in_hpgl = 1;
      }
      else if(data == 0x1B) pcl_state = 1;
   }
   else {
      pcl_state = 0;  // should never happen
      in_hpgl = 1;
   }
}
#endif


//
//
//   HPGL renderer
//
//

//
//
//  Coordinate scaling and screen mapping routines
//
//

int scale_x(double x)
{
   // convert graphics X coord into screen coord
   if(0 && plotter_mode) {
      if(gl_user_scaling) {
         x = ((x-gl_sc_ymin) / (gl_sc_ymax-gl_sc_ymin)) * (gl_ip_ymax-gl_ip_ymin);
         x += gl_ip_ymin;
      }

      x = ((ROWS-1) * (x-GL_YMIN)) / (GL_YMAX-GL_YMIN);

      if(gl_rotate_coords & 0x02) x = ROWS-x-1;

      return ((int) x)+TOP_MARGIN;
   }
   else {
      if(gl_user_scaling) {
         x = ((x-gl_sc_xmin) / (gl_sc_xmax-gl_sc_xmin)) * (gl_ip_xmax-gl_ip_xmin);
         x += gl_ip_xmin;
      }

      x = ((COLS-1) * (x-GL_XMIN)) / (GL_XMAX-GL_XMIN);

      if(gl_rotate_coords & 0x02) x = COLS-x-1;

      return ((int) x)+LEFT_MARGIN;
   }
}


int scale_y(double y)
{
   // convert graphics Y coord into screen coord
   if(0 && plotter_mode) {
      if(gl_user_scaling) {
         y = ((y-gl_sc_xmin) / (gl_sc_xmax-gl_sc_xmin)) * (gl_ip_xmax-gl_ip_xmin);
         y += gl_ip_xmin;
      }

      y = ((COLS-1) * (y-GL_XMIN)) / (GL_XMAX-GL_XMIN);

      if((gl_rotate_coords & 0x02) == 0) y = COLS-y-1;

      return ((int) y)+LEFT_MARGIN;
   }
   else {
      if(gl_user_scaling) {
         y = ((y-gl_sc_ymin) / (gl_sc_ymax-gl_sc_ymin)) * (gl_ip_ymax-gl_ip_ymin);
         y += gl_ip_ymin;
      }

      y = ((ROWS-1) * (y-GL_YMIN)) / (GL_YMAX-GL_YMIN);

      if((gl_rotate_coords & 0x02) == 0) y = ROWS-y-1;

      return ((int) y)+TOP_MARGIN;
   }
}

double unscale_x(COORD x)
{
double fx;

   // convert screen X coord into graphics coord
   if(0 && plotter_mode) {
      x -= TOP_MARGIN;
      if(gl_rotate_coords & 0x02) x = ROWS-x-1;
      fx = x;
      fx = GL_YMIN + ( (GL_YMAX-GL_YMIN) * fx / ((double) (ROWS-1)) );

      if(gl_user_scaling) {
         fx = gl_sc_ymin + (((gl_sc_ymax-gl_sc_ymin) * (fx-gl_ip_ymin)) / (gl_ip_ymax-gl_ip_ymin));
      }
   }
   else {
      x -= LEFT_MARGIN;
      if(gl_rotate_coords & 0x02) x = COLS-x-1;
      fx = x;
      fx = GL_XMIN + ( (GL_XMAX-GL_XMIN) * fx / ((double) (COLS-1)) );

      if(gl_user_scaling) {
         fx = gl_sc_xmin + (((gl_sc_xmax-gl_sc_xmin) * (fx-gl_ip_xmin)) / (gl_ip_xmax-gl_ip_xmin));
      }
   }

   return fx;
}

double unscale_y(COORD y)
{
double fy;

   // convert screen Y coord into graphics coord
   if(0 && plotter_mode) {
      y -= LEFT_MARGIN;
      if((gl_rotate_coords & 0x02) == 0) y = COLS-y-1;
      fy = y;
      fy = GL_XMIN + ( (GL_XMAX-GL_XMIN) * (fy) / ((double) (COLS-1)) );

      if(gl_user_scaling) {
         fy = gl_sc_xmin + (((gl_sc_xmax-gl_sc_xmin) * (fy-gl_ip_xmin)) / (gl_ip_xmax-gl_ip_xmin));
      }
   }
   else {
      y -= TOP_MARGIN;
      if((gl_rotate_coords & 0x02) == 0) y = ROWS-y-1;
      fy = y;
      fy = GL_YMIN + ( (GL_YMAX-GL_YMIN) * (fy) / ((double) (ROWS-1)) );

      if(gl_user_scaling) {
         fy = gl_sc_ymin + (((gl_sc_ymax-gl_sc_ymin) * (fy-gl_ip_ymin)) / (gl_ip_ymax-gl_ip_ymin));
      }
   }

   return fy;
}

double rotate_x(double x, double y)
{
   // rotate x coord into y axis
   if(gl_rotate_coords & 0x01) {
      if(gl_user_scaling) {
         y = (y-gl_sc_ymin);
         y = (gl_sc_ymax-gl_sc_ymin) - y - 1;
         x = y * (gl_sc_xmax-gl_sc_xmin) / (gl_sc_ymax-gl_sc_ymin);
         x += gl_sc_xmin;
      }
      else {
         y = (y-gl_ip_ymin);
         y = (gl_ip_ymax-gl_ip_ymin) - y - 1;
         x = y * (gl_ip_xmax-gl_ip_xmin) / (gl_ip_ymax-gl_ip_ymin);
         x += gl_ip_xmin;
      }
   }
   return x;
}

double rotate_y(double x, double y)
{
   // rotate y coord into x axis
   if(gl_rotate_coords & 0x01) {
      if(gl_user_scaling) {
         x = (x-gl_sc_xmin);
         y = x * (gl_sc_ymax-gl_sc_ymin) / (gl_sc_xmax-gl_sc_xmin);
         y += gl_sc_ymin;
      }
      else {
         x = (x-gl_ip_xmin);
         y = x * (gl_ip_ymax-gl_ip_ymin) / (gl_ip_xmax-gl_ip_xmin);
         y += gl_ip_ymin;
      }
   }
   return y;
}


void gl_clip_window(u08 flag)
{
int ix1,iy1;
int ix2,iy2;
int t;
u08 oldc;

// if(1 && (gl_clip == 1)) {  // setup screen clipping window
   if(gl_clip) {  // setup screen clipping window
      oldc = gl_user_scaling;
      ix1 = scale_x(rotate_x(gl_clip_x1,gl_clip_y1));
      iy1 = scale_y(rotate_y(gl_clip_x1,gl_clip_y1));

      ix2 = scale_x(rotate_x(gl_clip_x2,gl_clip_y2));
      iy2 = scale_y(rotate_y(gl_clip_x2,gl_clip_y2));
      gl_user_scaling = oldc;

      if(ix1 > ix2) {
         t = ix1;
         ix1 = ix2;
         ix2 = t;
      }
      if(iy1 > iy2) {
         t = iy1;
         iy1 = iy2;
         iy2 = t;
      }

      VP.WinX1 = ix1;
      VP.WinY1 = iy1;
      VP.WinX2 = ix2;
      VP.WinY2 = iy2;
      select_viewport(VP);  // inform viewing transforms and clipper about current viewport
      gl_clip = 2;
//printf("Clip %d: %d,%d  %d,%d\n", flag, ix1,iy1,  ix2,iy2);
   }
   else if(gl_clip == 0) {
      VP.WinX1 = 0;
      VP.WinY1 = 0;
      VP.WinX2 = SCREEN_SIZE_X-1;    // these are the full screen size, not our window size
      VP.WinY2 = SCREEN_SIZE_Y-1;
   }
}


//
//
//   Map pen color to screen color
//
#define GL_BLACK    0  
#define GL_GREEN   10     
#define GL_MAGENTA 13
#define GL_BROWN    5
#define GL_WHITE   15 
#define GL_GREY     8
#define GL_BLUE     9
#define GL_CYAN    11 
#define GL_RED     12
#define GL_YELLOW  14

void gl_set_color(u08 pen)
{
COLOR gl_color;

set_color(pen);
return;

   // map pen to hardware color
   if(pen == 0) {
      if(background) gl_color = GL_WHITE;  
      else           gl_color = GL_BLACK;  
   }
   else if(pen == 1) {
      if(background) gl_color = GL_BLACK;  
      else           gl_color = GL_WHITE;  
   }
   else if(pen == 2) gl_color = GL_RED;
   else if(pen == 3) gl_color = GL_GREEN;
   else if(pen == 4) gl_color = GL_YELLOW;
   else if(pen == 5) gl_color = GL_BLUE;
   else if(pen == 6) gl_color = GL_MAGENTA;
   else if(pen == 7) gl_color = GL_CYAN;
   else {
      if(background) gl_color = GL_BLACK;  
      else           gl_color = GL_WHITE;  
   }

   #ifdef CPU_CLOCK     // kluge for identifying MegaDonkey compile
      set_color(WHITE);
      set_bg(BLACK);
   #else 
      set_color(gl_color); // set hardware drawing color
   #endif

}



//
//
//   Character and text output routines
//
//
void gl_char_space(u08 flag)
{
double vx,vy;
int sx,sy;

   // calculate text char spacing in graphics coordinates
   if(gl_rel_char == 2) {  // mmmm
      if(gl_user_scaling && (gl_sc_xmax != gl_sc_xmin) && (gl_sc_ymax != gl_sc_ymin)) {
         vx = gl_char_sx * (gl_sc_xmax-gl_sc_xmin);
         vy = gl_char_sy * (gl_sc_ymax-gl_sc_ymin);
printf("user chars:%g,%g  vxy:%g,%g\n", gl_char_sx,gl_char_sy, vx,vy);
      }
      else return;
   }
   else if(gl_rel_char) {
      if(gl_user_scaling) {
         vx = gl_char_sx * COLS; 
         vy = gl_char_sy * ROWS; 
      }
      else {
         vx = (unscale_x(1+LEFT_MARGIN)-unscale_x(0+LEFT_MARGIN));
         vy = (unscale_y(1+TOP_MARGIN)-unscale_y(0+TOP_MARGIN));
if(vx < 0.0) vx = 0.0-vx;
if(vy < 0.0) vy = 0.0-vy;
         vx = gl_char_sx * (gl_ip_xmax-gl_ip_xmin) / vx;
         vy = gl_char_sy * (gl_ip_ymax-gl_ip_ymin) / vy;
      }
   }
   else {  // convert cm to pixels to plotter units
      vx = gl_char_sx * ((double) COLS) / ((GL_XMAX-GL_XMIN) * MM_PER_GRID/10.0);
      vy = gl_char_sy * ((double) ROWS) / ((GL_YMAX-GL_YMIN) * MM_PER_GRID/10.0);
   }
 vx *= 0.87;  // a touch of downscale makes some graphs not overflow the edge
// vy *= 0.99;

   VX = vx;
   VY = vy;

   // round up calculated font size,  any excess oveflows into spacing
   #ifdef CPU_CLOCK     // kluge for identifying MegaDonkey compile
      sx=vx; sy=vy;                  // technically correct
//    sx=(vx+0.999); sy=(vy+0.999);  // round up font size
   #else
      sx=(int) (vx+2.999); sy=(int) (vy+2.999);      // allow font to pretty much fill char space
   #endif

//vx = sx;
//vy = sy;
   if(sx < 0.0) sx = 0-sx;
   if(sy < 0.0) sy = 0-sy;
   if(0) {  // !!! force minimally readable font size
      if(sx < 4) sx = 4;
      if(sy < 6) sy = 6;
   }
   vchar_set_fontsize(sx, sy);

   if(gl_user_scaling) {
      if(gl_dvx & 0x01) {  // up/down text
         gl_texty = ((gl_sc_xmax-gl_sc_xmin) * (vy / (double) COLS)); 
         gl_textx = ((gl_sc_ymax-gl_sc_ymin) * (vx / (double) ROWS)); 
      }
      else {               // left/right text
         gl_textx = ((gl_sc_xmax-gl_sc_xmin) * (vx / (double) COLS)); 
         gl_texty = ((gl_sc_ymax-gl_sc_ymin) * (vy / (double) ROWS)); 
      }
   }
   else {
      if(gl_dvx & 0x01) {  // up/down text
         gl_texty = ((gl_ip_xmax-gl_ip_xmin) * (vy / (double) COLS)); 
         gl_textx = ((gl_ip_ymax-gl_ip_ymin) * (vx / (double) ROWS)); 
      }
      else {               // left/right text
         gl_textx = ((gl_ip_xmax-gl_ip_xmin) * (vx / (double) COLS)); 
         gl_texty = ((gl_ip_ymax-gl_ip_ymin) * (vy / (double) ROWS)); 
      }
   }

// gl_spacex = gl_textx * 0.5;  // half char cell width
// gl_spacey = gl_texty;        // full char cell height
gl_spacex = gl_textx * 0.50;    // 1/4 char cell width
gl_spacey = gl_texty * 1.125;   // char cell height

   gl_spacex += (gl_spacex * gl_extra_x);
   gl_spacey += (gl_spacey * gl_extra_y);
#ifdef BIG_PLOT
//bbbb gl_spacex *= 6.0;  // !!!!!!!!! kludge
//bbbb gl_spacey *= 6.0;
gl_spacex *= 3.0;  // !!!!!!!!! kludge
gl_spacey *= 6.0;
#endif
// printf("char size %d: %g,%g\n", flag, gl_textx,gl_texty);
}

void gl_text_params(u08 flag)
{
double x1, y1;

   // setup vector char drawing orientation
   set_rotate(0x00);
   if(gl_rotate_chars == 3) {        // vertical up text
      if     (gl_rotate_coords == 0) set_rotate(ROT_CHAR_90 | ROT_CHAR_HORIZ);    // 0 deg
      else if(gl_rotate_coords == 1) set_rotate(ROT_CHAR_VERT | ROT_CHAR_HORIZ);  // 90 deg
      else if(gl_rotate_coords == 2) set_rotate(ROT_CHAR_90 | ROT_CHAR_VERT);     // 180 deg
      else if(gl_rotate_coords == 3) set_rotate(0x00);                            // 270 deg
   }
   else if(gl_rotate_chars == 2) {   // vertical down text
      if     (gl_rotate_coords == 0) set_rotate(ROT_CHAR_90 | ROT_CHAR_VERT);     // 0 deg
      else if(gl_rotate_coords == 1) set_rotate(0x00);                            // 90 deg
      else if(gl_rotate_coords == 2) set_rotate(ROT_CHAR_90 | ROT_CHAR_HORIZ);    // 180 deg
      else if(gl_rotate_coords == 3) set_rotate(ROT_CHAR_HORIZ | ROT_CHAR_VERT);  // 270 deg
   }
   else if(gl_rotate_chars == 1) {   // left going horizontal text
      if     (gl_rotate_coords == 0) set_rotate(ROT_CHAR_HORIZ | ROT_CHAR_VERT);  // 0 deg
      else if(gl_rotate_coords == 1) set_rotate(ROT_CHAR_90 | ROT_CHAR_VERT);     // 90 deg
      else if(gl_rotate_coords == 2) set_rotate(0x00);                            // 180 deg
      else if(gl_rotate_coords == 3) set_rotate(ROT_CHAR_90 | ROT_CHAR_HORIZ);    // 270 deg
   }
   else {                            // normal horizontal text
      if     (gl_rotate_coords == 0) set_rotate(0x00);                            // 0 deg
      else if(gl_rotate_coords == 1) set_rotate(ROT_CHAR_90 | ROT_CHAR_HORIZ);    // 90 deg
      else if(gl_rotate_coords == 2) set_rotate(ROT_CHAR_VERT | ROT_CHAR_HORIZ);  // 180 deg
      else if(gl_rotate_coords == 3) set_rotate(ROT_CHAR_90 | ROT_CHAR_VERT);     // 270 deg
   }

   if(gl_char_sx < 0.0) {
      set_rotate(rotate ^ ROT_CHAR_HORIZ);
   }
   if(gl_char_sy < 0.0) {
      set_rotate(rotate ^ ROT_CHAR_VERT);
   }


   // tweak character offset for maximum natural goodness
   x1 = y1 = 0.0;
  
   if(gl_dvx == 3) { // up
      if(gl_rotate_coords == 3) { x1-=gl_texty; y1+=gl_textx; }
      else if(gl_rotate_coords == 2) { y1+=gl_textx; }
      else if(gl_rotate_coords == 1) { y1+=gl_textx+gl_spacex; }
      else { x1-=gl_texty; y1+=gl_textx*2; }
   }
   else if(gl_dvx == 1) { // down
      if(gl_rotate_coords == 3) { y1-=(gl_textx+gl_spacex);}
      else if(gl_rotate_coords == 2) { x1+=gl_texty; y1-=(gl_textx+gl_spacex);}
      else if(gl_rotate_coords == 1) { x1+=gl_texty+gl_spacey/4; y1-=gl_textx;}
      else { y1 -= gl_spacex;}
   }
   else if(gl_dvx == 2) {   //left
      if(gl_rotate_coords == 2)  {x1-=gl_textx; y1-=gl_texty;}
      else if(gl_rotate_coords == 1)  {}
      else if(gl_rotate_coords == 3)  {x1-=(gl_textx+gl_spacex);  y1-=gl_texty;}
      else { x1-=(gl_textx+gl_spacex); }
   }
   else {  // right (normal)
      if(gl_rotate_coords == 3) { x1+=(gl_spacex/2); y1+=gl_spacey/4; }
      else if(gl_rotate_coords == 2) { x1+=(gl_textx+gl_spacex); }
      else if(gl_rotate_coords == 1) { x1+=(gl_textx+gl_spacex); y1+=gl_texty; }
      else { x1+=gl_spacex; y1+=gl_texty; }
   }
  
   gl_text_ofsx = x1;
   gl_text_ofsy = y1;
}


void gl_vchar(double x1, double y1, int data)
{
int ix,iy;
double rx,ry;     //uuuu

   x1 += gl_text_ofsx;
   y1 += gl_text_ofsy;

   pause_line_pattern();
   if(0 && (gl_dl_font[gl_alt_font][data][0] != (-999999))) {
printf("draw user char %c: %f,%f\n", data, x1,y1);
      draw_user_char(x1,y1, gl_alt_font, 0, data);
   }
   else {
      rx = rotate_x(x1,y1);
      ry = rotate_y(x1,y1);
      ix = scale_x(rx);
      iy = scale_y(ry);
      vchar_char(ix,iy, data);
   }
   resume_line_pattern();
}


void gl_line_feed()
{
   if(gl_dvx == 3) {       // up text
      if(gl_dvy) gl_x -= (gl_texty+gl_spacey);  
      else       gl_x += (gl_texty+gl_spacey);  
   }
   else if(gl_dvx == 1) {  // down text
      if(gl_dvy) gl_x += (gl_texty+gl_spacey);  
      else       gl_x -= (gl_texty+gl_spacey);  
   }
   else if(gl_dvx == 2) {  // left text
      if(gl_dvy) gl_y -= (gl_texty+gl_spacey); 
      else       gl_y += (gl_texty+gl_spacey); 
   }
   else {                  // right text
      if(gl_dvy) gl_y += (1.0*gl_texty+gl_spacey); 
      else       gl_y -= (1.0*gl_texty+gl_spacey); 
   }
gl_cr_y = gl_cr_y;  // llll
// gl_cr_y = gl_y;  // llll
}


int gl_set_lo(double len, int do_cr); // llll

u08 gl_label_char(int data) //uuuu
{
   if(gl_td) goto label_it;  // transparent data mode - everything prints 


   if(data == 0x08) {      // BS
      if(gl_dvx == 3)      gl_y -= (gl_textx+gl_spacex); // up text
      else if(gl_dvx == 1) gl_y += (gl_textx+gl_spacex); // down text
      else if(gl_dvx == 2) gl_x += (gl_textx+gl_spacex); // left text 
      else                 gl_x -= (gl_textx+gl_spacex); // right text
      if(--gl_char_num < 0) gl_char_num = (GL_TAB_STOP+gl_char_num);
   }
   else if(data == 0x09) { // TAB
      gl_char_num = GL_TAB_STOP - (gl_char_num % GL_TAB_STOP);
      if(gl_dvx == 3)      gl_y += (gl_textx+gl_spacex) * gl_char_num; // up text
      else if(gl_dvx == 1) gl_y -= (gl_textx+gl_spacex) * gl_char_num; // down text
      else if(gl_dvx == 2) gl_x -= (gl_textx+gl_spacex) * gl_char_num; // left text 
      else                 gl_x += (gl_textx+gl_spacex) * gl_char_num; // right text
      gl_char_num = 0;
   }
   else if(data == 0x0A) { // LF
//printf("line feed glxy:%g,%g  loxy:%g,%g", gl_x,gl_y, gl_lo_x,gl_lo_y);
      gl_line_feed();
      return 1;
//printf("  new glxy:%g,%g  loxy:%g,%g\n", gl_x,gl_y, gl_lo_x,gl_lo_y);
   }
   else if(data == 0x0D) { // CR
      if(gl_dvx & 0x01) gl_y = gl_cr_y;  // up/down
      else              gl_x = gl_cr_x;  // right/left
      gl_char_num = 0;
   }
   else if(data == 0x0E) { // shift out - should select alt font
      gl_cr_x=gl_x;  gl_cr_y=gl_y;
      gl_alt_font = 1;    //llll
      gl_char_num = 0;
   }
   else if(data == 0x0F) { // shift in - should select std font
      gl_cr_x=gl_x;  gl_cr_y=gl_y;
      gl_alt_font = 0;    //llll
      gl_char_num = 0;
   }
   else if(data < ' ') {   // ignore all other control chars
   }
   else if(0 && (data > 0x7E)) {  // ignore all other non-ASCII chars
   }
   else {   // printing character
      label_it:
      if((gl_charset == 3) && (data == '[')) data = '0';

//printf("label char %c(%02X): txy:%g,%g  sxy:%g,%g  at:%g,%g\n", 
//data,data, gl_textx,gl_texty, gl_spacex,gl_spacey, gl_x,gl_y);
      gl_vchar(gl_x+gl_lo_x, gl_y+gl_lo_y, data);

      if(gl_dvx == 3)      gl_y += (gl_textx+gl_spacex); // up text
      else if(gl_dvx == 1) gl_y -= (gl_textx+gl_spacex); // down text
      else if(gl_dvx == 2) gl_x -= (gl_textx+gl_spacex); // left text 
      else                 gl_x += (gl_textx+gl_spacex); // right text
      ++gl_char_num;
   }
   return 0;
}

void gl_new_coords(u08 flag)
{
   // setup various coordinate/rotation related values
   gl_char_space(flag);
   gl_text_params(flag);
   gl_clip_window(flag);
}

//  
//  If we store labels in a buffer,  we can implement the 
//  "LO" label offset command to center/justify labels
//  and the "BL" Buffer Label command and the "PB" Print Buffer command
//

#define BUFFER_LABELS 256

#ifdef BUFFER_LABELS
char gl_label[BUFFER_LABELS+1];
int gl_label_index;
u08 gl_save_only = 0;  // print/store label flag

u08 gl_buffer_label(u08 data)
{
   if(data) {
      if((data != gl_term) || (gl_term_type == 0)) {  // add char to label buffer
         if(gl_label_index < BUFFER_LABELS) {
            gl_label[gl_label_index++] = data;
            gl_label[gl_label_index] = 0;
         }
      }
      if(data == gl_term) data = 0;
   }
   return data;
}

  
/*
int gl_set_lo(double len, int do_cr)  // llll
{
int lo;
u08 concat;
double x;

   if(len < 0.0) len = 0.0;

   // label offsets  
   lo = (gl_lo_code % 10);
   if(lo == 0) lo = 1;
   gl_lo_x = gl_lo_y = 0;

   //!!! for centering to work properly we should scan the label for tabs, 
   //!!! backspaces and non-printing chars to get the actual printing length
printf("set lo: index:%d  len:%g\n", gl_label_index, len);

   concat = 0;
   if((lo == 1) || (lo == 2) || (lo == 3)) {  // left justify label;
      if(gl_dvx == 0) concat = 1;
      if(gl_dvx == 2) len += 0.5;
      gl_lo_x = 0.0; // 0.5*(gl_textx + gl_spacex);
      if(gl_dvx == 2) {
         if(gl_lo_code < 10) gl_lo_x -= gl_textx*1.00;
      }
//      gl_lo_x = 0.0 - (gl_textx + gl_spacex) * (len-gl_label_index);
   }
   else if((lo == 4) || (lo == 5) || (lo == 6)) {  // center justify label
      if(gl_dvx == 3) len += 0.5;
      else if(gl_dvx == 2) len += 1.0;
      gl_lo_x = 0.0 - (((gl_textx + gl_spacex) * len) / 2.0);
      gl_lo_x -= 0.5*(gl_textx + gl_spacex);
      if(gl_dvx == 1) {
         gl_lo_x += gl_textx*0.500;
      }
      else if(gl_dvx == 2) {
         if(gl_lo_code < 10) gl_lo_x += gl_textx*0.50;
         else                gl_lo_x += gl_textx*1.00;
      }
      else if(gl_dvx == 3) {
         if(gl_lo_code < 10) gl_lo_x += gl_textx*1.00;
      }
   }
   else if((lo == 7) || (lo == 8) || (lo == 9)) {  // right justify label
      if(gl_dvx == 2) concat = 1;
      if(gl_dvx == 2) len += 0.5;
      gl_lo_x = 0.0 - (((gl_textx + gl_spacex) * len));
      gl_lo_x -= 0.5*(gl_textx + gl_spacex);
      if(gl_lo_code >= 10) gl_lo_x -= gl_textx;
   }
   if(gl_lo_code >= 10) {
      gl_lo_x -= (gl_textx / 2.0);
   }


   if((lo == 1) || (lo == 4) || (lo == 7)) {
      if(gl_dvx == 1) concat = 1;
      if(gl_dvx == 3) gl_lo_y -= gl_texty;//gl_texty/3.0;
      else if(gl_dvx == 2) {
         gl_lo_y -= (gl_texty*1.00);
         if(gl_lo_code >= 10) {
            gl_lo_y -= (gl_texty*1.00);
         }
      }
      else if(gl_dvx == 1) gl_lo_y += gl_texty/3.0;
      if(gl_lo_code >= 10) {
         gl_lo_y += (gl_texty * 0.750);
      }
   }
   else if((lo == 2) || (lo == 5) || (lo == 8)) {
//    gl_lo_y -= (gl_texty / 2.0);
      if(gl_dvx == 3) gl_lo_y -= gl_texty/3.0;
      else if(gl_dvx == 1) gl_lo_y += gl_texty/3.0;
      if(gl_lo_code < 10) {
         gl_lo_y -= (gl_texty / 2.0);
      }
   }
   else if((lo == 3) || (lo == 6) || (lo == 9)) {
      if(gl_dvx == 3) concat = 1;
      if(gl_dvx == 3) gl_lo_y += gl_texty;
      else if(gl_dvx == 2) {
         gl_lo_y += gl_texty;
         if(gl_lo_code >= 10) {
            gl_lo_y += (gl_texty);
         }
      }
      else if(gl_dvx == 1) {
         gl_lo_y += gl_texty/3.0;
      }
      gl_lo_y -= gl_texty;
   }
   if(gl_lo_code >= 10) {
      gl_lo_y -= (gl_texty / 2.0);
   }

   if(gl_dvx == 3) {       // up text
      x = gl_lo_x;
      gl_lo_x = gl_lo_y;
      gl_lo_y = x;
      gl_lo_x *= (-1.0);
      if(gl_lo_code < 10) gl_lo_y -= gl_texty;
      else gl_lo_y += gl_texty;
   }
   else if(gl_dvx == 2) {  // left text
      gl_lo_x *= (-1.0);
      gl_lo_y *= (-1.0);
   }
   else if(gl_dvx == 1) {  // down text
      x = gl_lo_x;
      gl_lo_x = gl_lo_y;
      gl_lo_y = x;
      gl_lo_y *= (-1.0);
      gl_lo_y += gl_texty*1.50;
   }
   else {
      if(gl_lo_code < 10) gl_lo_x -= gl_textx;
   }

printf(" glloxy:%g,%g\n", gl_lo_x, gl_lo_y);
   return concat;
}
*/

  
int gl_set_lo(double len, int do_cr)  // llll
{
int lo;
u08 concat;
double x;

   if(len < 0.0) len = 0.0;

   // label offsets  
   lo = (gl_lo_code % 10);
   if(lo == 0) lo = 1;
   gl_lo_x = gl_lo_y = 0;

   //!!! for centering to work properly we should scan the label for tabs, 
   //!!! backspaces and non-printing chars to get the actual printing length
// printf("set lo: index:%d  len:%g\n", gl_label_index, len);

   concat = 0;
   if((lo == 1) || (lo == 2) || (lo == 3)) {  // left justify label;
      if(gl_dvx == 0) concat = 1;
      if(gl_dvx == 2) len += 0.5;
//      gl_lo_x = 0.0 - (gl_textx + gl_spacex) * (len-gl_label_index);
      gl_lo_x = 0.0; // 0.5*(gl_textx + gl_spacex);
   }
   else if((lo == 4) || (lo == 5) || (lo == 6)) {  // center justify label
      if(gl_dvx == 3) len += 0.5;
      else if(gl_dvx == 2) len += 1.0;
      gl_lo_x = 0.0 - (((gl_textx + gl_spacex) * len) / 2.0);
      gl_lo_x -= 0.5*(gl_textx + gl_spacex);
   }
   else if((lo == 7) || (lo == 8) || (lo == 9)) {  // right justify label
      if(gl_dvx == 2) concat = 1;
      if(gl_dvx == 2) len += 0.5;
      gl_lo_x = 0.0 - (((gl_textx + gl_spacex) * len));
      gl_lo_x -= 0.5*(gl_textx + gl_spacex);
   }
   if(gl_lo_code >= 10) {
      gl_lo_x -= (gl_textx / 2.0);
   }


   if((lo == 1) || (lo == 4) || (lo == 7)) {
      if(gl_dvx == 1) concat = 1;
      if(gl_dvx == 3) gl_lo_y -= 0.0;//gl_texty/3.0;
      else if(gl_dvx == 3) gl_lo_y += gl_texty/3.0;
      if(gl_lo_code >= 10) {
         gl_lo_y += (gl_texty * 0.750);
      }
   }
   else if((lo == 2) || (lo == 5) || (lo == 8)) {
//    gl_lo_y -= (gl_texty / 2.0);
      if(gl_dvx == 3) gl_lo_y -= gl_texty/3.0;
      else if(gl_dvx == 1) gl_lo_y += gl_texty/3.0;
      if(0 && (gl_lo_code < 10)) {    //mmmm
         gl_lo_y -= (gl_texty / 2.0);
      }
   }
   else if((lo == 3) || (lo == 6) || (lo == 9)) {
      if(gl_dvx == 3) concat = 1;
      if(gl_dvx == 3) gl_lo_y -= gl_texty/3.0;
      else if(gl_dvx == 1) gl_lo_y += gl_texty/3.0;
      gl_lo_y -= gl_texty;
   }
   if(gl_lo_code >= 10) {
      gl_lo_y -= (gl_texty / 2.0);
   }

   if(gl_dvx == 3) {       // up text
      x = gl_lo_x;
      gl_lo_x = gl_lo_y;
      gl_lo_y = x;
      gl_lo_x *= (-1.0);
   }
   else if(gl_dvx == 1) {  // down text
      x = gl_lo_x;
      gl_lo_x = gl_lo_y;
      gl_lo_y = x;
      gl_lo_y *= (-1.0);
//mmmm      gl_lo_y += gl_texty;
   }
   else if(gl_dvx == 2) {  // left text
      gl_lo_x *= (-1.0);
      gl_lo_y *= (-1.0);
   }

//printf(" glloxy:%g,%g\n", gl_lo_x, gl_lo_y);
   return concat;
}
  

void gl_print_buffer(char *buf, int do_lo)
{
int i, j;
int gl_label_index;
int gl_label_start;   //llll
int len;
u08 concat;
double x,y;
int old_color;
int old_width;
double old_space;

old_color = color;
old_width = VCharWidth;
old_space = gl_spacex;
if(color == 0) {     // !!!!! kludge for added captions
   color = 1;
   VCharWidth *= 3;
   VCharWidth /= 4;
///gl_spacex *= 1.0;   //llll
///gl_spacex /= 32.0;
}

   if(1 || do_lo) {
      concat = gl_set_lo(0, do_lo);  //llll
   }
   else {
      gl_lo_x = gl_lo_y = 0.0;
      concat = 0;
   }

   x=gl_x; y=gl_y;

   gl_label_index = strlen(buf);
   gl_label_start = 0;
 if(gl_label_index == 0) goto no_label;
//printf("print label(%02X) st:%d  ndx:%d  do_lo:%d,%d: {%s}\n", buf[0], gl_label_start,gl_label_index, do_lo,gl_lo_code, buf);
   len = 0;
   for(i=0; i<gl_label_index; i++) {
//printf("%c", buf[i]);
      if((buf[i] == 0x0A) || (i == (gl_label_index - 1))) {  // label end-of_line;
//printf("\nlen2:%d\n", len);
         if(1 || do_lo) concat = gl_set_lo((double) len, do_lo);  // llll
         for(j=gl_label_start; j<=i; j++) {
//printf("%c(%d) ", buf[j], j);
            gl_label_char(buf[j]);
         }
//printf("\n\n");
         gl_label_start = i+1;
         len = 0;
      } 
      else if(buf[i] != 0x0D) {
         ++len;
      }
      else {
//printf("saw 0x0D\n");
      }
   }

   no_label:
   if(0 && (concat == 0)) {   //!!! non-concatenation is proper... but may mess up legacy data
      gl_x=x;  gl_y=y;
   }

color = old_color;
VCharWidth = old_width;
gl_spacex = old_space;
}
#endif


//
//
//  Line patterning and drawing
//
//
#define PM

#define MAX_LINE_TYPES  (8+1)  // 0..8
#define USER_LINE_TYPES 8      // 1..8

// line type patterns: 
//    number of run segments, [pen down percent, pen up percent], ...
u08 lt0[] PM = { 0 };          // line type 0 is special          
u08 lt1[] PM = { 2, 0,100 };
u08 lt2[] PM = { 2, 50,50 };
u08 lt3[] PM = { 2, 70,30 };
u08 lt4[] PM = { 4, 80,10, 0,10 };
u08 lt5[] PM = { 4, 70,10, 10,10 };
u08 lt6[] PM = { 6, 50,10, 10,10, 10,10 };
u08 lt7[] PM = { 6, 70,10,  0,10,  0,10 };
u08 lt8[] PM = { 8, 50,10,  0,10, 10,10, 0,10 };

u08 *lt[9] PM = {
   &lt0[0],
   &lt1[0], 
   &lt2[0], 
   &lt3[0], 
   &lt4[0], 
   &lt5[0], 
   &lt6[0], 
   &lt7[0], 
   &lt8[0] 
};


#ifdef USER_LINE_TYPES
u08 gl_user_linetype;
u08 gl_user_pattern[USER_LINE_TYPES+1][MAX_PATTERN_LENGTH+1];
#endif


void restart_line_pattern()
{
   gl_set_color(gl_pen);  // restore original pen color
   if(line_pattern) {     // line pattern is currently enabled
      enable_line_pattern();  // start it over at the beginning
   }
}

void gl_setup_pattern()
{
double d;
u08 *lpat;
int i;
u08 pat_val;

   d = sqrt((double)ROWS*(double)ROWS + (double)COLS*(double)COLS);
   d *= 3.17;  // adjust scale factor
   if(gl_lt_rel) {  // relative pattern is percent of screen diagnonal
      d = (d * gl_lt_len) / 100.0;
   }
   else {  // abs pattern is mm (diag is 325.28 mm)
      d = (d * gl_lt_len) / GL_DIAG_MM;
   }

   #ifdef USER_LINE_TYPES
      if(gl_lt && gl_user_pattern[gl_lt][0]) lpat = &gl_user_pattern[gl_lt][0];
      else lpat = lt[gl_lt];
   #else
      lpat = lt[gl_lt];
   #endif

   pattern_length = lpat[0];
   if(pattern_length > MAX_PATTERN_LENGTH) pattern_length = MAX_PATTERN_LENGTH;
   
   for(i=1; i<=pattern_length; i++) {  // scale run length percenatages into pixel counts
      pat_val = (u08) ((d * (double) lpat[i]) / 100.0); 
      if(pat_val == 0) { // make sure at least one pixel per run
         if(1 || lpat[i] || (i == 1)) pat_val = 1;
      }
      pattern_list[i-1] = pat_val; 
   }

   restart_line_pattern();
}

// we use line to draw dots so that clipping/patterning is consistent with lines
#define gl_dot(x,y) gl_line((x),(y),  (x),(y), 1, gl_pw[gl_pen])

#define POLYGONS 50000
u08 gl_poly_number = 0;       // if non-zero we are defining a polygon
int gl_poly_index = 0;        // next available entry in polygon vertex list
int gl_poly_start = 0;
double gl_poly_x, gl_poly_y;  // save current drawing position here while defining a polygon
u08 poly_closed = 1;          // used to make first move in polygon with pen up

#define POLY_PENDOWN 0x80

struct POLY_POINT {  // the polygon vertex buffer
   u08 flags;
   double x,y;
} gl_poly_buf[POLYGONS];


u08 gl_add_poly(double x,double y, u08 flags)
{
   if(gl_poly_index >= POLYGONS) return 1;
   if(poly_closed == 2) return 0;  // circle is treated as a sub-polygon

//printf("poly add %d: %f,%f  flags:%02X closed:%d  start:%d\n", gl_poly_index, x,y, flags, poly_closed, gl_poly_start);
   gl_poly_buf[gl_poly_index].flags = flags;
   gl_poly_buf[gl_poly_index].x = x;
   gl_poly_buf[gl_poly_index].y = y; 
   ++gl_poly_index;
   return 0;
}

void gl_close_polygon()
{
// if(gl_poly_start == gl_poly_index) return;  // no polygon to close
   if(gl_poly_number == 0) return;
   if(gl_poly_index == 0) return;

//   if(
//      (gl_poly_buf[gl_poly_index-1].x == gl_poly_buf[gl_poly_start].x) &&
//      (gl_poly_buf[gl_poly_index-1].y == gl_poly_buf[gl_poly_start].y)
//   ) return;  // polygon is already closed

   // add pen-up edge to close the polygon
//gl_add_poly(
//   gl_poly_buf[gl_poly_start].x,gl_poly_buf[gl_poly_start].y, 
//   gl_poly_buf[gl_poly_start].flags | (POLY_PENDOWN)
//);
   gl_add_poly(
      gl_poly_buf[gl_poly_start].x,gl_poly_buf[gl_poly_start].y, 
////  (gl_poly_buf[gl_poly_start].flags & (~POLY_PENDOWN)) | 0x20
      (gl_poly_buf[gl_poly_start].flags | 0x20 | POLY_PENDOWN)
   );
poly_closed = 1;  //3
   return;
}

void gl_line(double x1,double y1, double x2,double y2, u08 pen_down, u08 width);

void gl_render_polygon(u08 fill)
{
double x,y;
double x0,y0;
int i;

   // !!! for all this to be worthwhile,  we need to handle fill
   x0 = 0.0;  //qqqq
   y0 = 0.0;
   x = gl_x;
   y = gl_y;
//printf("render poly %f,%f: index=%d\n", x0,y0, gl_poly_index);  //pppp
   for(i=0; i<gl_poly_index; i++) {
//printf("  %d: %f,%f .. %f,%f flags:%02X\n", i, x,y, gl_poly_buf[i].x,gl_poly_buf[i].y, gl_poly_buf[i].flags);
      if((gl_poly_buf[i].flags & POLY_PENDOWN) || fill) {  // draw line from last point to this one
         gl_line(
            x,y, 
            x0+gl_poly_buf[i].x, y0+gl_poly_buf[i].y, 
            (gl_poly_buf[i].flags & POLY_PENDOWN), 
            gl_pw[gl_pen]
         );
      }

      x = x0+gl_poly_buf[i].x;
      y = y0+gl_poly_buf[i].y;
   }
}

void gl_line(double x1,double y1, double x2,double y2, u08 pen_down, u08 width)
{  // pen width == 0 (1 pixel wide)
int ix1,iy1;
int ix2,iy2;
int x,y;                 // stuff used to do patterned lines
u08 temp_pattern;
u08 temp_pattern_count;
COLOR temp_color;

   #ifdef POLYGONS
      if(gl_poly_number) {   // in polygon definition mode - record the vertices in the polygon buffer
if(1) ; else
         if((gl_poly_buf[gl_poly_index-1].x != x1) ||
            (gl_poly_buf[gl_poly_index-1].y != y1)) 
         {
            gl_add_poly(x1,y1, gl_poly_number | 0x10); // pen up move to start point
         }

         if(poly_closed) pen_down &= (~POLY_PENDOWN);
         else if(pen_down) pen_down = POLY_PENDOWN;
         gl_add_poly(x2,y2, gl_poly_number | pen_down);
         if(poly_closed) --poly_closed;
poly_closed = 0;
         return;
      }
   #endif

   if(pen_down == 0) return;    // don't draw the line

   ix1 = scale_x(rotate_x(x1,y1));    // convert graphics coords to screen coords
   iy1 = scale_y(rotate_y(x1,y1));
   ix2 = scale_x(rotate_x(x2,y2));
   iy2 = scale_y(rotate_y(x2,y2));

   if(line_pattern && (pattern_length == 0) && (gl_lt == 0)) {  // special line type,  only one dot per line
      gl_set_color(gl_pen);
      ix2 = ix1;
      iy2 = iy1;
   }

   temp_pattern = line_pattern;   // save current line pattern residue
   temp_pattern_count = pattern_count;
   temp_color = color;
   for(y=0; y<=width; y++) {
      for(x=0; x<=width; x++) {
         line_pattern = temp_pattern;       // restore line pattern residue
         pattern_count = temp_pattern_count;
         set_color(temp_color);
         clipped_line(ix1+x,iy1+y, ix2+x,iy2+y);
      }
   }
}

void gl_move_pen()
{
   gl_line(gl_x,gl_y,  gl_newx,gl_newy, gl_pendown, gl_pw[gl_pen]);
   
   gl_x = gl_newx;
   gl_y = gl_newy;

   if(gl_symbol) {  // draw marker symbol at each endpoint
//    gl_vchar(gl_x-gl_textx*6.0/8.0,gl_y-gl_texty*5.0/8.0, gl_symbol);
#ifdef BIG_PLOT
//    gl_vchar(gl_x-gl_spacex,gl_y-gl_texty*4.0/8.0, gl_symbol);
      gl_vchar(gl_x-gl_textx*0.875,gl_y-gl_texty*4.0/8.0, gl_symbol);
#else
      gl_vchar(gl_x-gl_textx*0.875,gl_y-gl_texty*4.0/8.0, gl_symbol);
#endif
   }

   gl_cr_x=gl_x; gl_cr_y=gl_y;
   gl_char_num = 0;
}


//
//
//   Shape drawing routines
//
//
void gl_box(double x1,double y1, double x2, double y2, u08 fill)
{
int ix1,iy1;
int ix2,iy2;
u08 temp_pattern;
u08 temp_pattern_count;
COLOR temp_color;

   temp_pattern = line_pattern;   // save current line pattern residue
   temp_pattern_count = pattern_count;
   temp_color = color;

   ix1 = scale_x(rotate_x(x1,y1));
   iy1 = scale_y(rotate_y(x1,y1));
   ix2 = scale_x(rotate_x(x2,y2));
   iy2 = scale_y(rotate_y(x2,y2));

   if(fill) clipped_filled_box(ix1,iy1, ix2,iy2);
   else     clipped_box(ix1,iy1, ix2,iy2);

   line_pattern = temp_pattern;       // restore line pattern residue
   pattern_count = temp_pattern_count;
   set_color(temp_color);
}


void gl_wedge(double x,double y,  double r, double a1,double a2, u08 wedge)
{  // tttt
double x1,y1;
double x2,y2;
double incr;
double ang;
double d, b;
double chord_angle;
int steps;
u08 temp_pattern;
u08 temp_pattern_count;
COLOR temp_color;

   temp_pattern = line_pattern;  // save current line pattern residue
   temp_pattern_count = pattern_count;
   temp_color = color;

   incr = DEG_TO_RAD;

   if(a1 || a2) steps = 1;
   else steps = 0;

   if(gl_chord_angle <= 0.0) chord_angle = DEFAULT_CHORD;
   else if(gl_ct_mode != 0) {   // do chord deviation mode
      d = (r - gl_chord_angle);
      if(d >= r) chord_angle = DEFAULT_CHORD;
      else {
         b = sqrt(r*r - d*d);
//printf("ct1 mode: r:%g d:%g, b:%g, ang:%g\n", r,d,b,gl_chord_angle);
         if(d == 0.0) chord_angle = 0.0;
         else chord_angle = 2.0 * atan(b / d);
         chord_angle /= DEG_TO_RAD;
//printf("    mode: r:%g d:%g, b:%g, ang:%g\n", r,d,b,chord_angle);
         if(chord_angle <= 0.0) chord_angle = DEFAULT_CHORD;
      }
   }
   else chord_angle = gl_chord_angle;

   if((a1 == a2) && steps) {
      ang = 360.0;
   }
   else if(a2 >= a1) {
      ang = (a2-a1);
   }
   else {
      ang = (a1-a2);
      incr = 0.0 - incr;
   }

   steps = (int) (ang / chord_angle);
   incr *= chord_angle;
   if(steps == 0) steps = 1;
   if(incr >= 0.0) incr = ang / steps;
   else            incr = 0.0 - (ang / steps);
//printf("circle: %f..%f ang:%g   steps:%d  chord:%g incr:%g\n", a1,a2, ang, steps, gl_chord_angle, incr);

   incr *= DEG_TO_RAD;
   a1 *= DEG_TO_RAD;
   a2 *= DEG_TO_RAD;

   x1 = x2 = r * cos(a1);
   y1 = y2 = r * sin(a1);
   pause_line_pattern();
   gl_dot(x+x1,y+y1);
   resume_line_pattern();

   while(steps--) {
      a1 += incr;
      x2 = r * cos(a1);
      y2 = r * sin(a1);
      gl_line(x+x1,y+y1, x+x2,y+y2, 1, gl_pw[gl_pen]);
      if(wedge == 2) {
         pause_line_pattern();
set_color(gl_pen);  //ccccc
         gl_line(x,y, x+x2,y+y2, 1, 0);
         resume_line_pattern();
      }

      x1=x2; y1=y2;
   }

   if(wedge) {
      gl_line(x,y, x+x1,y+y1, 1, gl_pw[gl_pen]);
   }
   else {
      gl_x = x+x2;
      gl_y = y+y2;
   }

   gl_cr_x=gl_x; gl_cr_y=gl_y;
   gl_char_num = 0;

   line_pattern = temp_pattern;       // restore line pattern residue
   pattern_count = temp_pattern_count;
   set_color(temp_color);
}

void gl_circle(double x,double y, double r) 
{
   gl_wedge(x,y, r, 0.0,360.0, 0);
   gl_x=x; gl_y=y;
}

void gl_arc(double x,double y, double theta)
{
double radius;
double a0;

   radius = sqrt((gl_x-x)*(gl_x-x) + (gl_y-y)*(gl_y-y));

   if(theta == 0.0) {
      a0 = 0.0;
   }
   else if(gl_x == x) {
      a0 = (90.0);
      if(gl_y < y) a0 = 0 - a0;
   }
   else {
      a0 = atan2(gl_y-y, gl_x-x) / DEG_TO_RAD;
   }
   theta += a0;

   gl_wedge(x,y, radius, a0,theta, 0);
}


#define det(a,b,c, d,e,f, g,h,i) ( ((a)*(e)*(i)) + ((b)*(f)*(g)) + ((c)*(d)*(h)) - ((g)*(e)*(c)) - ((h)*(f)*(a)) - ((i)*(d)*(b)) )

void gl_three_arc()
{
double x0, y0;
double r;
double a0, a2, a4;

   r = det(X1, Y1, 1.0,
           X2, Y2, 1.0,
           X4, Y4, 1.0) * 2.0;

   if(r == 0.0) {
      gl_line(X1,Y1, X2,Y2, 1, gl_pw[gl_pen]);
      gl_line(X3,Y3, X4,Y4, 1, gl_pw[gl_pen]);
      return;
   }

   x0 = det(X1*X1+Y1*Y1,  Y1,  1.0,
            X2*X2+Y2*Y2,  Y2,  1.0,
            X4*X4+Y4*Y4,  Y4,  1.0) / r;

   y0 = det(X1, X1*X1+Y1*Y1,  1.0,
            X2, X2*X2+Y2*Y2,  1.0,
            X4, X4*X4+Y4*Y4,  1.0) / r;

   r = sqrt( ((X1-x0)*(X1-x0)) + ((Y1-y0)*(Y1-y0)) );
// r = sqrt( ((X2-x0)*(X2-x0)) + ((Y2-y0)*(Y2-y0)) );
// r = sqrt( ((X4-x0)*(X4-x0)) + ((Y4-y0)*(Y4-y0)) );

   a0 = atan2(Y1-y0, X1-x0) / DEG_TO_RAD;
   a2 = atan2(Y2-y0, X2-x0) / DEG_TO_RAD;
   a4 = atan2(Y4-y0, X4-x0) / DEG_TO_RAD;

   gl_wedge(x0,y0, r, a0,a2, 0);
   gl_wedge(x0,y0, r, a2,a4, 0);
}


void gl_bez_curve(u08 pw)
{
double PrevX,PrevY;

   bezier_init(24L);   // 24 segments per point
   PrevX = X1;
   PrevY = Y1;

   while(DrawCmdActive) {             
      bezier_nextpoint();             
      gl_line(PrevX,PrevY, CurX,CurY, 1, pw);   
      PrevX = CurX;
      PrevY = CurY;
   }

   gl_cr_x=PrevX; gl_cr_y=PrevY;
   gl_char_num = 0;
}                   


//
//
//   Encoded polyline support (PE command)
//      (note that the example data in some HP documentation is wrong)
//
//
#ifdef ENCODED_POLYLINES
u08 gl_encode_base = 0;     // used to do encoded polylines
u32 gl_encoded_value = 0;   
u32 gl_encoded_mult = 1;
u08 gl_frac_bits = 0;

void gl_encoded_poly(u08 data)
{
int sx, sy;

   sy = 0;
   sx = data & 0x7F;
   if((sx >= 63) && (sx < (63+gl_encode_base))) {  // byte is part of a number
      if(gl_encode_base == 32) {
         sx = ((data+1) & 0x1F);
         if(data >= 95) sy = 1;    // last byte of value
      }
      else {
         sx = ((data+1) & 0x3F);
         if(data >= 127) sy = 1;   // last byte of value
      }
      gl_encoded_value = gl_encoded_value + (gl_encoded_mult * (u32) sx);
      gl_encoded_mult *= gl_encode_base;

      if(sy == 0) return;  // keep building this number

      gl_newx = gl_newy;
      if(gl_frac_bits) {
         gl_newy = (double) ((gl_encoded_value) & ((1L << gl_frac_bits)-1L)); 
         gl_newy /= (double) (1L << gl_frac_bits);
         gl_newy += (double) (gl_encoded_value >> (gl_frac_bits+1));
      }
      else gl_newy = gl_encoded_value;

      gl_newy /= 2.0;
      if(gl_encoded_value & 1) gl_newy = 0.0 - gl_newy;

      gl_encoded_value = 0;
      gl_encoded_mult = 1;

      if(gl_pair == 0) ++gl_pair;
      else if(gl_pair == 1) {  // relative move to coord
         gl_newx += gl_x;
         gl_newy += gl_y;
         goto pe_move;
      }
      else if(gl_pair == 10) ++gl_pair;
      else if(gl_pair == 11) {  // abs move to coord
         pe_move:
         gl_move_pen();
         gl_pendown = 1;
         gl_pair = 0;
      }
      else if(gl_pair == 20) {  // this value is the fractional bit count specifier
         if(gl_newy < 0.0) gl_newy = 0.0 - gl_newy;
         gl_frac_bits = (u08) gl_newy;
         gl_pendown = 1;
         gl_pair = 0;
      }
      else if(gl_pair == 30) {  // this value is the pen number
         restart_line_pattern();
         if((gl_newy >= 0) && (gl_newy < PEN_COUNT)) {
            gl_pen = (u08) gl_newy;
            gl_set_color(gl_pen);
         }
         gl_pendown = 1;
         gl_pair = 0;
      }
      else {
         gl_pendown = 1;
         gl_pair = 0;
      }
   }
   else {
      if(data == '=') gl_pair = 10;       // abs move to next coord
      else if(data == '>') gl_pair = 20;       // fractional bits follows
      else if(data == ':') gl_pair = 30;       // pen number follows
      else if(data == ';') {  // exit encoded polyline mode
         gl_encode_base = 0;  
         gl_state = 1;   
      }
      else if(data == '<') {  // pen up move to next coord
         gl_pendown = 0;  
         gl_pair &= 0xFE; 
      }
      else if(data == '7') {  // base 32 mode
         gl_encode_base = 32;  
         gl_pair = 0;
      }
   }

   return;
}
#endif

//
//  Polygon support only makes sense if one can fill polygons... we don't
//




//
//
//  Command decoding and parsing
//
//

void gl_reset_dl()
{
int i;

    for(i=0; i<257; i++) {
       gl_dl_font[0][i][0] = -999999;  // end-of char
       gl_dl_font[1][i][0] = -999999;  // end-of char
    }
}

void gl_set_defaults(u08 init)
{
int i;

    // setup default parameter values
    if(init >= 2) {  // hard reset everything
       gl_state = 0;
       gl_length = (GL_XMAX-GL_XMIN);
       gl_width = (GL_YMAX-GL_YMIN);
    }

    if(init >= 1) {  // INitialize values
       gl_x = gl_y = 0;
       gl_ip_xmin=GL_XMIN; gl_ip_xmax=GL_XMAX;
       gl_ip_ymin=GL_YMIN; gl_ip_ymax=GL_YMAX;
       gl_clip = 0;

       gl_pen = 1;
       gl_set_color(gl_pen);
       gl_pwu = 0;
       for(i=0; i<PEN_COUNT; i++) gl_pw[i] = 0;
    }

    // DFault parameter values
//  color = 0;
    gl_cmd[0] = 0;
    uc_pendown = 0;       // cccc user defined character pen state
    gl_reset_dl();        // reset download chars
    gl_ct_mode = 0;              
    gl_chord_angle = 0.0; //tttt
    gl_uc_ptr = 0;
    gl_newx = 0.0; gl_newy = 0.0;  // current command argument values
    gl_lo_x=0.0; gl_lo_y=0.0;        // label offset command params

    gl_cmt = 0;         // parser related stuff
    gl_saw_comma = 0;
    gl_alt_font = 0;
    gl_pair = 0;
    gl_argptr = 0;
    gl_arg1[gl_argptr] = gl_arg2[gl_argptr] = 0;

    gl_pendown = 0;     // pen movement stuff
    gl_moveabs = 1;
    gl_rel_cmd = 0;

if(sc_enabled) gl_user_scaling = 1;  //qqqq
else           gl_user_scaling = 0;   // scaling stuff
    gl_clip_x1=gl_ip_xmin;  gl_clip_y1=gl_ip_ymin;
    gl_clip_x2=gl_ip_xmax;  gl_clip_y2=gl_ip_ymax;

    gl_rel_char = 1;           // char size/spacing
    gl_char_sx = 0.75 / 100.0;
    gl_char_sy = 1.5 / 100.0;
    gl_extra_x = gl_extra_y = 0.0;

    gl_dvx = gl_dvy = 0;       // char orientation
    gl_rotate_chars = 0;
    gl_rotate_coords = user_rotate;

    set_rotate(0x00);
    for(i=0; i<VCHAR_H; i++) vchar_slant[i] = 0;

    gl_symbol = 0;             // misc char stuff
    gl_charset = 0;
    gl_lo_code = 0;

    if(force_lb_term) gl_term = force_lb_term;
    else              gl_term = 0x03;            // LAbel stuff
    gl_term_type = 1;
    gl_td = 0;
    gl_char_num = 0;
    gl_cr_x=gl_x; gl_cr_y=gl_y;

    gl_lt = 2;                 // line drawing stuff
    gl_lt_rel = 0;
    gl_lt_len = 4.0;
    line_pattern = 0;
    pattern_count = pattern_list[0];

    gl_pos_tick = gl_neg_tick = 0.5 / 100.0;

    #ifdef ENCODED_POLYLINES
       gl_encode_base = 0;
       gl_encoded_value = 0;
       gl_encoded_mult = 1;
       gl_frac_bits = 0;
    #endif

    #ifdef POLYGONS
       gl_poly_index = gl_poly_start = 0;
       gl_poly_number = 0;
    #endif

    gl_new_coords(1);
}


u08 new_glargs(u08 next_state)
{
   // setup to fetch another pair of (or single) command values
   gl_argptr = 0;
   gl_arg1[0] = 0;
   gl_arg2[0] = 0;
   gl_saw_comma = 0;
   gl_state = next_state;

   return next_state;
}

u08 GL_SEP(u08 c) 
{
   // return 1 if *c* is a separator character
   if     (c == ',')   return 1;
   else if(c == ' ')   return 1;
   else if(c == '\t')  return 1;
else if(gl_state == 10) return 0;  //llll
   else if(c == 0x0A)  return 1;
   else if(c == 0x0D)  return 1;
   else                return 0;
}


void draw_user_char(double x,double y, int font, int set_xy, int c)
{
int i;
double lastx,lasty;

   if(c < 0) return;
   if(c > 256) return;
   if((font < 0) || (font > 1)) return;

   if(gl_rotate_coords == 3) {
      if(gl_dvx == 3) {
         y += gl_textx*1.000; x -= gl_texty*1.000;  // up text
         if(set_xy == 0) y -= gl_texty;
         if(set_xy == 0) x += (gl_textx*1.500);
if(set_xy) x += gl_texty*1.00; // moves it down
      }
      else if(gl_dvx == 1) {
         y -= gl_textx*2.250; x -= gl_texty*0.000;  // down text
if(set_xy) ; else 
         if(set_xy == 0) y += gl_textx*1.000;
      }
      else if(gl_dvx == 2) {
         x -= gl_textx*1.500; y -= gl_texty*0.875;  // left text 
         if(set_xy == 0) y += gl_texty*1.000;
      }
      else {
         x += gl_textx*0.125; y += gl_texty*1.333;  // right text
        if(set_xy == 0) y -= gl_texty;
else y -= gl_texty;
      }
   }
   else if(gl_rotate_coords == 2) {
      if(gl_dvx == 3) {
         y += gl_textx*1.000; x -= gl_texty*1.000;  // up text
if(set_xy) x += gl_texty*1.00; // moves it left
      }
      else if(gl_dvx == 1) {
         y -= gl_textx*2.000; x -= gl_texty*0.000; // down text
         if(set_xy == 0) x -= (gl_textx*1.500);
if(set_xy) ; else 
         if(set_xy == 0) y += gl_textx;
      }
      else if(gl_dvx == 2) {
         x -= gl_textx*1.000; y -= gl_texty*0.750;  // left text 
         if(set_xy == 0) x -= gl_textx*0.500;
         if(set_xy == 0) y += (gl_texty*1.0);
      }
      else {
         x += gl_textx*0.500; y += gl_texty*0.250;  // right text
         if(set_xy == 0) x -= gl_textx;
      }
   }
   else if(gl_rotate_coords == 1) {
      if(gl_dvx == 3)      {
         y += gl_textx*1.250; x -= gl_texty*1.125; // up text
         if(set_xy == 0) y -= (gl_texty*1.50);
if(set_xy) x += gl_texty*0.875; // moves it down
if(set_xy) y -= gl_textx*0.750; // moves it left
      }
      else if(gl_dvx == 1) {
         y -= gl_textx*2.875; x += gl_texty*0.750; // down text
         if(set_xy == 0) x -= (gl_texty*3.000);
         if(set_xy == 0) y += (gl_textx*0.500);
      }
      else if(gl_dvx == 2) {
         x -= gl_textx*1.250; y -= gl_texty*0.750; // left text 
//       if(set_xy == 0) y -= gl_texty;
         if(set_xy == 0) x -= (gl_textx*1.500);
      }
      else {
         x += gl_textx*0.250; y += gl_texty*0.375; // right text
         if(set_xy == 0) x -= gl_textx;
         if(set_xy == 0) y -= gl_texty;
      }
   }
   else {
      if(gl_dvx == 3) {
         y += gl_texty*0.250; x -= gl_textx*1.000;  // up text
         if(set_xy == 0) y -= (gl_texty*0.250);
         if(set_xy == 0) x += (gl_textx*1.000);
if(set_xy) x += gl_texty*0.50; // lower char
      }
      else if(gl_dvx == 1) {
         y -= gl_textx*3.000; x -= gl_texty*0.000;  // down text
         if(set_xy == 0) y += (gl_textx*0.500);
      }
      else if(gl_dvx == 2) {
         x -= gl_textx*0.500; y -= gl_texty*1.000;  // left text 
      }
      else {
         x += gl_textx*0.500; y += gl_texty*0.063;  // right text
         if(set_xy == 0) y -= gl_texty;
      }
   }

   lastx = x;
   lasty = y;

   for(i=0; i<DL_LEN+4; i++) {
      if(gl_dl_font[font][c][i] == (-999999)) break;  // end of strokes

      gl_newx = (double) gl_dl_font[font][c][i];
      gl_newy = (double) gl_dl_font[font][c][i+1];

      if(gl_newx >= 99.0) {  // pen down code  
         uc_pendown = 1;
      }
      else if(gl_newx <= (-99.0)) {  // pen up code
         uc_pendown = 0;
      }
      else {  // draw the character stroke
         ++i;
         if(gl_rotate_chars == 1) {       // left text
            gl_newx *= (-1);
            gl_newy *= (-1);
         }
         else if(gl_rotate_chars == 2) {  // down text
            X1 = gl_newx;
            gl_newx = gl_newy;
            gl_newy = X1;
            gl_newy *= (-1);
         }
         else if(gl_rotate_chars == 3) {  // up text
            X1 = gl_newx;
            gl_newx = gl_newy;
            gl_newy = X1;
            gl_newx *= (-1);
         }
         else {
         }

         gl_newx *= (gl_textx/VX); 
         gl_newy *= (gl_texty/VY);

         if(set_xy) {
            gl_newx *= ((double)COLS/640.0);  // !!! tweak user char scale factor
            gl_newy *= ((double)ROWS/480.0);  // !!! why do we have to do this???
         }
         else {
            if(gl_dvx == 3)      gl_newy -= 1.0*(gl_textx+gl_spacex); // up text
            else if(gl_dvx == 1) gl_newy += 1.0*(gl_textx+gl_spacex); // down text
            else if(gl_dvx == 2) gl_newx += 1.0*(gl_textx+gl_spacex); // left text 
            else                 gl_newx -= 1.0*(gl_textx+gl_spacex); // right text

            if(0 && (rotate & ROT_CHAR_HORIZ)) {
               gl_newx = gl_newx - (gl_textx + gl_spacex);
            }
            if(0 && (rotate & ROT_CHAR_VERT)) {
               gl_newy = gl_newy + (gl_texty + gl_spacey);
            }

         ///gl_newy -= gl_spacey;
         #ifdef BIG_PLOT
            gl_newx = (gl_newx / 32.0) * gl_textx/4.0;
            gl_newy = (gl_newy / 32.0) * gl_texty*2.0;
         #else
            gl_newx = (gl_newx / 32.0) * (gl_textx)*2.0;
            gl_newy = (gl_newy / 32.0) * (gl_texty)*2.0;
         #endif
         }

         if(uc_pendown) {
            pause_line_pattern();
            gl_line(lastx,lasty, x+gl_newx,y+gl_newy, uc_pendown, 0);
            resume_line_pattern();
         }

         lastx = x+gl_newx;
         lasty = y+gl_newy;
         if(set_xy) {
            x = x+gl_newx;
            y = y+gl_newy;
         }
      }
   }

   if(set_xy) {
      if(gl_dvx == 3)      gl_y += (gl_textx+gl_spacex); // up text
      else if(gl_dvx == 1) gl_y -= (gl_textx+gl_spacex); // down text
      else if(gl_dvx == 2) gl_x -= (gl_textx+gl_spacex); // left text 
      else                 gl_x += (gl_textx+gl_spacex); // right text
   }
}

int dl_char;


double sx1,sy1;  // ssss
double sx2,sy2;
double sx3,sy3;
int sc_type;

double uc_lastx, uc_lasty; // kkkk


u08 gl_render_cmd(u08 data)
{
int sx, sy;
int font; 
//int cx1,cy1;
//int cx2,cy2;

   // EC - enable cutter
   // RM - ?
   // SA - select alternate font
   // SS - select standard font
   // VS - velocity select

//
//
//   For speed, the pen movement commands are checked first,
//   All the other commands are in alphabetical order.
//
   if(!strcmp(gl_cmd, "PA")) {  // absolute pen movements
      move_abs:
      gl_moveabs = 1;
      gl_cmd[0]='P';  gl_cmd[1]='A';

      do_move:
      if(gl_arg1[0] == 0) {  // no arguments, draw a single point line
         gl_newx=gl_x; gl_newy=gl_y;
      }

      gl_move_pen();
      if(GL_SEP(data)) { 
         return new_glargs(5);
      }
   }
   else if(!strcmp(gl_cmd, "PR")) {  // relative pen movements
      move_rel:
      gl_moveabs = 0;
      gl_cmd[0]='P';  gl_cmd[1]='R';

      gl_newx += gl_x;
      gl_newy += gl_y;
      goto do_move;
   }
   else if(!strcmp(gl_cmd, "PD")) {  // pen down (and move)
      gl_pendown = 0x01;
      if(gl_moveabs) goto move_abs;
      else           goto move_rel;
   }
   else if(!strcmp(gl_cmd, "PU")) {  // pen up (and move)
      gl_pendown = 0;
if(poly_closed) poly_closed = 2;
      if(gl_moveabs) goto move_abs;
      else           goto move_rel;
   }
   else if(!strcmp(gl_cmd, "AA")) {  // abs arc
      gl_rel_cmd = 0;

      draw_arc:
      if(gl_pair == 0) {  // arc center
         X1=gl_newx; Y1=gl_newy;
         if(gl_rel_cmd) {
            X1+=gl_x;  Y1+=gl_y;
         }
         ++gl_pair;
         if(GL_SEP(data)) return new_glargs(5);
      }
      else {    // angle
         if(gl_arg2[0]) gl_chord_angle = gl_newy; //tttt
         else           gl_chord_angle = 0.0;
         gl_arc(X1,Y1, gl_newx);   
      }
   }
   else if(!strcmp(gl_cmd, "AC")) {  // anchor corner - !!! do something
      restart_line_pattern();
   }
   else if(!strcmp(gl_cmd, "AF")) {  // new page
      new_page:
      gl_x = gl_y = 0.0;
   }
   else if(!strcmp(gl_cmd, "AR")) {  // relative arc
      gl_rel_cmd = 1;
      goto draw_arc;
   }
   else if(!strcmp(gl_cmd, "AT")) {  // three point arc
      gl_rel_cmd = 0;

      tp_arc:
      if(gl_pair == 0) {   
         X1=gl_x; Y1=gl_y;        // first coord
         X2=gl_newx; Y2=gl_newy;  // middle coord
         X3=X2; Y3=Y2;
         ++gl_pair;
         if(GL_SEP(data)) return new_glargs(5);
      }
      else if(gl_pair == 1) { // here we only fetch one arg, makes it easier to get the chord angle since it is a fifth argument
         X4 = gl_newx;
         strcpy(gl_arg1, gl_arg2);
         gl_argptr = 0;
         gl_arg2[gl_argptr] = 0;
         ++gl_pair;
         gl_state = 4;  // next we get the final one or two args
         return 4;
      }
      else {  // get that coordinate (and optionally the chord angle)
         Y4 = gl_newx;
         if(gl_arg2[0]) gl_chord_angle = gl_newy;
         else           gl_chord_angle = 0;

         if(gl_rel_cmd) {
            X2+=gl_x; Y2+=gl_y;
            X3+=gl_x; Y3+=gl_y;
            X4+=gl_x; Y4+=gl_y;
         }

         gl_three_arc();

//       gl_x=gl_newx;  gl_y=gl_newy;
         gl_cr_x=gl_x; gl_cr_y=gl_y;
         gl_char_num = 0;
      }
   }
   else if(!strcmp(gl_cmd, "BP")) {  // begin plot (we use to start a new plot)
      gl_set_defaults(1);
   }
   else if(!strcmp(gl_cmd, "BR")) {  // relative bezier curve
      gl_rel_cmd = 1;
      goto bezier;
   }
   else if(!strcmp(gl_cmd, "BZ")) {  // bezier curve
      gl_rel_cmd = 0;

      bezier:
      if(gl_pair == 0) {  // second control point
         X1=gl_x; Y1=gl_y;
         X2=gl_newx; Y2=gl_newy;
         ++gl_pair;
         if(GL_SEP(data)) return new_glargs(5);
      }
      else if(gl_pair == 1) {    // third control point
         X3=gl_newx; Y3=gl_newy;
         ++gl_pair;
         if(GL_SEP(data)) return new_glargs(5);
      }
      else if(gl_pair == 2) {    // last control point
         X4=gl_newx; Y4=gl_newy;
         if(gl_rel_cmd) {
            X2+=X1; Y2+=Y1;
            X3+=X1; Y3+=Y1;
            X4+=X1; Y4+=Y1;
         }
         gl_x=X4;  gl_y=Y4;

         gl_bez_curve(gl_pw[gl_pen]);

         gl_pair = 0;   // prepare to fetch new bezier parameters
         if(GL_SEP(data)) return new_glargs(5);
      }
   }
   else if(!strcmp(gl_cmd, "CA")) {  // character set
      gl_charset = (u08) gl_newx;
      gl_alt_font = 1;
   }
   else if(!strcmp(gl_cmd, "CI")) {  // circle (radius, tolerance)
      if(gl_arg2[0]) gl_chord_angle = gl_newy;  // tttt
      else           gl_chord_angle = 0.0;
      gl_close_polygon();
      gl_circle(gl_x, gl_y, gl_newx);
      poly_closed = 2;
   }
   else if(!strcmp(gl_cmd, "CP")) {  // move character position
// printf("cp: xy:%f   ofs:%f,%f  textx:%f,%f  space:%f,%f\n", gl_x,gl_y,  gl_newx,gl_newy, gl_textx,gl_texty, gl_spacex,gl_spacey);
      if((gl_arg1[0] == 0) && (gl_arg2[0] == 0)) {  // no arguments - do CRLF
         if(gl_dvx & 0x01) gl_y = gl_cr_y;   // vertical text
         else              gl_x = gl_cr_x;   // horizontal text
         gl_char_num = 0;
         gl_line_feed();
      }
      else { // move X & Y character positions
         if(gl_dvx == 3) {
            gl_y += ((gl_textx+gl_spacex) * gl_newx);
            gl_x -= ((gl_texty*1.0+1.0*gl_spacey) * gl_newy);
         }
         else if(gl_dvx == 1) {
            gl_y -= ((gl_textx+gl_spacex) * gl_newx);
            gl_x += ((gl_texty*1.0+1.0*gl_spacey) * gl_newy);
         }
         else if(gl_dvx == 2) {
            gl_x -= ((gl_textx+gl_spacex) * gl_newx);
            gl_y -= ((gl_texty*1.0+1.0*gl_spacey) * gl_newy);
         }
         else {
            gl_x += ((gl_textx+gl_spacex) * gl_newx);
            gl_y += ((gl_texty*1.0+1.0*gl_spacey) * gl_newy);
         }
      }
   }
   else if(!strcmp(gl_cmd, "CS")) {  // character set
      gl_charset = (u08) gl_newx;
      gl_alt_font = 0;
   }
   else if(!strcmp(gl_cmd, "CT")) {  // chord tolerance mode 
      gl_ct_mode = (u08) gl_newx;
   }
   else if(!strcmp(gl_cmd, "DF")) {  // set defaults
      gl_set_defaults(0);
   }
   else if(!strcmp(gl_cmd, "DI")) {  // character orientation  
      // !!!!!! we only do 90 degree increments
      text_dir:
      if(gl_newx >= gl_newy) {  
         if(gl_newy < 0.0) {  // 0,-1 = down
            gl_rotate_chars = 2;
            gl_dvx=1; gl_dvy=0;
         }
         else {  // 1,0 = normal left-right
            gl_rotate_chars = 0;
            gl_dvx=0; gl_dvy=0;
         }
      }
      else {
         if(gl_newx < 0.0) {  // -1,0 = right-left, inverted
            gl_rotate_chars = 1;
            gl_dvx=2; gl_dvy=0;
         }
         else { // 0,1 = up
            gl_rotate_chars = 3;
            gl_dvx=3; gl_dvy=0;
         }
      }
      gl_cr_x=gl_x; gl_cr_y=gl_y;
      gl_char_num = 0;
      gl_new_coords(3);
   }
   else if(!strcmp(gl_cmd, "DL")) {  // download char
      if((gl_pair == 0) && (gl_arg1[0] == 0)) {  // !!!! no arguments... erase download chars
         gl_reset_dl();
         return 0;
      }
      font = 1;  //gl_alt_font;  // !!!! only alt font gets download chars
      if(gl_pair == 0) {  // first coord, get dl_char number
         dl_char = (int) gl_newx;
         if((dl_char < 0) || (dl_char > 255)) dl_char = 0;
         gl_uc_ptr = 0;
         gl_dl_font[font][dl_char][gl_uc_ptr] = (-999999);
         ++gl_pair;

         next_dl:
         strcpy(gl_arg1, gl_arg2);  // kludge to fetch an argument pair
         gl_argptr = 0;
         gl_arg2[gl_argptr] = 0;
         gl_state = 4;  // next we get the second arg
         return 4;
      }
      else if(gl_newx <= (-128.0)) {  // pen up
         gl_dl_font[font][dl_char][gl_uc_ptr++] = (-128); //pen down for first stroke
         gl_dl_font[font][dl_char][gl_uc_ptr] = (-999999);
         goto next_dl;
      }
      else if(gl_arg1[0] == 0) {  // we just erased a char
      }
      else {
         if(gl_uc_ptr == 0) gl_dl_font[font][dl_char][gl_uc_ptr++] = (-128); //pen down for first stroke
         gl_dl_font[font][dl_char][gl_uc_ptr++] = (int) gl_newx; //pen down for first stroke
         gl_dl_font[font][dl_char][gl_uc_ptr++] = (int) gl_newy;
         gl_dl_font[font][dl_char][gl_uc_ptr++] = 99; // pen down next stroke
         gl_dl_font[font][dl_char][gl_uc_ptr] = (-999999);
      }
      if(GL_SEP(data)) return new_glargs(5);
   }
   else if(!strcmp(gl_cmd, "DR")) {  // direction relative
      goto text_dir;
   }
   else if(!strcmp(gl_cmd, "DT")) {  // get terminator character type
      // terminator character was set in the parser,  here we set terminator type
      if(gl_arg2[0] && (gl_newy == 0)) gl_term_type = 0;
      else                             gl_term_type = 1;
   }
   else if(!strcmp(gl_cmd, "DV")) {  // direction vertical
      if(gl_newx < 4)  gl_dvx = (u08) gl_newx;
      else             gl_dvx = 0;

      if(gl_newy == 1) gl_dvy = 1;
      else             gl_dvy = 0;

      gl_new_coords(4);
      gl_cr_x=gl_x; gl_cr_y=gl_y;
      gl_char_num = 0;
   }
   else if(!strcmp(gl_cmd, "EA")) {  // abs rectangle
      draw_rect:
      gl_box(gl_x,gl_y, gl_newx,gl_newy, 0);
   }
   else if(!strcmp(gl_cmd, "ER")) {  // relative rectangle
      gl_newx += gl_x;
      gl_newy += gl_y;
      goto draw_rect;
   }
#ifdef POLYGONS
   else if(!strcmp(gl_cmd, "EP")) {  // edge polygon
//printf("poly edge %d\n", gl_poly_index);
      if(gl_poly_index) gl_render_polygon(0);
   }
#endif
   else if(!strcmp(gl_cmd, "ES")) {  // extra space       
      gl_extra_x = gl_newx;
      gl_extra_y = gl_newy;
      gl_new_coords(5);
   }
   else if(!strcmp(gl_cmd, "EW")) {  // edge wedge (center) (angles)
      gl_rel_cmd = 1;
      wedgie:
      if(gl_pair == 0) {
         X1=gl_newx;  X2=gl_newy;
         ++gl_pair;
         if(GL_SEP(data)) return new_glargs(5);
      }
      else {
         if(gl_arg2[0]) gl_chord_angle = gl_newy; // tttt
         else           gl_chord_angle = 0.0;
         gl_wedge(gl_x,gl_y, X1, X2,X2+gl_newx, gl_rel_cmd);
      }
   }
#ifdef POLYGONS
   else if(!strcmp(gl_cmd, "FP")) {  // fill polygon
// printf("poly fill: %d\n", gl_poly_index);
      if(gl_poly_index) gl_render_polygon(1);
   }
#endif
   else if(!strcmp(gl_cmd, "IN")) {  // initialize
      gl_set_defaults(1);
   }
   else if(!strcmp(gl_cmd, "IP")) {  // input points
      gl_rel_cmd = 0;
      ip_window:
      restart_line_pattern();
      if(gl_pair == 0) { 
         if(gl_arg1[0] == 0) {  // no args,  so reset input points
            sx = scale_x(gl_x); sy = scale_y(gl_y);
            gl_ip_xmin=GL_XMIN; gl_ip_xmax=GL_XMAX;
            gl_ip_ymin=GL_YMIN; gl_ip_ymax=GL_YMAX;
            gl_x=unscale_x(sx); gl_y=unscale_y(sy);
            gl_new_coords(6);
         }
         else {
            if(gl_rel_cmd) {  // convert relative percents into abs values
               gl_newx = (gl_newx/100.0) * (GL_XMAX-GL_XMIN);
               gl_newy = (gl_newy/100.0) * (GL_YMAX-GL_YMIN);
            }

            sx = scale_x(gl_x); sy = scale_y(gl_y);
            gl_ip_xmax = (gl_ip_xmax - gl_ip_xmin);  // remember current window sizes
            gl_ip_ymax = (gl_ip_ymax - gl_ip_ymin);
            gl_ip_xmin = gl_newx;      // set new lower corner
            gl_ip_ymin = gl_newy;
            gl_ip_xmax += gl_ip_xmin;  // make the new window the same size
            gl_ip_ymax += gl_ip_ymin;
            gl_x=unscale_x(sx); gl_y=unscale_y(sy);
            gl_new_coords(7);

            ++gl_pair;
            if(GL_SEP(data)) return new_glargs(5);
         }
      }
      else if(gl_pair == 1) {  // decode y_min, y_max
         if(gl_rel_cmd) {  // convert relative percents into abs values
            gl_newx = (gl_newx/100.0) * (GL_XMAX-GL_XMIN);
            gl_newy = (gl_newy/100.0) * (GL_YMAX-GL_YMIN);
         }

         sx = scale_x(gl_x); sy = scale_y(gl_y);
         gl_ip_xmax = gl_newx;
         gl_ip_ymax = gl_newy;
         if(gl_ip_xmax == gl_ip_xmin) ++gl_ip_xmax;
         if(gl_ip_ymax == gl_ip_ymin) ++gl_ip_ymax;
         gl_x=unscale_x(sx); gl_y=unscale_y(sy);
         gl_new_coords(8);
      }
   }
   else if(!strcmp(gl_cmd, "IR")) {  // relative input point window size
      gl_rel_cmd = 1;
      goto ip_window;
   }
   else if(!strcmp(gl_cmd, "IW")) {  // input clipping window
      restart_line_pattern();
      if(gl_pair == 0) {        // lower left of clipping window
         if(gl_arg1[0] == 0) {  // no args,  set window to full size
            gl_clip_x1=GL_XMIN; gl_clip_y1=GL_YMIN; 
            gl_clip_x2=GL_XMAX; gl_clip_y2=GL_YMAX;
            gl_clip = 0;
            gl_new_coords(9);
         }
         else {
            X1=gl_newx; Y1=gl_newy;
            ++gl_pair;
            if(GL_SEP(data)) return new_glargs(5);
         }
      }
      else if(gl_pair == 1) {  // upper right of clipping window
         gl_clip_x1=X1; gl_clip_y1=Y1;
         gl_clip_x2=gl_newx; gl_clip_y2=gl_newy;
         gl_clip = 1;
if((gl_clip_x1 == 0) && (gl_clip_y1 == 1000) && (gl_clip_x2 == 10300) && (gl_clip_y2 == 6800)) {
   gl_clip = 0;  // !!!!! kludge for bogus Wiltron 8350B clipping window
}
         gl_new_coords(9);
      }
   }
   else if(!strcmp(gl_cmd, "LA")) {  // line attributes !!! do something
      restart_line_pattern();
   }
   else if(!strcmp(gl_cmd, "LT")) {  // line type
      restart_line_pattern();
      if(gl_pair == 0) {
         line_pattern = 0;
         if(gl_arg1[0]) {  // line type code
            line_pattern = 1;
            if(gl_newx < 0.0) {   // adaptive pattern type
               gl_newx = 0.0 - gl_newx;  // !!! we treat as normal patterns
            }
            if(gl_newx < MAX_LINE_TYPES) gl_lt = (u08) gl_newx;
            else line_pattern = 0;
         }

         if(gl_arg2[0]) {  // pattern length
            gl_lt_len = gl_newy;
         }
         else {
            gl_lt_len = 4.0;
         }

         gl_setup_pattern();

         ++gl_pair;
         if(GL_SEP(data)) return new_glargs(5);
      }
      else if(gl_pair == 1) {  // third param is rel/abs pattern length type
         if(gl_newx) gl_lt_rel = 1;
         else        gl_lt_rel = 0;
         gl_setup_pattern();
      }
   }
#ifdef BUFFER_LABELS
   else if(!strcmp(gl_cmd, "LO")) {  // label_offset
      // for label offsets to work,  we need to buffer the label
      // so we can center/justify it.
      gl_lo_code = (int) gl_newx;
      gl_cr_x=gl_x; gl_cr_y=gl_y;
      gl_char_num = 0;
      gl_new_coords(10);
   }
#endif
#ifdef BUFFER_LABELS
   else if(!strcmp(gl_cmd, "PB")) {  // print buffered label
      gl_lo_x = gl_lo_y = 0;
gl_print_buffer(&gl_label[0], 0);
//    for(sx=0; sx<gl_label_index; sx++) {  // print the label text
//       if(gl_label_char(gl_label[sx])) {  // llll
//          gl_set_lo(sx+1, 0); 
//       }
//    }
   }
#endif
   else if(!strcmp(gl_cmd, "PG")) {  // new page
      goto new_page;
   }
#ifdef POLYGONS
   else if(!strcmp(gl_cmd, "PM")) {  // polygon mode
      if((gl_poly_number == 0) || (gl_newx == 0)) {  // flush polygon buffer
//printf("poly flush\n");
         if(gl_poly_number) {
            gl_x=gl_poly_x;  gl_y=gl_poly_y;
            gl_poly_number = 0;
         }
         gl_poly_index = 0;
         poly_closed = 1;  //3
      }

      if(gl_newx == 0) {  // enter polygon definition mode
//qqqq   gl_x = gl_y = 0;  // !!!! we should not have to do this
         gl_poly_x=gl_x;  gl_poly_y=gl_y;
         gl_poly_number += 2;
         gl_poly_start = gl_poly_index;
         poly_closed = 1; //3
//printf("poly enter #%d: glxy:%g,%g  start:%d\n", gl_poly_number, gl_poly_x,gl_poly_y, gl_poly_start);
      }
      else if(gl_newx == 1) { 
//printf("poly close #%d\n", gl_poly_number);
         if(gl_poly_number) {
            gl_close_polygon();   // close current sub-polygon
            gl_poly_start = gl_poly_index;
            poly_closed = 1; //2
         }
//printf("poly resume start:%d\n", gl_poly_start);
      }
      else if(gl_newx == 2) {     // exit polygon definition mode
//printf("poly exit %d: glxy:%f,%f\n", gl_poly_number, gl_poly_x,gl_poly_y);
         if(gl_poly_number) {
            gl_close_polygon();
            gl_poly_number = 0;
            gl_x=gl_poly_x;  gl_y=gl_poly_y;
         }
      }
      else gl_poly_number = 0;
   }
#endif
   else if(!strcmp(gl_cmd, "PS")) {  // plot size
      if(gl_arg1[0] == 0) {
         gl_length = (GL_XMAX-GL_XMIN);
         gl_width = (GL_YMAX-GL_YMIN);
      }
      else {
         gl_length = gl_newx;
         gl_width = gl_newy;
      }
   }
   else if(!strcmp(gl_cmd, "PW")) {  // pen width
      gl_newx *= sqrt(((double) ROWS*(double) ROWS) + ((double) COLS*(double) COLS));
      if(gl_newx < 0.0) gl_newx = 0.0;
      if(gl_pwu) gl_newx /= 100.0;   // relative pen width (percent of screen diag)
      else       gl_newx /= GL_DIAG_MM;

      if((gl_arg2[0]) && (gl_newy >= 0.0) && (gl_newy < PEN_COUNT)) {  // set specified pen
         gl_pw[(int) gl_newy] = (u08) gl_newx;  
      }
      else {  // set all pens
         for(sx=0; sx<PEN_COUNT; sx++) gl_pw[sx] = (u08) gl_newx;
      }
      restart_line_pattern();
   }
   else if(!strcmp(gl_cmd, "RA")) {  // abs filled rectangle
      fill_rect:
      gl_box(gl_x,gl_y, gl_newx,gl_newy, 1);
   }
   else if(!strcmp(gl_cmd, "RF")) {  // raster fill - !!! do something
      restart_line_pattern();
   }
   else if(!strcmp(gl_cmd, "RO")) {  // coordinate rotation
      restart_line_pattern();
      gl_rotate_coords = (((int)gl_newx) % 360) / 90;
gl_rotate_coords += user_rotate;
gl_rotate_coords %= 4;
      if(gl_rotate_coords > 3) gl_rotate_coords = 0;

      gl_new_coords(11);
      gl_cr_x=gl_x; gl_cr_y=gl_y;
      gl_char_num = 0;
   }
   else if(!strcmp(gl_cmd, "RR")) {  // relative filled rectangle
      gl_newx += gl_x;
      gl_newy += gl_y;
      goto fill_rect;
   }
   else if(!strcmp(gl_cmd, "RT")) {  // relative three point arc
      gl_rel_cmd = 1;
      goto tp_arc;
   }
   else if(!strcmp(gl_cmd, "SA")) {  // select alternate font
      gl_alt_font = 1;
   }
   else if(!strcmp(gl_cmd, "SS")) {  // select standard font
      gl_alt_font = 0;
   }
   else if(!strcmp(gl_cmd, "SC")) {  // ssss user coordinate scale factors
      restart_line_pattern();
      if((gl_pair == 0) && (gl_arg1[0] == 0)) {  // no arguments... 
         return 0;
      }

if(sc_enabled) ; else  // scale factors have been forced...
      if(gl_pair == 0) {  // decode x_min, x_max;
         sx1 = gl_newx; sy1 = gl_newy;
         sc_type = 0;
         ++gl_pair;
         if(GL_SEP(data)) return new_glargs(5);
      }
      else if(gl_pair == 1) {  // decode y_min, y_max
         sx2 = gl_newx; sy2 = gl_newy;
         ++gl_pair;
         if(GL_SEP(data)) return new_glargs(5);
      }
      else if(gl_pair == 2) {
         sc_type = (int) gl_newx;
         strcpy(gl_arg1, gl_arg2);
         gl_argptr = 0;
         gl_arg2[gl_argptr] = 0;
         ++gl_pair;
         gl_state = 4;
         return 4;
      }
      else if(gl_pair == 3) {
         sx3 = gl_newx;
         sy3 = gl_newy;
         if(gl_arg1[0] == 0) {  // no top,bottom given, use 50%
            ++gl_pair;
            sx3 = 50.0;
            sy3 = 50.0;
         }
      }
   }
   else if(!strcmp(gl_cmd, "SI")) {  // absolute char size
      if(gl_newx) gl_char_sx = gl_newx;
      else        gl_char_sx = 0.19;
      if(gl_newy) gl_char_sy = gl_newy;
      else        gl_char_sy = 0.27;
      gl_rel_char = 0;
      gl_new_coords(14);
   }
   else if(!strcmp(gl_cmd, "SL")) {  // slant angle
      gl_newy = 0;
      for(sy=VCHAR_H-1; sy>=0; sy--) {
         vchar_slant[sy] = (s08) gl_newy;
         gl_newy += gl_newx;
      }
   }
   else if(!strcmp(gl_cmd, "SM")) {  // symbol mode
      // this command was handled in the parser
   }
   else if(!strcmp(gl_cmd, "SP")) {  // select pen
      // !!! (pen 0 should mean stop plotting)
      restart_line_pattern();
      if((gl_newx >= 0) && (gl_newx < PEN_COUNT)) {
         gl_pen = (u08) gl_newx;
         gl_set_color(gl_pen);
      }
   }
   else if(!strcmp(gl_cmd, "SR")) {  // relative char size
      if(gl_newx) gl_char_sx = gl_newx / 100.0;
      else        gl_char_sx = 0.75 / 100.0;
      if(gl_newy) gl_char_sy = gl_newy / 100.0;
      else        gl_char_sy = 1.50 / 100.0;
      gl_rel_char = 1;
      gl_new_coords(15);
   }
   else if(!strcmp(gl_cmd, "SU")) {  // mmmm char size in user units
      if(gl_newx) gl_char_sx = gl_newx;
      else        gl_char_sx = 0.19;
      if(gl_newy) gl_char_sy = gl_newy;
      else        gl_char_sy = 0.27;
      gl_rel_char = 2;
      gl_new_coords(16);
   }
   else if(!strcmp(gl_cmd, "TD")) {  // transparent data
      if(gl_newx == 1) gl_td = 1;
      else             gl_td = 0;
   }
   else if(!strcmp(gl_cmd, "TL")) {  // tick length
      if(gl_arg1[0]) {
         gl_pos_tick = gl_newx / 100.0;
         gl_neg_tick = gl_pos_tick;
      }
      else {  //ccccc
         gl_pos_tick = .5 / 100.0;
         gl_neg_tick = gl_pos_tick;
      }
      if(gl_arg2[0]) {
         gl_neg_tick = gl_newy / 100.0;
      }
      else {
         gl_neg_tick = 0.5 / 100.0;
      }
   }
   else if(!strcmp(gl_cmd, "TR")) {  // transparent drawing mode - !!! do something
      restart_line_pattern();
   }
   else if(!strcmp(gl_cmd, "UC")) { // user char
      if(gl_arg1[0] == 0) return 0; // no arguments, ignore the command
      if(gl_arg2[0] == 0) return 0; // cccc no arguments, ignore the command
      if(gl_uc_ptr >= DL_LEN) return 0;
      if(gl_pair == 0) gl_uc_ptr = 0;

      if(gl_newx >= 99.0) {  // pen down code  
         next_uc_arg:
         gl_dl_font[gl_alt_font][UC_CHAR][gl_uc_ptr++] = (int) gl_newx;
         gl_dl_font[gl_alt_font][UC_CHAR][gl_uc_ptr] = (-999999);
         strcpy(gl_arg1, gl_arg2);  // kludge to fetch an argument pair
         gl_argptr = 0;
         gl_arg2[gl_argptr] = 0;
         gl_state = 4;  // next we get the second arg
         gl_pair = 1;
         return 4;
      }
      else if(gl_newx <= (-99.0)) {  // pen up code
         gl_pair = 1;
         goto next_uc_arg;
      }
      else {  // draw the character stroke
         gl_pair = 1;
         gl_dl_font[gl_alt_font][UC_CHAR][gl_uc_ptr++] = (int) gl_newx;
         gl_dl_font[gl_alt_font][UC_CHAR][gl_uc_ptr++] = (int) gl_newy;
         gl_dl_font[gl_alt_font][UC_CHAR][gl_uc_ptr] = (-999999);
      }
      if(GL_SEP(data)) return new_glargs(5);
   }
   else if(!strcmp(gl_cmd, "UL")) {  // user defined line pattern
#ifdef USER_LINE_TYPES
      if(gl_pair == 0) {  // first argument to the command is line type number
         if((gl_arg1[0] == 0) || (gl_newx == 0)) {  // no argument - reset all user line types
            for(sx=0; sx<USER_LINE_TYPES; sx++) gl_user_pattern[sx][0] = 0;
         }
         else if(gl_arg2[0] == 0) {  // reset single user line type
            sx = (int) gl_newx;
            if(sx <= 0) sx = 0;
            else if(sx >= USER_LINE_TYPES) sx = 0;
            gl_user_pattern[sx][0] = 0;
         }
         else {  // user line type run length pairs follow
            gl_user_linetype = (u08) gl_newx;
            if(gl_user_linetype < 0) gl_user_linetype = 0;
            else if(gl_user_linetype > USER_LINE_TYPES) gl_user_linetype = 0;
            gl_user_pattern[gl_user_linetype][0] = 0;
            ++gl_pair;
            gl_setup_pattern();
            goto next_uc_arg;  // prepare to start fetching run length pairs
         }
      }
      else if(gl_pair < (MAX_PATTERN_LENGTH-1)) {
         gl_user_pattern[gl_user_linetype][0] += 2;   
         gl_user_pattern[gl_user_linetype][gl_pair++] = (u08) gl_newx;   
         gl_user_pattern[gl_user_linetype][gl_pair++] = (u08) gl_newy;   
         if(GL_SEP(data)) {
            gl_setup_pattern();
            return new_glargs(5);
         }
      }
#endif
      gl_setup_pattern();
   }
   else if(!strcmp(gl_cmd, "WG")) {  // filled wedge
      gl_rel_cmd = 2;
      goto wedgie;
   }
   else if(!strcmp(gl_cmd, "WU")) {  // pen width units
      if(gl_newx) gl_pwu = 1;  // pen size is percent of page size
      else        gl_pwu = 0;  // pen size is millimeters
      for(sx=0; sx<PEN_COUNT; sx++) {  // default pen size is one pixel
         gl_pw[sx] = 0;
      }
      restart_line_pattern();
   }
   else if(!strcmp(gl_cmd, "XT")) {  // X axis tick mark  //ccccc
      if(gl_user_scaling) {
         gl_line(gl_x, gl_y, gl_x, gl_y-(gl_neg_tick*(gl_sc_ymax-gl_sc_ymin)), 1, gl_pw[gl_pen]);
         gl_line(gl_x, gl_y, gl_x, gl_y+(gl_pos_tick*(gl_sc_ymax-gl_sc_ymin)), 1, gl_pw[gl_pen]);
      }
      else {
         gl_line(gl_x, gl_y, gl_x, gl_y-(gl_neg_tick*(gl_ip_ymax-gl_ip_ymin)), 1, gl_pw[gl_pen]);
         gl_line(gl_x, gl_y, gl_x, gl_y+(gl_pos_tick*(gl_ip_ymax-gl_ip_ymin)), 1, gl_pw[gl_pen]);
      }
   }
   else if(!strcmp(gl_cmd, "YT")) {  // Y axis tick mark
      if(gl_user_scaling) {
         gl_line(gl_x, gl_y, gl_x+(gl_pos_tick*(gl_sc_xmax-gl_sc_xmin)), gl_y, 1, gl_pw[gl_pen]);
         gl_line(gl_x, gl_y, gl_x-(gl_neg_tick*(gl_sc_xmax-gl_sc_xmin)), gl_y, 1, gl_pw[gl_pen]);
      }
      else {
         gl_line(gl_x, gl_y, gl_x+(gl_pos_tick*(gl_ip_xmax-gl_sc_xmin)), gl_y, 1, gl_pw[gl_pen]);
         gl_line(gl_x, gl_y, gl_x-(gl_neg_tick*(gl_ip_xmax-gl_sc_xmin)), gl_y, 1, gl_pw[gl_pen]);
      }
   }
   else {
//    printf("Unknown cmd: %s\n", gl_cmd);
   }

   return 0;  // no more args to process for this command
}



void do_gl_scale()   // ssss
{
int sx, sy;
int cx1, cy1;
int cx2, cy2;
double xy;

   restart_line_pattern();
if(sc_enabled) {
   return;  // scale factors have been forced...
}

   sx = scale_x(gl_x); sy = scale_y(gl_y); // current screen location of the cursor position
   cx1 = scale_x(gl_clip_x1); cy1 = scale_y(gl_clip_y1);
   cx2 = scale_x(gl_clip_x2); cy2 = scale_y(gl_clip_y2);
gl_clip = 0;  // !!!! we should not have to do this, but Advantest plots mess up otherwise

   if((gl_pair == 0) || (sx1 == sy1) || (sx2 == sy2)) {
      gl_user_scaling = 0;
   }
   else if(sc_type == 2) {  // scale to specified number of plotter units
      gl_user_scaling = 3;
      gl_sc_xmin = sx1;
      if(sy1) {
         gl_sc_xmax = 0.0*gl_sc_xmin + ((gl_ip_xmax-gl_ip_xmin) / sy1);
         gl_sc_xmax /= 2.0; // !!!! why do we have to do this?
      }

      gl_sc_ymin = sx2;
      if(sy2) {
         gl_sc_ymax = 0.0*gl_sc_ymin + ((gl_ip_ymax-gl_ip_ymin) / sy2);
         gl_sc_ymax /= 2.0; // !!!! why do we have to do this? 
      }
   }
   else if(sc_type == 1) {  // isotropic scaling
      gl_user_scaling = 2;
      gl_sc_xmin = sx1;
      gl_sc_xmax = sy1;

      gl_sc_ymin = sx2;
      gl_sc_ymax = sy2;

      xy = (gl_sc_xmax-gl_sc_xmin) / (gl_sc_ymax - gl_sc_ymin);
      if(xy < 0.0) xy = 0.0 - xy;
      if(xy == 1.0) ;
      else if(xy > 1.0) {
         gl_sc_ymax *= xy;
         gl_sc_ymin *= xy;
         gl_sc_ymax -= ((gl_sc_xmax - gl_sc_xmin) / 2.0 * (sy3/100.0)*1.333);
         gl_sc_ymin -= ((gl_sc_xmax - gl_sc_xmin) / 2.0 * (sy3/100.0)*1.333);
      }
      else {
         gl_sc_xmax /= xy;
         gl_sc_xmin /= xy;
         gl_sc_xmax -= ((gl_sc_xmax - gl_sc_xmin) / 2.0 * (sx3/100.0)/1.000);
         gl_sc_xmin -= ((gl_sc_xmax - gl_sc_xmin) / 2.0 * (sx3/100.0)/1.000);
      }
   }
   else {  // anisotropic (normal) scaling
      gl_user_scaling = 1;
      gl_sc_xmin = sx1;
      gl_sc_xmax = sy1;

      gl_sc_ymin = sx2;
      gl_sc_ymax = sy2;
   }

   if(gl_sc_xmax == gl_sc_xmin) ++gl_sc_xmax;
   if(gl_sc_ymax == gl_sc_ymin) ++gl_sc_ymax;

   gl_x = unscale_x(sx); gl_y = unscale_y(sy);
   gl_clip_x2 = unscale_x(cx2); gl_clip_y2 = unscale_y(cy2);
   gl_clip_x1 = unscale_x(cx1); gl_clip_y1 = unscale_y(cy1);

   gl_new_coords(12);
   return;
}

void parse_hpgl(u08 data)
{
int next_state;

   //
   //  This routine parses a HPGL data stream.  It is a simple state driven
   //  parser that gets called with each character of the stream.  It decodes 
   //  the HPGL command syntax and calls gl_render_command to draw or 
   //  otherwise act on each individual HPGL command.
   //
   if(gl_encode_base) {  // we are currently doing an encoded polyline
      gl_encoded_poly(data);
      return;
   }

   // Three different types of comments are supported:
   //   1) The official CO" ... " 
   //   2) // comment to end of line
   //   3) /* ... */ (which can be nested 30 levels deep)
   if(gl_cmt & QUOTE_CMT) {  // we are skipping COmment text
      if(data == '"') ++gl_cmt;
      if((gl_cmt & 0x1F) >= 2) {  // 1=opening "     2=closing "
         gl_cmt = 0;
      }
      goto parse_exit;
   }

   if(gl_state == 0) {  // first call - initialize parser
      vchar_init();
      vchar_set_fontsize((u08) VX, (u08)VY);
      vchar_set_spacing(0,0);

//    lcd_clear();
//    gl_term = data;  // first HP5371A byte is text terminator character
      gl_state = 1;
      gl_new_coords(2);
      goto next_gl_cmd;
   }
   else if(gl_state == 1) {  // skip over terminators to the first char of the command
      next_gl_cmd:
      if(!strcmp(gl_cmd, "UC")) {  //uuuu
         draw_user_char(gl_x,gl_y, gl_alt_font, 1, UC_CHAR);   // draw the UC User Char
//       gl_vchar(gl_x,gl_y, UC_CHAR);   // draw the UC User Char
         gl_uc_ptr = 0;             // erase the UC char definition
         gl_dl_font[gl_alt_font][UC_CHAR][gl_uc_ptr] = (-999999);
         gl_cmd[0] = gl_cmd[1] = gl_cmd[2] = 0;
      }
      else if(!strcmp(gl_cmd, "SC")) {  // ssss
         do_gl_scale();
         gl_cmd[0] = 0;
         gl_cmd[1] = 0;
         gl_cmd[2] = 0;
      }

      gl_state = 1;
      data = toupper(data);
      if((data >= 'A') && (data <= 'Z')) {
         gl_cmd[0] = data;
         gl_cmd[1] = 0;
         gl_cmd[2] = 0;
         uc_pendown = 0;

         new_glargs(2);
         gl_pair = 0;
      }
#ifdef PCL_CODE
      else if(data == 0x1B) {  // escape to PCL mode
         in_hpgl = 0;
      }
#endif
      else if(data == '/') {   // '//' comments out to end-of-line
         if(++gl_cmt >= 2) {
            gl_cmt = 0;
            gl_state = 98;
         }
      }
      else if((data == '*') && (gl_cmt == 1)) {   // '/*' comments out a block until '*/'
         gl_cmt = 1;                              // these comments can nest
         gl_state = 97;
      }
      else gl_cmt = 0;
   }
   else if(gl_state == 2) { // second byte of command
      data = toupper(data);
      if((data >= 'A') && (data <= 'Z')) {
         gl_cmd[1] = data;
         gl_cmd[2] = 0;
         if(!strcmp(gl_cmd, "CO")) {  // comment is special case
            gl_cmt = QUOTE_CMT;
            gl_state = 1;
            goto parse_exit;
         }
#ifdef ENCODED_POLYLINES
         else if(!strcmp(gl_cmd, "PE")) {  // polyline encoded is special case
            gl_pair = 0;
            gl_frac_bits = 0;
            gl_encoded_value = 0;
            gl_encoded_mult = 1;
            gl_encode_base = 64;   // this enables the encoded polyline mode
            goto parse_exit;
         }
#endif
         new_glargs(3);
      }
      else if(1 && (gl_cmd[0] == 'U') && (gl_cmd[1] == 0) && (data == ';')) {  
         // !!!! HP3588 kludge - fake missing SCale command
         gl_user_scaling = 1;
         gl_sc_xmin=0;   gl_sc_ymin=0;
         gl_sc_xmax=480; gl_sc_ymax=400;
         gl_new_coords(0);
      }
      else {
         gl_state = 99;
         goto parse_exit;
      }
   }
   else if(gl_state == 3) { // we have the command, build first arg (if any)
      state3:
      gl_state = 3;
#ifdef BUFFER_LABELS
      if(!strcmp(gl_cmd, "LB")) {  // print text label is special case
         gl_label_index = 0;
         gl_label[0] = 0;

         gl_save_only = 0;  // save label text in buffer, then print
         goto get_label;
      }
      else if(!strcmp(gl_cmd, "BL")) {  // save text label is special case
         gl_label_index = 0;
         gl_label[0] = 0;
         gl_save_only = 1;  // save label text in buffer, don't print
         goto get_label;
      }
#else
      if(!strcmp(gl_cmd, "LB")) {  // print text label is special case
         goto get_label;
      }
#endif
      else if(!strcmp(gl_cmd, "DT") && (gl_argptr == 0)) {  // text terminator is special case
         if(data && (data != ';')) gl_term = data;
         else gl_term = 0x03;
         gl_term_type = 1;
         gl_arg1[gl_argptr++] = '0';  //kludge... for parser to think it saw DT 0 value
         gl_arg1[gl_argptr] = 0;
         data = ' ';                  //kludge...
         gl_label_index = 0;   // reset current label
         gl_label[0] = 0;
      }
      else if(!strcmp(gl_cmd, "SM")) {  // symbol mode char is special case
         gl_symbol = 0;
         if(data && (data != ';') && (data != force_lb_term)) {
           if((data > ' ') && (data <= 0x7E)) gl_symbol = data;
           else if(1 && (data > 0x7E)) gl_symbol = data;   
           else if(gl_td) gl_symbol = data;  //!!! allow transparent data to work here
         }
         goto decode_cmd;
         gl_state = 99;
         goto parse_exit;
      }

      if(data == ',') gl_saw_comma = 1;  // for HP5372 bug fix

      if(data == ';') goto decode_cmd;
      else if(GL_SEP(data) && (gl_argptr == 0)) ;  // ignore separators before arg
      else if(GL_SEP(data)) {
         gl_argptr = 0;
         gl_arg2[gl_argptr] = 0;
         gl_state = 4;  // next we get the second arg
      }
      else if(gl_argptr && ((data == '+') || (data == '-'))) {
         gl_argptr = 0;
         gl_arg2[gl_argptr] = 0;
         gl_state = 4;  // next we get the second arg
      }
      else if(gl_argptr && (data == '.') && strchr(gl_arg1, '.')) {  //!!! this is proper, but messes up some instruments
//       gl_argptr = 0;
//       gl_arg2[gl_argptr] = 0;
//       gl_state = 4;  // next we get the second arg
      }
      else if((data == '+') || (data == '-') || (data == '.') || ((data >= '0') && (data <= '9'))) {
         if(gl_argptr < GL_ARGLEN) {
            gl_arg1[gl_argptr++] = data;
            gl_arg1[gl_argptr] = 0;
         }
      }
      else goto decode_cmd;
   }
   else if(gl_state == 4) { // we have the first arg, build second arg (if any)
      state4:
      if(data == ';') {
         if(gl_saw_comma == 0) goto decode_cmd;  //HP5372 bug fix
         gl_saw_comma = 0;
      }
      else if(GL_SEP(data) && (gl_argptr == 0)) ;  // ignore separators before arg
      else if(GL_SEP(data)) goto decode_cmd;  // process command args
      else if(gl_argptr && ((data == '+') || (data == '-'))) goto decode_cmd;
      else if(gl_argptr && (data == '.') && strchr(gl_arg2, '.')) {
//         goto decode_cmd;
      }
      else if((data == '+') || (data == '-') || (data == '.') || ((data >= '0') && (data <= '9'))) {
         if(gl_argptr < GL_ARGLEN) {
            gl_arg2[gl_argptr++] = data;
            gl_arg2[gl_argptr] = 0;
         }
      }
      else goto decode_cmd;
   }
   else if(gl_state == 5) {  // we are skipping white space
      if(GL_SEP(data)) ;
      else if(data == ';') {
      }
      else if(data == '+') goto state3;
      else if(data == '-') goto state3;
      else if(data == '.') goto state3;
      else if((data >= '0') && (data <= '9')) goto state3;
      else goto next_gl_cmd;
   }
   else if(gl_state == 10) {  // printing or saving label text
      get_label:
//printf("  label char %d: %02X\n", gl_label_index, data);
      gl_state = 10;

      #ifdef BUFFER_LABELS
         if(gl_buffer_label(data) == 0) {   // add char to label buffer
            if(gl_save_only == 0) gl_print_buffer(&gl_label[0], 1);    // llll print buffer on termination char
            gl_save_only = 0;
            gl_state = 1;
         }
      #else
         if((data == gl_term) || (data == 0x00)) {  // end of text string
            gl_state = 1;
            if(gl_term_type == 1) goto parse_exit;  // non-printing terminator
            else if(data == 0x00) goto parse_exit;
         }
         gl_label_char(data);  
      #endif
   }
   else if(gl_state == 20) {  // time to decode the command
      decode_cmd:
      if(gl_arg1[0]) gl_newx = (double) atof(gl_arg1);
      else gl_newx = 0.0;

      if(gl_arg2[0]) gl_newy = (double) atof(gl_arg2);
      else gl_newy = 0.0;

      next_state = gl_render_cmd(data);    // decode and execute the command

      if(next_state == 4) goto state4;     // UC and UL commands do not have simple coord pairs as args
      else if(next_state) goto parse_exit; //  this command has more coord pairs to do
      else goto next_gl_cmd;  // this command has no more coord pairs to process
   }
   else if(gl_state == 97) {  // skip to '*/'
      if(data == '*') {
         if(gl_cmt & SAW_SLASH) {  // '/*' nested comment starts
            ++gl_cmt;
            gl_cmt &= (~(SAW_STAR | SAW_SLASH));
         }
         else gl_cmt |= SAW_STAR;
      }
      else if(data == '/') {
         if(gl_cmt & SAW_STAR) {  // '*/' ends a comment nest level
            --gl_cmt;
            gl_cmt &= (~(SAW_STAR | SAW_SLASH));
         }
         else gl_cmt |= SAW_SLASH;
      }
      else {
         gl_cmt &= (~(SAW_STAR | SAW_SLASH));
      }
      if((gl_cmt & 0x1F) == 0x00) {
         gl_cmt = 0;
         gl_state = 1;
      }
   }
   else if(gl_state == 98) { // skip to end of line
      if(data == 0x0A) gl_state = 1;
      else if(data == 0x0D) gl_state = 1;
   }
   else if(gl_state == 99) { // skip to beginning of the next command
      gl_cmt = 0;
      if(data == ';') goto next_gl_cmd;
      else if((data >= 'A') && (data <= 'Z')) goto next_gl_cmd; 
   }

   parse_exit:
   return;
}
