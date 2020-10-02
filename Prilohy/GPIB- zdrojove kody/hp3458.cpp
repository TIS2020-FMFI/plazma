#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <dos.h>
#include <io.h>
#include <malloc.h>
#include <stdlib.h>
#include <conio.h>
#include <math.h>

//
//
//   HP3458A NVRAM dumper program.  By Mark S Sims.  Based on work by
//   Poul-Henning Kamp.
//
//   This program uses the undocumented MREAD command to read the HP3458A
//   DMM non-volatile, battery backed calibration or data NVRAM chips.  These
//   chips have a finite lifetime and once the cal ram battery goes dead you 
//   lose the calibration info and have to pay big bucks to get the meter
//   re-calibrated.  By saving a copy of the cal ram contents,  you can
//   program the data into a new NVRAM chip (using an external device programmer)
//   and replace the dead chip... hopefully avoiding exhorbitant repair and
//   re-calibration fees.  You probably don't need to back up the DATA ram,
//   but having a copy might not be a bad idea...
//
//   This program uses John Miles GPIBKIT GPIB library routines to manage
//   the GPIB controller interface.
//
//   Special note for National Instruments GPIB-232CV-A interfaces... the
//   interface must be operated in TIMEOUT mode (i.e. switches 6 and 7 OFF).
//   This does make it a bit slow...
//
//   You may need so set up your meter in "END ALWAYS" mode so it sends an
//   GPIB bus "EOI" flag at the end of each transfer.  Reported to be needed
//   with some National Instruments cards.
//
//   The HP3458A seems to be a bit funky when switching from receiving a command
//   and sending a response.  The first character of the response can 
//   occasionally get lost by a lot of GPIB controllers.  The HP3458A responses
//   end with 0x0D 0x0A.  This program ends the GBIB string with 0x0D,  so if 
//   the 0x0A gets lost,  no big deal.
//
//   This program has been tested with a NI GPIB-232CV-A interface and a home-
//   built emulator of the Prologix GPIB-RS232 interface.  Who knows if it works
//   with anything else.
//
//   It does check and verify the 4 checksum bytes of the CAL ram data.  The
//   DATA ram data does not have any known checksums.
//
//   The CAL ram data is in the upper byte of the 2048 words starting at
//   address 0x60000.  The DATA ram data seems to be in the 32768 16-bit words 
//   starting at address 0x120000.  The MREAD commands returns the memory WORD
//   value as a signed integer in ASCII.
//
//   For program usage help,  enter HP3458 with no parameters of file name.
//
//   CAL ram data is stored in the file filename.hi   DATA ram data in
//   filename.hi and filename.lo  ASCII dump data in filename.  Also a different
//   version of the ASCII data is written to stdout as the program executes.
//   Also progress info is written to stderr as the program executes.  If all 
//   is going well you should see a stream of MREAD, etc commands fly by.  
//
//   The program checks the response to the instrument "ID?" command and expects
//   to see "HP3458A" as a response.  Sometimes this gets garbled.  Try again...
//
//
//   Rev 1.1 - added listing and labeling of variable values to the output file.
//           - reformatted the output file data to include what was going to stdout
//           - removed most output to stdout
//
//


#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include "gpiblib.h"

S32 GPIB_ADDR = 22;    // GPIB address of the meter to dump
S32 eos_char  = 13;    // end-of-string char for GPIB reads
int err_exit  = 1;     // exit on errors
S32 reset_ovl = (-1);  // reset destructive overloads


void WINAPI GPIB_error(C8 *msg, S32 ibsta, S32 iberr, S32 ibcntl)
{
   fprintf(stderr, "%s",msg);
   printf("%s",msg);
   if(err_exit) exit(1);
}

void shutdown(void)
{
   GPIB_disconnect();
}


#define u08 unsigned char
#define u16 unsigned int
#define u32 unsigned long


unsigned long START = 393216L;      // cal ram data (in high byte)
unsigned long END =   397312L;


//#define HUGE   huge
//#define halloc halloc
//#define hfree  free
#define HUGE 
#define halloc calloc
#define hfree  free

unsigned char HUGE *lo_rom;
unsigned char HUGE *hi_rom;



FILE *f;
FILE *lo, *hi;   // ROM image files

char *init_string = "TRIG HOLD;ID?\r\n";

char cal_ram = 1;

unsigned long romp;  // used to address ROM image buffer



void send_string(char *s)
{
   // send a string to the GPIB device
fprintf(stderr, "sending:<%s>\n", s);
   if(!GPIB_write(s)) {
      fprintf(stderr, "ERROR: could not write to GPIB device.\n");
      if(err_exit) {
         GPIB_disconnect();
         exit(2);
      }
   }
//Sleep(10);  // allow bus to turn around
}



void get_string(char *msg)
{
int i;
   // get a string from the GPIB devics
   strcpy(msg, GPIB_read_ASC(128));
   for(i=strlen(msg)-1; i; i--) {  // trim end-of-string crap
      if(msg[i] == 0x0A) msg[i] = 0;
      else if(msg[i] == 0x0D) msg[i] = 0;
      else break;
   }

   for(i=0; i<strlen(msg); i++) { // fixup leading CR/LF crap
      if(msg[i] == 0x0A) msg[i] = ' ';
      else if(msg[i] == 0x0D) msg[i] = ' ';
      else break;
   }
fprintf(stderr, "got:<%s>\n", msg);
}

