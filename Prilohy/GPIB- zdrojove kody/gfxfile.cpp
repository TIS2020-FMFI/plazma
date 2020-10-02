// ------------------------------------------------------------------------
// General graphics file-related routines from various sources
//
// john@miles.io
// ------------------------------------------------------------------------

VFX_RGB * TGA_parse        (void *TGA_image, S32 *x_res, S32 *y_res);
bool      TGA_write_16bpp  (PANE *src, C8 *filename);
bool      GIF_write_16bpp  (PANE *src, C8 *filename);
bool      BMP_write_16bpp  (PANE *src, C8 *filename);
bool      PCX_write_16bpp  (PANE *src, C8 *filename);

#ifdef WINVFX_H
   #define WINVFX
#endif

#ifdef WINVFX
   #define PANE_DIMS(p,w,h) { w = (p)->x1-(p)->x0+1; h = (p)->y1-(p)->y0+1; }
#else
   #define PANE_DIMS(p,w,h) { w = (p)->width; h = (p)->height; }
#endif

/*
 * Portions excerpted from GraphApp by L. Patrick 
 * http://enchantia.com/software/graphapp/
 *
 * Gif.c - Cross-platform code for loading and saving GIFs
 *
 *  The LZW encoder and decoder used in this file were
 *  written by Gershon Elber and Eric S. Raymond as part of
 *  the GifLib package.
 *
 *  The remainder of the code was written by Lachlan Patrick
 *  as part of the GraphApp cross-platform graphics library.
 *
 *  GIF(sm) is a service mark property of CompuServe Inc.
 *  For better compression and more features than GIF,
 *  use PNG: the Portable Network Graphics format.
 */

/*
 *  Copyright and patent information:
 *
 *  Because the LZW algorithm has been patented by
 *  CompuServe Inc, you probably can't use this file
 *  in a commercial application without first paying
 *  CompuServe the appropriate licensing fee.
 *  Contact CompuServe for more information about that.
 */

//
// Addendum: The relevant patent (U.S. Patent 4,558,302, per CompuServe 
// license agreement at http://lpf.ai.mit.edu/Patents/Gif/gif_lic.html) 
// expires as of June 2003.  Still, you should seek legal 
// assistance prior to using the .GIF code in this module for 
// any commercial purpose.  See also http://lzw.info. -- jm
//

#define WRITE_COMPRESSED_FILE 1     // 0=uncompressed, 1=RLE compressed

#pragma pack(1)  // Do NOT allow compiler to reorder structs!

struct TGA_HDR
{
   U8    len_image_ID;
   U8    color_map_present;
   U8    image_type;
   S16   color_map_origin;
   S16   color_map_len;
   U8    color_map_entry_size;
   S16   X_origin;
   S16   Y_origin;
   S16   pixel_width;
   S16   pixel_height;
   U8    bits_per_pixel;
   U8    image_descriptor_flags;
};

#define TGA_INTERLEAVE_4 0x80
#define TGA_INTERLEAVE_2 0x40
#define TGA_ORIGIN_TOP   0x20
#define TGA_ORIGIN_RIGHT 0x10
#define TGA_ATTRIB_MASK  0x0F

#define TGA_FORMAT_RGB_UNCOMPRESSED 2
#define TGA_FORMAT_RGB_RLE          10

//
// .GIF stuff from GraphApp
//

typedef unsigned char      byte;
typedef unsigned long      Char;

typedef struct GifColor      GifColor;

struct GifColor {
 byte  alpha;    /* transparency, 0=opaque, 255=transparent */
 byte  red;      /* intensity, 0=black, 255=bright red */
 byte  green;    /* intensity, 0=black, 255=bright green */
 byte  blue;     /* intensity, 0=black, 255=bright blue */
};

typedef struct {
    int      length;
    GifColor * colours;
  } GifPalette;

typedef struct {
    int          width, height;
    int          has_cmap, color_res, sorted, cmap_depth;
    int          bgcolour, aspect;
    GifPalette * cmap;
  } GifScreen;

typedef struct {
    int             byte_count;
    unsigned char * bytes;
  } GifData;

typedef struct {
    int        marker;
    int        data_count;
    GifData ** data;
  } GifExtension;

typedef struct {
    int              left, top, width, height;
    int              has_cmap, interlace, sorted, reserved, cmap_depth;
    GifPalette *     cmap;
    unsigned char ** data;
  } GifPicture;

typedef struct {
    int            intro;
    GifPicture *   pic;
    GifExtension * ext;
  } GifBlock;

typedef struct {
    char        header[8];
    GifScreen * screen;
    int         block_count;
    GifBlock ** blocks;
  } Gif;

/*
 *  Gif internal definitions:
 */

#define LZ_MAX_CODE     4095    /* Largest 12 bit code */
#define LZ_BITS         12

#define FLUSH_OUTPUT    4096    /* Impossible code = flush */
#define FIRST_CODE      4097    /* Impossible code = first */
#define NO_SUCH_CODE    4098    /* Impossible code = empty */

#define HT_SIZE         8192    /* 13 bit hash table size */
#define HT_KEY_MASK     0x1FFF  /* 13 bit key mask */

#define IMAGE_LOADING   0       /* file_state = processing */
#define IMAGE_SAVING    0       /* file_state = processing */
#define IMAGE_COMPLETE  1       /* finished reading or writing */

typedef struct {
    FILE *file;
    int depth,
        clear_code, eof_code,
        running_code, running_bits,
        max_code_plus_one,
   prev_code, current_code,
        stack_ptr,
        shift_state;
    unsigned long shift_data;
    unsigned long pixel_count;
    int           file_state, position, bufsize;
    unsigned char buf[256];
    unsigned long hash_table[HT_SIZE];
  } GifEncoder;

typedef struct {
    FILE *file;
    int depth,
        clear_code, eof_code,
        running_code, running_bits,
        max_code_plus_one,
        prev_code, current_code,
        stack_ptr,
        shift_state;
    unsigned long shift_data;
    unsigned long pixel_count;
    int           file_state, position, bufsize;
    unsigned char buf[256];
    unsigned char stack[LZ_MAX_CODE+1];
    unsigned char suffix[LZ_MAX_CODE+1];
    unsigned int  prefix[LZ_MAX_CODE+1];
  } GifDecoder;


void * gif_alloc(long bytes);

int    read_gif_int(FILE *file);
void   write_gif_int(FILE *file, int output);

GifData * new_gif_data(int size);
GifData * read_gif_data(FILE *file);
void   del_gif_data(GifData *data);
void   write_gif_data(FILE *file, GifData *data);
void   print_gif_data(FILE *file, GifData *data);

GifPalette * new_gif_palette(void);
void   del_gif_palette(GifPalette *cmap);
void   read_gif_palette(FILE *file, GifPalette *cmap);
void   write_gif_palette(FILE *file, GifPalette *cmap);
void   print_gif_palette(FILE *file, GifPalette *cmap);

GifScreen * new_gif_screen(void);
void   del_gif_screen(GifScreen *screen);
void   read_gif_screen(FILE *file, GifScreen *screen);
void   write_gif_screen(FILE *file, GifScreen *screen);
void   print_gif_screen(FILE *file, GifScreen *screen);

GifExtension *new_gif_extension(void);
void   del_gif_extension(GifExtension *ext);
void   read_gif_extension(FILE *file, GifExtension *ext);
void   write_gif_extension(FILE *file, GifExtension *ext);
void   print_gif_extension(FILE *file, GifExtension *ext);

GifDecoder * new_gif_decoder(void);
void   del_gif_decoder(GifDecoder *decoder);
void   init_gif_decoder(FILE *file, GifDecoder *decoder);

int   read_gif_code(FILE *file, GifDecoder *decoder);
void   read_gif_line(FILE *file, GifDecoder *decoder, unsigned char *line, int length);

GifEncoder * new_gif_encoder(void);
void   del_gif_encoder(GifEncoder *encoder);
void   write_gif_code(FILE *file, GifEncoder *encoder, int code);
void   init_gif_encoder(FILE *file, GifEncoder *encoder, int depth);
void   write_gif_line(FILE *file, GifEncoder *encoder, unsigned char *line, int length);
void   flush_gif_encoder(FILE *file, GifEncoder *encoder);

GifPicture * new_gif_picture(void);
void   del_gif_picture(GifPicture *pic);
void   read_gif_picture(FILE *file, GifPicture *pic);
void   write_gif_picture(FILE *file, GifPicture *pic);
void   print_gif_picture(FILE *file, GifPicture *pic);

GifBlock *new_gif_block(void);
void   del_gif_block(GifBlock *block);
void   read_gif_block(FILE *file, GifBlock *block);
void   write_gif_block(FILE *file, GifBlock *block);
void   print_gif_block(FILE *file, GifBlock *block);

Gif *   new_gif(void);
void   del_gif(Gif *gif);
void   read_gif(FILE *file, Gif *gif);
void   read_one_gif_picture(FILE *file, Gif *gif);
void   write_gif(FILE *file, Gif *gif);
void   print_gif(FILE *file, Gif *gif);

Gif *   read_gif_file(char *filename);
int     write_gif_file(char *filename, Gif *gif);

//
// End of .GIF stuff
//

#pragma pack()

//****************************************************************************
//*                                                                          *
//* RGBUTILS: RGB color manipulation library                                 *
//*                                                                          *
//* 32-bit protected-mode source compatible with MSVC 10.2                   *
//*                                                                          *
//* Version 1.00 of 28-Jan-97: Initial, derived from IMAGEMAN RGBUTILS.H     *
//*                                                                          *
//* Author: John Miles                                                       *
//*                                                                          *
//****************************************************************************
//*                                                                          *
//* Copyright (C) 1997 Miles Design, Inc.                                    *
//*                                                                          *
//****************************************************************************

#ifndef RGBUTILS_H
#define RGBUTILS_H

class RGB_BOX
{
public:
    S32 r0;    // min value, exclusive
    S32 r1;    // max value, inclusive

    S32 g0;  
    S32 g1;

    S32 b0;  
    S32 b1;

    S32 vol;
};

class CQT
{
public:
   S32 table[33][33][33];
};

class CMAP
{
   S16       *best;
   S16       *second_best;
   VFX_RGB   *palette;
   U32        colors;

public:
   //
   // Color remapping functions
   //

   CMAP(VFX_RGB *palette, U32 colors);
  ~CMAP();

   U8 nearest_neighbor (VFX_RGB *triplet, S32 dither);
};

class CQ
{
   CQT wt;
   CQT mr;
   CQT mg;
   CQT mb;

   DOUBLE *m2;

   DOUBLE Var   (RGB_BOX *cube);
   void   M3d   (S32     *vwt,  S32 *vmr, S32 *vmg, S32 *vmb);
   S32    Vol   (RGB_BOX *cube,                  CQT *mmt);
   S32    Bottom(RGB_BOX *cube, U8 dir,          CQT *mmt);
   S32    Top   (RGB_BOX *cube, U8 dir, S32 pos, CQT *mmt);

   DOUBLE Maximize(RGB_BOX *cube,
                   U8       dir, 
                   S32      first, 
                   S32      last, 
                   S32     *cut,
                   S32      whole_r, 
                   S32      whole_g, 
                   S32      whole_b, 
                   S32      whole_w);

   S32 Cut(RGB_BOX *set1, RGB_BOX *set2);

public:
   CQ();
  ~CQ();

   //
   // Color quantization functions
   //

   void  reset     (void);
   void  add_color (VFX_RGB *triplet);
   U32   quantize  (VFX_RGB *out, U32 colors);
};

