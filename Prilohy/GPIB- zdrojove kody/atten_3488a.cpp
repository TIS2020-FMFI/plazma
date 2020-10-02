#include <stdio.h>
#include <conio.h>
#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include "gpiblib.h"

S32 switches = 0;
S32 prev_switches = 0;

void WINAPI GPIB_error(C8 *msg, S32 ibsta, S32 iberr, S32 ibcntl)
{
   printf("%s",msg);
   exit(1);
}

void shutdown(void)
{
   GPIB_disconnect();
}

void update_switches(bool force = FALSE)
{
   for (S32 i=0x80; i > 0x00; i >>= 1)             // Update each switch state individually (slow but minimizes peak current draw and 104/105 contact life)
      {                                            
      S32 state = switches      & i;
      S32 prev  = prev_switches & i;               

      if ((state == prev) && (!force))
         {
         continue;
         }

      if (i == 0x01) GPIB_write("CLOSE 100");      // Switch state needs to be updated; connect it to the power bus
      if (i == 0x02) GPIB_write("CLOSE 101"); 
      if (i == 0x04) GPIB_write("CLOSE 102"); 
      if (i == 0x08) GPIB_write("CLOSE 103"); 
      if (i == 0x10) GPIB_write("CLOSE 106"); 
      if (i == 0x20) GPIB_write("CLOSE 107"); 
      if (i == 0x40) GPIB_write("CLOSE 108"); 
      if (i == 0x80) GPIB_write("CLOSE 109"); 

      if (state)                                   
         {
         GPIB_write("CLOSE 104");                  // Toggle switch 104 to apply pulse with engagement polarity
         Sleep(500);                               
         GPIB_write("OPEN 104");                   
         }
      else
         {
         GPIB_write("CLOSE 105");                  // Toggle switch 105 to apply pulse with disengagement polarity
         Sleep(500);
         GPIB_write("OPEN 105");
         }

      if (i == 0x01) GPIB_write("OPEN 100");       // Disengage the switch from the bus
      if (i == 0x02) GPIB_write("OPEN 101"); 
      if (i == 0x04) GPIB_write("OPEN 102"); 
      if (i == 0x08) GPIB_write("OPEN 103"); 
      if (i == 0x10) GPIB_write("OPEN 106"); 
      if (i == 0x20) GPIB_write("OPEN 107"); 
      if (i == 0x40) GPIB_write("OPEN 108"); 
      if (i == 0x80) GPIB_write("OPEN 109"); 
      }

   prev_switches = switches;
}

void set_atten_dB(DOUBLE atten_dB)
{
   printf("\rSetting attenuation = %.1lf dB\n", atten_dB);

   switches = 0;
   atten_dB += 0.5;

   if (atten_dB >= 40.0) { atten_dB -= 40.0; switches |= 0x80; }     
   if (atten_dB >= 40.0) { atten_dB -= 40.0; switches |= 0x40; }
   if (atten_dB >= 20.0) { atten_dB -= 20.0; switches |= 0x20; }
   if (atten_dB >= 10.0) { atten_dB -= 10.0; switches |= 0x10; }
   if (atten_dB >= 4.0)  { atten_dB -= 4.0;  switches |= 0x08; }
   if (atten_dB >= 4.0)  { atten_dB -= 4.0;  switches |= 0x04; }
   if (atten_dB >= 2.0)  { atten_dB -= 2.0;  switches |= 0x02; }
   if (atten_dB >= 1.0)  { atten_dB -= 1.0;  switches |= 0x01; }
}

void main(S32 argc, C8 **argv)
{
   setbuf(stdout,NULL);

   if (argc < 2)
      {
      printf("Usage: atten_3488a <address> [<atten_dB>]\n");
      printf("\n");
      printf("Assumes HP 44470A in slot 1 connected to HP 33004 and 33005 step attenuators\n");
      exit(1);
      }

   atexit(shutdown);

   GPIB_connect(atoi(argv[1]),
                GPIB_error,
                0,
                3000);

   GPIB_write("CRESET 1");
   GPIB_write("CMON 1");

   switches = 0;
   update_switches(TRUE);

   if (argc >= 3)
      {
      DOUBLE atten_dB = 0.0;
      sscanf(argv[2],"%lf", &atten_dB);

      set_atten_dB(atten_dB);
      update_switches();
      exit(0);
      }

   printf("1/!: 1 dB\n");
   printf("2/@: 2 dB\n");
   printf("3/#: 4 dB\n");
   printf("4/$: 4 dB\n");
   printf("5/%%: 10 dB\n");
   printf("6/^: 20 dB\n");
   printf("7/&: 40 dB\n");
   printf("8/*: 40 dB\n");
   printf("  a: Enter attenuation in dB\n");
   printf("  r: Random attenuation\n");
   printf("Esc: Exit\n\n");

   bool random = FALSE;

   for (;;)
      {
      if (random)
         {
         if (_kbhit())
            {
            _getch();
            break;
            }

         DOUBLE atten_dB = (DOUBLE) (rand() % 122);      // 33004+33005 = 11 dB+110 dB = 121 dB max

         set_atten_dB(atten_dB);
         }
      else
         {
         C8 ch = _getch();

         if (ch == 27)
            {
            break;
            }

         switch (ch)
            {
            case 'r': random = TRUE; break;
            case '1': switches |=  0x01; break;
            case '!': switches &= ~0x01; break;
            case '2': switches |=  0x02; break;
            case '@': switches &= ~0x02; break;
            case '3': switches |=  0x04; break;
            case '#': switches &= ~0x04; break;
            case '4': switches |=  0x08; break;
            case '$': switches &= ~0x08; break;
            case '5': switches |=  0x10; break;
            case '%': switches &= ~0x10; break;
            case '6': switches |=  0x20; break;
            case '^': switches &= ~0x20; break;
            case '7': switches |=  0x40; break;
            case '&': switches &= ~0x40; break;
            case '8': switches |=  0x80; break;
            case '*': switches &= ~0x80; break;

            case 'a': 
               {
               printf("Atten: ");
               DOUBLE atten_dB = 0.0;
               scanf("%lf", &atten_dB);

               set_atten_dB(atten_dB);
               break;
               }
            }
         }

      update_switches();
      }
}
