#include <stdio.h>
#include <conio.h>
#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include "gpiblib.h"

//
// This program demonstrates how to retrieve a binary data block of arbitrary size, 
// terminating only when the instrument has no data left to send.  It avoids any 
// message-size limitations imposed by the GPIB layer, using the same basic 
// technique as 7470.EXE
//

#define READ_CHUNK_BYTES 100        // Read up to this many bytes per chunk

void WINAPI GPIB_error(C8 *msg, S32 ibsta, S32 iberr, S32 ibcntl)
{
   printf("%s",msg);

   exit(1);
}

void shutdown(void)
{
   GPIB_disconnect();
   _fcloseall();
}

int main(S32 argc, C8 **argv)
{
   setbuf(stdout,NULL);

   if (argc < 4)
      {
      printf("\nUsage: binquery <address> <command> <filename_to_save> <timeout_ms>\n\n");
      printf("Examples:\n");
      printf("         binquery 7 \"HCIMAG SCOL;HCCMPRS OFF;BMP?\" bitmap.bmp 2000\n\nRequests a .bmp screen dump from an Advantest R3267 analyzer at address 7\n");
      printf("with a 2000-millisecond timeout (default=1000 ms)\n");
      exit(1);
      }

   atexit(shutdown);

   S32 timeout_ms = 1000;

   if (argc >= 5)
      {
      timeout_ms = atoi(argv[4]);
      printf("Using %d ms timeout\n", timeout_ms);
      }

   GPIB_connect(atoi(argv[1]),
                GPIB_error,
                0,
                timeout_ms);

   if (!GPIB_write(argv[2]))
      {
      printf("Write error!\n");
      exit(1);
      }

   FILE *out = fopen(argv[3],"wb");
   if (out == NULL)
      {
      printf("File open error!\n");
      exit(1);
      }

   S32 total_bytes = 0;
   S32 progress = 0;

   while (!_kbhit())
      {
      S32 len = -1;
      U8 *result = (U8 *) GPIB_read_BIN(READ_CHUNK_BYTES,FALSE,FALSE,&len);

      if ((len <= 0) && (total_bytes == 0))
         {
         printf("Waiting for transmission to start...\n");
         continue;
         }

      if (len <= 0)                  // timeout occurred, nothing left to read
         {
         break;
         }

      if (fwrite(result, 1, len, out) != (U32) len)
         {
         printf("File write error!\n");
         exit(1);
         }

      if (total_bytes == 0)
         {
         printf("Receiving data");
         }

      if (++progress > 10)
         {
         printf(".");
         progress = 0;
         }

      total_bytes += len;

      if (len != READ_CHUNK_BYTES)   // we got the final partial chunk, so we don't need to try again
         {
         break;
         }
      }

   printf("\n%d bytes saved to file '%s'\n",total_bytes,argv[3]);

   if (_kbhit())
      {
      printf("(Interrupted by keypress)\n");
      _getch();
      }

   return 0;
}
