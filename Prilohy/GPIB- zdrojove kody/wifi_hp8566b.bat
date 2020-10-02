@echo off

rem Initialize HP 8566B, then select 2.4 GHz band, 5 dB/division, -35 dBm reference level,
rem 1 MHz RBW, 0 dB of RF attenuation, using positive-peak detection and fast GPIB operation

talk 18 "IP;FA 2350 MHz;FB 2550 MHZ;LG 5 DB;RL -35 DB;RB 1 MHZ;AT 0 DB;KSb;KSS;"

rem Launch SSM using analyzer at GPIB address 18, trace max accumulation, fast acquisition,
rem discrete (unsmoothed) colors in waterfall display, and WiFi band-boundary markers
rem Also disable GPIB timeout checking for better reliability on slow TCP/IP connections

start ssm 18 -M -f -cs -w -t