u08 valid_number(char *s)
{
int state;
int i;

   state = 0;
   for(i=0; i<strlen(s); i++) {
      if((s[i] == ' ') && (state == 0)) state = 0;      // leading blanks
      else if((state == 0) && (s[i] == '-')) state = 1; // sign
      else if((state == 0) && (s[i] == '+')) state = 1; // sign
      else if((state == 0) && isdigit(s[i])) state = 2; // first digit
      else if((state == 1) && isdigit(s[i])) state = 2; // first digit
      else if((state == 2) && isdigit(s[i])) state = 2; // digits
      else if((state == 2) && (s[i] == ' ')) state = 3; // trailing spaces
      else if((state == 3) && (s[i] == ' ')) state = 3;
      else {
fprintf(stderr, " [i:%d  c:%c(%02X)  state:%d]\n", i, s[i], s[i], state);
         return 0;
      }
   }

   if(state == 2) return 1;
   else if(state == 3) return 1;
fprintf(stderr, " [i:%d  c:%c(%02X)  state:%d]\n", i, s[i], s[i], state);
   return 0;
}


void get_nvram()
{
char s[128];
char msg[128];
int p;
unsigned long addr;
long val;

   hi_rom = (u08 *) halloc(32768L, 1);
   if(hi_rom == 0) {
      fprintf(stderr, "ERROR: could not allocate hi rom buffer\n");
      exit(3);
   }

   lo_rom = (u08 *) halloc(32768L, 1);
   if(lo_rom == 0) {
      fprintf(stderr, "ERROR: could not allocate low rom buffer\n");
      exit(4);
   }


   send_string(init_string);  // initialize and query the meter
   get_string(msg);          // get response to ID? query
   if(strstr(msg, "HP3458A") == 0) {
      fprintf(stderr, "ERROR: ID? response not HP3458A: %s\n", msg);
      if(err_exit) exit(5);
   }
   fprintf(f, "Instrument ID:%s\n", msg);
   fprintf(stderr, "Instrument ID:%s\n", msg);

   addr = START;
   p = 0;
   romp = 0L;

   while(1) {
      if(_kbhit()) {
        _getch();
        break;
      }

      sprintf(s, "MREAD %ld\r\n", addr);
      send_string(s);
      fprintf(f, "%05lX ", addr);

      fprintf(stderr, "MREAD %05lX: ", addr);

      get_string(msg);
      sscanf(msg, "%ld", &val);


      if(valid_number(msg)) {
         fprintf(f, "%02X %02X %s\n", ((val>>8)&0xFF), (val&0xFF), msg);
         lo_rom[romp] = (u08) (val & 0xFF);
         hi_rom[romp] = (u08) ((val >> 8) & 0xFF);
//printf("got %04lX:%s %02X %02X\n", romp, msg, hi_rom[romp],lo_rom[romp]);
//fflush(stdout);
         ++romp;
         if(romp >= 32768L) break;

         addr += 2L;
         if(addr >= END) break;
      }
else {
printf("Bad value read:<%s>... retrying.\n", msg);
fprintf(stderr, "Bad value read:<%s>... retrying.\n", msg);
}
   }

}


//
//  CAL ram data checksum routines
//

u16 crs(u32 a0, u32 a1)
{
u16 t;

   t = 0;
   while(a0 < a1) {
      t += hi_rom[a0];
      a0 += 1;
   }
   return (t & 0xFFFF);
}

u16 crb(u32 a0, u32 a1) 
{
u16 t;
u08 v;

   t = 0;
   while(a0 < a1) {
      v = hi_rom[0x718 + (int) (a0/8)];
      if(v & (1 << (a0 & 7))) {
         t += 1;
      }
      a0 += 1;
   }
   return (t & 0xFFFF);
}


u16 crx(u32 a0, u32 a1)
{
   return (crs(a0, a1) + crb(a0, a1)) & 0xFFFF;
}


u16 fword(u32 a)
{
u16 v;

   // get a two byte word from the high rom data

   v = hi_rom[a+0L];
   v <<= 8;
   v |= (hi_rom[a+1L] & 0xFF);
   return (v & 0xFFFF);
}



char option_switch(char *arg)
{
char c;

   c = tolower(arg[1]);

   if(c == 'a') {       // gpib address
      GPIB_ADDR = 22;
      if((arg[2] == '=') || (arg[2] == ':')) {
         GPIB_ADDR = atoi(&arg[3]);
      }
fprintf(stderr, "GPIB address: %d\n", GPIB_ADDR);
   }
   else if(c == 'd') {  // data or cal ram select
      cal_ram ^= 1;
if(cal_ram) fprintf(stderr, "dumping CAL NVRAM\n");
else        fprintf(stderr, "dumping DATA NVRAMs\n");
   }
   else if(c == 'e') {  // eos char
      eos_char = 0;
      if((arg[2] == '=') || (arg[2] == ':')) {
         eos_char = atoi(&arg[3]);
      }
fprintf(stderr, "EOS char: %d\n", eos_char);
   }
   else if(c == 'o') {
      reset_ovl = 0L;
      if((arg[2] == '=') || (arg[2] == ':')) {
         reset_ovl = atoi(&arg[3]);
      }
   }
   else if(c == 'x') {
      err_exit ^= 1;
   }
   else {
      return c;
   }

   return 0;
}


