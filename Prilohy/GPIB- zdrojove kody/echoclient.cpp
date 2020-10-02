#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include "typedefs.h"

#include "gpiblib/comblock.h"

TCPBLOCK *TCP;

void shutdown(void)
{
   if (TCP != NULL)
      {
      delete TCP;
      TCP = NULL;
      }  
}

void main(S32 argc, C8 **argv)
{
   setbuf(stdout,NULL);

   if (argc < 2)
      {
      printf("\nUsage: echoclient <address of echoserver>\n\nExample: echoclient 127.0.0.1\n\n(Echoserver process must be launched first)\n");
      exit(1);
      }

   TCP = new TCPBLOCK(argv[1], NULL, 4242);

   atexit(shutdown);

   //
   // Send 64K bytes of random data
   //

   printf("Transmitting data to echoserver process...\n");

   U8 xmit[65536];

   S32 i;
   for (i=0; i < sizeof(xmit); i++)
      {
      xmit[i] = (U8) (rand() & 0xff);
      }

   S32 xs = timeGetTime();

   TCP->send((U8 *) xmit, sizeof(xmit));

   S32 xe = timeGetTime();

   //
   // Receive and verify it
   //

   printf("Receiving transmitted data via localhost...\n");

   TERM_REASON reason = SR_IN_PROGRESS;
   S32         rx_cnt = 0;

   S32 rs = timeGetTime();

   U8 *rx = (U8 *) TCP->receive(sizeof(xmit),
                               -1,
                                10000,
                                10000,
                                NULL,
                               &rx_cnt,
                               &reason);
       
   S32 re = timeGetTime();

   for (i=0; i < sizeof(xmit); i++)
      {
      if (rx[i] != xmit[i])
         {
         printf("Data mismatch error, index %d (sent %d, received %d)\n",i, xmit[i], rx[i]);
         exit(1);
         }
      }

   printf("\nData verified OK, %d ms to send, %d ms to receive\n", xe-xs, re-rs);
   printf("Receive data rate = %d bytes/second\n",sizeof(xmit) * 1000 / (re-rs));
}
