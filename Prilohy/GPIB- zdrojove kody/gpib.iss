; -- GPIB.iss --
; Inno Setup script file for KE5FX GPIB Toolkit installation
;
; See www.jrsoftware.com/isinfo.php for more information on
; Inno Setup
;

[Setup]
AppName=GPIB Toolkit
AppVerName=GPIB Toolkit Version 1.981 of 26-Mar-20
DefaultDirName={pf}\KE5FX\GPIB
DefaultGroupName=KE5FX GPIB Toolkit
UninstallDisplayIcon={app}\pn.exe
Compression=lzma
SolidCompression=yes
ChangesAssociations=yes

[Registry]
Root: HKCR; Subkey: ".ssm"; ValueType: string; ValueName: ""; ValueData: "KE5FXSSMFile"; Flags: uninsdeletevalue; Tasks: associate
Root: HKCR; Subkey: "KE5FXSSMFile"; ValueType: string; ValueName: ""; ValueData: "Spectrum recording file"; Flags: uninsdeletekey; Tasks: associate 
Root: HKCR; Subkey: "KE5FXSSMFile\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\SSM.EXE,0"; Tasks: associate 
Root: HKCR; Subkey: "KE5FXSSMFile\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\SSM.EXE"" ""%1"""; Tasks: associate 

Root: HKCR; Subkey: ".pnp"; ValueType: string; ValueName: ""; ValueData: "KE5FXPNPFile"; Flags: uninsdeletevalue; Tasks: associate 
Root: HKCR; Subkey: "KE5FXPNPFile"; ValueType: string; ValueName: ""; ValueData: "Phase noise plot file"; Flags: uninsdeletekey; Tasks: associate 
Root: HKCR; Subkey: "KE5FXPNPFile\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\PN.EXE,0"; Tasks: associate 
Root: HKCR; Subkey: "KE5FXPNPFile\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\PN.EXE"" ""%1"""; Tasks: associate 

Root: HKCR; Subkey: ".plt"; ValueType: string; ValueName: ""; ValueData: "KE5FXPLTFile"; Flags: uninsdeletevalue; Tasks: associate 
Root: HKCR; Subkey: "KE5FXPLTFile"; ValueType: string; ValueName: ""; ValueData: "HP-GL plotter file"; Flags: uninsdeletekey; Tasks: associate 
Root: HKCR; Subkey: "KE5FXPLTFile\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\7470.EXE,0"; Tasks: associate 
Root: HKCR; Subkey: "KE5FXPLTFile\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\7470.EXE"" ""%1"""; Tasks: associate 

Root: HKCR; Subkey: ".pgl"; ValueType: string; ValueName: ""; ValueData: "KE5FXPGLFile"; Flags: uninsdeletevalue; Tasks: associat2
Root: HKCR; Subkey: "KE5FXPGLFile"; ValueType: string; ValueName: ""; ValueData: "HP-GL plotter file"; Flags: uninsdeletekey; Tasks: associat2
Root: HKCR; Subkey: "KE5FXPGLFile\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\7470.EXE,0"; Tasks: associat2
Root: HKCR; Subkey: "KE5FXPGLFile\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\7470.EXE"" ""%1"""; Tasks: associat2

Root: HKCR; Subkey: ".hgl"; ValueType: string; ValueName: ""; ValueData: "KE5FXHGLFile"; Flags: uninsdeletevalue; Tasks: associat2
Root: HKCR; Subkey: "KE5FXHGLFile"; ValueType: string; ValueName: ""; ValueData: "HP-GL plotter file"; Flags: uninsdeletekey; Tasks: associat2
Root: HKCR; Subkey: "KE5FXHGLFile\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\7470.EXE,0"; Tasks: associat2
Root: HKCR; Subkey: "KE5FXHGLFile\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\7470.EXE"" ""%1"""; Tasks: associat2

Root: HKCR; Subkey: ".hpg"; ValueType: string; ValueName: ""; ValueData: "KE5FXHPGFile"; Flags: uninsdeletevalue; Tasks: associat2
Root: HKCR; Subkey: "KE5FXHPGFile"; ValueType: string; ValueName: ""; ValueData: "HP-GL plotter file"; Flags: uninsdeletekey; Tasks: associat2
Root: HKCR; Subkey: "KE5FXHPGFile\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\7470.EXE,0"; Tasks: associat2
Root: HKCR; Subkey: "KE5FXHPGFile\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\7470.EXE"" ""%1"""; Tasks: associat2

