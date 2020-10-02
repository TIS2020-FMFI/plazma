@echo off
if %1i == i goto usage

rem Set HP 5071A clock from pool.ntp.org
rem (assumes Thunderbolt 1pps is connected to rear panel SYNC IN jack)

5071a %1 2400 %2 pool.ntp.org rear

goto bail

:usage
echo Usage: set5071a COM_port [cson / csoff]
:bail
