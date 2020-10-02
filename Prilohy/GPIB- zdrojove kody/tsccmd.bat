@echo off
if %1i == i goto usage
if %2i == i goto noaddr

set TSCIPADDR=%1
set TSCCMD=%2
goto xmit

:noaddr
set TSCIPADDR=192.168.1.225
set TSCCMD=%1

:xmit
if %TSCTIMEOUTMS%i == i goto defaulttime
goto indefinite

rem
rem Send command and wait 500 milliseconds for a response
rem

:defaulttime
tcplist %TSCIPADDR%:1299 10 0 500 %TSCCMD%
goto bail

rem
rem Send command and wait for response line(s) with specified timeout 
rem

:indefinite
tcplist %TSCIPADDR%:1299 10 0 %TSCTIMEOUTMS% %TSCCMD%
goto bail

:usage
echo Batch file to control Symmetricom TSC 5115A/5120A/5125A test set via
echo TCP/IP port 1299
echo.
echo Usage: tsccmd [address] command
echo.
echo Some available commands:
echo   help
echo   start
echo   stop
echo   reset
echo   prompt
echo   restorefactorydefaults
echo   set (option)
echo   help set
echo   show (option)
echo   help show
echo.
echo Examples:
echo.
echo   tsccmd start
echo.
echo   tsccmd "button 6"
echo.
echo   set TSCTIMEOUTMS=100
echo   tsccmd "help show"
echo.
echo   set TSCTIMEOUTMS=500
echo   tsccmd "show spectrum"
echo.
echo   tsccmd "set phaserate 10"
echo   tcplist %TSCIPADDR%:1298 10 0 10000
echo.
echo Notes:
echo   - Address defaults to 192.168.1.225 if not specified
echo.
echo   - After the command is transmitted, tsccmd will display all text 
echo     received in response until a 500-millisecond timeout period elapses
echo     after the last incoming line.  You can set TSCTIMEOUTMS to another
echo     timeout value if desired, or 0 if you want to monitor response traffic
echo     until ESC is pressed
echo.
echo   - The IP address used to send the last TSC command will be stored in
echo     the environment variable TSCIPADDR.  You can use a command such as
echo     'tcplist %TSCIPADDR%:1298 10 0 10000' (without the quotes) to receive
echo     phase-difference data after starting the acquisition with tsccmd.  
echo     (This example will display incoming data until a 10-second timeout 
echo     period expires or ESC is pressed.)
echo.
:bail
