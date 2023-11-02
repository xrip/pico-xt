//
// Created by xrip on 23.10.2023.
//

#pragma once
#ifndef TINY8086_FLASHRAM_H
#define TINY8086_FLASHRAM_H
//ESP8266 version of the SRAM handler
//Runs with a flashbased cached MMU swap system giving the emulator 1MB of RAM
//Raspberry Pi Pico version of the SRAM handler
//Runs with the internal flash memory swap system giving the emulator 1MB of RAM

#if PICO_ON_DEVICE
#include <hardware/flash.h>
#include <hardware/sync.h>
#include "f_util.h"
#include "ff.h"

static FATFS fs;
static FIL file1;
static FRESULT result1;
#else
SDL_RWops *file;
#endif

uint8_t lowmemRAM[0x600];
uint8_t cacheRAM1[4096] = {};
uint8_t cacheRAM2[4096] = {};
uint8_t cacheRAM3[4096] = {};
uint8_t cacheRAM4[4096] = {};
uint32_t block1=0xffffffff;
uint32_t block2=0xffffffff;
uint32_t block3=0xffffffff;
uint32_t block4=0xffffffff;
uint8_t dirty1=0;
uint8_t dirty2=0;
uint8_t dirty3=0;
uint8_t dirty4=0;
uint8_t NextCacheUse=0;


void SRAM_init() {
#if PICO_ON_DEVICE
    result1 = f_mount(&fs, "", 1);
    printf("swap :: f_mount result: %s (%d)\r\n", FRESULT_str(result1), result1);

    result1 = f_open(&file1, "\\XT\\swap.mem", FA_CREATE_ALWAYS | FA_WRITE | FA_READ);
    printf("swap :: f_open result: %s (%d)\r\n", FRESULT_str(result1), result1);

    result1 = f_expand(&file1, (512 << 10), 1);
    printf("swap :: f_expand result: %s (%d)\r\n", FRESULT_str(result1), result1);
#else
    file = SDL_RWFromFile("memory.swp", "w+");
    const uint8_t  bbb[4096] = { 'F', 'U', 'C', 'K' };
    SDL_RWseek(file, 5, RW_SEEK_SET);
    SDL_RWwrite(file, bbb, sizeof bbb, 1);
    printf("Error %s", SDL_GetError());
#endif
}

uint8_t SRAM_read(uint32_t address) {
    uint32_t block=(address&0xfffff000); //Else look into cached RAM

//    printf("SRAM_read %x\r\n", address);
    if (block1==block) {
//        printf("R1: %x %X\r\n", address&0x00000fff, cacheRAM1[address&0x00000fff]);
        return cacheRAM1[address&0x00000fff]; //Cache hit in cache1
    }
    if (block2==block) {
//            printf("R2: %x %X\r\n", address&0x00000fff, cacheRAM2[address&0x00000fff]);
            return cacheRAM2[address&0x00000fff]; //Cache hit in cache2
    }
    if (block3==block) {
//        printf("R3: %x %X\r\n", address&0x00000fff, cacheRAM3[address&0x00000fff]);
        return cacheRAM3[address&0x00000fff]; //Cache hit in cache3
    }
    if (block4==block) {
//        printf("R4: %x %X\r\n", address&0x00000fff, cacheRAM4[address&0x00000fff]);
        return cacheRAM4[address&0x00000fff]; //Cache hit in cache4
    }

    //Cache miss, fetch it

    if (NextCacheUse==0) {
        if (dirty1) {           //Write back the cached block if it's dirty
            SDL_RWseek(file, block1, RW_SEEK_SET);
            SDL_RWwrite(file, cacheRAM1, 4096, 1);

            dirty1=0;
        }

        SDL_RWseek(file, block, RW_SEEK_SET);
        SDL_RWread(file, cacheRAM1, 4096, 1);

        block1=(address&0xfffff000);
        NextCacheUse+=1;
        NextCacheUse&=3;
        return cacheRAM1[address&0x00000fff]; //Now found it in cache1
    }

    if (NextCacheUse==1) {

        if (dirty2) {           //Write back the cached block if it's dirty
            SDL_RWseek(file, block2, RW_SEEK_SET);
            SDL_RWwrite(file,cacheRAM2, 4096, 1);

            dirty2=0;
        }

        SDL_RWseek(file, block, RW_SEEK_SET);
        SDL_RWread(file,cacheRAM2, 4096, 1);

        block2=(address&0xfffff000);
        NextCacheUse+=1;
        NextCacheUse&=3;
        return cacheRAM2[address&0x00000fff]; //Now found it in cache2
    }

    if (NextCacheUse==2) {
        if (dirty3) {           //Write back the cached block if it's dirty

            SDL_RWseek(file, block3, RW_SEEK_SET);
            SDL_RWwrite(file,cacheRAM3, 4096, 1);
            dirty3=0;
        }

        SDL_RWseek(file, block, RW_SEEK_SET);
        SDL_RWread(file,cacheRAM3, 4096, 1);

        block3=(address&0xfffff000);
        NextCacheUse+=1;
        NextCacheUse&=3;
        return cacheRAM3[address&0x00000fff]; //Now found it in cache3
    }

    if (NextCacheUse==3) {
        if (dirty4) {           //Write back the cached block if it's dirty
            SDL_RWseek(file, block4, RW_SEEK_SET);
            SDL_RWwrite(file,cacheRAM4, 4096, 1);


            dirty4=0;
        }

        SDL_RWseek(file, block, RW_SEEK_SET);
        SDL_RWread(file,cacheRAM4, 4096, 1);

        block4=(address&0xfffff000);
        NextCacheUse+=1;
        NextCacheUse&=3;
        return cacheRAM4[address&0x00000fff]; //Now found it in cache4
    }

}

