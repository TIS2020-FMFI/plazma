@echo off

rem
rem Launch SSM using analyzer at GPIB address 3, trace max accumulation, fast acquisition,
rem discrete (unsmoothed) colors in waterfall display, and WiFi band-boundary markers
rem Also disable GPIB timeout checking for better reliability on slow TCP/IP connections
rem
rem At startup time, initialize the Tek 49x analyzer, then select the 2.4 GHz band, 5 dB/division, 
rem -35 dBm reference level, 1 MHz RBW, and turn the readout off to help avoid screen burn-in during
rem unattended recording
rem
rem Finally, before exiting SSM, turn the readout back on
rem
rem This example illustrates the use of the -connect and -disconnect command line options, as well as
rem how to use the talk.exe utility to send a setup command string.  The setup string could
rem also have been passed to SSM via the -startup command-line option.  However, using -connect 
rem and -disconnect to control the analyzer's readout not only works every time the connection is 
rem toggled, but also allows the batch file to exit cleanly after launching SSM
rem 

talk 3 "INIT;FREQ 2450 MHZ;SPAN 20 MHZ;VRTDSP LOG:5;REFLVL -35 DBM;RES 1 MHZ;"
start ssm 3 -M -f -cs -w -t -connect:"REDOUT OFF;" -disconnect:"REDOUT ON;"