[Files]
Source: "readme.htm";   DestDir: "{app}"; Flags: isreadme
Source: "faq.htm";   DestDir: "{app}"; Flags: isreadme
Source: "gpib.iss";  DestDir: "{app}"
Source: "makefile";  DestDir: "{app}"  
Source: "r.bat";  DestDir: "{app}"  
Source: "m.bat";  DestDir: "{app}"  
Source: "esm.bat";  DestDir: "{app}"  
Source: "mvs8.bat";  DestDir: "{app}"  
Source: "wifi_hp8566b.bat";  DestDir: "{app}"  
Source: "wifi_e4406a.bat";  DestDir: "{app}"  
Source: "wifi_tek490.bat";  DestDir: "{app}"  
Source: "gps_sa44.bat"; DestDir: "{app}"
Source: "wifi_sa44.bat"; DestDir: "{app}"
Source: "r3267.bat";  DestDir: "{app}"  
Source: "tdsbmp.bat";  DestDir: "{app}"  
Source: "IMD_8566B_8568B.bat"; DestDir: "{app}"
Source: "tsccmd.bat";  DestDir: "{app}"  
Source: "tsc2pnp.bat"; DestDir: "{app}"
Source: "set5071a.bat"; DestDir: "{app}"

Source: "5071a.exe";  DestDir: "{app}"
Source: "5345a.exe";  DestDir: "{app}"
Source: "5370.exe";  DestDir: "{app}"
Source: "7470.exe";  DestDir: "{app}"
Source: "vna.exe";  DestDir: "{app}"
Source: "tcheck.exe";  DestDir: "{app}"
Source: "8672a.exe";  DestDir: "{app}"
Source: "11848a.exe";  DestDir: "{app}"
Source: "dts2070.exe";  DestDir: "{app}"
Source: "psaplot.exe";  DestDir: "{app}"
Source: "dso6000.exe";  DestDir: "{app}"
Source: "hp3458.exe";  DestDir: "{app}"
Source: "atten_3488a.exe";  DestDir: "{app}"
Source: "txt2pnp.exe"; DestDir: "{app}"
Source: "pn.exe";  DestDir: "{app}"
Source: "ssm.exe";  DestDir: "{app}"
Source: "t962.exe";  DestDir: "{app}"
Source: "mx20.exe";  DestDir: "{app}"
Source: "ssmdump.exe";  DestDir: "{app}"
Source: "satrace.exe";  DestDir: "{app}"
Source: "gps.exe";  DestDir: "{app}"
Source: "sgentest.exe";  DestDir: "{app}"
Source: "dataq.exe";  DestDir: "{app}"
Source: "cal_tek490.exe";  DestDir: "{app}"
Source: "prologix.exe"; DestDir: "{app}"
Source: "parse.exe";  DestDir: "{app}"
Source: "talk.exe";  DestDir: "{app}"
Source: "listen.exe";  DestDir: "{app}"
Source: "query.exe";  DestDir: "{app}"
Source: "binquery.exe";  DestDir: "{app}"
Source: "tcplist.exe";  DestDir: "{app}"
Source: "visadmm.exe";  DestDir: "{app}"
Source: "ntpquery.exe";  DestDir: "{app}"
Source: "printhpg.exe";  DestDir: "{app}"
Source: "echoclient.exe";  DestDir: "{app}"
Source: "echoserver.exe";  DestDir: "{app}"
Source: "hound2pipe.exe";  DestDir: "{app}"

Source: "gpiblib.dll";   DestDir: "{app}"
Source: "specan.dll";   DestDir: "{app}"
Source: "gnss.dll";   DestDir: "{app}"
Source: "w32sal.dll";   DestDir: "{app}"
Source: "winvfx16.dll"; DestDir: "{app}"
Source: "sh_api.dll"; DestDir: "{app}"
Source: "ftd2xx.dll"; DestDir: "{app}"

Source: "7470.ini"; DestDir: "{app}"
Source: "pn.ini"; DestDir: "{app}"
Source: "serial.ini"; DestDir: "{app}"
Source: "beep.wav"; DestDir: "{app}"
Source: "colors.bin"; DestDir: "{app}"
Source: "gpscolors.bin"; DestDir: "{app}"
Source: "default_connect.ini"; DestDir: "{app}"
Source: "default_7470.ini"; DestDir: "{app}"
Source: "default_7470user.ini"; DestDir: "{app}"
Source: "default_pn.ini"; DestDir: "{app}"
Source: "default_colors.bin"; DestDir: "{app}"