//****************************************************************************
//*                                                                          *
//* VFX RGB color manipulation library                                       *
//*                                                                          *
//* 32-bit protected-mode source compatible with MSVC 10.2                   *
//*                                                                          *
//* Version 1.00 of 28-Jan-97: Initial, derived from IMAGEMAN RGBUTILS.H     *
//*                                                                          *
//* Author: John Miles                                                       *
//*                                                                          *
//****************************************************************************
//*                                                                          *
//* Copyright (C) 1997 Miles Design, Inc.                                    *
//*                                                                          *
//****************************************************************************
//*                                                                          *
//*  Contains C++ implementation of Wu's color quantizer (v. 2)              *
//*  (See Graphics Gems vol. II, pp. 126-133)                                *
//*                                                                          *
//*  Author:     Xiaolin Wu                                                  *
//*              Dept. of Computer Science                                   *
//*              Univ. of Western Ontario                                    *
//*              London, Ontario N6A 5B7                                     *
//*              wu@csd.uwo.ca                                               *
//*                                                                          *
//*  Algorithm:  Greedy orthogonal bipartition of RGB space for variance     *
//*              minimization aided by inclusion-exclusion tricks.           *
//*                                                                          *
//*  The author thanks Tom Lane at Tom_Lane@G.GP.CS.CMU.EDU for much         *
//*  additional documentation and a cure to a previous bug.                  *
//*  Free to distribute, comments and suggestions are appreciated.           *
//*                                                                          *
//*  Modifications:                                                          *
//*                                                                          *
//*  25-Sep-93: (John Miles) Modified to use 386FX 32-bit types and          *
//*                          class-style API; added CMAP_class (nearest-     *
//*                          neighbor mapping)                               *
//*                                                                          *
//*  28-Jan-97: (John Miles) Modified to use TYPEDEFS.H, ported to C++       *
//*                                                                          *
//****************************************************************************

#define RGBU_RED   2
#define RGBU_GREEN 1   
#define RGBU_BLUE  0

//****************************************************************************
//
// At conclusion of the histogram step, we can interpret
//   wt[r] [g] [b] = sum over voxel of P(c)
//   mr[r] [g] [b] = sum over voxel of r*P(c), similarly for mg, mb
//   m2[r] [g] [b] = sum over voxel of c^2*P(c)
//
// Actually each of these should be divided by 'size' to give the usual
// interpretation of P() as ranging from 0 to 1, but we needn't do that here.
//
// We convert histogram into cumulative moments so that we can 
// rapidly calculate the sums of the above quantities over any desired box.
//
//****************************************************************************

void CQ::M3d(S32 *vwt, S32 *vmr, S32 *vmg, S32 *vmb)
{
   U32    ind1, ind2;
   U8     i, r, g, b;
   S32    line, line_r, line_g, line_b,
          area[33], area_r[33], area_g[33], area_b[33];
   DOUBLE line2, area2[33];

   for (r=1; r<=32; ++r)
     {
     for (i=0; i<=32; ++i)
        {
        area2[i] = 0.0F;
        area[i] = area_r[i] = area_g[i] = area_b[i] = 0;
        }

     for (g=1; g<=32; ++g)
        {
        line2 = 0.0F;
        line = line_r = line_g = line_b = 0;

        for (b=1; b<=32; ++b)
           {
           //
           // [r] [g] [b]
           //

           ind1 = (r<<10) + (r<<6) + r + (g<<5) + g + b;

           line   += vwt[ind1];
           line_r += vmr[ind1]; 
           line_g += vmg[ind1]; 
           line_b += vmb[ind1];

           line2  += m2[ind1];

           area[b]   += line;
           area_r[b] += line_r;
           area_g[b] += line_g;
           area_b[b] += line_b;

           area2[b] += line2;

           //
           // [r-1] [g] [b]
           //

           ind2 = ind1 - (33*33);

           vwt[ind1] = vwt[ind2] + area  [b];
           vmr[ind1] = vmr[ind2] + area_r[b];
           vmg[ind1] = vmg[ind2] + area_g[b];
           vmb[ind1] = vmb[ind2] + area_b[b];

           m2[ind1] = m2[ind2] + area2[b];
           }
        }
     }
}

//****************************************************************************
//
// Compute sum over a box of any given statistic
//
//****************************************************************************

S32 CQ::Vol(RGB_BOX *cube, CQT *mmt)
{
   return (mmt->table [cube->r1] [cube->g1] [cube->b1] 
          -mmt->table [cube->r1] [cube->g1] [cube->b0]
          -mmt->table [cube->r1] [cube->g0] [cube->b1]
          +mmt->table [cube->r1] [cube->g0] [cube->b0]
          -mmt->table [cube->r0] [cube->g1] [cube->b1]
          +mmt->table [cube->r0] [cube->g1] [cube->b0]
          +mmt->table [cube->r0] [cube->g0] [cube->b1]
          -mmt->table [cube->r0] [cube->g0] [cube->b0]);
}

//****************************************************************************
//
// The next two routines allow a slightly more efficient calculation
// of Vol() for a proposed subbox of a given box.  The sum of Top()
// and Bottom() is the Vol() of a subbox split in the given direction
// and with the specified new upper bound.
//
// Compute part of Vol(cube, mmt) that doesn't depend on r1, g1, or b1
// (depending on dir)
//
//****************************************************************************

S32 CQ::Bottom(RGB_BOX *cube, U8 dir, CQT *mmt)
{
   switch (dir)
      {
      case RGBU_RED   : return (-mmt->table [cube->r0] [cube->g1] [cube->b1]
                           +mmt->table [cube->r0] [cube->g1] [cube->b0]
                           +mmt->table [cube->r0] [cube->g0] [cube->b1]
                           -mmt->table [cube->r0] [cube->g0] [cube->b0]);
                                
                                
      case RGBU_GREEN : return (-mmt->table [cube->r1] [cube->g0] [cube->b1]
                           +mmt->table [cube->r1] [cube->g0] [cube->b0]
                           +mmt->table [cube->r0] [cube->g0] [cube->b1]
                           -mmt->table [cube->r0] [cube->g0] [cube->b0]);
                                
      case RGBU_BLUE  : return (-mmt->table [cube->r1] [cube->g1] [cube->b0]
                           +mmt->table [cube->r1] [cube->g0] [cube->b0]
                           +mmt->table [cube->r0] [cube->g1] [cube->b0]
                           -mmt->table [cube->r0] [cube->g0] [cube->b0]);
      }

   return 0;
}

//****************************************************************************
//
// Compute remainder of Vol(cube, mmt), substituting pos for
// r1, g1, or b1 (depending on dir)
//
//****************************************************************************

S32 CQ::Top(RGB_BOX *cube, U8 dir, S32 pos, CQT *mmt)
{
   switch (dir)
      {
      case RGBU_RED   : return (mmt->table  [pos] [cube->g1] [cube->b1]  
                          -mmt->table  [pos] [cube->g1] [cube->b0]
                          -mmt->table  [pos] [cube->g0] [cube->b1]
                          +mmt->table  [pos] [cube->g0] [cube->b0]);
                               
      case RGBU_GREEN : return (mmt->table  [cube->r1] [pos] [cube->b1] 
                          -mmt->table  [cube->r1] [pos] [cube->b0]
                          -mmt->table  [cube->r0] [pos] [cube->b1]
                          +mmt->table  [cube->r0] [pos] [cube->b0]);
                               
      case RGBU_BLUE  : return (mmt->table  [cube->r1] [cube->g1] [pos]
                          -mmt->table  [cube->r1] [cube->g0] [pos]
                          -mmt->table  [cube->r0] [cube->g1] [pos]
                          +mmt->table  [cube->r0] [cube->g0] [pos]);
      }

   return 0;
}

//****************************************************************************
//
// Compute the weighted variance of a box
// NB: as with the raw statistics, this is really the variance * size
//
//****************************************************************************

DOUBLE CQ::Var(RGB_BOX *cube)
{
   DOUBLE dr, dg, db, xx;

   dr = (DOUBLE) Vol(cube, &mr); 
   dg = (DOUBLE) Vol(cube, &mg); 
   db = (DOUBLE) Vol(cube, &mb);

   xx = m2 [cube->r1*33*33 + cube->g1*33 + cube->b1] 
       -m2 [cube->r1*33*33 + cube->g1*33 + cube->b0]
       -m2 [cube->r1*33*33 + cube->g0*33 + cube->b1]
       +m2 [cube->r1*33*33 + cube->g0*33 + cube->b0]
       -m2 [cube->r0*33*33 + cube->g1*33 + cube->b1]
       +m2 [cube->r0*33*33 + cube->g1*33 + cube->b0]
       +m2 [cube->r0*33*33 + cube->g0*33 + cube->b1]
       -m2 [cube->r0*33*33 + cube->g0*33 + cube->b0];

   return (xx - (dr*dr + dg*dg + db*db) / (DOUBLE) Vol(cube, &wt));    
}

//****************************************************************************
//
// We want to minimize the sum of the variances of two subboxes.
// The sum(c^2) terms can be ignored since their sum over both subboxes
// is the same (the sum for the whole box) no matter where we split.
// The remaining terms have a minus sign in the variance formula,
// so we drop the minus sign and MAXIMIZE the sum of the two terms.
//
//****************************************************************************

DOUBLE CQ::Maximize(RGB_BOX *cube, //)
                   U8    dir, 
                   S32     first, 
                   S32     last, 
                   S32    *cut,
                   S32     whole_r, 
                   S32     whole_g, 
                   S32     whole_b, 
                   S32     whole_w)
{
   S32    half_r, half_g, half_b, half_w;
   S32    base_r, base_g, base_b, base_w;
   S32    i;
   DOUBLE temp, max;

   base_r = Bottom(cube, dir, &mr);
   base_g = Bottom(cube, dir, &mg);
   base_b = Bottom(cube, dir, &mb);
   base_w = Bottom(cube, dir, &wt);

   max = 0.0F;
   *cut = -1;

   for (i=first; i<last; ++i)
      {
      half_r = base_r + Top(cube, dir, i, &mr);
      half_g = base_g + Top(cube, dir, i, &mg);
      half_b = base_b + Top(cube, dir, i, &mb);
      half_w = base_w + Top(cube, dir, i, &wt);

      //
      // Now half_x is sum over lower half of box, if split at i 
      //
      // Subbox could be empty of pixels; never split into an empty box
      // 

      if (half_w == 0)
         continue;

      temp = ((DOUBLE) half_r*half_r +
              (DOUBLE) half_g*half_g +
              (DOUBLE) half_b*half_b) / half_w;

      half_r = whole_r - half_r;
      half_g = whole_g - half_g;
      half_b = whole_b - half_b;
      half_w = whole_w - half_w;

      //
      // Subbox could be empty of pixels; never split into an empty box
      // 

      if (half_w == 0)
         continue;

      temp += ((DOUBLE) half_r*half_r +
               (DOUBLE) half_g*half_g +
               (DOUBLE) half_b*half_b) / half_w;

      if (temp > max)
         {
         max=temp;
         *cut=i;
         }
      }

   return max;
}

S32 CQ::Cut(RGB_BOX *set1, RGB_BOX *set2)
{
   U8     dir;
   S32    cutr, cutg, cutb;
   DOUBLE maxr, maxg, maxb;
   S32    whole_r, whole_g, whole_b, whole_w;

   whole_r = Vol(set1, &mr);
   whole_g = Vol(set1, &mg);
   whole_b = Vol(set1, &mb);
   whole_w = Vol(set1, &wt);

   maxr = Maximize(set1, RGBU_RED,   set1->r0+1, set1->r1, &cutr,
                   whole_r, whole_g, whole_b, whole_w);

   maxg = Maximize(set1, RGBU_GREEN, set1->g0+1, set1->g1, &cutg,
                   whole_r, whole_g, whole_b, whole_w);

   maxb = Maximize(set1, RGBU_BLUE,  set1->b0+1, set1->b1, &cutb,
                   whole_r, whole_g, whole_b, whole_w);

   if ((maxr >= maxg) && (maxr >= maxb))
      {
      dir = RGBU_RED;
                            
      if (cutr < 0)
         return 0;
      }
   else
      if ((maxg >= maxr) && (maxg >= maxb))
         {
         dir = RGBU_GREEN;

         if (cutg < 0)
            return 0;
         }
      else
         {
         dir = RGBU_BLUE;

         if (cutb < 0)
            return 0;
         }

    set2->r1 = set1->r1;
    set2->g1 = set1->g1;
    set2->b1 = set1->b1;

    switch (dir)
      {
      case RGBU_RED:

          set2->r0 = set1->r1 = cutr;
          set2->g0 = set1->g0;
          set2->b0 = set1->b0;
          break;

      case RGBU_GREEN:

          set2->g0 = set1->g1 = cutg;
          set2->r0 = set1->r0;
          set2->b0 = set1->b0;
          break;

      case RGBU_BLUE:

          set2->b0 = set1->b1 = cutb;
          set2->r0 = set1->r0;
          set2->g0 = set1->g0;
          break;
      }

    set1->vol = (set1->r1 - set1->r0) *
                (set1->g1 - set1->g0) *
                (set1->b1 - set1->b0);

    set2->vol = (set2->r1 - set2->r0) *
                (set2->g1 - set2->g0) *
                (set2->b1 - set2->b0);

    return 1;
}

