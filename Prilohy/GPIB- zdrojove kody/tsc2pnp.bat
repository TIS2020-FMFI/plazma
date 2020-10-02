@echo off

if %1i == i goto help
if %2i == i goto default

goto launch

:help
echo Usage: tsc2pnp IP address:port filename.pnp
goto bail

rem Call TCPLIST.EXE to fetch a series of text blocks from the TSC5115A or TSC512XA
rem at the specified IP address

:default
tcplist ke5fx.dyndns.org:1299 10 0 2000 "show title" 1000 >temp.txt
tcplist ke5fx.dyndns.org:1299 10 0 2000 "show date" 1000 >>temp.txt
tcplist ke5fx.dyndns.org:1299 10 0 2000 "show version" 1000 >>temp.txt
tcplist ke5fx.dyndns.org:1299 10 0 2000 "show inputs" 1000 >>temp.txt
tcplist ke5fx.dyndns.org:1299 10 0 2000 "show spectrum" 1000 >>temp.txt
txt2pnp temp.txt %1
goto bail

:launch
tcplist %1:1299 10 0 2000 "show title" 1000 >temp.txt
tcplist %1:1299 10 0 2000 "show date" 1000 >>temp.txt
tcplist %1:1299 10 0 2000 "show version" 1000 >>temp.txt
tcplist %1:1299 10 0 2000 "show inputs" 1000 >>temp.txt
tcplist %1:1299 10 0 2000 "show spectrum" 1000 >>temp.txt
txt2pnp temp.txt %2

:bail
