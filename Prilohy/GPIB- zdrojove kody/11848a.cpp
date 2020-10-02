#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <assert.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include "gpiblib.h"

bool diag_mode;
S32  GPIB_addr;

U8 global_latches[13];  

const C8 *cmds[] = {
                   "preset",
                   "10mhza",
                   "10mhzb",
                   "400mhzosc",
                   "400mhzvco",
                   "K1",
                   "K2",
                   "K3",
                   "K4",
                   "K5",
                   "K6",
                   "K7",
                   "K8",
                   "K9",
                   "K10",
                   "K11",
                   "K12",
                   "K13",
                   "Test",
                   "S1",
                   "S2",
                   "S3",
                   "S4",
                   "S5",
                   "S6",
                   "S7",
                   "S8",
                   "S9",
                   "F0",
                   "F1",
                   "F2",
                   "F3",
                   "F4",
                   "F5",
                   "L3",
                   "L4",
                   "L5",
                   "L6",
                   "L7",
                   "L10",
                   "L11",

                   "-10mhza",   
                   "-10mhzb",   
                   "-400mhzosc",
                   "-400mhzvco",
                   "-K1",
                   "-K2",
                   "-K3",
                   "-K4",
                   "-K5",
                   "-K6",
                   "-K7",
                   "-K8",
                   "-K9",
                   "-K10",
                   "-K11",
                   "-K12",
                   "-K13",
                   "-Test",
                   "-S1",
                   "-S2",
                   "-S3",
                   "-S4",
                   "-S5",
                   "-S6",
                   "-S7",
                   "-S8",
                   "-S9" ,
                   "-F0",
                   "-F1",
                   "-F2",
                   "-F3",
                   "-F4",
                   "-F5",
                   "-L3",
                   "-L4",
                   "-L5",
                   "-L6",
                   "-L7",
                   "-L10",
                   "-L11"
                   };

const C8 *exec[] = { 
                   "S8,L3",
                   "L17,L18",    // A6 = L17 and L18, per schematic notes in manual
                   "L15",
                   "L17,-L18",   // A8 = L17 xor L18?
                   "-L17,L18",   // A9 = L17 and !L18?
                   "K1",
                   "K2",
                   "K3",
                   "K4",
                   "K5",
                   "K6",
                   "K7",
                   "K8",
                   "K9",
                   "K10",
                   "K11",
                   "K12",
                   "K13",
                   "S8,L3,L17,-L18,K12",
                   "S1",
                   "S2",
                   "S3",
                   "S4",
                   "S5",
                   "S6",
                   "S7",
                   "S8",
                   "S9",
                   "F0",
                   "F1",
                   "F2",
                   "F3",
                   "F4",
                   "F5",
                   "L3",
                   "L4",
                   "L5",
                   "L6",
                   "L7",
                   "L10",
                   "L11",

                   "-L17,-L18",     // turns 10 M A off (A6), but does this also turn A8 and A9 on?
                   "-L15",
                   "-L17,L18",
                   "L17,L18",
                   "-K1",
                   "-K2",
                   "-K3",
                   "-K4",
                   "-K5",
                   "-K6",
                   "-K7",
                   "-K8",
                   "-K9",
                   "-K10",
                   "-K11",
                   "-K12",
                   "-K13",
                   "S8,L3,-L17,L18,-K12",
                   "-S1",
                   "-S2",
                   "-S3",
                   "-S4",
                   "-S5",
                   "-S6",
                   "-S7",
                   "-S8",
                   "-S9",
                   "-F0",
                   "-F1",
                   "-F2",
                   "-F3",
                   "-F4",
                   "-F5",
                   "-L3",
                   "-L4",
                   "-L5",
                   "-L6",
                   "-L7",
                   "-L10",
                   "-L11"
                   };

