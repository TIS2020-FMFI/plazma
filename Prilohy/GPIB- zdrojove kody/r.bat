@echo off
call setset c14
call m.bat -a
touch gpib.iss
nmake output\setup.exe
