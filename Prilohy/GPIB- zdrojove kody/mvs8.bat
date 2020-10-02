@echo off

rem
rem Change line below to call the vcvarsall.bat file in your own VS8 installation
rem directory tree
rem

call "\program files\microsoft visual studio 8\vc\vcvarsall" x86

nmake /f project.mak
