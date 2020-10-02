//
// HP7470A plotter emulator
//
// File dump parser
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
      printf("Usage: parse <plotter data file>\n");
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

   plot_end = &plot_data[plot_len];

   //
   // Remove CR/LF sequences (and additional junk chars) that appear at end of physical 
   // lines in HP 54110D
   //
   // To avoid altering LB statements (HP 8566B, HP 3040A), do this only if at 
   // least one !\r\n sequence appears in the file, and no DT\r statement appears
   // 

   if ((strstr(plot_data,"DT\r")  == NULL) && 
       (strstr(plot_data,"!\r\n") != NULL))
      {
      while (1)
         {
         C8 *t = strstr(plot_data,"!\r\n ");

         if (t == NULL)
            {
            break;
            }

         memmove(t, &t[4], plot_end-t-4);

         plot_len  -= 4;
         plot_end  -= 4;
         *plot_end = 0;
         }

      while (1)
         {
         C8 *t = strstr(plot_data,"\r\n");

         if (t == NULL)
            {
            break;
            }

         memmove(t, &t[2], plot_end-t-2);

         plot_len  -= 2;
         plot_end  -= 2;
         *plot_end = 0;
         }
      }

   ptr = plot_data;

   S32 terminator = 3;

   while (1)
      {
      S32 cmd = fetch_command();

      switch (cmd)
         {
         case CMD_CA:           
            {
            printf("CA (Designate Alternate Character Set): %d\n",
               fetch_integer());
            break;
            }

         case CMD_CP:           
            {
            printf("CP (Character Plot): ");

            if (next_number())
               {
               S32 spaces = (S32) fetch_float();
               fetch_comma();
               S32 lines = (S32) fetch_float();

               printf("%d spaces, %d lines\n",spaces,lines);
               }
            else
               {
               printf("CR/LF\n");
               }

            break;
            }

         case CMD_CS:           
            {
            printf("CS (Designate Standard Character Set): ");
            
            if (next_number())
               {
               S32 set = fetch_integer();

               printf("%d\n",set);
               }
            else
               {
               printf("Default\n");
               }
            break;
            }

         case CMD_DC:           
            {
            printf("DC (Digitize Clear)\n");
            break;
            }

         case CMD_DF:           
            {
            printf("DF (Default)\n");
            terminator = 3;
            break;
            }

         case CMD_DI:           
            {
            SINGLE run  = 1;
            SINGLE rise = 0;

            if (next_number())
               {
               SINGLE run = fetch_float();
               fetch_comma();
               SINGLE rise = fetch_float();
               }

            printf("DI (Absolute Direction): run %f, rise %f\n",run,rise);
            break;
            }

         case CMD_DP:           
            {
            printf("DP (Digitize Point)\n");
            break;
            }

         case CMD_DR:           
            {
            printf("DR (Relative Direction): ");

            if (next_number())
               {
               SINGLE run = fetch_float();
               fetch_comma();
               SINGLE rise = fetch_float();

               printf("run %f, rise %f\n",run,rise);
               }
            else
               {
               printf("horizontal labels\n");
               }

            break;
            }

         case CMD_DT:           
            {
            terminator = *ptr++;

            printf("DT (Define Terminator for labels): %d\n",terminator);
            break;
            }

         case CMD_IM:           
            {
            S32 e = 223;
            S32 s = 0;
            S32 p = 0;

            if (next_number())
               {
               e = fetch_integer();

               if (next_comma())
                  {
                  fetch_comma();
                  s = fetch_integer();

                  if (next_comma())
                     {
                     fetch_comma();
                     p = fetch_integer();
                     }
                  }
               }


            printf("IM (Input Mask): E=%d, S=%d, P=%d\n",e,s,p);
            break;
            }

         case CMD_IN:           
            {
            printf("IN (Initialize)\n");
            terminator = 3;
            break;
            }

         case CMD_IP:           
            {
            SINGLE p1x = 250;
            SINGLE p1y = 279;
            SINGLE p2x = 10250;
            SINGLE p2y = 7479;

            if (next_number())
               {
               p1x = fetch_float();
               fetch_comma();
               p1y = fetch_float();
               fetch_comma();
               p2x = fetch_float();
               fetch_comma();
               p2y = fetch_float();
               }

            printf("IP (Input P1 and P2): P1X=%f, P1Y=%f, P2X=%f, P2Y=%f\n",
               p1x,p1y,p2x,p2y);

            break;
            }

         case CMD_IW:           
            {
            printf("IW (Input Window): ");

            SINGLE Xll;
            SINGLE Yll;
            SINGLE Xur;
            SINGLE Yur;

            if (next_number())
               {
               Xll = fetch_float();
               fetch_comma();
               Yll = fetch_float();
               fetch_comma();
               Xur = fetch_float();
               fetch_comma();
               Yur = fetch_float();

               printf("left=%f, bottom=%f, right=%f, top=%f\n",
                  Xll,Yll,Xur,Yur);
               }
            else
               {
               printf("entire page\n");
               }
            break;
            }

         case CMD_LB:           
            {
            printf("LB (Label): %s\n",fetch_text(terminator));
            break;
            }

         case CMD_LT:           
            {
            printf("LT (Line Type): ");

            if (!next_number())
               {
               printf("solid line\n");
               }
            else
               {
               printf("pattern #%d",fetch_integer());

               if (next_comma())
                  {
                  fetch_comma();
                  printf(", length %f",fetch_float());
                  }

               printf("\n");
               }

            break;
            }

         case CMD_OA:           
            {
            printf("OA (Output Actual Position and Pen Status)\n");
            break;
            }

         case CMD_OC:           
            {
            printf("OC (Output Commanded Position and Pen Status)\n");
            break;
            }

         case CMD_OD:           
            {
            printf("OD (Output Digitized Point and Pen Status)\n");
            break;
            }

         case CMD_OE:           
            {
            printf("OE (Output Error)\n");
            break;
            }

         case CMD_OF:           
            {
            printf("OF (Output Factors)\n");
            break;
            }

         case CMD_OI:           
            {
            printf("OI (Output Identification)\n");
            break;
            }

         case CMD_OO:           
            {
            printf("OO (Output Options)\n");
            break;
            }

         case CMD_OP:           
            {
            printf("OP (Output P1 and P2)\n");
            break;
            }

         case CMD_OS:           
            {
            printf("OS (Output Status)\n");
            break;
            }

         case CMD_OW:           
            {
            printf("OW (Output Window)\n");
            break;
            }

         case CMD_PA:           
            {
            printf("PA (Plot Absolute)");

            if (!next_number())
               {
               printf("\n");
               break;
               }

            printf(": ");

            S32 pair = 0;

            while (1)
               {
               if (!next_number())
                  {
                  break;
                  }

               //
               // Fetch coordinate pair
               // 

               SINGLE cx = fetch_float();
               if (!next_comma())
                  {
                  break;                  // workaround for truncated PA commands from Tek 370
                  }

               fetch_comma();
               swallow_character(';');    // workaround for HP 5372A firmware bug

               if (!next_number())
                  {
                  break;                  // workaround for truncated PA commands from Tek 370 
                  }
               SINGLE cy = fetch_float();

               printf("(%f,%f)",cx,cy);

               if (!next_comma())
                  {
                  printf("\n");
                  break;
                  }

               printf(" to ");
               fetch_comma();

               if (pair++ > 2)
                  {
                  printf("\n                    ");
                  pair = 0;
                  }
               }

            break;
            }

         case CMD_PD:           
            {
            printf("PD (Pen Down)");

            if (!next_number())
               {
               printf("\n");
               break;
               }

            printf(": ");

            S32 pair = 0;

            while (1)
               {
               //
               // Fetch coordinate pair
               // 

               SINGLE cx = fetch_float();
               fetch_comma();
               SINGLE cy = fetch_float();

               printf("(%f,%f)",cx,cy);

               if (!next_comma())
                  {
                  printf("\n");
                  break;
                  }

               printf(" to ");
               fetch_comma();

               if (pair++ > 2)
                  {
                  printf("\n                ");
                  pair = 0;
                  }
               }

            break;
            }

         case CMD_PR:           
            {
            printf("PR (Plot Relative)");

            if (!next_number())
               {
               printf("\n");
               break;
               }

            printf(": ");

            S32 pair = 0;

            while (next_number())
               {
               //
               // Fetch coordinate pair
               // 

               SINGLE cx = fetch_float();
               fetch_comma();
               swallow_character(';');    // workaround for HP 5372A firmware bug
               SINGLE cy = fetch_float();

               printf("(%f,%f)",cx,cy);

               if (!next_comma())
                  {
                  printf("\n");
                  break;
                  }

               printf(" to ");
               fetch_comma();

               if (pair++ > 2)
                  {
                  printf("\n                    ");
                  pair = 0;
                  }
               }

            break;
            }

         case CMD_PU:           
            {
            printf("PU (Pen Up)");

            if (!next_number())
               {
               printf("\n");
               break;
               }

            printf(": ");

            S32 pair = 0;

            while (1)
               {
               //
               // Fetch coordinate pair
               // 

               SINGLE cx = fetch_float();
               fetch_comma();
               SINGLE cy = fetch_float();

               printf("(%f,%f)",cx,cy);

               if (!next_comma())
                  {
                  printf("\n");
                  break;
                  }

               printf(" to ");
               fetch_comma();

               if (pair++ > 2)
                  {
                  printf("\n              ");
                  pair = 0;
                  }
               }

            break;
            }

         case CMD_SA:           
            {
            printf("SA (Select Alternate Character Set)\n");
            break;
            }

         case CMD_SC:           
            {
            if (!next_number())
               {
               printf("SC (Scale)\n");
               }
            else
               {
               SINGLE a,b,c,d;

               a = fetch_float();
               fetch_comma();

               b = fetch_float();
               fetch_comma();

               c = fetch_float();
               fetch_comma();

               d = fetch_float();

               printf("SC (Scale): x0=%f, x1=%f, y0=%f, y1=%f\n",a,b,c,d);
               }

            break;
            }

         case CMD_SI:           
            {
            SINGLE width = 0.19F;
            SINGLE height = 0.27F;

            if (next_number())
               {
               width = fetch_float();
               fetch_comma();
               height = fetch_float();
               }

            printf("SI (Absolute Character Size): %f cm, %f cm\n",width,height);
            break;
            }

         case CMD_SL:           
            {
            printf("SL (Character Slant): ");

            if (next_number())   
               {
               printf("tangent=%f\n",fetch_float());
               }
            else
               {
               printf("none\n");
               }

            break;
            }

         case CMD_SM:           
            {
            S32 sym = *ptr++;

            printf("SM (Symbol Mode): %d (%c)\n",sym,sym);
            break;
            }

         case CMD_SP:           
            {
            printf("SP (Pen Select): ");

            S32 n = 0;

            if (next_number())
               {
               n = fetch_integer();
               }

            if (n)
               {
               printf("%d\n",n);
               }
            else
               {
               printf("store pen\n");
               }

            break;
            }

         case CMD_SR:           
            {
            SINGLE x = 0.75F;
            SINGLE y = 1.5F;

            if (next_number())
               {
               x = fetch_float();
               fetch_comma();
               y = fetch_float();
               }

            printf("SR (Relative Character Size): %f%%, %f%%\n",x,y);
            break;
            }

         case CMD_SS:           
            {
            printf("SS (Select Standard Character Set)\n");
            break;
            }

         case CMD_TL:           
            {
            SINGLE tp = 0.0F;
            SINGLE tn = 0.0F;

            if (next_number())
               {
               tp = fetch_float();

               if (next_comma())
                  {
                  fetch_comma();
                  tn = fetch_float();
                  }
               }

            printf("TL (Tick Length): %f%%, %f%%\n",tp,tn);
            break;
            }

         case CMD_UC:           
            {
            printf("UC (User Defined Character)\n");

            if (!next_number())
               {
               printf("CR/LF\n");
               break;
               }

            while (next_number())
               {
               SINGLE n = fetch_float();

               if (next_comma())
                  {
                  fetch_comma();
                  }

               if (n >= 98.99F)
                  {
                  printf("   Pen down\n");
                  continue;
                  }
               else if (n <= -98.99F)
                  {
                  printf("   Pen up\n");
                  continue;
                  }

               SINGLE x = n;
               SINGLE y = fetch_float();

               if (next_comma())
                  {
                  fetch_comma();
                  }

               printf("   (%f, %f)\n",x,y);
               }

            break;
            }

         case CMD_VS:           
            {
            SINGLE v = 38.1F;

            if (next_number())
               {
               v = fetch_float();
               }

            printf("VS (Velocity Select): %f cm/sec\n",v);
            break;
            }

         case CMD_XT:           
            {
            printf("XT (X-Tick)\n");
            break;
            }

         case CMD_YT:           
            {
            printf("YT (Y-Tick)\n");
            break;
            }

         case CMD_END_OF_DATA:
            
            printf("(End)\n");
            exit(0);
         }
      }
}
