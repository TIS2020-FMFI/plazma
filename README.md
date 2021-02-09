# plazma
Konfigurácia, čítanie a vizualizácia údajov z meracieho prístroja pre experimenty s fyzikou plazmy.

Náš projekt využíva program hpctrl, ktorý si treba stiahnut z https://github.com/TIS2020-FMFI/hpctrl.

HPCTRL si môžete stiahnúť ako .zip a extrahovať.
Potrebné je vytvoriť hpctrl.exe - a to tak, že otvoríte hpctrl-main/src/vna.sln vo Visual Studio, treba hore nastaviť konfiguráciu na x86 a spustiť debugovanie.
V hpctrl-main/src/Debug by mal takto vzniknúť hpctrl.exe, ktorý spolu s gpiblib.dll a winvfx16.dll presuniete do priečinka plazma/src v našom programe.

Na vytvorenie plazma.exe je použitá pythonovská knižnica Pyinstaller. Viac: https://pyinstaller.readthedocs.io/en/stable/.
Stačí otvoriť CMD v plazma/src priečinku, a spustiť príkaz:
Pyinstaller Program.py -n Plazma -w -i plazma.ico

Treba nainštalovať ovládač ku GPIB zariadeniu.
Do cieľového adresára treba nakopírovať súbory: 

gpiblib.dll
prologix.exe
winvfx16.dll
connect.ini

z balíka GPIB Toolkit, http://www.ke5fx.com/gpib/readme.htm   
a hpctrl.exe (linka na github vyššie)

Pred prvým spustením treba odštartovať PROLOGIX.EXE a nastaviť GPIB zariadenie, a vybrať "update CONNECT.INI". 

Potom program odštartujete PLAZMA.EXE