Source: "5071a.cpp";  DestDir: "{app}"
Source: "5345a.cpp";  DestDir: "{app}"
Source: "5370.cpp";  DestDir: "{app}"
Source: "7470.cpp";  DestDir: "{app}"
Source: "vna.cpp";  DestDir: "{app}"
Source: "tcheck.cpp";  DestDir: "{app}"
Source: "renderer.cpp"; DestDir: "{app}"
Source: "7470lex.cpp";  DestDir: "{app}"
Source: "8672a.cpp";  DestDir: "{app}"
Source: "11848a.cpp";  DestDir: "{app}"
Source: "psaplot.cpp";  DestDir: "{app}"
Source: "dts2070.cpp";  DestDir: "{app}"
Source: "dso6000.cpp";  DestDir: "{app}"
Source: "hp3458.cpp";  DestDir: "{app}"
Source: "atten_3488a.cpp";  DestDir: "{app}"
Source: "txt2pnp.cpp";  DestDir: "{app}"
Source: "pn.cpp";  DestDir: "{app}"
Source: "ssm.cpp";  DestDir: "{app}"
Source: "ssmdump.cpp";  DestDir: "{app}"
Source: "satrace.cpp";  DestDir: "{app}"
Source: "t962.cpp";  DestDir: "{app}"
Source: "mx20.cpp";  DestDir: "{app}"
Source: "gps.cpp";  DestDir: "{app}"
Source: "sgentest.cpp";  DestDir: "{app}"
Source: "specan.cpp";  DestDir: "{app}"
Source: "gnss.cpp";  DestDir: "{app}"
Source: "dataq.cpp";  DestDir: "{app}"
Source: "cal_tek490.cpp"; DestDir: "{app}"
Source: "prologix.cpp"; DestDir: "{app}"
Source: "parse.cpp";  DestDir: "{app}"
Source: "talk.cpp";  DestDir: "{app}"
Source: "listen.cpp";  DestDir: "{app}"
Source: "query.cpp";  DestDir: "{app}"
Source: "binquery.cpp";  DestDir: "{app}"
Source: "tcplist.cpp";  DestDir: "{app}"
Source: "ntpquery.cpp";  DestDir: "{app}"
Source: "printhpg.cpp";  DestDir: "{app}"
Source: "echoserver.cpp"; DestDir: "{app}"
Source: "echoclient.cpp";  DestDir: "{app}"
Source: "appfile.cpp"; DestDir: "{app}"
Source: "xy.cpp";  DestDir: "{app}"
Source: "recorder.cpp";  DestDir: "{app}"
Source: "waterfall.cpp";  DestDir: "{app}"
Source: "spline.cpp";  DestDir: "{app}"
Source: "gfxfile.cpp";  DestDir: "{app}"
Source: "ipconn.cpp";  DestDir: "{app}"
Source: "timeutil.cpp";  DestDir: "{app}"
Source: "comport.cpp";  DestDir: "{app}"
Source: "di154.cpp";  DestDir: "{app}"
Source: "visadmm.cpp";  DestDir: "{app}"

Source: "pnres.h";   DestDir: "{app}" 
Source: "tires.h";   DestDir: "{app}" 
Source: "ssmres.h";   DestDir: "{app}" 
Source: "7470res.h";   DestDir: "{app}" 
Source: "prores.h";   DestDir: "{app}" 
Source: "gpiblib.h";    DestDir: "{app}" 
Source: "dsplib.h";    DestDir: "{app}" 
Source: "specan.h";    DestDir: "{app}" 
Source: "gnss.h";    DestDir: "{app}" 
Source: "8566plt.h";   DestDir: "{app}" 
Source: "49xplt.h";   DestDir: "{app}" 
Source: "recorder.h";   DestDir: "{app}" 
Source: "typedefs.h";   DestDir: "{app}" 
Source: "chartype.h";   DestDir: "{app}"
Source: "stdtpl.h"; DestDir: "{app}"
Source: "w32sal.h";   DestDir: "{app}" 
Source: "winvfx.h";   DestDir: "{app}" 
Source: "xy.h";   DestDir: "{app}" 
Source: "waterfall.h";   DestDir: "{app}" 
Source: "sparams.cpp";   DestDir: "{app}" 
Source: "shapi.h";   DestDir: "{app}" 
Source: "stdafx.h";   DestDir: "{app}" 
Source: "visa.h";   DestDir: "{app}" 
Source: "visatype.h";   DestDir: "{app}" 