//
//   Data value dump routines
//

void show_double(int addr, char *s)
{
int i;
double *p;
u08 v[8];
char t[80];

   addr &= 0x1FFF;
   for(i=0; i<8; i++) {
      v[i] = hi_rom[addr+7-i];
   }
   p = (double *) (void *) &v[0];

   fprintf(f, "%04X:", addr);
   for(i=0; i<8; i++) fprintf(f, " %02X", hi_rom[addr+i]);

   sprintf(t, "%.9g", *p);
   for(i=strlen(t); i<16; i++) {
      t[i] = ' ';
      t[i+1] = 0;
   }
   fprintf(f, "  %s  %s\n", t, s);
}

void show_long(int addr, char *s)
{
int i;
long *p;
u08 v[4];

   addr &= 0x1FFF;
   for(i=0; i<4; i++) {
      v[i] = hi_rom[addr+3-i];
   }
   p = (long *) (void *) &v[0];

   fprintf(f, "%04X:", addr);
   for(i=0; i<8; i++) {
      if(i<4) fprintf(f, " %02X", hi_rom[addr+i]);
      else    fprintf(f, "   ");
   }
   fprintf(f, "  %-10ld        %s\n", *p, s);
}


void show_word(int addr, char *s)
{
int i;
long *p;
long x;
u08 v[4];

   addr &= 0x1FFF;
   for(i=0; i<2; i++) {
      v[i] = hi_rom[addr+1-i];
   }
   v[2] = v[3] = 0;
   p = (long *) (void *) &v[0];

   x = (*p & 0x0000FFFFL);
   if(x & 0x8000L) x= (x | 0xFFFF0000L);

   fprintf(f, "%04X:", addr);
   for(i=0; i<8; i++) {
      if(i<2) fprintf(f, " %02X", hi_rom[addr+i]);
      else    fprintf(f, "   ");
   }
   fprintf(f, "  %-10ld        %s\n", x, s);
}

void show_byte(int addr, char *s)
{
int i;
long x;

   addr &= 0x1FFF;
   x = (long) hi_rom[addr];
   if(x & 0x80L) x |= 0xFFFFFF80L;

   fprintf(f, "%04X:", addr);
   for(i=0; i<8; i++) {
      if(i<1) fprintf(f, " %02X", hi_rom[addr+i]);
      else    fprintf(f, "   ");
   }
   fprintf(f, "  %-10ld        %s\n", x, s);
}


void show_hex(int addr, char *s)
{
int i;

   addr &= 0x1FFF;

   fprintf(f, "%04X:", addr);
   for(i=0; i<8; i++) fprintf(f, " %02X", hi_rom[addr+i]);

   fprintf(f, "                    %s\n", s);
}


