@echo off

rem
rem Request .bmp file from Tektronix TDS-series oscilloscope
rem at specified address
rem
rem Use 3000 ms timeout
rem

if %1i == i goto usage
if %2i == i goto usage

talk %1 "HARDC:FORM BMPCOLOR"
talk %1 "HARDC:LAYOUT PORTRAIT"
rem talk %1 "HARDC:PALETTE HARDCOPY"
talk %1 "HARDC:PALETTE CURRENT"
rem talk %1 "HARDC:PORT GPIB"

binquery %1 "HARDCOPY START" %2 3000
goto bail

:usage
echo.
echo Retrieves a color .BMP file from a Tektronix TDS-series
echo oscilloscope
echo.
echo Usage: tdsbmp GPIB_addr filename
echo.
echo Example: tdsbmp 3 test.bmp
echo.
:bail