void relay_bit_stream(const C8 *relays, U8 *latch)
{
   //
   // Fill command list with array indexes
   //
   // Positive index = turn latch on
   // Negative index = turn latch off
   //
   // This routine does not affect any bits in the latch array
   // other than those associated with the command.  In particular,
   // the high bits need to stay on, since they are used to distinguish
   // between latch values and latch indexes
   //

   S8 stack[20];
   memset(stack, 0, sizeof(stack));

   S32 ptr = 0;
   const C8 *src = relays;

   while (*src)
      {
      if ((!isalpha(*src)) && (*src != '-'))
         {
         ++src;            // comma or other non-command character
         continue;
         }

      S8 add  = -1;        // fetch next command
      S8 mult =  1;

      for (;;)
         {
         const C8 *srcptr = src;
         C8 ch = toupper(*src++);

         assert(ch);       // (command string ended prematurely)

         if (isspace(ch)) 
            { 
            continue; 
            }

         if (ch == '-')
            {
            mult = -1;
            continue;
            }

         switch (ch)
            {
            case 'K': add = 0;  break; 
            case 'S': add = 20; break; 
            case 'F': add = 30; break; 
            case 'L': add = 40; break;
            }

         if (isdigit(ch))
            {
            assert(add != -1);
            stack[ptr++] = mult * (add + atoi(srcptr));
            break;         // command ended normally
            }
         }
      }

   //
   // Execute commands in list 
   //

   const S32 latch_data[61][2] = {{2,3},{1,6},{1,2},{1,2},{0,5},{0,3},{0,7},{4,7},{4,7},         // 01-09  K1-K9   
                                  {4,3},{2,7},{2,5},{1,4},{2,7},{0,9},{0,9},{0,9},{0,9},{0,9},   // 10-19  K10-K19 
                                  {0,9},{9,6},{1,1},{0,4},{0,6},{2,6},{2,2},{3,1},{3,3},{0,9},   // 20-29  S0-S9   
                                  {8,6},{9,4},{9,4},{0,9},{0,9},{0,9},{0,9},{0,9},{0,9},{0,9},   // 30-39  F0-F9   
                                  {0,9},{7,4},{7,2},{0,9},{0,9},{0,9},{0,9},{3,2},{9,2},{0,9},   // 40-49  L0-L9   
                                  {9,5},{9,4},{0,9},{0,9},{0,9},{6,7},{6,6},{0,1},{0,2},{0,9},   // 50-59  L10-L19 
                                  {0,9},{0,9}};                                                  // 60-61  L20-L21 

   for (S32 i=0; i < ptr; i++)
      {               
      S8 kind = stack[i];

      if ((abs(kind) >= 43) && (abs(kind) <= 46))
         {
         U8 lines = kind-43;             // check for L3 through L6

         latch[7] = latch[7] & ~(1 << 6);                 // set 7,7 to 0
         latch[7] = latch[7] & ~(1 << 4);                 // set 7,5 to 0
         latch[7] = latch[7] |  ((lines & 1) << 6);       // set 7,7 to 1 if needed
         latch[7] = latch[7] | (((lines & 2) >> 1) << 4); // set 7,5 to 1 if needed
         }

      if ((kind >= 25) && (kind <= 28))
         {
         for (S32 k=25; k <= 28; k++)     // clear inputs to 3561A FFT analyzer 
            {                             // (referred to as "3582A" in RMB source, probably left over from 3047A) 
            S32 lnum = latch_data[k-1][0];
            S32 bit  = latch_data[k-1][1];
            latch[lnum] &= ~(1 << (bit-1));
            }
         }

      if ((kind >= 30) && (kind <= 32))
         {
         S32 lnum = latch_data[29][0];
         S32 bit  = latch_data[29][1];
         latch[lnum] &= ~(1 << (bit-1));    // 86=0

         if (kind == 32)
            {
            latch[lnum] |= (1 << (bit-1));  // 86=1
            }

         lnum = latch_data[30][0];
         bit  = latch_data[30][1];
         latch[lnum] &= ~(1 << (bit-1));    // 94=0

         if ((kind == 31) || (kind == 32))
            {
            latch[lnum] |= (1 << (bit-1));  // 94=1
            }
         }
      else
         {
         S32 lnum = latch_data[abs(kind)-1][0];
         S32 bit  = latch_data[abs(kind)-1][1];

         if (kind >= 0)
            latch[lnum] |= (1 << (bit-1)); 
         else
            latch[lnum] &= ~(1 << (bit-1));
         }
      }
}

S32 GPIB_write (void *string, S32 len)
{
   if (diag_mode)
      {
      U8 *t = (U8 *) string;
      for (S32 i=0; i < len; i++)
         {
         printf("  0x%.02X ", t[i] );
         }

      printf("\n");
      }

   if (GPIB_addr == 0) 
      {
      return len;
      }

   return GPIB_write_BIN(string, len);
}


void flip_clock1(U8 *latches)
{
   if (diag_mode)
      {
      printf("Clock 1:\n");
      }

   U8 string[2];
   string[0] = 7;
   string[1] = latches[7] | 1;

   if (!GPIB_write(string,2))
      {
      printf("Write error!\n");
      exit(1);
      }

   string[0] = 7;
   string[1] = latches[7];

   if (!GPIB_write(string,2))
      {
      printf("Write error!\n");
      exit(1);
      }
}

void flip_clock2(U8 *latches)
{
   if (diag_mode)
      {
      printf("Clock 2:\n");
      }

   U8 string[2];
   string[0] = 8;
   string[1] = latches[11] | 2;

   if (!GPIB_write(string,2))
      {
      printf("Write error!\n");
      exit(1);
      }

   string[0] = 8;
   string[1] = latches[11];

   if (!GPIB_write(string,2))
      {
      printf("Write error!\n");
      exit(1);
      }
}

void WINAPI GPIB_error(C8 *msg, S32 ibsta, S32 iberr, S32 ibcntl)
{
   printf("%s",msg);

   exit(1);
}

