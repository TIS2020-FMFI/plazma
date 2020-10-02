#include <stdio.h>
#include <conio.h>
#include <math.h>
#include <stdlib.h>
#include <windows.h>

#if 0
  #define ANSI_CONSOLE_COLORS
  #define AC_SEP AC_NORM
#else
  #define AC_SEP "\b: "
#endif

#include "typedefs.h"

#include "visa.h"

#define DEFAULT_DMM_ADDR "TCPIP0::192.168.1.221::inst0::INSTR"

ViSession viSession;
ViSession V;

C8 buf[65536];

void shutdown(void)
{
   if (V != NULL)
      {
      viClose(V);
      V = NULL;
      }

   if (viSession != NULL)
      {
      viClose(viSession);
      viSession = NULL;
      }

   _fcloseall();
}

DOUBLE Th100K_degsC(DOUBLE ohms)
{
   //
   // Convert 100K thermistor readings to temperature in C
   //

   DOUBLE Rt1 = 100E3;
   DOUBLE Rt2 = ohms;
   DOUBLE T1  = 25.0 + 273.15;

#if 0
      DOUBLE B   = 3950.0;    // Vishay NTCS0402E3104JHT SMT 0402, 25-85C
#else
      DOUBLE B   = 4303.0;    // Murata NXFT15WF104FA thru-hole, 25-80C
#endif

   DOUBLE a = log(Rt1 / Rt2) / B;
   DOUBLE b = (1.0 / T1) - a;
   DOUBLE c = 1.0 / b;

   return c - 273.15;
}

int main(int argc, char **argv)
{
   setbuf(stdout, NULL);

   if (argc < 2)
      {
      printf("Usage: visadmm interval_ms [address] [outfile.txt]\n\n");
      printf("Example:\n\n\tvisadmm 1000\n\nconnects to the default address (TCPIP0::192.168.1.221::inst0::INSTR) and \n");
      printf("requests readings at 1-second intervals\n\n");
      printf("[More] "); _getch();
      printf("\rOther address examples include:\n\n");

      //
      // Examples from pyvisa.readthedocs.io/en/stable/names.html
      //

      printf(AC_CYAN "ASRL::1.2.3.4::2::INSTR " AC_SEP "A serial device attached to port 2 of the ENET Serial\ncontroller at address 1.2.3.4\n\n");
      printf(AC_CYAN "ASRL1::INSTR " AC_SEP "A serial device attached to interface ASRL1\n\n"); 
      printf(AC_CYAN "GPIB::1::0::INSTR " AC_SEP "A GPIB device at primary address 1 and secondary\naddress 0 in GPIB interface 0\n\n"); 
      printf(AC_CYAN "GPIB2::INTFC " AC_SEP "Interface or raw board resource for GPIB interface 2\n\n"); 
      printf(AC_CYAN "PXI::15::INSTR " AC_SEP "PXI device number 15 on bus 0 with implied function 0\n\n"); 
      printf(AC_CYAN "PXI::2::BACKPLANE " AC_SEP "Backplane resource for chassis 2 on the default PXI system,\nwhich is interface 0\n\n"); 
      printf(AC_CYAN "PXI::CHASSIS1::SLOT3 " AC_SEP "PXI device in slot number 3 of the PXI chassis\nconfigured as chassis 1\n\n"); 
      printf(AC_CYAN "PXI0::2-12.1::INSTR " AC_SEP "PXI bus number 2, device 12 with function 1\n\n"); 
      printf(AC_CYAN "PXI0::MEMACC " AC_SEP "PXI MEMACC session\n\n"); 
      printf(AC_CYAN "TCPIP::dev.company.com::INSTR " AC_SEP "A TCP/IP device using VXI-11 or LXI located at\nthe specified address. This uses the default LAN Device Name of inst0\n\n"); 
      printf(AC_CYAN "TCPIP0::1.2.3.4::999::SOCKET " AC_SEP "Raw TCP/IP access to port 999 at the specified\nIP address\n\n"); 
      printf(AC_CYAN "USB::0x1234::125::A22-5::INSTR " AC_SEP "A USB Test & Measurement class device with\nmanufacturer ID 0x1234, model code 125, and serial number A22-5. This uses the\ndevice's first available USBTMC interface. This is usually number 0\n\n"); 
      printf(AC_CYAN "USB::0x5678::0x33::SN999::1::RAW " AC_SEP "A raw USB nonclass device with manufacturer\nID 0x5678, model code 0x33, and serial number SN999. This uses the\ndevice's interface number 1\n\n"); 
      printf(AC_CYAN "visa://hostname/ASRL1::INSTR " AC_SEP "The resource ASRL1::INSTR on the specified\nremote system\n\n"); 
      printf(AC_CYAN "VXI::1::BACKPLANE " AC_SEP "Mainframe resource for chassis 1 on the default VXI\nsystem, which is interface 0\n\n"); 
      printf(AC_CYAN "VXI::MEMACC " AC_SEP "Board-level register access to the VXI interface\n\n"); 
      printf(AC_CYAN "VXI0::1::INSTR " AC_SEP "A VXI device at logical address 1 in VXI interface VXI0\n\n"); 
      printf(AC_CYAN "VXI0::SERVANT " AC_SEP "Servant/device-side resource for VXI interface 0\n\n"); 

      exit(1);
      }

   U32 read_interval_ms = atoi(argv[1]);

   C8 *addr = DEFAULT_DMM_ADDR;

   if (argc > 2)
      {
      addr = argv[2];
      }

   FILE *logfile = NULL;

   if (argc > 3)
      {
      logfile = fopen(argv[3],"wt");

      if (logfile == NULL)
         {
         fprintf(stderr,"Can't open %s\n", argv[3]);
         exit(1);
         }
      }

   //
   // Open transport-independent session to device (USB/Ethernet/GPIB/etc)
   //

   viOpenDefaultRM(&viSession);

   ViStatus result = viOpen(viSession, 
                            addr,
                            VI_NULL, 
                            VI_NULL, 
                           &V);

   if (result != VI_SUCCESS)
      {
      printf("Error: viStatus = 0x%.08X\n", result);
      exit(1);
      }

   atexit(shutdown);

   //
   // Optionally reset and identify it
   // 

   viPrintf(V, "*IDN?\n");

   viScanf(V, "%t", &buf);

   fprintf(stderr, "Connected to %s", buf);
   fprintf(stderr, "Press ESC to exit\n\n");

   //
   // Fetch readings every READ_INTERVAL_MS milliseconds until ESC pressed 
   //

   U32 next = timeGetTime();
      
   for (;;)
      {
      C8 ch = _kbhit();

      if (ch)
         {
         _getch();
         break;
         }

      Sleep(10);

      if ((timeGetTime() - next) < read_interval_ms)
         {
         continue;
         }

      next += read_interval_ms;

      viPrintf(V, "INIT;FETC?\n");
      viScanf(V, "%t", &buf);

      DOUBLE val = 0.0;
      sscanf(buf, "%lf", &val);

      if (val == 0.0)
         {
         printf("Invalid reading (0.0)\n");
         exit(1);
         }

#if 1
      val = Th100K_degsC(val);
#endif

      printf("%.3lf\n", val);

      if (logfile != NULL)
         {
         fprintf(logfile,"%.3lf\n", val);
         fflush(logfile);
         }
      }

}
