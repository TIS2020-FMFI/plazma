@echo off
taskkill /IM tcheck.exe
taskkill /IM vna.exe
taskkill /F /IM ssm.exe
cd gpiblib
call m.bat %1
cd ..
copy gpiblib\gpiblib.dll
copy gpiblib\gpiblib.lib
copy gpiblib\gpiblib.h

if exist c:\dev\sdcc\sdcc.ico goto inhouse_build
nmake /f makefile %1
goto bail

:inhouse_build
call setset c14
nmake /f makefile %1
:bail

