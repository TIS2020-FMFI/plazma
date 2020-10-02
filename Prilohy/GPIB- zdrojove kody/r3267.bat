@echo off

rem
rem Set EOI termination (so our .bmp file doesn't end up
rem with 0D 0A terminators)
rem

talk 7 "DL2"

rem
rem Request .bmp file from Advantest R3264/R3267/R3273
rem spectrum analyzer at address 7
rem

binquery 7 "HCIMAG SCOL;HCCMPRS OFF;BMP?" bitmap.bmp

rem
rem Restore normal CR/LF/EOI termination, so future ASCII queries
rem will be terminated properly
rem

talk 7 "DL0"