//****************************************************************************
//
// Construct an instance of class CQ
//
//****************************************************************************

CQ::CQ(void)
{
   //
   // Warning: virtual functions unsupported!
   //
   
   memset(this, 0, sizeof(*this));

   m2 = (DOUBLE *) calloc(33*33*33,sizeof(DOUBLE));

   reset();
}

//****************************************************************************
//
// Free an instance of CQ
//
//****************************************************************************

CQ::~CQ(void)
{
   free(m2);
}

//****************************************************************************
//
// Initialize color histogram
//
// Histogram is in elements 1..HISTSIZE along each axis,
// element 0 is for base or marginal value
//
//****************************************************************************

void CQ::reset(void)
{
   U32 i, j, k;
   
   for (i=0; i<33; ++i)
      for (j=0; j<33; ++j)
         for (k=0; k<33; ++k)
            {
            wt.table [i] [j] [k] = 0;
            mr.table [i] [j] [k] = 0;
            mg.table [i] [j] [k] = 0;
            mb.table [i] [j] [k] = 0;

            m2[i*33*33 + j*33 + k] = 0.0F;
            }
}

//****************************************************************************
//
// Build 3D color histogram of color counts
//
//****************************************************************************

void CQ::add_color(VFX_RGB *triplet)
{
   static S32 table[256];
   static S32 table_valid = 0;
   S32        r, g, b;
   S32        inr, ing, inb;
   S32        i;

   //
   // Build table of squares, if not already valid
   // 

   if (!table_valid)
      {
      for (i=0; i<256; ++i)
         {
         table[i] = i*i;
         }

      table_valid = 1;
      }
      
   r = triplet->r; 
   g = triplet->g; 
   b = triplet->b;

   inr = (r >> 3) + 1; 
   ing = (g >> 3) + 1; 
   inb = (b >> 3) + 1;

   wt.table [inr] [ing] [inb]++;
   mr.table [inr] [ing] [inb] += r;
   mg.table [inr] [ing] [inb] += g;
   mb.table [inr] [ing] [inb] += b;

   m2 [inr*33*33 + ing*33 + inb] += (DOUBLE)(table[r] + table[g] + table[b]);
}

//****************************************************************************
//
// Generate optimal color palette based on input
//
//****************************************************************************

U32 CQ::quantize(VFX_RGB *out, U32 colors)

{
   RGB_BOX *cube;
   U32      next;
   U32      k,i;
   S32      weight;
   DOUBLE  *vv, temp;

   if (colors == 0)
      {
      return 0;
      }

   if (colors > 256)
      {
      colors = 256;
      }

   if ((vv = (DOUBLE *) calloc(colors,sizeof(DOUBLE))) == NULL)
      return 0;

   if ((cube = (RGB_BOX *) calloc(colors,sizeof(RGB_BOX))) == NULL)
      {
      free(vv);
      return 0;
      }

   M3d((S32 *) &wt.table,
       (S32 *) &mr.table,
       (S32 *) &mg.table,
       (S32 *) &mb.table);

   cube[0].r0 = cube[0].g0 = cube[0].b0 = 0;
   cube[0].r1 = cube[0].g1 = cube[0].b1 = 32;

   next = 0;

   for (i=1; i < colors; ++i)
      {
      if (Cut(&cube[next], &cube[i]))
         {
         //
         // Volume test ensures we won't try to cut one-cell box
         //

         vv[next] = (cube[next].vol > 1) ?
                    Var(&cube[next]) : 0.0F;

         vv[i]    = (cube[i].vol > 1)    ?
                    Var(&cube[i])    : 0.0F;
         }
      else
         {
         //
         // Don't try to split this box again
         // 

         vv[next] = 0.0F;
         i--;
         }

      next = 0; temp = vv[0];

      for (k=1; k <= i; ++k)
         {
         if (vv[k] > temp)
            {
            temp = vv[k];
            next = k;
            }
         }

      if (temp <= 0.0F)
         {
         colors = i+1;
         break;
         }
      }

   for (k=0; k < colors; ++k)
      {
      weight = Vol(&cube[k], &wt);

      if (weight)
         {
         out[k].r = (U8) (Vol(&cube[k], &mr) / weight);
         out[k].g = (U8) (Vol(&cube[k], &mg) / weight);
         out[k].b = (U8) (Vol(&cube[k], &mb) / weight);
         }
      else
         {
         out[k].r = out[k].g = out[k].b = 0;
         }
      }

   free(cube);
   free(vv);

   return colors;
}

/***************************************************************************/
//
// CMAP_class notes:
//
// Like CQ above, the CMAP functions require 8-bit RGB values
// for proper operation, and operate at 5-bit resolution internally.
//
/***************************************************************************/

/***************************************************************************/
//
// Construct an instance of CMAP_class
//
/***************************************************************************/

CMAP::CMAP(VFX_RGB *_palette, U32 _colors)
{
   U32 i;

   //
   // Warning: virtual functions unsupported!
   //
   
   memset(this, 0, sizeof(*this));

   best        = (S16 *) calloc(32768, sizeof(S16));
   second_best = (S16 *) calloc(32768, sizeof(S16));

   //
   // Initialize all RGB scoreboard values to -1 (unmapped)
   //

   for (i=0; i < 32768; i++)
      {
      best[i]        = -1;
      second_best[i] = -1;
      }

   palette = _palette;
   colors  = _colors;
}

/***************************************************************************/
//
// Free an instance of CMAP
//
/***************************************************************************/

CMAP::~CMAP()
{
   if (best != NULL)
      {
      free(best);
      best = NULL;
      }

   if (second_best != NULL)
      {
      free(second_best);
      second_best = NULL;
      }
}

/***************************************************************************/
//
// Find palette color whose RGB value is closest to *triplet
//
// Minimize sum of square axis displacements; true Euclidean 
// distance is not needed for comparisons
//
/***************************************************************************/

U8 CMAP::nearest_neighbor(VFX_RGB *triplet, S32 dither)
{
   U32        r,g,b,key,c1,c2,min,dist;
   S32        i,dr,dg,db;
   static U32 square[511];
   static U32 square_valid = 0;

   //
   // Convert 8-bit RGB to 5-bit RGB
   //

   r = triplet->r;
   g = triplet->g;
   b = triplet->b;

   //
   // If dithering, set up to return best or second-best match
   //

   S16 *choice = best;

   if (dither)
      {
      if ((rand() & 0x0f) >= dither)
         {
         choice = second_best;
         }
      }

   //
   // See if this triplet has already been remapped; if so, return
   // proper value immediately
   //

   key = ((r>>3) << 10) | ((g>>3) << 5) | (b>>3);

   if (choice[key] != -1)
      {
      return (U8) choice[key];
      }

   //
   // Build square[] table if not already valid
   //

   if (!square_valid)
      {
      for (i=-255; i<=255; i++)
         {
         square[i+255] = i*i;
         }

      square_valid = 1;
      }

   //
   // Find first- and second- best-fit palette entry
   //

   i   =  colors;
   min =  ULONG_MAX;
   c1  =  0;

   while (i > 0)
      {
      i--;
   
      dr = (S32) palette[i].r - (S32) r + 255;
      dg = (S32) palette[i].g - (S32) g + 255;
      db = (S32) palette[i].b - (S32) b + 255;

      dist = square[dr] + square[dg] + square[db];

      if (dist <= min)
         {
         c1 = i;

         if (dist > 0)
            {
            min = dist;
            }
         else
            {
            break;
            }
         }
      }

   i   =  colors;
   min =  ULONG_MAX;
   c2  = (U32) -1;

   while (i > 0)
      {
      i--;

      if (i == (S32) c1)
         {
         continue;
         }
   
      dr = (S32) palette[i].r - (S32) r + 255;
      dg = (S32) palette[i].g - (S32) g + 255;
      db = (S32) palette[i].b - (S32) b + 255;

      dist = square[dr] + square[dg] + square[db];

      if (dist <= min)
         {
         c2 = i;

         if (dist > 0)
            {
            min = dist;
            }
         else
            {
            break;
            }
         }
      }

   //
   // Log match in best-fit and second-best-fit scoreboards to avoid 
   // redundant searches later
   //

   best       [key] = (S16) c1;
   second_best[key] = (S16) c2;

   return (U8) choice[key];
}

#endif

//****************************************************************************
//
// Parse memory-resident copy of .TGA file, returning dimensions and pointer
// to decoded pixel block
//
//****************************************************************************

VFX_RGB *  TGA_parse(void *TGA_image, //)
                            S32  *x_res, 
                            S32  *y_res)
{
   //
   // Acquire header pointer and validate file type
   //

   TGA_HDR *file = (TGA_HDR *) TGA_image;

   switch (file->image_type)
      {
      case 0:
         SAL_alert_box("Error","No image data in file\n");
         return NULL;

      case TGA_FORMAT_RGB_UNCOMPRESSED:
      case TGA_FORMAT_RGB_RLE:
         break;

      default:
         SAL_alert_box("Error","Unrecognized image format %d -- 16-, 24- or 32-bpp uncompressed or RLE file required\n",
            file->image_type);
         return NULL;
      }

   //
   // Require 16-bit, 24-bit, or 32-bit files
   //

   S32 BPP = file->bits_per_pixel / 8;

   if ((BPP != 3) && 
       (BPP != 4) &&
       (BPP != 2))
      {
      SAL_alert_box("Error","File has %d bits per pixel -- 16-, 24-, or 32-bpp file required\n",
         file->bits_per_pixel);
      return NULL;
      }

   //
   // We don't support interleaved files or files with weird origins...
   //

   if (file->image_descriptor_flags & (TGA_INTERLEAVE_2 | 
                                       TGA_INTERLEAVE_4 |
                                       TGA_ORIGIN_RIGHT))
      {
      SAL_alert_box("Error","Unrecognized image descriptor %X\n",
         file->image_descriptor_flags);
      return NULL;
      }

   //
   // Input pointer follows header
   //

   U8 *in = ((U8 *) file) + sizeof(TGA_HDR);

   //
   // Skip image ID field, if present
   //

   in += file->len_image_ID;

   //
   // Skip color map, if present
   //

   if (file->color_map_present)
      {
      in += (((file->color_map_entry_size) / 8) * file->color_map_len);
      }

   //
   // Allocate memory for output RGB data
   //

   VFX_RGB *out = (VFX_RGB *) malloc(sizeof(VFX_RGB) * 
                                     file->pixel_width *
                                     file->pixel_height);
   if (out == NULL)
      {
      SAL_alert_box("Error","Could not allocate output data block\n");
      return NULL;
      }

   //
   // Set up Y origin and increment
   //

   S32 y,dy,ylim;

   if (file->image_descriptor_flags & TGA_ORIGIN_TOP)
      {
      y    =  0;
      dy   =  1;
      ylim =  file->pixel_height;
      }
   else
      {
      y    =  file->pixel_height-1;
      dy   = -1;
      ylim = -1;
      }       

   //
   // Unpack file to output RGB buffer
   // 

   S32 w = file->pixel_width;

   switch (file->image_type)
      {
      //
      // Uncompressed RGB data
      //

      case TGA_FORMAT_RGB_UNCOMPRESSED:
         {
         while (y != ylim)
            {
            for (S32 x=0; x < w; x++)
               {
               VFX_RGB RGB;

               if (BPP == 2)
                  {
                  RGB.r = ((in[1] >> 2) & 31) << 3;
                  RGB.g = (((in[0] & 0xe0) >> 5) | ((in[1] & 0x03) << 3)) << 3;
                  RGB.b = (in[0] & 0x1f) << 3;
                  }
               else
                  {
                  RGB.r = in[2];
                  RGB.g = in[1];
                  RGB.b = in[0];
                  }

               in += BPP;

               out[(y * w) + x] = RGB;
               }

            y += dy;
            }
         break;
         }

      //
      // RLE-encoded RGB data
      //

      case TGA_FORMAT_RGB_RLE:
         {
         enum {rep,raw};
         S32  state;
         S32  cnt;
         U8  *val;

         state = (*in & 0x80) ? rep : raw;
         cnt   = (*in & 0x7f);
         val   = &in[1];

         while (y != ylim)
            {
            for (S32 x=0; x < w; x++)
               {
               VFX_RGB RGB;

               if (BPP == 2)
                  {
                  RGB.r = ((val[1] >> 2) & 31) << 3;
                  RGB.g = (((val[0] & 0xe0) >> 5) | ((val[1] & 0x03) << 3)) << 3;
                  RGB.b = (val[0] & 0x1f) << 3;
                  }
               else
                  {
                  RGB.r = val[2];
                  RGB.g = val[1];
                  RGB.b = val[0];
                  }

               if (state == raw)
                  {
                  val += BPP;
                  }

               if (!cnt--)
                  {
                  in    = &val[(state == rep) * BPP];
                  state =  (*in & 0x80) ? rep : raw;
                  cnt   =  (*in & 0x7f);
                  val   = &in[1];
                  }

               out[(y * w) + x] = RGB;
               }

            y += dy;
            }

         break;
         }
      }
 
   //
   // Return file location and size to caller
   //

   if (x_res != NULL)
      {
      *x_res = file->pixel_width;
      }

   if (y_res != NULL)
      {
      *y_res = file->pixel_height;
      }

   return out;
}