Source: "pn.ico";  DestDir: "{app}"  
Source: "pn.rc";   DestDir: "{app}" 
Source: "pn.res";   DestDir: "{app}" 
Source: "7470.ico";  DestDir: "{app}"  
Source: "7470.rc";   DestDir: "{app}" 
Source: "7470.res";   DestDir: "{app}" 
Source: "ssm.ico";  DestDir: "{app}"  
Source: "ssm.rc";   DestDir: "{app}" 
Source: "ssm.res";   DestDir: "{app}"
Source: "vna.ico";  DestDir: "{app}"  
Source: "vna.rc";   DestDir: "{app}" 
Source: "vna.res";   DestDir: "{app}" 
Source: "vnares.h";   DestDir: "{app}" 
Source: "tchkres.h";   DestDir: "{app}" 
Source: "tcheck.ico";  DestDir: "{app}"  
Source: "tcheck.rc";   DestDir: "{app}" 
Source: "tcheck.res";   DestDir: "{app}" 
Source: "gpib.ico";  DestDir: "{app}"  
Source: "prologix.rc";   DestDir: "{app}" 
Source: "prologix.res";  DestDir: "{app}"

Source: "gpiblib.lib";  DestDir: "{app}"  
Source: "winvfx16.lib";   DestDir: "{app}" 
Source: "w32sal.lib";   DestDir: "{app}" 
Source: "specan.lib";   DestDir: "{app}" 
Source: "gnss.lib";   DestDir: "{app}" 
Source: "sh_api.lib";   DestDir: "{app}" 
Source: "agvisa32.lib";   DestDir: "{app}" 

Source: "7470.gif"; DestDir: "{app}"
Source: "prologix.gif"; DestDir: "{app}"
Source: "acq.gif"; DestDir: "{app}"
Source: "pn.gif"; DestDir: "{app}"
Source: "11729c.gif"; DestDir: "{app}"
Source: "lan_edit.png"; DestDir: "{app}"
Source: "ssmopt.gif"; DestDir: "{app}"
Source: "ssm.gif"; DestDir: "{app}"
Source: "vna.gif"; DestDir: "{app}"
Source: "gpiberr.gif"; DestDir: "{app}"
Source: "7470.htm"; DestDir: "{app}"
Source: "pn.htm"; DestDir: "{app}"
Source: "ssm.htm"; DestDir: "{app}"
Source: "tcheck.pdf"; DestDir: "{app}"

Source: "Composite noise baseline plots\2784.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\492p.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\492bp.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\494ap.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\494p.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\8560a.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\8560e.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\8560e wide.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\8563e.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\8566b 2430.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\8566b 2528.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\8566b 2639.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\8566b 2747.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\8568b VT.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\8568b W1QG.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\8568a 2216.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\8568a 2338.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\8596e.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\70900a.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\E4401B.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\E4402B.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\E4406A.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\E4440A.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\3585a.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\3588a.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\4195a.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\4396a.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\N9923A.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\R3132.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\R3131.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\R3267.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\R3361A.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\R3463.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\R3465.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\TR4172.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\FSIQ.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\FSP.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\FSP_no_FFT.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\limit_mask_example.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\494ap RF floor.pnp"; DestDir: "{app}\Composite noise baseline plots"
Source: "Composite noise baseline plots\8566b 2747 RF floor.pnp"; DestDir: "{app}\Composite noise baseline plots"

Source: "Sample S2P files\8753C_35mm_cal.S2P"; DestDir: "{app}\Sample S2P files"
Source: "Sample S2P files\N9923A_SOLT12_cal_FF41_kit.S2P"; DestDir: "{app}\Sample S2P files"

Source: "Sample HPGL files\Advantest R3361A.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\Advantest R3361A lin.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\Advantest R3463.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\Advantest R3762AH.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\Advantest R4131B.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\Advantest R9211C.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\Advantest R9211E.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\Advantest TR4172.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\Agilent Data Capture.hpg"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\Bruel & Kjaer 2012.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\CISPR.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\Gigatronics 8003.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\Gould 4072.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\HP 3040A.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\HP 54110D graticule.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\HP 54110D trace.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\HP 54201A.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\HP 54502A.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\HP 8510.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\HP 8510C.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\HP 8510C quad.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\HP 853A and 8559A.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\HP 8560A.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\HP 8562A.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\HP 8563E.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\HP 8566B.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\HP 8569B.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\HP 8591E.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\HP 8591E limit line.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\HP 8591EM.plt"; DestDir: "{app}\Sample HPGL files" 
Source: "Sample HPGL files\HP 8594Q.plt"; DestDir: "{app}\Sample HPGL files" 
Source: "Sample HPGL files\HP 8714C.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\HP 8752C.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\HP 8753A.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\HP 8753A Smith chart.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\HP 8756A.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\HP 8757A.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\HP 8970B.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\HP 3560A.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\HP 3561A.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\HP 3562A.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\HP 3577A.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\HP 3588A.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\HP 3585A.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\HP 3585A via Agilent utility.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\HP 4195A.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\HP 4195A Smith chart.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\HP 5372A.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\HP 70906.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\HP 70820A 70004A 71500a.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\HP 85671A.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\HP 87510A.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\LeCroy 9354AM.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\Marconi 6500_6311.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\Maury Microwave MT2075.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\R&S FSA.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\R&S UPL 1.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\R&S UPL 2.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\Tektronix 2430A.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\Tektronix 2710.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\Tektronix 2784.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\Tektronix 370.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\Tektronix 370A.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\Tektronix 492P.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\Tektronix 494AP.plt"; DestDir: "{app}\Sample HPGL files" 
Source: "Sample HPGL files\Tektronix 494P.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\Tektronix TDS694C.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\Tektronix TDS754C.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\Tektronix TDS784D.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\Tektronix TDS820.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\Tektronix SCD1000.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\Tektronix 11402A.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\Tektronix 11801B.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\Wiltron 562.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\Yokogawa DL2700.plt"; DestDir: "{app}\Sample HPGL files"
Source: "Sample HPGL files\Rohde & Schwarz FSP.plt"; DestDir: "{app}\Sample HPGL files"

