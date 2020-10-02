#include <stdio.h>
#include <conio.h>
#include <assert.h>
#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "gpiblib.h"

S32 addr;

void WINAPI GPIB_error(C8 *msg, S32 ibsta, S32 iberr, S32 ibcntl)
{
   printf("%s",msg);

   exit(1);
}

void connect(void)
{
   GPIB_connect(addr,
                GPIB_error);
}

void disconnect(void)
{
   GPIB_disconnect();
}

void ds_cal(void)
{
   GPIB_CTYPE type = GPIB_connection_type();

   if (type != GC_NI488)
      {
      printf("Error: This test requires a National Instruments GPIB interface -- it will\n");
      printf("not work with Prologix adapters\n");
      return;
      }

   C8 C[1024];
   C[0] = 64;

   S32 k = 125;
   S32 i1 = 0;

   for (S32 i=1; i <= 10; i++)
      {
      for (S32 j=1; j <= 100; j++)
         {
         C[i1+j] = k;
         }

      k-=25;
      i1+=100;

      if (!(k >= 25))
         {
         k = 225;
         }
      }

   GPIB_write("SIGSWP");
   GPIB_write(C, 1001);

   printf("\nPress any key when test complete... ");
   _getch();
   printf("Done\n");
   GPIB_write("INIT");
}

void cf_cal(void)
{
   printf("\nConnect calibrator, then press any key when test complete... ");

   GPIB_write("INIT; REF -20; SPAN 2M; SIG");

   while (!_kbhit())
      {
      GPIB_write("FREQ 100M;DEG;SIG;WAIT;");
      GPIB_write("FREQ 1.8G;DEG;SIG;WAIT;");
      }

   _getch();
   printf("Done\n");
}

void lo1_sens_cal(void)
{
   GPIB_write("INIT; FREQ 10M; SPAN 100K");
   disconnect();

   printf("\nCenter marker on screen, then press any key to continue... ");
   _getch();

   connect();
   printf("\nPress any key when test complete... ");

   while (!_kbhit())
      {
      GPIB_write("TUNE 5M;SIG;WAIT;");
      GPIB_write("TUNE -5M;SIG;WAIT;");
      }

   _getch();
   printf("Done\n");
}

void lo2_range_cal(void)
{
   printf("\nPress any key when test complete... ");

   while (!_kbhit())
      {
      GPIB_write("TUNE 2M;SIG;WAIT;");
      GPIB_write("TUNE -2M;SIG;WAIT;");
      }

   _getch();
   printf("Done\n");
}

void lo2_sens_cal(void)
{
   printf("\nPress any key when test complete... ");

   while (!_kbhit())
      {
      GPIB_write("TUNE 2K;SIG;WAIT;");
      GPIB_write("TUNE -2K;SIG;WAIT;");
      }

   _getch();
   printf("Done\n");
}

void shutdown(void)
{
   GPIB_disconnect();
}

void main(S32 argc, C8 **argv)
{
   printf("\n");
   printf("This program assists with calibration of Tektronix 49xP/275xP-series\n");
   printf("spectrum analyzers, using routines from volume 1 of the Tektronix 492P\n");
   printf("service manual (070-3783-01, FEB 1984 edition).\n");

   if (argc < 2)
      {
      printf("\nUsage: cal_tek490 <address>\n");
      exit(1);
      }

   addr = atoi(argv[1]);

   atexit(shutdown);

   for (;;)
      {
      printf("\n    1) Digital storage calibration (page 3-70)\n"
             "    2) 1st LO sense gain/offset (LO driver R1031, CF control R1032, page 3-51)\n"
             "    3) 0 MHz\n"
             "    4) 4.2 GHz (step 2c, page 3-52)\n"
             "    5) 4.278 GHz\n"
             "    6) 5.5 GHz (step 2c, page 3-52)\n"
             "    7) 1st LO tune sensitivity (CF control R1028, page 3-55)\n"
             "    8) 2nd LO tune range (CF control R4040, page 3-55)\n"
             "    9) 2nd LO tune sensitivity (CF control R3040, page 3-56)\n"
             "  ESC) Exit\n\n"
             "Choice: ");

      C8 ch = _getch();

      if (ch == 27)
         {
         printf("Exit\n");
         exit(0);
         }

      printf("%c\n",ch);

      connect();

      switch (ch)
         {
         case '1': 
            ds_cal(); 
            break;

         case '2':
            cf_cal();
            break;

         case '3':
            GPIB_write("FREQ 0M");
            break;
            
         case '4':
            GPIB_write("FREQ 4200M");
            break;

         case '5':
            GPIB_write("FREQ 4278M");
            break;

         case '6':
            GPIB_write("FREQ 5500M");
            break;

         case '7':
            lo1_sens_cal();
            break;

         case '8':
            lo2_range_cal();
            break;

         case '9':
            lo2_sens_cal();
            break;
         }

      disconnect();
      }
}