//****************************************************************************
//
// Write contents of VFX pane to .TGA file, returning 1 if OK or 0 on error 
//
//****************************************************************************

bool TGA_write_16bpp  (PANE *src, //)
                       C8   *filename)
{
   //
   // Open output file
   //

   FILE *out = fopen(filename,"w+b");
   
   if (out == NULL)
      {
      return 0;
      }

   //
   // Get pane dimensions
   //

   S32 w = 0;
   S32 h = 0;
   PANE_DIMS(src, w, h)

   //
   // Compose and write TGA header
   //

   TGA_HDR TGA;

   TGA.len_image_ID           = 0;
   TGA.color_map_present      = 0;

#if WRITE_COMPRESSED_FILE
   TGA.image_type             = TGA_FORMAT_RGB_RLE;
#else
   TGA.image_type             = TGA_FORMAT_RGB_UNCOMPRESSED;
#endif

   TGA.color_map_origin       = 0;
   TGA.color_map_len          = 0;
   TGA.color_map_entry_size   = 0;
   TGA.X_origin               = 0;
   TGA.Y_origin               = 0;
   TGA.pixel_width            = (S16) w;
   TGA.pixel_height           = (S16) h;
   TGA.bits_per_pixel         = 16;
   TGA.image_descriptor_flags = TGA_ORIGIN_TOP;

   if (fwrite(&TGA,
               sizeof(TGA),
               1,
               out) != 1)
      {
      fclose(out);
      return 0;
      }

   //
   // Write pixel data
   //

#if WRITE_COMPRESSED_FILE

   U16 raw_data_record[128];
   U32 raw_data_len = 0;

   for (S32 y=0; y < h; y++)
      {
      for (S32 x=0; x < w; x++)
         {
         //
         // Read pixel at (x,y)
         //

         VFX_RGB *RGB = VFX_RGB_value(VFX_pixel_read(src, x, y));

         U16 out_word = ((RGB->r >> 3) << 10) |
                        ((RGB->g >> 3) << 5 ) |
                        ((RGB->b >> 3) << 0 );

         //
         // Count # of successive occurrences of this pixel, up to 128 in a row
         //

         S32 r;
         S32 n = 1;

         for (r=x+1; r < w; r++)
            {
            RGB = VFX_RGB_value(VFX_pixel_read(src, r, y));

            U16 test_word = ((RGB->r >> 3) << 10) |
                            ((RGB->g >> 3) << 5 ) |
                            ((RGB->b >> 3) << 0 );

            if (test_word != out_word)
               {
               --r;
               break;
               }
            else
               {
               ++n;

               if (n == 128)
                  {
                  break;
                  }
               }
            }

         x = r;

         //
         // If we have more than one repetition, write RLE record
         // (0x80 | n-1 followed by word to repeat), then continue
         //

         if (n > 1)
            {
            //
            // First, flush any raw data we have buffered
            //

            if (raw_data_len > 0)
               {
               U8 raw_byte = (U8) (raw_data_len - 1);

               if (fwrite(&raw_byte,
                           sizeof(raw_byte),
                           1,
                           out) != 1)
                  {
                  fclose(out);
                  return 0;
                  }

               if (fwrite(&raw_data_record,
                           sizeof(raw_data_record[0]),
                           raw_data_len,
                           out) != raw_data_len)
                  {
                  fclose(out);
                  return 0;
                  }

               raw_data_len = 0;
               }

            //
            // Finally, write RLE record
            //

            U8 RLE_byte = (U8) (0x80 | (n - 1));

            if (fwrite(&RLE_byte,
                        sizeof(RLE_byte),
                        1,
                        out) != 1)
               {
               fclose(out);
               return 0;
               }

            if (fwrite(&out_word,
                        sizeof(out_word),
                        1,
                        out) != 1)
               {
               fclose(out);
               return 0;
               }

            //
            // Continue with next pixel in line
            //

            continue;
            }

         //
         // Current pixel is not repeated, so add it to the raw-data-record
         // buffer and continue
         //

         raw_data_record[raw_data_len++] = out_word;

         //
         // If raw data record full, flush it
         //

         if (raw_data_len == 128)
            {
            U8 raw_byte = (U8) (raw_data_len - 1);

            if (fwrite(&raw_byte,
                        sizeof(raw_byte),
                        1,
                        out) != 1)
               {
               fclose(out);
               return 0;
               }

            if (fwrite(&raw_data_record,
                        sizeof(raw_data_record[0]),
                        raw_data_len,
                        out) != raw_data_len)
               {
               fclose(out);
               return 0;
               }

            raw_data_len = 0;
            }
         }

      //
      // Flush raw data buffer, if not empty
      //

      if (raw_data_len > 0)
         {
         U8 raw_byte = (U8) (raw_data_len - 1);

         if (fwrite(&raw_byte,
                     sizeof(raw_byte),
                     1,
                     out) != 1)
            {
            fclose(out);
            return 0;
            }

         if (fwrite(&raw_data_record,
                     sizeof(raw_data_record[0]),
                     raw_data_len,
                     out) != raw_data_len)
            {
            fclose(out);
            return 0;
            }

         raw_data_len = 0;
         }

      //
      // Continue with next row of pixels
      //
      }

#else

   for (S32 y=0; y < h; y++)
      {
      for (S32 x=0; x < w; x++)
         {
         VFX_RGB *RGB = VFX_RGB_value(VFX_pixel_read(src, x, y));

         U16 out_word = ((RGB->r >> 3) << 10) |
                        ((RGB->g >> 3) << 5 ) |
                        ((RGB->b >> 3) << 0 );

         if (fwrite(&out_word,
                     sizeof(out_word),
                     1,
                     out) != 1)
            {
            fclose(out);
            return 0;
            }
         }
      }

#endif

   //
   // Close output file and return success
   //

   fclose(out);

   return 1;
}

//****************************************************************************
//
// GIF routines from GraphApp
//
//****************************************************************************

#define app_zero_alloc(x) calloc(1, x)
#define app_alloc         malloc
#define app_realloc       realloc
#define app_free          free
#define app_open_file     fopen
#define app_close_file    fclose

#define rgb(r,g,b)             app_new_rgb((r),(g),(b))

GifColor app_new_rgb(int r, int g, int b)
{
   GifColor c;

   c.alpha  = 0;
   c.red    = (byte) r;
   c.green  = (byte) g;
   c.blue   = (byte) b;

   return c;
}

/*
 *  GIF memory allocation helper functions.
 */

void * gif_alloc(long bytes)
{
   return app_zero_alloc(bytes);
}

/*
 *  GIF file input/output functions.
 */

static unsigned char read_byte(FILE *file)
{
   int ch = getc(file);
   if (ch == EOF)
      ch = 0;
   return (unsigned char) ch;
}

static int write_byte(FILE *file, int ch)
{
   return putc(ch, file);
}

static int read_stream(FILE *file, unsigned char buffer[], int length)
{
   int count = (int) fread(buffer, 1, length, file);
   int i = count;
   while (i < length)
      buffer[i++] = '\0';
   return count;
}

static int write_stream(FILE *file, unsigned char buffer[], int length)
{
   return (int) fwrite(buffer, 1, length, file);
}

int read_gif_int(FILE *file)
{
   int output;
   unsigned char buf[2];

   if (fread(buf, 1, 2, file) != 2)
      return 0;
   output = (((unsigned int) buf[1]) << 8) | buf[0];
   return output;
}

void write_gif_int(FILE *file, int output)
{
   putc ( (output & 0xff), file);
   putc ( (((unsigned int) output) >> 8) & 0xff, file);
}

/*
 *  Gif data blocks:
 */

GifData * new_gif_data(int size)
{
   GifData *data = (GifData *) gif_alloc(sizeof(GifData));
   if (data) {
      data->byte_count = size;
      data->bytes = (unsigned char *) app_zero_alloc(size * sizeof(unsigned char));
   }
   return data;
}

void del_gif_data(GifData *data)
{
   app_free(data->bytes);
   app_free(data);
}

/*
 *  Read one code block from the Gif file.
 *  This routine should be called until NULL is returned.
 *  Use app_free() to free the returned array of bytes.
 */
GifData * read_gif_data(FILE *file)
{
   GifData *data;
   int size;

   size = read_byte(file);

   if (size > 0) {
      data = new_gif_data(size);
      read_stream(file, data->bytes, size);
   }
   else {
      data = NULL;
   }
   return data;
}

/*
 *  Write a Gif data block to a file.
 *  A Gif data block is a size-byte followed by that many
 *  bytes of data (0 to 255 of them).
 */
void write_gif_data(FILE *file, GifData *data)
{
   if (data) {
      write_byte(file, data->byte_count);
      write_stream(file, data->bytes, data->byte_count);
   }
   else
      write_byte(file, 0);
}

#ifdef GIF_DEBUG
void print_gif_data(FILE *file, GifData *data)
{
   int i, ch, prev;
   int ch_printable, prev_printable;

   if (data) {
      fprintf(file, "(length=%d) [", data->byte_count);
      prev_printable = 1;
      for (i=0; i < data->byte_count; i++) {
         ch = data->bytes[i];
         ch_printable = isprint(ch) ? 1 : 0;

         if (ch_printable != prev_printable)
            fprintf(file, " ");

         if (ch_printable)
            fprintf(file, "%c", (char)ch);
         else
            fprintf(file, "%02X,", ch);

         prev = ch;
         prev_printable = isprint(prev) ? 1 : 0;
      }
      fprintf(file, "]\n");
   }
   else {
      fprintf(file, "[]\n");
   }
}
#endif

/*
 *  Read the next byte from a Gif file.
 *
 *  This function is aware of the block-nature of Gif files,
 *  and will automatically skip to the next block to find
 *  a new byte to read, or return 0 if there is no next block.
 */
