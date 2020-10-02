#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include "typedefs.h"
#include "timeutil.cpp"

#define IPCONN_DEFAULT_RECV_BUF_BYTES 32768
#define IPCONN_SO_RECVBUF_BYTES       32768

#include "ipconn.cpp"

struct CONN : public IPCONN 
{
   virtual void message_sink(IPMSGLVL level,   
                             C8      *text)
      {
      fprintf(stderr,"%s\n",text);
      }
}
IPC;

void main(S32 argc, C8 **argv)
{
   if (argc < 2)
      {
      printf("Usage: tcplist <IP_address[:port]> [EOS_character] [repeat_cnt] [timeout_msec]\n");
      printf("               [\"string to transmit\"] [wait_msec]\n\n");
      printf("   - EOS_character defaults to 10\n");
      printf("   - repeat_cnt defaults to 0 (indefinite)\n");
      printf("   - timeout_ms defaults to 0 (no timeout)\n");
      printf("   - wait_ms defaults to 0 (do not wait before connecting)\n");
      printf("   - Press ESC to terminate connection\n");
      exit(1);
      }

   S32 EOS = 10;
   S32 repeat_cnt = 0;
   S64 timeout_usec = 0;
   S32 wait_msec = 0;
   
   if (argc >= 3) EOS          = atoi(argv[2]);
   if (argc >= 4) repeat_cnt   = atoi(argv[3]);
   if (argc >= 5) timeout_usec = (S64) atoi(argv[4]) * 1000LL;
   if (argc >= 7) wait_msec    = atoi(argv[6]);

   Sleep(wait_msec);

   IPC.connect(argv[1], 1298);

   if (argc >= 6)
      {
      fprintf(stderr,"Transmitting \"%s\" ...\n", argv[5]);
      printf("%s\r\n", argv[5]);
      IPC.send_printf("%s\r\n", argv[5]);
      }

   C8 incoming_line[512];
   S32 len = 0;

   S32 lines_received = 0;

   USTIMER timer;

   S64 start_time = timer.us();
   S64 last_line_time = start_time;

   BOOL done = FALSE;

   while (!done)
      {
      if ((_kbhit()) && (_getch() == 27))
         {
         break;
         }

      if ((timeout_usec > 0) && ((timer.us() - last_line_time) >= timeout_usec))
         {
         break;
         }

      if (!IPC.status())
         {
         exit(1);
         }

      S32 avail = IPC.receive_poll();

      if (avail == 0)
         {
         Sleep(10);
         continue;
         }
      
      for (S32 i=0; i < avail; i++)
         {
         if ((_kbhit()) && (_getch() == 27))
            {
            done = 1;
            break;
            }

         C8 ch = 0;
         assert(IPC.read_block(&ch, 1));

         incoming_line[len++] = ch;

         if ((ch == EOS) || (len == (sizeof(incoming_line)-1)))
            {
            incoming_line[len] = 0;
            printf("%s", incoming_line);
            len = 0;

            last_line_time = timer.us();

            if ((++lines_received >= repeat_cnt) && (repeat_cnt != 0))
               {
               done = 1;
               break;
               }

            if ((timeout_usec > 0) && ((timer.us() - last_line_time) >= timeout_usec))
               {
               done = 1;
               break;
               }
            }
         }
      }

   if (lines_received > 0)
      {
      DOUBLE Hz = ((DOUBLE) lines_received * 1E6) / ((DOUBLE) (timer.us() - start_time));
      fprintf(stderr,"\nDone, %d line(s) received\nAverage rate = %.1lf/sec\n",lines_received, Hz);
      }
}