void shutdown(void)
{
   GPIB_disconnect();
}

void main(S32 argc, C8 **argv)
{
   setbuf(stdout,NULL);
   assert(ARY_CNT(cmds) == ARY_CNT(exec));

   if (argc < 3)
      {
      printf("Usage: 11848A <address> [<command> ...]\n\n");
      printf("Examples:\n");
      printf("          11848a 20 preset         Issue preset (S8,L3)\n");
      printf("          11848a 20 [-]10mhza      10 MHz source 'A' (+15 dBm, +/- 100 Hz)\n");
      printf("          11848a 20 [-]10mhzb      10 MHz source 'B' (+5 dBm, +/- 1 kHz)\n");  
      printf("          11848a 20 [-]400mhzosc   400 MHz source (-5 dBm, fixed)\n");                
      printf("          11848a 20 [-]400mhzvco   350-500 MHz source (+17 dBm)\n\n");              
      printf("          11848a 20 [-]K8          Switch K8+9 on/off\n");
      printf("          11848a 20 [-]K10         Switch K10 on/off\n");
      printf("          11848a 20 [-]K11         Switch K11+14 on/off\n");
      printf("          11848a 20 [-]K12         Switch K12 on/off\n");
      printf("          11848a 20 [-]K13         Switch K13 on/off\n");
      printf("          11848a 20 [-]Test        400Mhz on + K12 on\n");
      printf("          11848a 20 [-]S5          Switch S5 on/off\n");
      printf("          11848a 20 [-]S6          Switch S6 on/off\n");
      printf("          11848a 20 [-]S7          Switch S7 on/off\n");
      printf("          11848a 20 [-]L7          Switch 100 kHz cal osc on/off\n");
      printf("          11848a 20 [-]L10         Switch L10 on/off\n");
      printf("          11848a 20 [-]L11         Switch L11 on/off\n");
      printf("Notes:\n");
      printf("          To enable multiple sources, use a command such as:\n\n");
      printf("              11848a 20 10mhza 10mhzb\n\n");
      printf("          Some source options are mutually exclusive, and cannot be enabled\n");
      printf("          simultaneously.  E.g., turning on 400mhzvco will turn off 400mhzosc\n\n");
      printf("          Use diag and -diag to control diagnostic text output for subsequent\n");
      printf("          commands; e.g.,\n\n");
      printf("              11848a 20 diag preset 10mhza -diag 10mhzb\n\n");
      printf("          shows diagnostic data for the preset and 10mhza commands, but not for\n");
      printf("          the 10mhzb command\n");
      exit(1);
      }

   atexit(shutdown);

   GPIB_addr = atoi(argv[1]);

   if (GPIB_addr > 0)
      {
      GPIB_connect(GPIB_addr,
                   GPIB_error,
                   0,
                   3000);
      }

   //
   // All latches have bit 7 set to differentiate them from
   // index values
   //

   for (S32 i=0; i < ARY_CNT(global_latches); i++)
      {
      global_latches[i] = 0x80;
      }

   //
   // Execute command(s)
   //

   for (S32 argnum = 2; argnum < argc; argnum++)
      {
      //
      // Enable/disable diagnostic mode
      //

      C8 *arg = argv[argnum];

      if (!_stricmp(arg, "diag"))
         {
         diag_mode = TRUE;
         continue;
         }

      if (!_stricmp(arg, "-diag"))
         {
         diag_mode = FALSE;
         continue;
         }

      //
      // Translate supported commands to latch-control bitstream
      //

      S32 special_message = FALSE;

      S32 valid = FALSE;

      for (S32 i=0; i < ARY_CNT(cmds); i++)
         {
         if (!_stricmp(arg,cmds[i]))
            {
            if (i == 0) 
               {
               special_message = TRUE;
               }

            if (diag_mode)
               {
               printf("\nCommand '%s': %s\n", cmds[i], exec[i]);
               }

            relay_bit_stream(exec[i], global_latches);
            valid = TRUE;
            break;
            }
         }

      if (!valid)
         {
         printf("Invalid command: %s\n",arg);
         exit(1);
         }

      //
      // Transmit latch data
      //

      for (S32 i=0; i <= 9; i++)
         {
         U8 string[2];
         string[0] = (U8) i;
         string[1] = global_latches[i];

         if (!GPIB_write(string,2))  
            {
            printf("Write error!\n");
            exit(1);
            }
         }

      flip_clock1(global_latches);

      if (special_message)
         {
         if (diag_mode)
            {
            printf("Special message:\n");
            }

         for (S32 i=7; i <= 9; i++)
            {
            U8 string[2];
            string[0] = (U8) i;
            string[1] = global_latches[i+3];

            if (!GPIB_write(string,2))  
               {
               printf("Write error!\n");
               exit(1);
               }
            }

         flip_clock2(global_latches);
         }

      Sleep(100);
      }
}
