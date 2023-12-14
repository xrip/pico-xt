# Raspberry Pi Pico PC XT (8086/8088) Emulator for MURMULATOR devboard

![build master](https://github.com/xrip/pico-xt/actions/workflows/release-on-tag.yml/badge.svg?branch=master)

To use HDD: save hdd.img (from hdd.img.zip) to <SD-card-drive>:\XT\hdd.img

To switch to USB-drives mode press: Enter + Backspace

To change overcloking: Left Ctrl + Tab + Numpad "+" or Numpad "-"

To use own FDD imgage, that will be shown as drive "B:", put fdd0.img file into <SD-card>:\XT\fdd01.img

Extra PSRAM support on pi pico pins:
            PSRAM_PIN_CS=17
            PSRAM_PIN_SCK=18
            PSRAM_PIN_MOSI=19
            PSRAM_PIN_MISO=20

To swap floppy drives A <-> B use: Left Ctrl + Tab + Backspace

To turn on/off
    Digital Sound Source on LPT1, use Ctrl + Tab + D. Default value - it is ON
    COVOX on LPT2, use Ctrl + Tab + C. Default value - it is ON
    Tandy 3-voices music device emulation, use Ctrl + Tab + T. Default value - it is ON
    Game Blaster (Creative Music System) device emulation, use Ctrl + Tab + G. Default value - it is OFF
    AdLib emulation, use Ctrl + Tab + A. Default value - it is OFF (experimental, not recomended)
    Whole sound emulation subsystem, use Ctrl + Tab + S. Default value - it is ON
    // Experimental: use before DOS (while BIOS test RAM)
    Etended Memory Manager (XMS), use Ctrl + Tab + X. Default value - it is OFF (have some issues with Wolf 3D)
    Epanded Memory Manager (EMS), use Ctrl + Tab + E. Default value - it is ON
    Upper Memory Blocks Manager (UMB), use Ctrl + Tab + U. Default value - it is ON
    Hight Memory Address Manager (HMA), use Ctrl + Tab + H. Default value - it is ON
    // TODO: save state to config file

To decrease or increase appropriated sound device volume, press related button and numpad "+" / "-",
for example "D" + "-" will decrease DSS volume, "S" + "+" - increase whole sound system volume.

# Hardware needed
To get it working you should have an Murmulator (development) board with VGA output. Schematics available here at https://github.com/AlexEkb4ever/MURMULATOR_classical_scheme
![Murmulator Schematics](https://github.com/javavi/pico-infonesPlus/blob/main/assets/Murmulator-1_BSchem.JPG)

![RAM extention](/psram.jpg)

# Contributors
![GitHub Contributors Image](https://contrib.rocks/image?repo=xrip/pico-xt)