static unsigned char read_gif_byte(FILE *file, GifDecoder *decoder)
{
   unsigned char *buf = decoder->buf;
   unsigned char next;

   if (decoder->file_state == IMAGE_COMPLETE)
      return '\0';

   if (decoder->position == decoder->bufsize)
   {   /* internal buffer now empty! */
      /* read the block size */
      decoder->bufsize = read_byte(file);
      if (decoder->bufsize == 0) {
         decoder->file_state = IMAGE_COMPLETE;
         return '\0';
      }
      read_stream(file, buf, decoder->bufsize);
      next = buf[0];
      decoder->position = 1;   /* where to get chars */
   }
   else {
      next = buf[decoder->position++];
   }

   return next;
}

/*
 *  Read to end of an image, including the zero block.
 */
static void finish_gif_picture(FILE *file, GifDecoder *decoder)
{
   unsigned char *buf = decoder->buf;

   while (decoder->bufsize != 0) {
      decoder->bufsize = read_byte(file);
      if (decoder->bufsize == 0) {
         decoder->file_state = IMAGE_COMPLETE;
         break;
      }
      read_stream(file, buf, decoder->bufsize);
   }
}

/*
 *  Write a byte to a Gif file.
 *
 *  This function is aware of Gif block structure and buffers
 *  chars until 255 can be written, writing the size byte first.
 *  If FLUSH_OUTPUT is the char to be written, the buffer is
 *  written and an empty block appended.
 */
static void write_gif_byte(FILE *file, GifEncoder *encoder, int ch)
{
   unsigned char *buf = encoder->buf;

   if (encoder->file_state == IMAGE_COMPLETE)
      return;

   if (ch == FLUSH_OUTPUT)
   {
      if (encoder->bufsize) {
         write_byte(file, encoder->bufsize);
         write_stream(file, buf, encoder->bufsize);
         encoder->bufsize = 0;
      }
      /* write an empty block to mark end of data */
      write_byte(file, 0);
      encoder->file_state = IMAGE_COMPLETE;
   }
   else {
      if (encoder->bufsize == 255) {
         /* write this buffer to the file */
         write_byte(file, encoder->bufsize);
         write_stream(file, buf, encoder->bufsize);
         encoder->bufsize = 0;
      }
      buf[encoder->bufsize++] = (unsigned char) ch;
   }
}

/*
 *  GifColor maps:
 */

GifPalette * new_gif_palette(void)
{
   return (GifPalette *) gif_alloc(sizeof(GifPalette));
}

void del_gif_palette(GifPalette *cmap)
{
   app_free(cmap->colours);
   app_free(cmap);
}

void read_gif_palette(FILE *file, GifPalette *cmap)
{
   int i;
   unsigned char r, g, b;

   cmap->colours = (GifColor *) app_alloc(cmap->length * sizeof(GifColor));

   for (i=0; i<cmap->length; i++) {
      r = read_byte(file);
      g = read_byte(file);
      b = read_byte(file);
      cmap->colours[i] = rgb(r,g,b);
   }
}

void write_gif_palette(FILE *file, GifPalette *cmap)
{
   int i;
   GifColor c;

   for (i=0; i<cmap->length; i++) {
      c = cmap->colours[i];
      write_byte(file, c.red);
      write_byte(file, c.green);
      write_byte(file, c.blue);
   }
}

#ifdef GIF_DEBUG
void print_gif_palette(FILE *file, GifPalette *cmap)
{
   int i;

   fprintf(file, "  GifPalette (length=%d):\n", cmap->length);
   for (i=0; i<cmap->length; i++) {
      fprintf(file, "   %02X = 0x", i);
      fprintf(file, "%02X", cmap->colours[i].red);
      fprintf(file, "%02X", cmap->colours[i].green);
      fprintf(file, "%02X\n", cmap->colours[i].blue);
   }
}
#endif

/*
 *  GifScreen:
 */

GifScreen * new_gif_screen(void)
{
   GifScreen *screen = (GifScreen *) gif_alloc(sizeof(GifScreen));
   if (screen)
      screen->cmap = new_gif_palette();
   return screen;
}

void del_gif_screen(GifScreen *screen)
{
   del_gif_palette(screen->cmap);
   app_free(screen);
}

void read_gif_screen(FILE *file, GifScreen *screen)
{
   unsigned char info;

   screen->width       = read_gif_int(file);
   screen->height      = read_gif_int(file);

   info                = read_byte(file);
   screen->has_cmap    =  (info & 0x80) >> 7;
   screen->color_res   = ((info & 0x70) >> 4) + 1;
   screen->sorted      =  (info & 0x08) >> 3;
   screen->cmap_depth  =  (info & 0x07)       + 1;

   screen->bgcolour    = read_byte(file);
   screen->aspect      = read_byte(file);

   if (screen->has_cmap) {
      screen->cmap->length = 1 << screen->cmap_depth;
      read_gif_palette(file, screen->cmap);
   }
}

void write_gif_screen(FILE *file, GifScreen *screen)
{
   unsigned char info;

   write_gif_int(file, screen->width);
   write_gif_int(file, screen->height);

   info = 0;
   info = info | (unsigned char) (screen->has_cmap ? 0x80 : 0x00);
   info = info | (unsigned char) ((screen->color_res - 1) << 4);
   info = info | (unsigned char) (screen->sorted ? 0x08 : 0x00);
   if (screen->cmap_depth > 0)
      info = info | (unsigned char) ((screen->cmap_depth) - 1);
   write_byte(file, info);

   write_byte(file, screen->bgcolour);
   write_byte(file, screen->aspect);

   if (screen->has_cmap) {
      write_gif_palette(file, screen->cmap);
   }
}

#ifdef GIF_DEBUG
void print_gif_screen(FILE *file, GifScreen *screen)
{
   fprintf(file, " GifScreen:\n");
   fprintf(file, "  width      = %d\n", screen->width);
   fprintf(file, "  height     = %d\n", screen->height);

   fprintf(file, "  has_cmap   = %d\n", screen->has_cmap ? 1:0);
   fprintf(file, "  color_res  = %d\n", screen->color_res);
   fprintf(file, "  sorted     = %d\n", screen->sorted ? 1:0);
   fprintf(file, "  cmap_depth = %d\n", screen->cmap_depth);

   fprintf(file, "  bgcolour   = %02X\n", screen->bgcolour);
   fprintf(file, "  aspect     = %d\n", screen->aspect);

   if (screen->has_cmap) {
      print_gif_palette(file, screen->cmap);
   }
}
#endif

/*
 *  GifExtension:
 */

GifExtension *new_gif_extension(void)
{
   return (GifExtension *) gif_alloc(sizeof(GifExtension));
}

void del_gif_extension(GifExtension *ext)
{
   int i;

   for (i=0; i < ext->data_count; i++)
      del_gif_data(ext->data[i]);
   app_free(ext->data);
   app_free(ext);
}

void read_gif_extension(FILE *file, GifExtension *ext)
{
   GifData *data;
   int i;

   ext->marker = read_byte(file);

   data = read_gif_data(file);
   while (data) {
      /* Append the data object: */
      i = ++ext->data_count;
      ext->data = (GifData **) app_realloc(ext->data, i * sizeof(GifData *));
      ext->data[i-1] = data;
      data = read_gif_data(file);
   }
}

void write_gif_extension(FILE *file, GifExtension *ext)
{
   int i;

   write_byte(file, ext->marker);

   for (i=0; i < ext->data_count; i++)
      write_gif_data(file, ext->data[i]);
   write_gif_data(file, NULL);
}

#ifdef GIF_DEBUG
void print_gif_extension(FILE *file, GifExtension *ext)
{
   int i;

   fprintf(file, " GifExtension:\n");
   fprintf(file, "  marker = 0x%02X\n", ext->marker);
   for (i=0; i < ext->data_count; i++) {
      fprintf(file, "  data = ");
      print_gif_data(file, ext->data[i]);
   }
}
#endif

/*
 *  GifDecoder:
 */

GifDecoder * new_gif_decoder(void)
{
   return (GifDecoder *) gif_alloc(sizeof(GifDecoder));
}

void del_gif_decoder(GifDecoder *decoder)
{
   app_free(decoder);
}

void init_gif_decoder(FILE *file, GifDecoder *decoder)
{
   int i, depth;
   int lzw_min;
   unsigned int *prefix;

   lzw_min = read_byte(file);
   depth = lzw_min;

   decoder->file_state   = IMAGE_LOADING;
   decoder->position     = 0;
   decoder->bufsize      = 0;
   decoder->buf[0]       = 0;
   decoder->depth        = depth;
   decoder->clear_code   = (1 << depth);
   decoder->eof_code     = decoder->clear_code + 1;
   decoder->running_code = decoder->eof_code + 1;
   decoder->running_bits = depth + 1;
   decoder->max_code_plus_one = 1 << decoder->running_bits;
   decoder->stack_ptr    = 0;
   decoder->prev_code    = NO_SUCH_CODE;
   decoder->shift_state  = 0;
   decoder->shift_data   = 0;

   prefix = decoder->prefix;
   for (i = 0; i <= LZ_MAX_CODE; i++)
      prefix[i] = NO_SUCH_CODE;
}

/*
 *  Read the next Gif code word from the file.
 *
 *  This function looks in the decoder to find out how many
 *  bits to read, and uses a buffer in the decoder to remember
 *  bits from the last byte input.
 */
int read_gif_code(FILE *file, GifDecoder *decoder)
{
   int code;
   unsigned char next_byte;
   static int code_masks[] = {
      0x0000, 0x0001, 0x0003, 0x0007,
      0x000f, 0x001f, 0x003f, 0x007f,
      0x00ff, 0x01ff, 0x03ff, 0x07ff,
      0x0fff
   };

   while (decoder->shift_state < decoder->running_bits)
   {
      /* Need more bytes from input file for next code: */
      next_byte = read_gif_byte(file, decoder);
      decoder->shift_data |=
        ((unsigned long) next_byte) << decoder->shift_state;
      decoder->shift_state += 8;
   }

   code = decoder->shift_data & code_masks[decoder->running_bits];

   decoder->shift_data >>= decoder->running_bits;
   decoder->shift_state -= decoder->running_bits;

   /* If code cannot fit into running_bits bits,
    * we must raise its size.
    * Note: codes above 4095 are used for signalling. */
   if (++decoder->running_code > decoder->max_code_plus_one
      && decoder->running_bits < LZ_BITS)
   {
      decoder->max_code_plus_one <<= 1;
      decoder->running_bits++;
   }
   return code;
}

/*
 *  Routine to trace the prefix-linked-list until we get
 *  a prefix which is a pixel value (less than clear_code).
 *  Returns that pixel value.
 *
 *  If the picture is defective, we might loop here forever,
 *  so we limit the loops to the maximum possible if the
 *  picture is okay, i.e. LZ_MAX_CODE times.
 */
static int trace_prefix(unsigned int *prefix, int code, int clear_code)
{
   int i = 0;

   while (code > clear_code && i++ <= LZ_MAX_CODE)
      code = prefix[code];
   return code;
}

/*
 *  The LZ decompression routine:
 *  Call this function once per scanline to fill in a picture.
 */
