//
// Created by xrip on 24.10.2023.
//

#ifndef TINY8086_RAM_H
#define TINY8086_RAM_H

#include "stdint.h"
//ESP8266 version of the SRAM handler
//Runs with a flashbased cached MMU swap system giving the emulator 1MB of RAM

uint8_t lowmemRAM[0x600];
uint8_t cacheRAM1[4096];
uint8_t cacheRAM2[4096];
uint8_t cacheRAM3[4096];
uint8_t cacheRAM4[4096];
uint32_t block1 = 0xffffffff;
uint32_t block2 = 0xffffffff;
uint32_t block3 = 0xffffffff;
uint32_t block4 = 0xffffffff;
uint8_t dirty1 = 0;
uint8_t dirty2 = 0;
uint8_t dirty3 = 0;
uint8_t dirty4 = 0;
uint8_t NextCacheUse = 0;

uint8_t SRAM_read(uint32_t address) {
    if (address < 0x600) return lowmemRAM[address]; //If RAM lower than 0x600 return direct RAM
    uint32_t block = (address & 0xfffff000) + 0x100000; //Else look into cached RAM
    if (block1 == block) return cacheRAM1[address & 0x00000fff]; //Cache hit in cache1
    if (block2 == block) return cacheRAM2[address & 0x00000fff]; //Cache hit in cache2
    if (block3 == block) return cacheRAM3[address & 0x00000fff]; //Cache hit in cache3
    if (block4 == block) return cacheRAM4[address & 0x00000fff]; //Cache hit in cache4

    //Cache miss, fetch it

    if (NextCacheUse == 0) {
        if (dirty1) {           //Write back the cached block if it's dirty

            spi_flash_erase_sector(block1 >> 12);

            spi_flash_write(block1, reinterpret_cast<uint32_t *>(cacheRAM1), 4096);

            dirty1 = 0;
        }

        spi_flash_read(block, reinterpret_cast<uint32_t *>(cacheRAM1), 4096);

        block1 = (address & 0xfffff000) + 0x100000;
        NextCacheUse += 1;
        NextCacheUse &= 3;
        return cacheRAM1[address & 0x00000fff]; //Now found it in cache1
    }

    if (NextCacheUse == 1) {
        if (dirty2) {           //Write back the cached block if it's dirty

            spi_flash_erase_sector(block2 >> 12);

            spi_flash_write(block2, reinterpret_cast<uint32_t *>(cacheRAM2), 4096);

            dirty2 = 0;
        }

        spi_flash_read(block, reinterpret_cast<uint32_t *>(cacheRAM2), 4096);

        block2 = (address & 0xfffff000) + 0x100000;
        NextCacheUse += 1;
        NextCacheUse &= 3;
        return cacheRAM2[address & 0x00000fff]; //Now found it in cache2
    }

    if (NextCacheUse == 2) {
        if (dirty3) {           //Write back the cached block if it's dirty

            spi_flash_erase_sector(block3 >> 12);

            spi_flash_write(block3, reinterpret_cast<uint32_t *>(cacheRAM3), 4096);

            dirty3 = 0;
        }

        spi_flash_read(block, reinterpret_cast<uint32_t *>(cacheRAM3), 4096);

        block3 = (address & 0xfffff000) + 0x100000;
        NextCacheUse += 1;
        NextCacheUse &= 3;
        return cacheRAM3[address & 0x00000fff]; //Now found it in cache3
    }

    if (NextCacheUse == 3) {
        if (dirty4) {           //Write back the cached block if it's dirty

            spi_flash_erase_sector(block4 >> 12);

            spi_flash_write(block4, reinterpret_cast<uint32_t *>(cacheRAM4), 4096);

            dirty4 = 0;
        }

        spi_flash_read(block, reinterpret_cast<uint32_t *>(cacheRAM4), 4096);

        block4 = (address & 0xfffff000) + 0x100000;
        NextCacheUse += 1;
        NextCacheUse &= 3;
        return cacheRAM4[address & 0x00000fff]; //Now found it in cache4
    }

}

void SRAM_write(uint32_t address, uint8_t value) {
    if (address < 0x600) {
        lowmemRAM[address] = value; //If RAM lower than 0x600 write direct RAM
        return;
    }
    uint32_t block = (address & 0xfffff000) + 0x100000; //Else write into cached block

    if (block1 == block) {
        cacheRAM1[address & 0x00000fff] = value; //Write in cache1
        dirty1 = 1; //mark cache1 as dirty
        return;
    }

    if (block2 == block) {
        cacheRAM2[address & 0x00000fff] = value; //Write in cache2
        dirty2 = 1; //mark cache2 as dirty
        return;
    }

    if (block3 == block) {
        cacheRAM3[address & 0x00000fff] = value; //Write in cache3
        dirty3 = 1; //mark cache3 as dirty
        return;
    }

    if (block4 == block) {
        cacheRAM4[address & 0x00000fff] = value; //Write in cache4
        dirty4 = 1; //mark cache4 as dirty
        return;
    }

    //Cache miss, fetch it

    if (NextCacheUse == 0) {
        if (dirty1) {           //Write back the cached block if it's dirty

            spi_flash_erase_sector(block1 >> 12);

            spi_flash_write(block1, reinterpret_cast<uint32_t *>(cacheRAM1), 4096);

        }

        spi_flash_read(block, reinterpret_cast<uint32_t *>(cacheRAM1), 4096);

        block1 = (address & 0xfffff000) + 0x100000;
        NextCacheUse += 1;
        NextCacheUse &= 3;
        cacheRAM1[address & 0x00000fff] = value; //Now write it in cache1
        dirty1 = 1;
        return;
    }

    if (NextCacheUse == 1) {
        if (dirty2) {           //Write back the cached block if it's dirty

            spi_flash_erase_sector(block2 >> 12);

            spi_flash_write(block2, reinterpret_cast<uint32_t *>(cacheRAM2), 4096);

        }

        spi_flash_read(block, reinterpret_cast<uint32_t *>(cacheRAM2), 4096);

        block2 = (address & 0xfffff000) + 0x100000;
        NextCacheUse += 1;
        NextCacheUse &= 3;
        cacheRAM2[address & 0x00000fff] = value; //Now write it in cache2
        dirty2 = 1;
        return;
    }

    if (NextCacheUse == 2) {
        if (dirty3) {           //Write back the cached block if it's dirty

            spi_flash_erase_sector(block3 >> 12);

            spi_flash_write(block3, reinterpret_cast<uint32_t *>(cacheRAM3), 4096);

        }

        spi_flash_read(block, reinterpret_cast<uint32_t *>(cacheRAM3), 4096);

        block3 = (address & 0xfffff000) + 0x100000;
        NextCacheUse += 1;
        NextCacheUse &= 3;
        cacheRAM3[address & 0x00000fff] = value; //Now write it in cache3
        dirty3 = 1;
        return;
    }

    if (NextCacheUse == 3) {
        if (dirty4) {           //Write back the cached block if it's dirty

            spi_flash_erase_sector(block4 >> 12);

            spi_flash_write(block4, reinterpret_cast<uint32_t *>(cacheRAM4), 4096);

        }

        spi_flash_read(block, reinterpret_cast<uint32_t *>(cacheRAM4), 4096);

        block4 = (address & 0xfffff000) + 0x100000;
        NextCacheUse += 1;
        NextCacheUse &= 3;
        cacheRAM4[address & 0x00000fff] = value; //Now write it in cache4
        dirty4 = 1;
        return;
    }

}


#endif //TINY8086_RAM_H
