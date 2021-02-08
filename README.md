# plazma
Konfigurácia, čítanie a vizualizácia údajov z meracieho prístroja pre experimenty s fyzikou plazmy.

Náš projekt využíva program hpctrl, ktorý si treba stiahnut z https://github.com/TIS2020-FMFI/hpctrl.

HPCTRL si môžete stiahnúť ako .zip a extrahovať.
Potrebné je vytvoriť hpctrl.exe - a to tak, že otvoríte hpctrl-main/src/vna.sln vo Visual Studio, treba hore nastaviť konfiguráciu na x86 a spustiť debugovanie.
V hpctrl-main/src/Debug by mal takto vzniknúť hpctrl.exe, ktorý spolu s gpiblib.dll a winvfx16.dll presuniete do priečinka plazma/src v našom programe.

Na vytvorenie plazma.exe je použitá pythonovská knižnica Pyinstaller. Viac: https://pyinstaller.readthedocs.io/en/stable/.
Stačí otvoriť CMD v plazma/src priečinku, a spustiť príkaz:
Pyinstaller Program.py -n Plazma -w -i plazma.ico
