# Raspberry Pi Pico PC XT (8086/8088) Emulator for MURMULATOR devboard

![build master](https://github.com/xrip/pico-xt/actions/workflows/build-on-push.yml/badge.svg?branch=master)

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

# Hardware needed
To get it working you should have an Murmulator (development) board with VGA output. Schematics available here at https://github.com/AlexEkb4ever/MURMULATOR_classical_scheme
![Murmulator Schematics](https://github.com/javavi/pico-infonesPlus/blob/main/assets/Murmulator-1_BSchem.JPG)

# Contributors
![GitHub Contributors Image](https://contrib.rocks/image?repo=xrip/pico-nes)
