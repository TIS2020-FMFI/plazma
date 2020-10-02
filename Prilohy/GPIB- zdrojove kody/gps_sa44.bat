@echo off

rem Launch SSM using Signal Hound USB-SA44/B, trace max accumulation,
rem discrete (unsmoothed) colors in waterfall display, and GPS band-boundary 
rem markers enabled
rem 
rem Save traces at 2 dB/division, 30 divisions total, reference level -60 dBm
rem
rem Log location data from NMEA receiver at COM5
rem Use gpscolors.bin palette

start ssm -sa44 -g -M -cs -CF:1575.42E6 -span:6E6 -divs:30 -logdiv:2 -RL:-60 -nmea:com5 -pal:"gpscolors.bin" 