void read_gif_line(FILE *file, GifDecoder *decoder,
         unsigned char *line, int length)
{
    int i = 0, j;
    int current_code, eof_code, clear_code;
    int current_prefix, prev_code, stack_ptr;
    unsigned char *stack, *suffix;
    unsigned int *prefix;

    prefix   = decoder->prefix;
    suffix   = decoder->suffix;
    stack   = decoder->stack;
    stack_ptr   = decoder->stack_ptr;
    eof_code   = decoder->eof_code;
    clear_code   = decoder->clear_code;
    prev_code   = decoder->prev_code;

    if (stack_ptr != 0) {
   /* Pop the stack */
   while (stack_ptr != 0 && i < length)
      line[i++] = stack[--stack_ptr];
    }

    while (i < length)
    {
   current_code = read_gif_code(file, decoder);

   if (current_code == eof_code)
   {
      /* unexpected EOF */
      if (i != length - 1 || decoder->pixel_count != 0)
      return;
      i++;
   }
   else if (current_code == clear_code)
   {
       /* reset prefix table etc */
       for (j = 0; j <= LZ_MAX_CODE; j++)
      prefix[j] = NO_SUCH_CODE;
       decoder->running_code = decoder->eof_code + 1;
       decoder->running_bits = decoder->depth + 1;
       decoder->max_code_plus_one = 1 << decoder->running_bits;
       prev_code = decoder->prev_code = NO_SUCH_CODE;
   }
   else {
       /* Regular code - if in pixel range
        * simply add it to output pixel stream,
        * otherwise trace code-linked-list until
        * the prefix is in pixel range. */
       if (current_code < clear_code) {
      /* Simple case. */
      line[i++] = (unsigned char) current_code;
       }
       else {
      /* This code needs to be traced:
       * trace the linked list until the prefix is a
       * pixel, while pushing the suffix pixels on
       * to the stack. If finished, pop the stack
       * to output the pixel values. */
      if ((current_code < 0) || (current_code > LZ_MAX_CODE))
         return; /* image defect */
      if (prefix[current_code] == NO_SUCH_CODE) {
          /* Only allowed if current_code is exactly
           * the running code:
           * In that case current_code = XXXCode,
           * current_code or the prefix code is the
           * last code and the suffix char is
           * exactly the prefix of last code! */
          if (current_code == decoder->running_code - 2) {
         current_prefix = prev_code;
         suffix[decoder->running_code - 2]
             = stack[stack_ptr++]
             = (unsigned char) trace_prefix(prefix, prev_code, clear_code);
          }
          else {
         return; /* image defect */
          }
      }
      else
          current_prefix = current_code;

      /* Now (if picture is okay) we should get
       * no NO_SUCH_CODE during the trace.
       * As we might loop forever (if picture defect)
       * we count the number of loops we trace and
       * stop if we get LZ_MAX_CODE.
       * Obviously we cannot loop more than that. */
      j = 0;
      while (j++ <= LZ_MAX_CODE
         && current_prefix > clear_code
         && current_prefix <= LZ_MAX_CODE)
      {
          stack[stack_ptr++] = suffix[current_prefix];
          current_prefix = prefix[current_prefix];
      }
      if (j >= LZ_MAX_CODE || current_prefix > LZ_MAX_CODE)
          return; /* image defect */

      /* Push the last character on stack: */
      stack[stack_ptr++] = (unsigned char) current_prefix;

      /* Now pop the entire stack into output: */
      while (stack_ptr != 0 && i < length)
          line[i++] = stack[--stack_ptr];
       }
       if (prev_code != NO_SUCH_CODE) {
      if ((decoder->running_code < 2) ||
        (decoder->running_code > LZ_MAX_CODE+2))
         return; /* image defect */
      prefix[decoder->running_code - 2] = prev_code;

      if (current_code == decoder->running_code - 2) {
          /* Only allowed if current_code is exactly
           * the running code:
           * In that case current_code = XXXCode,
           * current_code or the prefix code is the
           * last code and the suffix char is
           * exactly the prefix of the last code! */
          suffix[decoder->running_code - 2]
         = (unsigned char) trace_prefix(prefix, prev_code, clear_code);
      }
      else {
          suffix[decoder->running_code - 2]
         = (unsigned char) trace_prefix(prefix, current_code, clear_code);
      }
       }
       prev_code = current_code;
   }
    }

    decoder->prev_code = prev_code;
    decoder->stack_ptr = stack_ptr;
}

/*
 *  Hash table:
 */

/*
 *  The 32 bits contain two parts: the key & code:
 *  The code is 12 bits since the algorithm is limited to 12 bits
 *  The key is a 12 bit prefix code + 8 bit new char = 20 bits.
 */
#define HT_GET_KEY(x)   ((x) >> 12)
#define HT_GET_CODE(x)   ((x) & 0x0FFF)
#define HT_PUT_KEY(x)   ((x) << 12)
#define HT_PUT_CODE(x)   ((x) & 0x0FFF)

/*
 *  Generate a hash key from the given unique key.
 *  The given key is assumed to be 20 bits as follows:
 *    lower 8 bits are the new postfix character,
 *    the upper 12 bits are the prefix code.
 */
static int gif_hash_key(unsigned long key)
{
   return ((key >> 12) ^ key) & HT_KEY_MASK;
}

/*
 *  Clear the hash_table to an empty state.
 */
static void clear_gif_hash_table(unsigned long *hash_table)
{
   int i;
   for (i=0; i<HT_SIZE; i++)
      hash_table[i] = 0xFFFFFFFFL;
}

/*
 *  Insert a new item into the hash_table.
 *  The data is assumed to be new.
 */
static void add_gif_hash_entry(unsigned long *hash_table, unsigned long key, int code)
{
   int hkey = gif_hash_key(key);

   while (HT_GET_KEY(hash_table[hkey]) != 0xFFFFFL) {
      hkey = (hkey + 1) & HT_KEY_MASK;
   }
   hash_table[hkey] = HT_PUT_KEY(key) | HT_PUT_CODE(code);
}

/*
 *  Determine if given key exists in hash_table and if so
 *  returns its code, otherwise returns -1.
 */
static int lookup_gif_hash(unsigned long *hash_table, unsigned long key)
{
   int hkey = gif_hash_key(key);
   unsigned long htkey;

   while ((htkey = HT_GET_KEY(hash_table[hkey])) != 0xFFFFFL) {
      if (key == htkey)
         return HT_GET_CODE(hash_table[hkey]);
      hkey = (hkey + 1) & HT_KEY_MASK;
   }
   return -1;
}

/*
 *  GifEncoder:
 */

GifEncoder *new_gif_encoder(void)
{
   return (GifEncoder *) gif_alloc(sizeof(GifEncoder));
}

void del_gif_encoder(GifEncoder *encoder)
{
   app_free(encoder);
}

/*
 *  Write a Gif code word to the output file.
 *
 *  This function packages code words up into whole bytes
 *  before writing them. It uses the encoder to store
 *  codes until enough can be packaged into a whole byte.
 */
void write_gif_code(FILE *file, GifEncoder *encoder, int code)
{
   if (code == FLUSH_OUTPUT) {
      /* write all remaining data */
      while (encoder->shift_state > 0)
      {
         write_gif_byte(file, encoder,
            encoder->shift_data & 0xff);
         encoder->shift_data >>= 8;
         encoder->shift_state -= 8;
      }
      encoder->shift_state = 0;
      write_gif_byte(file, encoder, FLUSH_OUTPUT);
   }
   else {
      encoder->shift_data |=
         ((long) code) << encoder->shift_state;
      encoder->shift_state += encoder->running_bits;

      while (encoder->shift_state >= 8)
      {
         /* write full bytes */
         write_gif_byte(file, encoder,
            encoder->shift_data & 0xff);
         encoder->shift_data >>= 8;
         encoder->shift_state -= 8;
      }
   }

   /* If code can't fit into running_bits bits, raise its size.
    * Note that codes above 4095 are for signalling. */
   if (encoder->running_code >= encoder->max_code_plus_one
      && code <= 4095)
   {
          encoder->max_code_plus_one = 1 << ++encoder->running_bits;
   }
}

/*
 *   Initialise the encoder, given a GifPalette depth.
 */
void init_gif_encoder(FILE *file, GifEncoder *encoder, int depth)
{
   int lzw_min = depth = (depth < 2 ? 2 : depth);

   encoder->file_state   = IMAGE_SAVING;
   encoder->position     = 0;
   encoder->bufsize      = 0;
   encoder->buf[0]       = 0;
   encoder->depth        = depth;
   encoder->clear_code   = (1 << depth);
   encoder->eof_code     = encoder->clear_code + 1;
   encoder->running_code = encoder->eof_code + 1;
   encoder->running_bits = depth + 1;
   encoder->max_code_plus_one = 1 << encoder->running_bits;
   encoder->current_code = FIRST_CODE;
   encoder->shift_state  = 0;
   encoder->shift_data   = 0;

   /* Write the LZW minimum code size: */
   write_byte(file, lzw_min);

   /* Clear hash table, output Clear code: */
   clear_gif_hash_table(encoder->hash_table);
   write_gif_code(file, encoder, encoder->clear_code);
}

/*
 *  Write one scanline of pixels out to the Gif file,
 *  compressing that line using LZW into a series of codes.
 */
void write_gif_line(FILE *file, GifEncoder *encoder, unsigned char *line, int length)
{
    int i = 0, current_code, new_code;
    unsigned long new_key;
    unsigned char pixval;
    unsigned long *hash_table;

    hash_table = encoder->hash_table;

    if (encoder->current_code == FIRST_CODE)
   current_code = line[i++];
    else
   current_code = encoder->current_code;

    while (i < length)
    {
   pixval = line[i++]; /* Fetch next pixel from stream */

   /* Form a new unique key to search hash table for the code
    * Combines current_code as prefix string with pixval as
    * postfix char */
   new_key = (((unsigned long) current_code) << 8) + pixval;
   if ((new_code = lookup_gif_hash(hash_table, new_key)) >= 0) {
       /* This key is already there, or the string is old,
        * so simply take new code as current_code */
       current_code = new_code;
   }
   else {
       /* Put it in hash table, output the prefix code,
        * and make current_code equal to pixval */
       write_gif_code(file, encoder, current_code);
       current_code = pixval;

       /* If the hash_table if full, send a clear first
        * then clear the hash table: */
       if (encoder->running_code >= LZ_MAX_CODE) {
      write_gif_code(file, encoder, encoder->clear_code);
      encoder->running_code = encoder->eof_code + 1;
      encoder->running_bits = encoder->depth + 1;
      encoder->max_code_plus_one = 1 << encoder->running_bits;
      clear_gif_hash_table(hash_table);
       }
       else {
      /* Put this unique key with its relative code in hash table */
      add_gif_hash_entry(hash_table, new_key, encoder->running_code++);
       }
   }
    }

    /* Preserve the current state of the compression algorithm: */
    encoder->current_code = current_code;
}

void flush_gif_encoder(FILE *file, GifEncoder *encoder)
{
   write_gif_code(file, encoder, encoder->current_code);
   write_gif_code(file, encoder, encoder->eof_code);
   write_gif_code(file, encoder, FLUSH_OUTPUT);
}

/*
 *  GifPicture:
 */

GifPicture * new_gif_picture(void)
{
   GifPicture *pic = (GifPicture *) gif_alloc(sizeof(GifPicture));
   if (pic) {
      pic->cmap = new_gif_palette();
      pic->data = NULL;
   }
   return pic;
}

void del_gif_picture(GifPicture *pic)
{
   int row;

   del_gif_palette(pic->cmap);
   if (pic->data) {
      for (row=0; row < pic->height; row++)
         app_free(pic->data[row]);
      app_free(pic->data);
   }
   app_free(pic);
}

static void read_gif_picture_data(FILE *file, GifPicture *pic)
{
   GifDecoder *decoder;
   long w, h;
   int interlace_start[] = {0, 4, 2, 1};
   int interlace_step[]  = {8, 8, 4, 2};
   int scan_pass, row;

   w = pic->width;
   h = pic->height;
   pic->data = (unsigned char **) app_alloc(h * sizeof(unsigned char *));
   if (pic->data == NULL)
      return;
   for (row=0; row < h; row++)
      pic->data[row] = (unsigned char *) app_zero_alloc(w * sizeof(unsigned char));

   decoder = new_gif_decoder();
   init_gif_decoder(file, decoder);

   if (pic->interlace) {
     for (scan_pass = 0; scan_pass < 4; scan_pass++) {
       row = interlace_start[scan_pass];
       while (row < h) {
         read_gif_line(file, decoder, pic->data[row], w);
         row += interlace_step[scan_pass];
       }
     }
   }
   else {
     row = 0;
     while (row < h) {
       read_gif_line(file, decoder, pic->data[row], w);
       row += 1;
     }
   }
   finish_gif_picture(file, decoder);

   del_gif_decoder(decoder);
}

