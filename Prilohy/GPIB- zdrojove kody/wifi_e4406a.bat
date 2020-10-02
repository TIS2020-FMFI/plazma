@echo off

rem Initialize E4406A, then select 10 MHz-wide span center of WiFi channel 1 in 2.4 GHz band, 
rem -40 dBm reference level, 5 dB/div
rem
rem (Ch 1 centered at 2412 MHz, Ch 6 centered at 2437 MHz, and Ch 11 centered at 2462 MHz)

query 18 ":SYST:PRES; :SENS:FREQ:CENT 2412 MHz; :SENS:SPEC:FREQ:SPAN 10 MHz; :DISP:SPEC1:WIND1:TRAC:Y:SCAL:RLEV -40; :DISP:SPEC1:WIND1:TRAC:Y:SCAL:PDIV 5; *OPC?" >nul

rem Launch SSM using SCPI analyzer at GPIB address 18, trace max accumulation, fast acquisition,
rem discrete (unsmoothed) colors in waterfall display, and WiFi band-boundary markers
rem Also disable GPIB timeout checking for better reliability on slow TCP/IP connections

start ssm 18 -scpi -M -f -cs -w -t

