//
// MX20 comms test
//

#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <float.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include "typedefs.h"
#include "timeutil.cpp"
#include "comport.cpp"

#define VERSION "1.00"

COMPORT serial;

void shutdown(void)
{
   if (serial.connected())
      {
//      serial.printf("SYST:REM OFF\n");  serial.gets(); serial.gets();
      }
}

void main(int argc, char **argv)
{
   setbuf(stdout, NULL);

   //
   // Parse args
   //

   fprintf(stderr,"\nMX20 version "VERSION" of "__DATE__" by John Miles, KE5FX\n"
                  "_________________________________________________________________________\n\n");

   if (argc < 3)
      { 
      fprintf(stderr,"MX-20 comms test\n");
      exit(1);
      }

   atexit(shutdown);

   C8 *port = argv[1];

   S32 rc = serial.connect(port, 9600);

   if (rc != 0)
      {
      fprintf(stderr,"Could not open %s", port);
      exit(1);
      }

   for (;;)
      {
      if (_kbhit()) { _getch(); break; }

      S32 cnt = 4;
      U8 rm[4] = { 0x21, 0x4d, 0x52, 0x0d };
      serial.write(rm, cnt, &cnt);
      Sleep(100);

      U8 t123s[7] = { 0x21, 0x54, 0x31, 0x32, 0x33, 0x30, 0x0d };

      cnt = 7;
      serial.write(t123s, cnt, &cnt);
      Sleep(100);
      }

   serial.disconnect();
}

