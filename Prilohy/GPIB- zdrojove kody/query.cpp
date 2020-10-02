#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <math.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include "gpiblib.h"

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

   if (argc < 2)
      {
      printf("\nUsage: query <address> [<command>] [<repeat_time_mS>] [<dropout_time_mS>]\n\n");
      printf("Examples:\n");
      printf("         query 18 MKF? 1000\nIssues an MKF? query once per second\n\n");
      printf("         query 18 \"TS;MKPK HI;MKF?\" 1000\nDoes the same thing, but repositions the marker each sweep\n\n");
      printf("         query 18 \"TS;MKPK HI;MKF?\" -1 2000\nSame as above, but does not repeat.  Dropout time increased from\n500 to 2000 ms\n\n");
      exit(1);
      }

   atexit(shutdown);

   S32 repeat = -1;
   S32 dropout_ms = 500;

   if (argc >= 4)
      {
      repeat = atoi(argv[3]);
      }

   if (argc >= 5)
      {
      dropout_ms = atoi(argv[4]);
      }

   while (1)
      {
      if (_kbhit())
         {
         _getch();
         break;
         }

      GPIB_connect(atoi(argv[1]),
                   GPIB_error,
                   0,
                   10000);

      GPIB_set_serial_read_dropout(dropout_ms);

      if (argc >= 3)
         {
         if (!GPIB_write(argv[2]))
            {
            printf("Write error!\n");
            exit(1);
            }
         }

//      GPIB_set_EOS_mode(10, TRUE, FALSE);   // 8566A/8568A apps will need to set EOT mode to FALSE...
                           
#if 0
      S32 len = -1;
      U8 *result = (U8 *) GPIB_read_BIN(-1,TRUE,FALSE,&len);

      printf("%d bytes:\n",len);

      for (S32 i=0; i < len; i++)
         {
         printf("%X ",result[i]);
         }
#else
      printf("%s", GPIB_read_ASC(65536));
#endif

      GPIB_disconnect();

      if (repeat == -1)
         {
         break;
         }

      Sleep(repeat);
      }
}