void SRAM_write(uint32_t address, uint8_t value) {
    uint32_t block=(address&0xfffff000); //Else write into cached block
//    printf("SRAM_write %x\r\n", address);
    if (block1==block) {
        cacheRAM1[address&0x00000fff]=value; //Write in cache1
//        printf("W1: %x %X\r\n", address&0x00000fff, cacheRAM1[address&0x00000fff]);
        dirty1=1; //mark cache1 as dirty
        return;
    }

    if (block2==block) {
        cacheRAM2[address&0x00000fff]=value; //Write in cache2
//        printf("W2: %x %X\r\n", address&0x00000fff, cacheRAM2[address&0x00000fff]);
        dirty2=1; //mark cache2 as dirty
        return;
    }

    if (block3==block) {
//        printf("W3: %x %X\r\n", address&0x00000fff, cacheRAM3[address&0x00000fff]);
        cacheRAM3[address&0x00000fff]=value; //Write in cache3
        dirty3=1; //mark cache3 as dirty
        return;
    }

    if (block4==block) {
//        printf("W4: %x %X\r\n", address&0x00000fff, cacheRAM4[address&0x00000fff]);
        cacheRAM4[address&0x00000fff]=value; //Write in cache4
        dirty4=1; //mark cache4 as dirty
        return;
    }

    //Cache miss, fetch it

    if (NextCacheUse==0) {
        if (dirty1) {           //Write back the cached block if it's dirty
            SDL_RWseek(file, block1, RW_SEEK_SET);
            SDL_RWwrite(file,cacheRAM1, 4096, 1);

        }

        SDL_RWseek(file, block, RW_SEEK_SET);
        SDL_RWread(file,cacheRAM1, 4096, 1);

        block1=(address&0xfffff000);
        NextCacheUse+=1;
        NextCacheUse&=3;
        cacheRAM1[address&0x00000fff]=value; //Now write it in cache1
        dirty1=1;
        return;
    }

    if (NextCacheUse==1) {
        if (dirty2) {           //Write back the cached block if it's dirty
            SDL_RWseek(file, block2, RW_SEEK_SET);
            SDL_RWwrite(file,cacheRAM2, 4096, 1);

        }
        SDL_RWseek(file, block, RW_SEEK_SET);
        SDL_RWread(file,cacheRAM2, 4096, 1);

        block2=(address&0xfffff000);
        NextCacheUse+=1;
        NextCacheUse&=3;
        cacheRAM2[address&0x00000fff]=value; //Now write it in cache2
        dirty2=1;
        return;
    }

    if (NextCacheUse==2) {
        if (dirty3) {           //Write back the cached block if it's dirty
            SDL_RWseek(file, block3, RW_SEEK_SET);
            SDL_RWwrite(file,cacheRAM3, 4096, 1);

        }
        SDL_RWseek(file, block, RW_SEEK_SET);
        SDL_RWread(file,cacheRAM3, 4096, 1);

        block3=(address&0xfffff000);
        NextCacheUse+=1;
        NextCacheUse&=3;
        cacheRAM3[address&0x00000fff]=value; //Now write it in cache3
        dirty3=1;
        return;
    }

    if (NextCacheUse==3) {
        if (dirty4) {           //Write back the cached block if it's dirty
            SDL_RWseek(file, block4, RW_SEEK_SET);
            SDL_RWwrite(file,cacheRAM4, 4096, 1);


        }

        SDL_RWseek(file, block, RW_SEEK_SET);
        SDL_RWread(file,cacheRAM4, 4096, 1);

        block4=(address&0xfffff000);
        NextCacheUse+=1;
        NextCacheUse&=3;
        cacheRAM4[address&0x00000fff]=value; //Now write it in cache4
        dirty4=1;
        return;
    }

}


#endif