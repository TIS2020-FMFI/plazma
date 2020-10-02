#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <mswsock.h>

#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include "typedefs.h"

#include "timeutil.cpp"
#include "ipconn.cpp"

const S32 NTP_PORT_NUM = 123;           // NTP port number

int main(S32 argc, C8 **argv)
{
   C8 server_address[MAX_PATH] = { 0 };
    
   if (argc > 1)
      _snprintf(server_address, sizeof(server_address)-1, "%s", argv[1]);
   else
      strcpy(server_address,"pool.ntp.org");

   if (strlen(server_address) < 3)    // e.g. ? or /?
      {
      printf("Usage:\n");
      printf("         ntpquery <address>\n\n");
      printf("Examples:\n");
      printf("         ntpquery time.nist.gov\n");
      printf("                              queries time.nist.gov for the current time\n");
      printf("         ntpquery pool.ntp.org\n");
      printf("                              allows ntp.org to select an appropriate server\n\n");
      printf("Default with no arguments = pool.ntp.org\n");

      exit(1);
      }

   //
   // Start up Winsock and allocate a UDP socket
   //

   UDPXCVR NTP;

   if (!NTP.set_remote_address(server_address, NTP_PORT_NUM))
      {
      exit(1);
      }

   NTP.set_timeout_ms(3000);

   //
   // Send request packet
   //

   printf("\nQuerying %s ...\n", server_address);
   
   U8 msg[48] = { 0 };

   msg[0]  = 0xE3;    // Leap, Version, Mode
   msg[1]  = 0;       // Stratum
   msg[2]  = 6;       // Polling Interval
   msg[3]  = 0xEC;    // Peer Clock Precision
   msg[12] = 49;      // Reference Identifier
   msg[13] = 0x4E;    
   msg[14] = 49;
   msg[15] = 52;

   if (!NTP.send_block(msg, sizeof(msg)))
      {
      exit(1);
      }

   //
   // Read response
   //

   U8 buf[1024] = { 0 };

   S32 result = NTP.read_block(buf, sizeof(buf));

   if (result < 48)
      {
      printf("Error: NTP.read_block() returned %d, expected at least 48 bytes\n", result);
      exit(1);
      }

   C8 text[512] = { 0 };
   USTIMER::timestamp(text, sizeof(text), USTIMER::NTP_to_us(&buf[40]));

   printf("Result: %s\n", text);

   return 0;
}