void read_gif_picture(FILE *file, GifPicture *pic)
{
   unsigned char info;

   pic->left   = read_gif_int(file);
   pic->top    = read_gif_int(file);
   pic->width  = read_gif_int(file);
   pic->height = read_gif_int(file);

   info = read_byte(file);
   pic->has_cmap    = (info & 0x80) >> 7;
   pic->interlace   = (info & 0x40) >> 6;
   pic->sorted      = (info & 0x20) >> 5;
   pic->reserved    = (info & 0x18) >> 3;

   if (pic->has_cmap) {
      pic->cmap_depth  = (info & 0x07) + 1;
      pic->cmap->length = 1 << pic->cmap_depth;
      read_gif_palette(file, pic->cmap);
   }

   read_gif_picture_data(file, pic);
}

static void write_gif_picture_data(FILE *file, GifPicture *pic)
{
   GifEncoder *encoder;
   long w, h;
   int interlace_start[] = {0, 4, 2, 1};
   int interlace_step[]  = {8, 8, 4, 2};
   int scan_pass, row;

   w = pic->width;
   h = pic->height;

   encoder = new_gif_encoder();
   init_gif_encoder(file, encoder, pic->cmap_depth);

   if (pic->interlace) {
     for (scan_pass = 0; scan_pass < 4; scan_pass++) {
       row = interlace_start[scan_pass];
       while (row < h) {
         write_gif_line(file, encoder, pic->data[row], w);
         row += interlace_step[scan_pass];
       }
     }
   }
   else {
     row = 0;
     while (row < h) {
       write_gif_line(file, encoder, pic->data[row], w);
       row += 1;
     }
   }

   flush_gif_encoder(file, encoder);
   del_gif_encoder(encoder);
}

void write_gif_picture(FILE *file, GifPicture *pic)
{
   unsigned char info;

   write_gif_int(file, pic->left);
   write_gif_int(file, pic->top);
   write_gif_int(file, pic->width);
   write_gif_int(file, pic->height);

   info = 0;
   info = info | (pic->has_cmap    ? 0x80 : 0x00);
   info = info | (pic->interlace   ? 0x40 : 0x00);
   info = info | (pic->sorted      ? 0x20 : 0x00);
   info = info | ((pic->reserved << 3) & 0x18);
   if (pic->has_cmap)
      info = info | (unsigned char) (pic->cmap_depth - 1);

   write_byte(file, info);

   if (pic->has_cmap)
      write_gif_palette(file, pic->cmap);

   write_gif_picture_data(file, pic);
}

#ifdef GIF_DEBUG
static void print_gif_picture_data(FILE *file, GifPicture *pic)
{
   int pixval, row, col;

   for (row = 0; row < pic->height; row++) {
     fprintf(file, "   [");
     for (col = 0; col < pic->width; col++) {
       pixval = pic->data[row][col];
       fprintf(file, "%02X", pixval);
     }
     fprintf(file, "]\n");
   }
}

static void print_gif_picture_header(FILE *file, GifPicture *pic)
{
   fprintf(file, " GifPicture:\n");
   fprintf(file, "  left       = %d\n", pic->left);
   fprintf(file, "  top        = %d\n", pic->top);
   fprintf(file, "  width      = %d\n", pic->width);
   fprintf(file, "  height     = %d\n", pic->height);

   fprintf(file, "  has_cmap   = %d\n", pic->has_cmap);
   fprintf(file, "  interlace  = %d\n", pic->interlace);
   fprintf(file, "  sorted     = %d\n", pic->sorted);
   fprintf(file, "  reserved   = %d\n", pic->reserved);
   fprintf(file, "  cmap_depth = %d\n", pic->cmap_depth);
}

void print_gif_picture(FILE *file, GifPicture *pic)
{
   print_gif_picture_header(file, pic);

   if (pic->has_cmap)
      print_gif_palette(file, pic->cmap);

   print_gif_picture_data(file, pic);
}
#endif

/*
 *  GifBlock:
 */

GifBlock *new_gif_block(void)
{
   return (GifBlock *) gif_alloc(sizeof(GifBlock));
}

void del_gif_block(GifBlock *block)
{
   if (block->pic)
      del_gif_picture(block->pic);
   if (block->ext)
      del_gif_extension(block->ext);
   app_free(block);
}

void read_gif_block(FILE *file, GifBlock *block)
{
   block->intro = read_byte(file);
   if (block->intro == 0x2C) {
      block->pic = new_gif_picture();
      read_gif_picture(file, block->pic);
   }
   else if (block->intro == 0x21) {
      block->ext = new_gif_extension();
      read_gif_extension(file, block->ext);
   }
}

void write_gif_block(FILE *file, GifBlock *block)
{
   write_byte(file, block->intro);
   if (block->pic)
      write_gif_picture(file, block->pic);
   if (block->ext)
      write_gif_extension(file, block->ext);
}

#ifdef GIF_DEBUG
void print_gif_block(FILE *file, GifBlock *block)
{
   fprintf(file, " GifBlock (intro=0x%02X):\n", block->intro);
   if (block->pic)
      print_gif_picture(file, block->pic);
   if (block->ext)
      print_gif_extension(file, block->ext);
}
#endif

/*
 *  Gif:
 */

Gif * new_gif(void)
{
   Gif *gif = (Gif *) gif_alloc(sizeof(Gif));
   if (gif) {
      strcpy(gif->header, "GIF87a");
      gif->screen = new_gif_screen();
      gif->blocks = NULL;
   }
   return gif;
}

void del_gif(Gif *gif)
{
   int i;

   del_gif_screen(gif->screen);
   for (i=0; i < gif->block_count; i++)
      del_gif_block(gif->blocks[i]);
   app_free(gif);
}

void read_gif(FILE *file, Gif *gif)
{
   int i;
   GifBlock *block;

   for (i=0; i<6; i++)
      gif->header[i] = read_byte(file);
   if (strncmp(gif->header, "GIF", 3) != 0)
      return; /* error */

   read_gif_screen(file, gif->screen);

   for (;;) {
      block = new_gif_block();
      read_gif_block(file, block);

      if (block->intro == 0x3B) {   /* terminator */
         del_gif_block(block);
         break;
      }
      else  if (block->intro == 0x2C) {   /* image */
         /* Append the block: */
         i = ++gif->block_count;
         gif->blocks = (GifBlock **) app_realloc(gif->blocks, i * sizeof(GifBlock *));
         gif->blocks[i-1] = block;
      }
      else  if (block->intro == 0x21) {   /* extension */
         /* Append the block: */
         i = ++gif->block_count;
         gif->blocks = (GifBlock **) app_realloc(gif->blocks, i * sizeof(GifBlock *));
         gif->blocks[i-1] = block;
      }
      else {   /* error */
         del_gif_block(block);
         break;
      }
   }
}

void read_one_gif_picture(FILE *file, Gif *gif)
{
   int i;
   GifBlock *block;

   for (i=0; i<6; i++)
      gif->header[i] = read_byte(file);
   if (strncmp(gif->header, "GIF", 3) != 0)
      return; /* error */

   read_gif_screen(file, gif->screen);

   for (;;) {
      block = new_gif_block();
      read_gif_block(file, block);

      if (block->intro == 0x3B) {   /* terminator */
         del_gif_block(block);
         break;
      }
      else if (block->intro == 0x2C) { /* image */
         /* Append the block: */
         i = ++gif->block_count;
         gif->blocks = (GifBlock **) app_realloc(gif->blocks, i * sizeof(GifBlock *));
         gif->blocks[i-1] = block;
         break;
      }
      else if (block->intro == 0x21) { /* extension */
         /* Append the block: */
         i = ++gif->block_count;
         gif->blocks = (GifBlock **) app_realloc(gif->blocks, i * sizeof(GifBlock *));
         gif->blocks[i-1] = block;
         continue;
      }
      else {   /* error! */
         del_gif_block(block);
         break;
      }
   }
}

void write_gif(FILE *file, Gif *gif)
{
   int i;

   fprintf(file, "%s", gif->header);
   write_gif_screen(file, gif->screen);
   for (i=0; i < gif->block_count; i++)
      write_gif_block(file, gif->blocks[i]);
   write_byte(file, 0x3B);
}

#ifdef GIF_DEBUG
void print_gif(FILE *file, Gif *gif)
{
   int i;

   fprintf(file, "Gif header=%s\n", gif->header);
   print_gif_screen(file, gif->screen);
   for (i=0; i < gif->block_count; i++)
      print_gif_block(file, gif->blocks[i]);
   fprintf(file, "End of gif.\n\n");
}
#endif

/*
 *  Reading and Writing Gif files:
 */

Gif * read_gif_file(char *filename)
{
   Gif *gif;
   FILE *file;

   file = app_open_file(filename, "rb");
   if (file == NULL)
      return NULL;
   gif = new_gif();
   if (gif == NULL) {
      app_close_file(file);
      return NULL;
   }
   read_gif(file, gif);
   app_close_file(file);
   if (strncmp(gif->header, "GIF", 3) != 0) {
      del_gif(gif);
      gif = NULL;
   }
   return gif;
}

int write_gif_file(char *filename, Gif *gif)
{
   FILE *file;

   file = app_open_file(filename, "wb");
   if (file == NULL)
      return FALSE;
   if (gif == NULL) {
      app_close_file(file);
      return FALSE;
   }
   write_gif(file, gif);

   int result = ferror(file);

   app_close_file(file);

   return !result;
}

//****************************************************************************
//
// Write contents of VFX pane to .GIF file, returning 1 if OK or 0 on error 
//
//****************************************************************************

bool  GIF_write_16bpp  (PANE *src, //)
                        C8   *filename)
{
   //
   // Get pane dimensions
   //

   S32 iw = 0;
   S32 ih = 0;
   PANE_DIMS(src, iw, ih)

   S32 x,y;

   //
   // Create color quantizer object and pass pixel data to it
   //

   VFX_RGB palette[256];

   CQ quantizer;

   for (y=0; y < ih; y++)
      {
      for (x=0; x < iw; x++)
         {
         VFX_RGB *RGB = VFX_RGB_value(VFX_pixel_read(src, x, y));

         quantizer.add_color(RGB);
         }
      }

   quantizer.quantize(palette,  
                      256);

   //
   // Remap result to 8bpp surface
   //
   // Create row index table needed by heinous .GIF encoder
   //

   U8  *out       = (U8 *)  malloc(iw * ih);
   U8 **out_index = (U8 **) malloc(ih * sizeof (U8 *));

   if ((out == NULL) || (out_index == NULL))
      {
      return FALSE;
      }

   CMAP remap(palette, 256);

   for (y=0; y < ih; y++)
      {
      for (x=0; x < iw; x++)
         {
         VFX_RGB *RGB = VFX_RGB_value(VFX_pixel_read(src, x, y));

         out[(y*iw)+x] = remap.nearest_neighbor(RGB,
                                                0);
         }

      out_index[y] = &out[y*iw];
      }

   //
   // Write 8bpp surface to .GIF file
   //
   // Any colors greater than F8F8F8 are mapped to solid white to avoid
   // saving file with an artificial grayscale background
   //

   Gif *gif = new_gif();
 
	gif->screen->width      = iw;
	gif->screen->height     = ih;
	gif->screen->has_cmap   = 1;
	gif->screen->color_res  = 8;
	gif->screen->cmap_depth = 8;
   
	GifPalette *cmap = gif->screen->cmap;

	cmap->length = 1 << 8;
	cmap->colours = (GifColor *) app_alloc(cmap->length * sizeof(GifColor));

	for (S32 i=0; i < 256; i++) 
      {
      if ((palette[i].r >= 0xf8) &&
          (palette[i].g >= 0xf8) && 
          (palette[i].b >= 0xf8))
         {
         palette[i].r = 0xff;
         palette[i].g = 0xff;
         palette[i].b = 0xff;
         }

		cmap->colours[i] = rgb(palette[i].r,
                             palette[i].g,
                             palette[i].b);
      }

	GifPicture *pic = (GifPicture *) new_gif_picture();
	pic->width      = iw;
	pic->height     = ih;
   pic->interlace  = 0; 
	pic->has_cmap   = 0;
	pic->cmap_depth = 8;

	pic->data = out_index;

	GifBlock *block = (GifBlock *) new_gif_block();
	block->intro = 0x2C;
	block->pic   = pic;

	int size = ++gif->block_count;
	gif->blocks = (GifBlock **) app_realloc(gif->blocks, size * sizeof(GifBlock *));
	gif->blocks[size-1] = block;

	write_gif_file(filename, gif);

   //
   // Clean up and return success
   //

	pic->data = NULL;
   del_gif(gif);

   free(out);
   free(out_index);

   return TRUE;
}

