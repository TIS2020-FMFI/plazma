@echo off

rem Launch SSM using Signal Hound USB-SA44/B or USB-SA124A, trace max 
rem accumulation on, discrete (unsmoothed) colors in waterfall display, 
rem and WiFi band-boundary markers

start ssm -sa44 -w -M -cs -CF:2437E6 -span:80E6 -bins:64 -divs:20 -logdiv:5 -RL:-20
