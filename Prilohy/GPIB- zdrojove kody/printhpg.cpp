//
// HP7470A plotter emulator
//
// File printer
//

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <io.h>
#include <fcntl.h>
#include <assert.h>
#include "typedefs.h"

void SAL_alert_box(C8 *caption, C8 *err)
{
   printf("%s: %s\n",caption,err);
}

#include "7470lex.cpp"

void main(int argc, char **argv)
{
   if (argc != 2)
      {
      printf("Usage: printhpg <plotter data file>\n");
      exit(1);
      }

   S32 plot_len;

   plot_data = (C8 *) FILE_read(argv[1],
                               &plot_len);

   if (plot_data == NULL)
      {
      printf("Error: couldn't load %s\n",argv[1]);
      exit(1);
      }

   C8 *text = (C8 *) malloc(plot_len + 1);
   assert(text);

   memcpy(text, plot_data, plot_len);
   text[plot_len] = 0;

   //
   // Send it to PRN device
   //

   FILE *prn = fopen("PRN","wb");

   assert(prn != NULL);

   fprintf(prn,"\033E");            // reset printer
   fprintf(prn,"\033&l1O");         // landscape mode
   fprintf(prn,"\033%%0B");         // enter HP-GL/2 mode
   fprintf(prn,"IN");               // initialize HP-GL/2 mode

   fprintf(prn,"%s;",text);

   fprintf(prn,"\033%%0A");         // enter PCL at previous CAP
   fprintf(prn,"\033E");            // reset to end job / eject page

   fclose(prn);
}