//****************************************************************************
//
// Write contents of VFX pane to .BMP file, returning 1 if OK or 0 on error 
//
//****************************************************************************

bool  BMP_write_16bpp  (PANE *src, //)
                        C8   *filename)
{
   //
   // Get pane dimensions
   //

   S32 width = 0;
   S32 height = 0;
   PANE_DIMS(src, width, height)

   FILE *out = fopen(filename, "wb");

   if (out == NULL)
      {
      return FALSE;
      }

   S32        row_size    = (((width * 3) + 3) & ~3);
   S32        bitmap_size = row_size * height;
   BITMAPINFO bmi;

   bmi.bmiHeader.biSize          = sizeof (BITMAPINFOHEADER);
   bmi.bmiHeader.biWidth         = width;
   bmi.bmiHeader.biHeight        = height;
   bmi.bmiHeader.biPlanes        = 1;
   bmi.bmiHeader.biBitCount      = 24;
   bmi.bmiHeader.biCompression   = BI_RGB;
   bmi.bmiHeader.biSizeImage     = bitmap_size;
   bmi.bmiHeader.biXPelsPerMeter = 2952;
   bmi.bmiHeader.biYPelsPerMeter = 2952;
   bmi.bmiHeader.biClrUsed       = 0;
   bmi.bmiHeader.biClrImportant  = 0;

   BITMAPFILEHEADER bmfh;
   bmfh.bfType       = 'MB';
   bmfh.bfSize       = sizeof (bmfh) + sizeof (bmi) + bitmap_size;
   bmfh.bfReserved1  = 0;
   bmfh.bfReserved2  = 0;
   bmfh.bfOffBits    = sizeof (bmfh) + sizeof (bmi);

   U8 *lp24Buffer = (U8 *) malloc(bitmap_size);

   U8 *dest = lp24Buffer;

   S32 remnant = row_size - (3 * width);

   for (S32 y=height-1; y >= 0; y--)
      {
      for (S32 x=0; x < width; x++)
         {
         VFX_RGB *RGB = VFX_RGB_value(VFX_pixel_read(src, x, y));

         *dest++ = (U8) RGB->b;
         *dest++ = (U8) RGB->g;
         *dest++ = (U8) RGB->r;
         }

      for (S32 i=0; i < remnant; i++)
         {
         *dest++ = 0;
         }
      }

   fwrite (&bmfh,      1, sizeof (BITMAPFILEHEADER), out);
   fwrite (&bmi,       1, sizeof (BITMAPINFO),       out);
   fwrite (lp24Buffer, 1, bitmap_size,               out);

   free(lp24Buffer);

   if (ferror(out))
      {
      fclose(out);
      return FALSE;
      }

   fclose(out);

   return TRUE;
}

typedef struct
   {
   unsigned char manufacturer;   // 10
   unsigned char version;        // 5
   unsigned char encoding;       // 1
   unsigned char bitsperpixel;   // 8
   short         x0;
   short         y0;
   short         x1;
   short         y1;             // size inclusive
   short         wide;
   short         tall;
   unsigned char colormap[48];
   unsigned char reserved;       // 0
   unsigned char numcolorplanes; // 1
   short         bytesperline;   // always even (but see note below...)
   short         paletteinfo;    // 1
   short         screenWide;
   short         screenTall;
   unsigned char filler[54];     // 0
}
PCXHEADER;

/***************************************************************************/
//
// PCX code by Billy Zelsnack
//
/***************************************************************************/

static void encodePcxLine(FILE **fp, unsigned char *buf, int wide)
{
   static unsigned char blank[16384];
   int numbytes, count, ctr, color;

   if ((*fp) == NULL)
      {
      return;
      }

   numbytes = 0;
   ctr = 0;
   
   while (ctr < wide)
      {
      count = 1;
      color = buf[ctr++];

      while ((ctr < wide) && (count < 63))
         {
         if (buf[ctr] != color)
            {
            break;
            }

         ctr++;
         count++;
         }

      if ((count > 1) || ((color & 0xc0) == 0xc0))
         {
         blank[numbytes++] = (unsigned char) (192 + count);
      
         if (numbytes >= 16384)
            {
            fclose(*fp);
            (*fp) = NULL;
            return;
            }
         }

      blank[numbytes++] = (unsigned char) color;
   
      if (numbytes >= 16384)
         {
         fclose(*fp);
         (*fp) = NULL;
         return;
         }
      }

   if (fwrite(blank,numbytes,1,*fp) != 1)
      {
      fclose(*fp);
      (*fp) = NULL;
      }
}

/***************************************************************************/
unsigned char *PCX_load (char *filename, //)
                         int  *wide,
                         int  *tall)
{
   static unsigned char line[16384];
   unsigned char       *ptr,color;
   PCXHEADER            header;
   FILE                *fp;
   int                  bytesPerLine,i,j,ctr,count;

   fp = fopen(filename,"rb");

   if (fp==NULL)
      {
      return 0;
      }

   fseek(fp,0,SEEK_SET);

   fread(&header, sizeof(PCXHEADER), 1, fp);

   if (header.manufacturer != 10)
      {
      fclose(fp);
      return 0;
      }

   if (header.version != 5)
      {
      fclose(fp);
      return 0;
      }

   if (header.encoding != 1)
      {
      fclose(fp);
      return 0;
      }
   
   if (header.bitsperpixel != 8)
      {
      fclose(fp);
      return 0;
      }

   (*wide) = (header.x1-header.x0)+1;
   (*tall) = (header.y1-header.y0)+1;

   bytesPerLine = header.bytesperline * header.numcolorplanes;

   if (bytesPerLine < 0)
      {
      fclose(fp);
      return 0;
      }

   if (bytesPerLine > 16384)
      {
      fclose(fp);
      return 0;
      }

   ptr = (unsigned char *) malloc((*wide) * (*tall));

   for (j=0; j < (*tall); j++)
      {
      ctr = 0;

      while (ctr < bytesPerLine)
         {
         fread(&color,1,1,fp);

         if (feof(fp))
            {
            break;
            }

         count = 1;

         if ((color & 192) == 192)
            {
            count = color & 63;
   
            fread(&color,1,1,fp);

            if (feof(fp))
               {
               break;
               }
            }

         for (i=0; i < count; i++)
            {
            line[ctr++] = color;
            }
         }

      memcpy(ptr + (*wide) * j,
             line,
            (*wide));
      }

   fclose(fp);
   return ptr;
}

/***************************************************************************/
unsigned char *PCX_load_palette (char *filename)
{
   unsigned char *ptr;
   FILE          *fp;
   PCXHEADER      header;

   fp = fopen(filename,"rb");

   if (fp == NULL)
      {
      return NULL;
      }

   fseek(fp,0,SEEK_SET);

   fread(&header,sizeof(PCXHEADER),1,fp);

   if (header.manufacturer != 10)
      {
      fclose(fp);
      return NULL;
      }

   if (header.version != 5)
      {
      fclose(fp);
      return NULL;
      }

   if (header.encoding != 1)
      {
      fclose(fp);
      return NULL;
      }
   
   if (header.bitsperpixel != 8)
      {
      fclose(fp);
      return NULL;
      }

   fseek(fp, -768, SEEK_END);

   ptr = (unsigned char*) malloc(768);

   fread(ptr, 768, 1, fp);

   fclose(fp);
   return ptr;
}

//****************************************************************************
//
// Write contents of VFX pane to .PCX file, returning 1 if OK or 0 on error 
//
//****************************************************************************

bool  PCX_write_16bpp  (PANE *src, //)
                        C8   *filename)
{
   //
   // Get pane dimensions
   //

   S32 wide = 0;
   S32 tall = 0;
   PANE_DIMS(src, wide, tall)

   S32 x,y;

   //
   // Create color quantizer object and pass pixel data to it
   //

   VFX_RGB palette[256];

   CQ quantizer;

   for (y=0; y < tall; y++)
      {
      for (x=0; x < wide; x++)
         {
         VFX_RGB *RGB = VFX_RGB_value(VFX_pixel_read(src, x, y));

         quantizer.add_color(RGB);
         }
      }

   quantizer.quantize(palette,  
                      256);

   //
   // Remap result to 8bpp surface
   //

   U8 *data = (U8 *) malloc(wide * tall);

   if (data == NULL)
      {
      return FALSE;
      }

   CMAP remap(palette, 256);

   for (y=0; y < tall; y++)
      {
      for (x=0; x < wide; x++)
         {
         VFX_RGB *RGB = VFX_RGB_value(VFX_pixel_read(src, x, y));

         data[(y*wide)+x] = remap.nearest_neighbor(RGB,
                                                   0);
         }
      }

   //
   // Save to .PCX 
   //

   S32 x0,y0,x1,y1,j;
   unsigned char id;
   PCXHEADER pcxheader;
   FILE *fp;

   x0 = 0;
   y0 = 0;
   x1 = wide - 1;
   y1 = tall - 1;

   fp = fopen(filename,"wb");

   if (fp == NULL)
      {
      free(data);
      return 0;
      }

   fseek(fp,0L,SEEK_SET);

   pcxheader.manufacturer = 10;
   pcxheader.version      = 5;
   pcxheader.encoding     = 1;
   pcxheader.bitsperpixel = 8;
   pcxheader.x0           = (S16) x0;
   pcxheader.y0           = (S16) y0;
   pcxheader.x1           = (S16) x1;
   pcxheader.y1           = (S16) y1;
   pcxheader.wide         = 0;
   pcxheader.tall         = 0;

   memcpy(pcxheader.colormap,
          palette,
          48);

   pcxheader.reserved       = 0;
   pcxheader.numcolorplanes = 1;

   //
   // WARNING!
   //
   // PCX "standard" requires even-length lines, but if extra pad byte
   // is added to 'bytesperline', DPaint displays odd-width images
   // incorrectly
   //

   pcxheader.bytesperline   = (S16) ((x1-x0)+1);
   pcxheader.paletteinfo    = 1;
   pcxheader.screenWide     = 0;
   pcxheader.screenTall     = 0;

   memset(pcxheader.filler,
          0,
          54);

   if (fwrite(&pcxheader, sizeof(PCXHEADER), 1, fp) != 1)
      {
      fclose(fp);
      free(data);
      return 0;
      }

   for (j=0; j < tall; j++)
      {
      encodePcxLine(&fp, data + j * wide, wide);
      }

   id = 0x0c;

   if (fwrite(&id, 1, 1, fp) != 1)
      {
      fclose(fp);
      free(data);
      return 0;
      }

	for (S32 i=0; i < 256; i++) 
      {
      if ((palette[i].r >= 0xfc) &&
          (palette[i].g >= 0xfc) && 
          (palette[i].b >= 0xfc))
         {
         palette[i].r = 0xff;
         palette[i].g = 0xff;
         palette[i].b = 0xff;
         }
      }

   if (fwrite(palette,768,1,fp) != 1)
      {
      fclose(fp);
      free(data);
      return 0;
      }

   fclose(fp);
   free(data);
   return 1;
}


