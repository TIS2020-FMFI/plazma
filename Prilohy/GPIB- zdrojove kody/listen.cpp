#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "gpiblib.h"

void WINAPI GPIB_error(C8 *msg, S32 ibsta, S32 iberr, S32 ibcntl)
{
   printf("%s",msg);

   exit(1);
}

void shutdown(void)
{
   GPIB_disconnect(FALSE);
   _fcloseall();
}

void main(S32 argc, C8 **argv)
{
   setbuf(stdout,NULL);

   if (argc < 4)
      {
      printf("Usage:\n");
      printf("         listen <listener_address> <EOS_character> <timeout_msec> [<repeat_cnt>] [outfile]\n\n");
      printf("Examples:\n");
      printf("         listen -1 13 10000 1\n");
      printf("                              listens in 'listen-only' mode until a CR is\n");
      printf("                              received or 10 seconds elapses, whichever\n");
      printf("                              comes first\n\n");
      printf("         listen 5 13 10000 1\n");
      printf("                              listens on address 5 (the default for an HP-GL\n");
      printf("                              plotter)\n\n");
      printf("         listen -1 -1 10000 1\n");
      printf("                              listens in 'listen-only' mode until timing out\n");
      printf("                              in 10 seconds\n\n");
      printf("         listen -1 13 0 0\n");
      printf("                              listens in 'listen-only' mode until a CR is\n");
      printf("                              received, using an indefinite timeout period.\n");
      printf("                              The incoming string is printed to stdout, and\n");
      printf("                              the cycle repeats indefinitely until a key is\n");
      printf("                              pressed.\n\n");
      printf("Notes:\n\n");
      printf("- If the remote device is set to Talk-Only mode, you can use -1 as the\n");
      printf("  listener address.\n");
      printf("\n");
      printf("- The repeat_cnt parameter is optional, with a default value of 1.\n");
      printf("  If repeat_cnt is 0, then the program will listen until a key is pressed.\n");
      printf("\n");
      printf("- The outfile parameter causes a copy of each incoming line to be written to\n");
      printf("  the specified file.  This is equivalent to piping the output to the file, \n");
      printf("  while still allowing the traffic to be monitored in the console window.\n");
      printf("\n");
      printf("- You can exit from listen.exe by pressing any key.  However, the keyboard is\n");
      printf("  checked *only* between received strings.  If you specify a long or indefinite\n");
      printf("  timeout_msec parameter and your device stops talking, you'll have to use \n");
      printf("  ctrl-C to exit the program.  This can leave the GPIB or serial driver in an\n");
      printf("  unsupported state, preventing it from running again until rebooting.\n");
      printf("  For this reason you should exit from listen.exe by hitting the space bar\n");
      printf("  (or any other key besides ctrl-C) when possible.\n");

      exit(1);
      }

   atexit(shutdown);

   S32 addr       = atoi(argv[1]);
   S32 EOS        = atoi(argv[2]);
   S32 timeout_ms = atoi(argv[3]);
   S32 repeat_cnt = 1;

   if (argc >= 5)
      repeat_cnt = atoi(argv[4]);

   FILE *out = NULL;

   if (argc >= 6)
      {
      out = fopen(argv[5],"wt");

      if (out == NULL)
         {
         printf("Error: can't open %s for writing\n",argv[5]);
         exit(1);
         }                            

      setbuf(out,NULL);
      }

   GPIB_connect(LISTEN_ONLY_NO_COMMANDS,
                GPIB_error,
                0,
                timeout_ms,
                addr,
                0,
                65536);

   GPIB_set_EOS_mode(EOS);

   for (S32 i=0; (i < repeat_cnt) || (repeat_cnt == 0); i++)
      {
      if (_kbhit())
         {
         _getch();
         break;
         }

      C8 *result = GPIB_read_ASC(-1, FALSE);    // Don't treat timeouts as errors

      if ((result != NULL) && (result[0] != 0))
         {
         printf("%s",result);
         
         if (out != NULL)
            {
            fprintf(out,"%s",result);
            }
         }
      }
}
