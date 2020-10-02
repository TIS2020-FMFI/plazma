@echo off

rem R&S ESM 500A control batch file

rem Disable beep
talk 1 "V0"

if %1i == i goto help
if %1 == on goto on
if %1 == off goto off

rem Set frequency in MHz, or send other command
talk 1 %1 %2 %3 %4 %5 %6 %7 %8 %9
goto bail

:off
rem Squelch at maximum, AF filter on
talk 1 "C1 80 D1"
goto bail

:on
rem Squelch=manual control, AF filter off
talk 1 "D0 C8"
goto bail

:help
echo Usage: esm [on] [off] [Ffreq_in_MHz] [I1=AM] [I2=FM] [W2=15 kHz] [W4=100 kHz]
echo.
:bail