Source: "gpiblib\comblock.h"; DestDir: "{app}\gpiblib"
Source: "gpiblib\ftd2xx.h"; DestDir: "{app}\gpiblib"
Source: "gpiblib\decl-32.h"; DestDir: "{app}\gpiblib"
Source: "gpiblib\gpib-32.obj"; DestDir: "{app}\gpiblib"
Source: "gpiblib\gpiblib.cpp"; DestDir: "{app}\gpiblib"
Source: "gpiblib\gpiblib.dll"; DestDir: "{app}\gpiblib"
Source: "gpiblib\typedefs.h"; DestDir: "{app}\gpiblib"
Source: "gpiblib\gpiblib.h"; DestDir: "{app}\gpiblib"
Source: "gpiblib\gpiblib.lib"; DestDir: "{app}\gpiblib"
Source: "gpiblib\ni488.h"; DestDir: "{app}\gpiblib"
Source: "gpiblib\makefile"; DestDir: "{app}\gpiblib"

[InstallDelete]
Type: files; Name: "{app}\connect.ini"
Type: files; Name: "{userdesktop}\HP 8753 VNA.lnk"
Type: files; Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\HP 8753 VNA.lnk";
Type: files; Name: "{group}\HP 8753 VNA.lnk";

[Tasks]
Name: "desktopicon"; Description: "Create &Desktop shortcuts"; GroupDescription: "Additional icons:";
Name: "quickicon"; Description: "Create &Quick Launch shortcuts"; GroupDescription: "Additional icons:";
Name: "associate"; Description: "Associate GPIB Toolkit applications with .PLT, .SSM, and .PNP files"; GroupDescription: "File associations:";
Name: "associat2"; Description: "Associate 7470.EXE with .HPG, .HGL, and .PGL files"; GroupDescription: "File associations:";

[Icons]
Name: "{group}\Readme"; Filename: "{app}\readme.htm"
Name: "{group}\HP 7470A Emulator"; Filename: "{app}\7470.exe"
Name: "{group}\Phase Noise"; Filename: "{app}\pn.exe"
Name: "{group}\Spectrum Surveillance"; Filename: "{app}\ssm.exe"
Name: "{group}\VNA Utility"; Filename: "{app}\vna.exe"
Name: "{group}\VNA T-Check"; Filename: "{app}\tcheck.exe"
Name: "{group}\GPIB Configurator"; Filename: "{app}\prologix.exe"

Name: "{userdesktop}\HP 7470A Emulator"; Filename: "{app}\7470.exe"; Tasks: desktopicon
Name: "{userdesktop}\Phase Noise"; Filename: "{app}\pn.exe"; Tasks: desktopicon
Name: "{userdesktop}\Spectrum Surveillance"; Filename: "{app}\ssm.exe"; Tasks: desktopicon
Name: "{userdesktop}\VNA Utility"; Filename: "{app}\vna.exe"; Tasks: desktopicon
Name: "{userdesktop}\GPIB Configurator"; Filename: "{app}\prologix.exe"; Tasks: desktopicon

Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\HP 7470A Emulator"; Filename: "{app}\7470.exe"; Tasks: quickicon
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\Phase Noise"; Filename: "{app}\pn.exe"; Tasks: quickicon
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\Spectrum Surveillance"; Filename: "{app}\ssm.exe"; Tasks: quickicon
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\VNA Utility"; Filename: "{app}\vna.exe"; Tasks: quickicon
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\GPIB Configurator"; Filename: "{app}\prologix.exe"; Tasks: quickicon