void show_vars()
{
   fprintf(f, "\n\n");

   show_double(0x0000, "40K reference");
   show_double(0x0008, "7V reference");
   show_double(0x0010, "dcv zero front 100mV");
   show_double(0x0018, "dcv zero rear 100mV");
   show_double(0x0020, "dcv zero front 1V");
   show_double(0x0028, "dcv zero rear 1V");
   show_double(0x0030, "dcv zero front 10V");
   show_double(0x0038, "dcv zero rear 10V");
   show_double(0x0040, "dcv zero front 100V");
   show_double(0x0048, "dcv zero rear 100V");
   show_double(0x0050, "dcv zero front 1KV");
   show_double(0x0058, "dcv zero rear 1KV");
   show_double(0x0060, "ohm zero front 10");
   show_double(0x0068, "ohm zero front 100");
   show_double(0x0070, "ohm zero front 1K");
   show_double(0x0078, "ohm zero front 10K");
   show_double(0x0080, "ohm zero front 100K");
   show_double(0x0088, "ohm zero front 1M");
   show_double(0x0090, "ohm zero front 10M");
   show_double(0x0098, "ohm zero front 100M");
   show_double(0x00a0, "ohm zero front 1G");
   show_double(0x00a8, "ohm zero rear 10");
   show_double(0x00b0, "ohm zero rear 100");
   show_double(0x00b8, "ohm zero rear 1K");
   show_double(0x00c0, "ohm zero rear 10K");
   show_double(0x00c8, "ohm zero rear 100K");
   show_double(0x00d0, "ohm zero rear 1M");
   show_double(0x00d8, "ohm zero rear 10M");
   show_double(0x00e0, "ohm zero rear 100M");
   show_double(0x00e8, "ohm zero rear 1G");
   show_double(0x00f0, "ohmf zero front 10");
   show_double(0x00f8, "ohmf zero front 100");
   show_double(0x0100, "ohmf zero front 1K");
   show_double(0x0108, "ohmf zero front 10K");
   show_double(0x0110, "ohmf zero front 100K");
   show_double(0x0118, "ohmf zero front 1M");
   show_double(0x0120, "ohmf zero front 10M");
   show_double(0x0128, "ohmf zero front 100M");
   show_double(0x0130, "ohmf zero front 1G");
   show_double(0x0138, "ohmf zero rear 10");
   show_double(0x0140, "ohmf zero rear 100");
   show_double(0x0148, "ohmf zero rear 1K");
   show_double(0x0150, "ohmf zero rear 10K");
   show_double(0x0158, "ohmf zero rear 100K");
   show_double(0x0160, "ohmf zero rear 1M");
   show_double(0x0168, "ohmf zero rear 10M");
   show_double(0x0170, "ohmf zero rear 100M");
   show_double(0x0178, "ohmf zero rear 1G");
   show_long  (0x0180, "autorange offset ohm 10");
   show_long  (0x0184, "autorange offset ohm 100");
   show_long  (0x0188, "autorange offset ohm 1k");
   show_long  (0x018c, "autorange offset ohm 10k");
   show_long  (0x0190, "autorange offset ohm 100k");
   show_long  (0x0194, "autorange offset ohm 1M");
   show_long  (0x0198, "autorange offset ohm 10M");
   show_long  (0x019c, "autorange offset ohm 100M");
   show_long  (0x01a0, "autorange offset ohm 1G");
   show_double(0x01a4, "cal 0 temperature");
   show_double(0x01ac, "cal 10 temperature");
   show_double(0x01b4, "cal 10k temperature");
   show_word  (0x01bc, "Cal_Sum0");
   show_byte  (0x01be, "vos dac");
   show_byte  (0x01bf, "filler byte");
   show_double(0x01c0, "dci zero rear 100nA");
   show_double(0x01c8, "dci zero rear 1uA");
   show_double(0x01d0, "dci zero rear 10uA");
   show_double(0x01d8, "dci zero rear 100uA");
   show_double(0x01e0, "dci zero rear 1mA");
   show_double(0x01e8, "dci zero rear 10mA");
   show_double(0x01f0, "dci zero rear 100mA");
   show_double(0x01f8, "dci zero rear 1A");
   show_double(0x0200, "dcv gain 100mV");
   show_double(0x0208, "dcv gain 1V");
   show_double(0x0210, "dcv gain 10V");
   show_double(0x0218, "dcv gain 100V");
   show_double(0x0220, "dcv gain 1KV");
   show_double(0x0228, "ohm gain 10");
   show_double(0x0230, "ohm gain 100");
   show_double(0x0238, "ohm gain 1K");
   show_double(0x0240, "ohm gain 10K");
   show_double(0x0248, "ohm gain 100K");
   show_double(0x0250, "ohm gain 1M");
   show_double(0x0258, "ohm gain 10M");
   show_double(0x0260, "ohm gain 100M");
   show_double(0x0268, "ohm gain 1G");
   show_double(0x0270, "ohm ocomp gain 10");
   show_double(0x0278, "ohm ocomp gain 100");
   show_double(0x0280, "ohm ocomp gain 1k");
   show_double(0x0288, "ohm ocomp gain 10k");
   show_double(0x0290, "ohm ocomp gain 100k");
   show_double(0x0298, "ohm ocomp gain 1M");
   show_double(0x02a0, "ohm ocomp gain 10M");
   show_double(0x02a8, "ohm ocomp gain 100M");
   show_double(0x02b0, "ohm ocomp gain 1G");
   show_double(0x02b8, "dci gain 100nA");
   show_double(0x02c0, "dci gain 1uA");
   show_double(0x02c8, "dci gain 10uA");
   show_double(0x02d0, "dci gain 100uA");
   show_double(0x02d8, "dci gain 1mA");
   show_double(0x02e0, "dci gain 10mA");
   show_double(0x02e8, "dci gain 100mA");
   show_double(0x02f0, "dci gain 1A");
   show_byte  (0x02f8, "precharge dac");
   show_byte  (0x02f9, "mc dac");
   show_double(0x02fa, "high speed gain");
   show_double(0x0302, "il");
   show_double(0x030a, "il2");
   show_double(0x0312, "rin");
   show_double(0x031a, "low aperture");
   show_double(0x0322, "high aperture");
   show_double(0x032a, "high aperture slope .01 PLC");
   show_double(0x0332, "high aperture slope .1 PLC");
   show_double(0x033a, "high aperture null .01 PLC");
   show_double(0x0342, "high aperture null .1 PLC");
   show_long  (0x034a, "underload dcv 100mV");
   show_long  (0x034e, "underload dcv 1V");
   show_long  (0x0352, "underload dcv 10V");
   show_long  (0x0356, "underload dcv 100V");
   show_long  (0x035a, "underload dcv 1000V");
   show_long  (0x035e, "overload dcv 100mV");
   show_long  (0x0362, "overload dcv 1V");
   show_long  (0x0366, "overload dcv 10V");
   show_long  (0x036a, "overload dcv 100V");
   show_long  (0x036e, "overtoad dcv 1000V");
   show_long  (0x0372, "underload ohm 10");
   show_long  (0x0376, "underload ohm 100");
   show_long  (0x037a, "underload ohm 1k");
   show_long  (0x037e, "underload ohm 10k");
   show_long  (0x0382, "underload ohm 100k");
   show_long  (0x0386, "underload ohm 1M");
   show_long  (0x038a, "underload ohm 10M");
   show_long  (0x038e, "underload ohm 100M");
   show_long  (0x0392, "underload ohm 1G");
   show_long  (0x0396, "overload ohm 10");
   show_long  (0x039a, "overload ohm 100");
   show_long  (0x039e, "overload ohm 1k");
   show_long  (0x03a2, "overload ohm 10k");
   show_long  (0x03a6, "overload ohm 100k");
   show_long  (0x03aa, "overload ohm 1M");
   show_long  (0x03ae, "overload ohm 10M");
   show_long  (0x03b2, "overload ohm 100M");
   show_long  (0x03b6, "overload ohm 1G");
   show_long  (0x03ba, "underload ohm ocomp 10");
   show_long  (0x03be, "underload ohm ocomp 100");
   show_long  (0x03c2, "underload ohm ocomp 1k");
   show_long  (0x03c6, "underload ohm ocomp 10k");
   show_long  (0x03ca, "underload ohm ocomp 100k");
   show_long  (0x03ce, "underload ohm ocomp 1M");
   show_long  (0x03d2, "underload ohm ocomp 10M");
   show_long  (0x03d6, "underload ohm ocomp 100M");
   show_long  (0x03da, "underload ohm ocomp 1G");
   show_long  (0x03de, "overload ohm ocomp 10");
   show_long  (0x03e2, "overload ohm ocomp 100");
   show_long  (0x03e6, "overload ohm ocomp 1k");
   show_long  (0x03ea, "overload ohm ocomp 10k");
   show_long  (0x03ee, "overload ohm ocomp 100k");
   show_long  (0x03f2, "overload ohm ocomp 1M");
   show_long  (0x03f6, "overload ohm ocomp 10M");
   show_long  (0x03fa, "overload ohm ocomp 100M");
   show_long  (0x03fe, "overload ohm ocomp 1G");
   show_long  (0x0402, "underload dci 100nA");
   show_long  (0x0406, "underload dci 1uA");  
   show_long  (0x040a, "underload dci 10uA"); 
   show_long  (0x040e, "underload dci 100uA");
   show_long  (0x0412, "underload dci 1mA");  
   show_long  (0x0416, "underload dci 10mA"); 
   show_long  (0x041a, "underload dci 100mA");
   show_long  (0x041e, "underload dci 1A");   
   show_long  (0x0422, "overload dci 100nA");
   show_long  (0x0426, "overload dci 1uA");     
   show_long  (0x042a, "overload dci 10uA");    
   show_long  (0x042e, "overload dci 100uA");   
   show_long  (0x0432, "overload dci 1mA");     
   show_long  (0x0436, "overload dci 10mA");    
   show_long  (0x043a, "overload dci 100mA");   
   show_long  (0x043e, "overload dci 1A");      
   show_double(0x0442, "acal dcv temperature");
   show_double(0x044a, "acal ohm temperature");
   show_double(0x0452, "acal acv temperature");
   show_byte  (0x045a, "ac offset dac 10mV");
   show_byte  (0x045b, "ac offset dac 100mV");
   show_byte  (0x045c, "ac offset dac 1V");
   show_byte  (0x045d, "ac offset dac 10V");
   show_byte  (0x045e, "ac offset dac 100V");
   show_byte  (0x045f, "ac offset dac 1KV");
   show_byte  (0x0460, "acdc offset dac 10mV");
   show_byte  (0x0461, "acdc offset dac 100mV");
   show_byte  (0x0462, "acdc offset dac 1V");
   show_byte  (0x0463, "acdc offset dac 10V");
   show_byte  (0x0464, "acdc offset dac 100V");
   show_byte  (0x0465, "acdc offset dac 1KV");
   show_byte  (0x0466, "acdci offset dac 100uA");
   show_byte  (0x0467, "acdci offset dac 1mA");
   show_byte  (0x0468, "acdci offset dac 10mA");
   show_byte  (0x0469, "acdci offset dac 100mA");
   show_byte  (0x046a, "acdci offset dac 1A");
   show_byte  (0x046b, "filler byte");
   show_word  (0x046c, "flatness dac 10mV");
   show_word  (0x046e, "flatness dac 100mV");
   show_word  (0x0470, "flatness dac 1V");
   show_word  (0x0472, "flatness dac 10V");
   show_word  (0x0474, "flatness dac 100V");
   show_word  (0x0476, "flatness dac 1KV");
   show_byte  (0x0478, "level dac dc 1.2V");
   show_byte  (0x0479, "filler byte");
   show_byte  (0x047a, "level dac dc 12V");
   show_byte  (0x047b, "filler byte");
   show_byte  (0x047c, "level dac ac 1.2V");
   show_byte  (0x047d, "level dac ac 12V");
   show_byte  (0x047e, "dcv trigger offset 100mV");
   show_byte  (0x047f, "dcv trigger offset 1V");
   show_byte  (0x0480, "dcv trigger offset 10V");
   show_byte  (0x0481, "dcv trigger offset 100V");
   show_byte  (0x0482, "dcv trigger offset 1000V");
   show_byte  (0x0483, "filler byte");
   show_double(0x0484, "acdcv sync offset 10mV");
   show_double(0x048c, "acdcv sync offset 100mV");
   show_double(0x0494, "acdcv sync offset 1V");
   show_double(0x049c, "acdcv sync offset 10V");
   show_double(0x04a4, "acdcv sync offset 100V");
   show_double(0x04ac, "acdcv sync offset 1KV");
   show_double(0x04b4, "acv sync offset 10mV");
   show_double(0x04bc, "acv sync offset 100mV");
   show_double(0x04c4, "acv sync offset 1V");
   show_double(0x04cc, "acv sync offset 10V");
   show_double(0x04d4, "acv sync offset 100V");
   show_double(0x04dc, "acv sync offset 1KV");
   show_double(0x04e4, "acv sync gain 10mV");
   show_double(0x04ec, "acv sync gain 100mV");
   show_double(0x04f4, "acv sync gain 1V");
   show_double(0x04fc, "acv sync gain 10V");
   show_double(0x0504, "acv sync gain 100V");
   show_double(0x050c, "acv sync gain 1KV");
   show_double(0x0514, "ab ratio");
   show_double(0x051c, "gain ratio");
   show_double(0x0524, "acv ana gain 10mV");
   show_double(0x052c, "acv ana gain 100mV");
   show_double(0x0534, "acv ana gain 1V");
   show_double(0x053c, "acv ana gain 10V");
   show_double(0x0544, "acv ana gain 100V");
   show_double(0x054c, "acv ana gain 1KV");
   show_double(0x0554, "acv ana offset 10mV");
   show_double(0x055c, "acv ana offset 100mV");
   show_double(0x0564, "acv ana offset 1V");
   show_double(0x056c, "acv ana offset 10V");
   show_double(0x0574, "acv ana offset 100V");
   show_double(0x057c, "acv ana offset 1KV");
   show_double(0x0584, "rmsdc ratio");
   show_double(0x058c, "sampdc ratio");
   show_double(0x0594, "aci gain");
   show_word  (0x059c, "Cal_Sum1");
   show_double(0x059e, "Cal_59e");
   show_double(0x05a6, "Cal_5a6");
   show_double(0x05ae, "Cal_5ae");
   show_double(0x05b6, "freq gain");
   show_byte  (0x05be, "attenuator high frequency dac");
   show_byte  (0x05bf, "filler byte");
   show_byte  (0x05c0, "amplifier high frequency dac 10mV");
   show_byte  (0x05c1, "amplifier high frequency dac 100mV");
   show_byte  (0x05c2, "amplifier high frequency dac 1V");
   show_byte  (0x05c3, "amplifier high frequency dac 10V");
   show_byte  (0x05c4, "amplifier high frequency dac 100V");
   show_byte  (0x05c5, "amplifier high frequency dac 1KV");
   show_byte  (0x05c6, "interpolator");
   show_byte  (0x05c7, "filler byte");
   show_word  (0x05c8, "Cal_Sum2");
   show_byte  (0x05ca, "Start of ASCII text area");
   show_byte  (0x05cb, "");
   show_byte  (0x05cc, "");
   show_byte  (0x05cd, "");
   show_byte  (0x05ce, "");
   show_byte  (0x05cf, "");
   show_byte  (0x05d0, "");
   show_byte  (0x05d1, "");
   show_byte  (0x05d2, "");
   show_byte  (0x05d3, "");
   show_byte  (0x05d4, "");
   show_byte  (0x05d5, "");
   show_byte  (0x05d6, "");
   show_byte  (0x05d7, "");
   show_byte  (0x05d8, "");
   show_byte  (0x05d9, "");
   show_byte  (0x05da, "");
   show_byte  (0x05db, "");
   show_byte  (0x05dc, "");
   show_byte  (0x05dd, "");
   show_byte  (0x05de, "");
   show_byte  (0x05df, "");
   show_byte  (0x05e0, "");
   show_byte  (0x05e1, "");
   show_byte  (0x05e2, "");
   show_byte  (0x05e3, "");
   show_byte  (0x05e4, "");
   show_byte  (0x05e5, "");
   show_byte  (0x05e6, "");
   show_byte  (0x05e7, "");
   show_byte  (0x05e8, "");
   show_byte  (0x05e9, "");
   show_byte  (0x05ea, "");
   show_byte  (0x05eb, "");
   show_byte  (0x05ec, "");
   show_byte  (0x05ed, "");
   show_byte  (0x05ee, "");
   show_byte  (0x05ef, "");
   show_byte  (0x05f0, "");
   show_byte  (0x05f1, "");
   show_byte  (0x05f2, "");
   show_byte  (0x05f3, "");
   show_byte  (0x05f4, "");
   show_byte  (0x05f5, "");
   show_byte  (0x05f6, "");
   show_byte  (0x05f7, "");
   show_byte  (0x05f8, "");
   show_byte  (0x05f9, "");
   show_byte  (0x05fa, "");
   show_byte  (0x05fb, "");
   show_byte  (0x05fc, "");
   show_byte  (0x05fd, "");
   show_byte  (0x05fe, "");
   show_byte  (0x05ff, "");
   show_byte  (0x0600, "");
   show_byte  (0x0601, "");
   show_byte  (0x0602, "");
   show_byte  (0x0603, "");
   show_byte  (0x0604, "");
   show_byte  (0x0605, "");
   show_byte  (0x0606, "");
   show_byte  (0x0607, "");
   show_byte  (0x0608, "");
   show_byte  (0x0609, "");
   show_byte  (0x060a, "");
   show_byte  (0x060b, "");
   show_byte  (0x060c, "");
   show_byte  (0x060d, "");
   show_byte  (0x060e, "");
   show_byte  (0x060f, "");
   show_byte  (0x0610, "");
   show_byte  (0x0611, "");
   show_byte  (0x0612, "");
   show_byte  (0x0613, "");
   show_byte  (0x0614, "");
   show_byte  (0x0615, "");
   show_byte  (0x0616, "");
   show_byte  (0x0617, "");
   show_byte  (0x0618, "");
   show_byte  (0x0619, "End of ASCII text area");
   show_long  (0x061a, "Calnum");
   show_long  (0x061e, "Cal_SecureCode");
   show_byte  (0x0622, "Cal_AcalSecure");
   show_byte  (0x0623, "filler byte");
   show_word  (0x0624, "Cal_Sum3");
   show_long  (0x0626, "Destructive Overloads");
   show_long  (0x062a, "Defeats");

   show_byte  (0x062e, "filler byte?");
   show_byte  (0x062f, "filler byte?");
   show_hex   (0x0630, "---- filler bytes ----");
   show_hex   (0x0638, "");
   show_hex   (0x0640, "");
   show_hex   (0x0648, "");
   show_hex   (0x0650, "");
   show_hex   (0x0658, "");
   show_hex   (0x0660, "");
   show_hex   (0x0668, "");
   show_hex   (0x0670, "");
   show_hex   (0x0678, "");
   show_hex   (0x0680, "");
   show_hex   (0x0688, "");
   show_hex   (0x0690, "");
   show_hex   (0x0698, "");
   show_hex   (0x06A0, "");
   show_hex   (0x06A8, "");
   show_hex   (0x06B0, "");
   show_hex   (0x06B8, "");
   show_hex   (0x06C0, "");
   show_hex   (0x06C8, "");
   show_hex   (0x06D0, "");
   show_hex   (0x06D8, "");
   show_hex   (0x06E0, "");
   show_hex   (0x06E8, "");
   show_hex   (0x06F0, "");
   show_hex   (0x06F8, "");
   show_hex   (0x0700, "");
   show_hex   (0x0708, "");
   show_hex   (0x0710, "");
   show_hex   (0x0718, "Cal ram checksum/protection flags");
   show_hex   (0x0720, "");
   show_hex   (0x0728, "");
   show_hex   (0x0730, "");
   show_hex   (0x0738, "");
   show_hex   (0x0740, "");
   show_hex   (0x0748, "");
   show_hex   (0x0750, "");
   show_hex   (0x0758, "");
   show_hex   (0x0760, "");
   show_hex   (0x0768, "");
   show_hex   (0x0770, "");
   show_hex   (0x0778, "");
   show_hex   (0x0780, "");
   show_hex   (0x0788, "");
   show_hex   (0x0790, "");
   show_hex   (0x0798, "");
   show_hex   (0x07A0, "");
   show_hex   (0x07A8, "");
   show_hex   (0x07B0, "");
   show_hex   (0x07B8, "");
   show_hex   (0x07C0, "");
   show_hex   (0x07C8, "");
   show_hex   (0x07D0, "");
   show_hex   (0x07D8, "");
   show_hex   (0x07E0, "");
   show_hex   (0x07E8, "");
   show_hex   (0x07F0, "");
   show_hex   (0x07F8, "");

   fprintf(f, "\n\n");
}


