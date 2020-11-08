run debug/hpctrl.exe to see the usage:

usage: hpctrl [-a n] [-i | [-S11][-S21][-S12][-S22]]

    -a n    specify device address, default=16
    -Sxy    retrieve measurement from channel xy
    -i      interactive mode, accepted commands:
              CONNECT      ... connect to the device
              DISCONNECT   ... disconnect the device
              S11 .. S22   ... configure (add) a channel for measurement
              ALL          ... configure measurement of all 4 channels
              CLEAR        ... reset measurement config to no channels
              MEASURE      ... perform configured measurement
              GETSTATE     ... dump the device state
              SETSTATE     ... set the device state (followed in next line)
              GETCALIB     ... get the device calibration
              SETCALIB     ... set the device calibration (followed in next line)
              RESET        ... reset instrument
              FACTRESET    ... factory reset instrument
              CMD          ... send direct command specified at the next line
              HELP         ... send direct command specified at the next line
              EXIT         ... terminate the application

you may need to copy the following dll files to this folder:

ftd2xx.dll
gnss.dll
gpiblib.dll
sh_api.dll
specan.dll
w32sal.dll
winvfx16.dll

