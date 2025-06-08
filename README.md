# This project evolved in the form of [Pico-286](https://github.com/xrip/pico-286), this repository is preserved for the sake of history and now archied.






### Raspberry Pi Pico PC XT (8086/8088) Emulator for MURMULATOR devboard

### To achieve best performance you SHOULD have PSRAM chip installed

![build master](https://github.com/xrip/pico-xt/actions/workflows/release-on-tag.yml/badge.svg?branch=master)

To use HDD: save hdd.img (from hdd.img.zip) to <SD-card-drive>:\XT\hdd.img

To switch to USB-drives mode press: Enter + Backspace
* In the File Manager press "F10"
* To exit from the File Manager, use Ctrl + F10

To change overcloking: Left Ctrl + Tab + Numpad "+" or Numpad "-"

To use own FDD imgage, that will be shown as drive "B:", put fdd0.img file into <SD-card>:\XT\fdd01.img

Extra PSRAM support on pi pico pins:
* PSRAM_PIN_CS=18
* PSRAM_PIN_SCK=19
* PSRAM_PIN_MOSI=20
* PSRAM_PIN_MISO=21

To swap floppy drives A <-> B use: Left Ctrl + Tab + Backspace
* Or Enter + Backspace -> File Manager
* In the File Manager press "F9"

To turn on/off:
* Digital Sound Source on LPT1, use Ctrl + Tab + D. Default value - it is ON
* COVOX on LPT2, use Ctrl + Tab + C. Default value - it is ON
* Tandy 3-voices music device emulation, use Ctrl + Tab + T. Default value - it is ON
* Game Blaster (Creative Music System) device emulation, use Ctrl + Tab + G. Default value - it is OFF
* AdLib emulation, use Ctrl + Tab + A. Default value - it is OFF (experimental, not recomended)
* Whole sound emulation subsystem, use Ctrl + Tab + S. Default value - it is ON
* // Experimental: use before DOS (while BIOS test RAM)
* Extended Memory Manager (XMS), use Ctrl + Tab + X. Default value - it is OFF (have some issues with Wolf 3D)
* Expanded Memory Manager (EMS), use Ctrl + Tab + E. Default value - it is ON
* Upper Memory Blocks Manager (UMB), use Ctrl + Tab + U. Default value - it is ON
* Hight Memory Address Manager (HMA), use Ctrl + Tab + H. Default value - it is ON
* // TODO: save state to config file

To decrease or increase appropriated sound device volume, press Ctrl + related button and numpad "+" / "-",
for example Ctrl + "D" + "-" will decrease DSS volume, Ctrl + "S" + "+" - increase whole sound system volume.

To swap DSS and Covox LPT1 and LPT2, use Ctrl + "D" + "C".

# Hardware needed
To get it working you should have an Murmulator (development) board with VGA output. Schematics available here at https://github.com/AlexEkb4ever/MURMULATOR_classical_scheme
![Murmulator Schematics](https://github.com/javavi/pico-infonesPlus/blob/main/assets/Murmulator-1_BSchem.JPG)

![RAM extention](/psram.jpg)

# Contributors
![GitHub Contributors Image](https://contrib.rocks/image?repo=xrip/pico-xt)