void main(int argc, char *argv[])
{
int i;
int key;
char s[256+1];
char in_name[256+1];
char fn[256+1];
char *dot;
unsigned long a;
u16 sum0, sum1, sum2, sum3;
u16 f0,   f1,   f2,   f3;

   key = ' ';
   s[0] = 0;

   for(i=1; i<argc; i++) {  /* process the command options */
      if((argv[i][0] == '/') || (argv[i][0] == '-')) {
         key = option_switch(argv[i]);
         if(key) {
            help:
            printf("HP3458A NVRAM data dumper program. Version 1.10\n");
            printf("   Who knows if this thing really works?\n");
            printf("   Trust, but verify... use at your own risk.\n");
            printf("\n");
            printf("Program usage:\n");
            printf("   HP3458 file_name /option /option ...\n");
            printf("   (ROM image binary data is written to file_name.hi and file_name.lo)\n");
            printf("   (text data is written to file_name)\n");
            printf("\n");
            printf("Valid opations are:\n");
            printf("   /a=addr - use GPIB device address *addr* (default is 22)\n");
            printf("   /d      - dump DATA rams (default is CAL ram)\n");
            printf("   /e=#    - set end-of-string char (default=13, 0=none)\n");
            printf("   /o[=#]  - reset destructive overload counter in CAL RAM image\n");
            printf("   /x      - ignore GPIB errors.\n");
            exit(6);
         }
      }
      else {
         strcpy(s, argv[i]);
      }
   }

   if(s[0] == 0) {
      key = ' ';
      printf("ERROR: no output file name was given!\n");
      goto help;
   }

   if(cal_ram) {   // set address ranges to dump
      START  = 393216L;   // CAL ram data (in high byte)
      END    = 397312L;
   }
   else {
      START = 0x120000L;  // SRAM data
      END   = 0x130000L;
   }

   if(!strchr(s, '.')) {
      if(cal_ram) strcat(s, ".CAL");
      else        strcat(s, ".DAT");
   }

   f = fopen(s, "w");
   if(f == 0) {
      key = ' ';
      fprintf(stderr, "ERROR: could not open output file: %s\n", s);
      goto help;
   }
   strcpy(in_name, s);

   strcpy(fn, s);
   dot = strstr(fn, ".");
   if(dot) strcpy(dot, ".HI");
   else    strcat(fn, ".HI");
   hi = fopen(fn, "wb");
   if(hi == 0) {
      key = ' ';
      fprintf(stderr, "ERROR: could not open high byte ROM binary output file: %s\n", fn);
      goto help;
   }

   if(cal_ram == 0) {
      strcpy(fn, s);
      dot = strstr(fn, ".");
      if(dot) strcpy(dot, ".LO");
      else    strcat(fn, ".LO");
      lo = fopen(fn, "wb");
      if(lo == 0) {
         key = ' ';
         fprintf(stderr, "ERROR: could not open low byte ROM binary output file: %s\n", fn);
         goto help;
      }
   }


   if(!GPIB_connect(GPIB_ADDR, GPIB_error, 0, 10000)) {
      fprintf(stderr, "ERROR: could not connect to GPIB device at address %d\n", GPIB_ADDR);
      if(err_exit) exit(7);
   }
   if(eos_char) {  // set end-of-string char
      GPIB_set_EOS_mode(eos_char);
   }

   get_nvram();       // get the NVRAM data

   GPIB_disconnect();


   fprintf(stderr, "Making %ld byte rom files...\n", romp);

   if(reset_ovl >= 0) {  // patch destructive overload counter dword
      fprintf(f, "\n\nPatched Destructive Overload counter to %ld\n\n", reset_ovl);
      hi_rom[0x0626+0] = (reset_ovl >> 24) & 0xFF;
      hi_rom[0x0626+1] = (reset_ovl >> 16) & 0xFF;
      hi_rom[0x0626+2] = (reset_ovl >>  8) & 0xFF;
      hi_rom[0x0626+3] = (reset_ovl >>  0) & 0xFF;
   }

   for(a=0; a<romp; a++) {  // write the ROM image files
//      printf("%06lX %02X %02X\n", a, hi_rom[a], lo_rom[a]);
      if(hi) fprintf(hi, "%c", hi_rom[a]);
      if(lo) fprintf(lo, "%c", lo_rom[a]);
   }

   if(cal_ram) {
      if(romp != 2048L) {
         printf("ERROR: CAL ram data count not 2048.\n");
         fprintf(f, "ERROR: CAL ram data count not 2048.\n");
         fprintf(stderr, "ERROR: CAL ram data count not 2048.\n");
      }
   }
   else {
      if(romp != 32768L) {
         printf("ERROR: DATA ram data count not 32768.\n");
         fprintf(f, "ERROR: DATA ram data count not 32768.\n");
         fprintf(stderr, "ERROR: DATA ram data count not 32768.\n");
      }
   }

   if(cal_ram) {     // verify checksums
      show_vars();

      sum0 = crx(0x0, 0x40) + crx(0x60, 0x1bc);
      sum1 = crx(0x40, 0x60) + crx(0x1c0, 0x59c);
      sum2 = crx(0x5a0, 0x5c8);
      sum3 = crx(0x5ca, 0x624);

      f0 = fword(0x1BC);
      f1 = fword(0x59C);
      f2 = fword(0x5C8);
      f3 = fword(0x624);

      fprintf(f, "check0:%04X  sum0: %04X\n", f0, sum0);
      fprintf(f, "check1:%04X  sum1: %04X\n", f1, sum1);
      fprintf(f, "check2:%04X  sum2: %04X\n", f2, sum2);
      fprintf(f, "check3:%04X  sum3: %04X\n", f3, sum3);

      if((f0 != sum0) || (f1 != sum1) || (f2 != sum2) || (f3 != sum3)) {
         printf("Data checksums DO NOT MATCH!!!\n");
         fprintf(f, "Data checksums DO NOT MATCH!!!\n");
         fprintf(stderr, "Data checksums DO NOT MATCH!!!\n");
      }
      else {
         printf("Checksums OK!\n");
         fprintf(f, "Checksums OK!\n");
         fprintf(stderr, "Checksums OK!\n");
      }
   }

   if(lo) fclose(lo);
   if(hi) fclose(hi);
   if(f)  fclose(f);

   if(hi_rom) hfree(hi_rom);
   if(lo_rom) hfree(lo_rom);

   fprintf(stderr, "Done!\n");
   exit(0);
}
