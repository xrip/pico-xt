//
// Created by xrip on 23.10.2023.
//
#pragma once
#ifndef TINY8086_DISK_H
#define TINY8086_DISK_H

#include "stdint.h"
#include "memory.h"
#include "cpu8086.h"

#if PICO_ON_DEVICE
#else
#include <SDL2/SDL.h>
#endif

#if PICO_ON_DEVICE
#include "f_util.h"
#include "ff.h"
static FATFS fs;
#else
#define _FILE SDL_RWops
#endif
struct struct_drive {
    FIL * diskfile;
    size_t filesize;
    uint16_t cyls;
    uint16_t sects;
    uint16_t heads;
    uint8_t inserted;
    uint8_t readonly;
    char *data;
};

// FIXME!! Уменьшить до 2 флопи, 2 хдд
struct struct_drive disk[4];

static uint8_t sectorbuffer[512];
FIL file;

void ejectdisk(uint8_t drivenum) {
    if (drivenum & 0x80) drivenum -= 126;

        if (disk[drivenum].inserted) {
        disk[drivenum].inserted = 0;
        if (drivenum >= 0x80)
            hdcount--;
        else
            fdcount--;
    }
}

uint8_t insertdisk(uint8_t drivenum, size_t size, char *ROM, char* pathname) {
    if (drivenum & 0x80) drivenum -= 126;
    if (pathname != NULL) {
        FRESULT result = f_mount(&fs, "", 1);
        printf("drivenum %i :: f_mount result: %s (%d)\r\n", drivenum, FRESULT_str(result), result);

        result = f_open(&file, pathname, FA_READ | FA_WRITE);
        printf("drivenum %i :: f_open result: %s (%d)\r\n", drivenum, FRESULT_str(result), result);
        if (FR_OK != result) {
            return 1;
        }
        size = f_size(&file);
    }
    const char *err = "?";
    if (size < 360 * 1024) {
        err = "Disk image is too small!";
        goto error;
    }
    if (size > 0x1f782000UL) {
        err = "Disk image is too large!";
        goto error;
    }
    if ((size & 511)) {
        err = "Disk image size is not multiple of 512 bytes!";
        goto error;
    }
    uint16_t cyls, heads, sects;
    if (drivenum >= 2) { //it's a hard disk image
        sects = 63;
        heads = 16;
        cyls = size / (sects * heads * 512);
    } else {   //it's a floppy image
        cyls = 80;
        sects = 18;
        heads = 2;
        if (size <= 1228800)
            sects = 15;
        if (size <= 737280)
            sects = 9;
        if (size <= 368640) {
            cyls = 40;
            sects = 9;
        }
        if (size <= 163840) {
            cyls = 40;
            sects = 8;
            heads = 1;
        }
    }
    if (cyls > 1023 || cyls * heads * sects * 512 != size) {
        err = "Cannot find some CHS geometry for this disk image file!";
        //goto error;
        // FIXME!!!!
    }
    // Seems to be OK. Let's validate (store params) and print message.
    ejectdisk(drivenum);    // close previous disk image for this drive if there is any
    disk[drivenum].diskfile = &file;
    disk[drivenum].filesize = size;
    disk[drivenum].inserted = true;
    disk[drivenum].readonly = disk[drivenum].data ? true : false;
    disk[drivenum].cyls = cyls;
    disk[drivenum].heads = heads;
    disk[drivenum].sects = sects;
    disk[drivenum].data = ROM;

    if (drivenum >= 2)
        hdcount++;
    else
        fdcount++;
    printf(
            "DISK: Disk 0%02Xh has been attached %s from file %s size=%luK, CHS=%d,%d,%d\n",
            drivenum,
            disk[drivenum].readonly ? "R/O" : "R/W",
            "ROM",
            (unsigned long) (size >> 10),
            cyls,
            heads,
            sects
    );
    return 0;
    error:
    fprintf(stderr, "DISK: ERROR: cannot insert disk 0%02Xh as %s because: %s\r\n", drivenum, "ROM", err);
    return 1;
}

// Call this ONLY if all parameters are valid! There is no check here!
static size_t chs2ofs(int drivenum, int cyl, int head, int sect) {
    return (((size_t) cyl * (size_t) disk[drivenum].heads + (size_t) head) * (size_t) disk[drivenum].sects +
            (size_t) sect - 1) * 512UL;
}


static void
bios_readdisk(uint8_t drivenum, uint16_t dstseg, uint16_t dstoff, uint16_t cyl, uint16_t sect, uint16_t head,
              uint16_t sectcount, int is_verify) {
    uint32_t cursect, memdest, lba;
    size_t fileoffset;
    FRESULT result;

    if (!disk[drivenum].inserted) {
        CPU_AH = 0x31;    // no media in drive
        goto error;
    }
    if (!sect || sect > disk[drivenum].sects || cyl >= disk[drivenum].cyls || head >= disk[drivenum].heads) {
        CPU_AH = 0x04;    // sector not found
        goto error;
    }
    //lba =  ((uint32_t) cyl * (uint32_t) disk[drivenum].heads + (uint32_t) head) * (uint32_t) disk[drivenum].sects + (uint32_t) sect - 1;
    //fileoffset = lba * 512;
    fileoffset = chs2ofs(drivenum, cyl, head, sect);

    if (disk[drivenum].data == NULL && fileoffset > 0) {
        result = f_lseek(&file, fileoffset);
        printf("drivenum %i :: f_lseek offs %i result: %s (%d)\r\n", drivenum,  fileoffset, FRESULT_str(result), result);
    }
        //SDL_RWseek(disk[drivenum].diskfile, fileoffset, RW_SEEK_SET);

    if (fileoffset > disk[drivenum].filesize) {
        CPU_AH = 0x04;    // sector not found
        goto error;
    }

    memdest = ((uint32_t) dstseg << 4) + (uint32_t) dstoff;
    // for the readdisk function, we need to use write86 instead of directly fread'ing into
    // the RAM array, so that read-only flags are honored. otherwise, a program could load
    // data from a disk over BIOS or other ROM code that it shouldn't be able to.

    for (cursect = 0; cursect < sectcount; cursect++) {
//        if (hostfs_read(disk[drivenum].diskfile, sectorbuffer, 512, 1) != 1)
//            break;
        //memcpy(&RAM[memdest], &disk[drivenum].data[fileoffset], 512);
        if (disk[drivenum].data != NULL) {
            memcpy(sectorbuffer, &disk[drivenum].data[fileoffset], 512);
        } else {
            //SDL_RWread(disk[drivenum].diskfile, sectorbuffer, 512,1);
            UINT bytes_read;
            result = f_read(&file, sectorbuffer, 512, &bytes_read);
            printf("drivenum %i :: f_read result: %s (%d)\r\n", drivenum, FRESULT_str(result), bytes_read);
            if (FR_OK != result)
                break;
        }
        fileoffset += 512;
        /*
        if (disk[drivenum].diskfile != NULL)
            result = f_lseek(disk[drivenum].diskfile, fileoffset);
*/
        //memcpy(&RAM[memdest], sectorbuffer, sizeof sectorbuffer);

        if (is_verify) {
            for (int sectoffset = 0; sectoffset < 512; sectoffset++) {
                // FIXME: segment overflow condition?
                if (read86(memdest++) != sectorbuffer[sectoffset]) {
                    // sector verify failed!
                    printf("                    // sector verify failed!");
                    CPU_AL = cursect;
                    CPU_FL_CF = 1;
                    CPU_AH = 0xBB;    // error code?? what we should say in this case????
                    return;
                }
            }
        } else {
            for (int sectoffset = 0; sectoffset < 512; sectoffset++) {
                // FIXME: segment overflow condition?
                write86(memdest++, sectorbuffer[sectoffset]);
            }

        }
    }
    if (sectcount && !cursect) {
        CPU_AH = 0x04;    // sector not found
        goto error;            // not even one sector could be read?
    }
    CPU_AL = cursect;
    CPU_FL_CF = 0;
    CPU_AH = 0;
    return;
    error:
    printf("DISK ERROR %i 0x%x\r\n", drivenum, CPU_AH);
    // AH must be set with the error code
    CPU_AL = 0;
    CPU_FL_CF = 1;
}

void bios_read_boot_sector(int drive, uint16_t dstseg, uint16_t dstoff) {
    bios_readdisk(drive, dstseg, dstoff, 0, 1, 0, 1, 0);
}


static void
bios_writedisk(uint8_t drivenum, uint16_t dstseg, uint16_t dstoff, uint16_t cyl, uint16_t sect, uint16_t head,
               uint16_t sectcount) {
    uint32_t cursect, memdest, lba;
    size_t fileoffset;FRESULT result;
    //printf("bios_writedisk\r\n");
    if (!disk[drivenum].inserted) {
        CPU_AH = 0x31;    // no media in drive
        goto error;
    }
    if (!sect || sect > disk[drivenum].sects || cyl >= disk[drivenum].cyls || head >= disk[drivenum].heads) {
        CPU_AH = 0x04;    // sector not found
        goto error;
    }
    lba = ((uint32_t) cyl * (uint32_t) disk[drivenum].heads + (uint32_t) head) * (uint32_t) disk[drivenum].sects +
          (uint32_t) sect - 1;
    fileoffset = lba * 512;
    //fileoffset = chs2ofs(drivenum, cyl, head, sect);
    if (fileoffset > disk[drivenum].filesize) {
        CPU_AH = 0x04;    // sector not found
        goto error;
    }

    if (disk[drivenum].readonly) {
        CPU_AH = 0x03;    // drive is read-only
        goto error;
    }

    if (disk[drivenum].filesize < fileoffset) {
        CPU_AH = 0x04;    // sector not found
        goto error;
    }

    if (disk[drivenum].diskfile != NULL && f_lseek(disk[drivenum].diskfile, fileoffset) != FR_OK) {
        //printf("Write seek error");
        CPU_AH = 0x04;	// sector not found
        goto error;
    }

    memdest = ((uint32_t) dstseg << 4) + (uint32_t) dstoff;
    for (cursect = 0; cursect < sectcount; cursect++) {
        for (int sectoffset = 0; sectoffset < 512; sectoffset++) {
            // FIXME: segment overflow condition?
            sectorbuffer[sectoffset] = read86(memdest++);
        }
        UINT writen_bytes;
        if (disk[drivenum].data == NULL) {
            result = f_write(&file, sectorbuffer, 512, &writen_bytes);
            printf("drivenum %i :: f_write result: %s (%d)\r\n", drivenum, FRESULT_str(result), writen_bytes);
        }
    }

    if (sectcount && !cursect) {
        CPU_AH = 0x04;    // sector not found
        goto error;            // not even one sector could be written?
    }
    CPU_AL = cursect;
    CPU_FL_CF = 0;
    CPU_AH = 0;
    return;
    error:
    // AH must be set with the error code
    CPU_AL = 0;
    CPU_FL_CF = 1;
}

void diskhandler(void) {
    static uint8_t lastdiskah[4], lastdiskcf[4];
    uint8_t drivenum;
    drivenum = CPU_DL;
    if (drivenum & 0x80) drivenum -= 126;

    //printf("DISK interrupt function %02Xh\r\n", CPU_AH);
    switch (CPU_AH) {
        case 0: //reset disk system
            //printf("Disk reset %i\r\n", CPU_DL);
            if (disk[drivenum].inserted) {
                CPU_AH = 0;
                CPU_FL_CF = 0; //useless function in an emulator. say success and return.
            } else {
                CPU_FL_CF = 1;
            }
            break;
        case 1: //return last status
            CPU_AH = lastdiskah[drivenum];
            CPU_FL_CF = lastdiskcf[drivenum];
            return;
        case 2: //read sector(s) into memory
/*            printf("bios_readdisk %i %i %i %i %i %i %i\r\n",
                    CPU_DL,                // drivenum
                    CPU_ES, CPU_BX,            // segment & offset
                    CPU_CH + (CPU_CL / 64) * 256,    // cylinder
                    CPU_CL & 63,            // sector
                    CPU_DH,                // head
                    CPU_AL,                // sectcount
                    0                // is verify (!=0) or read (==0) operation?
            );*/
            bios_readdisk(
                    drivenum,                // drivenum
                    CPU_ES, CPU_BX,            // segment & offset
                    CPU_CH + (CPU_CL / 64) * 256,    // cylinder
                    CPU_CL & 63,            // sector
                    CPU_DH,                // head
                    CPU_AL,                // sectcount
                    0                // is verify (!=0) or read (==0) operation?
            );
            break;
        case 3: //write sector(s) from memory
/*            printf("bios_writedisk %i %i %i %i %i %i %i\r\n",   CPU_DL,                // drivenum
                   CPU_ES, CPU_BX,            // segment & offset
                   CPU_CH + (CPU_CL / 64) * 256,    // cylinder
                   CPU_CL & 63,            // sector
                   CPU_DH,                // head
                   CPU_AL                // sectcount
            );*/
            bios_writedisk(
                    drivenum,                // drivenum
                    CPU_ES, CPU_BX,            // segment & offset
                    CPU_CH + (CPU_CL / 64) * 256,    // cylinder
                    CPU_CL & 63,            // sector
                    CPU_DH,                // head
                    CPU_AL                // sectcount
            );
            break;
        case 4:    // verify sectors ...
/*            printf("bios_verify %i %i %i %i %i %i %i\r\n",
                   CPU_DL,                // drivenum
                   CPU_ES, CPU_BX,            // segment & offset
                   CPU_CH + (CPU_CL / 64) * 256,    // cylinder
                   CPU_CL & 63,            // sector
                   CPU_DH,                // head
                   CPU_AL,                // sectcount
                   1                                // is verify (!=0) or read (==0) operation?
            );*/
            bios_readdisk(
                    drivenum,                // drivenum
                    CPU_ES, CPU_BX,            // segment & offset
                    CPU_CH + (CPU_CL / 64) * 256,    // cylinder
                    CPU_CL & 63,            // sector
                    CPU_DH,                // head
                    CPU_AL,                // sectcount
                    1                                // is verify (!=0) or read (==0) operation?
            );
            break;
        case 5: //format track
            // pretend success ...
            // TODO: at least fill area (ie, the whole track) with zeroes or something, pretending the formatting was happened :)
            CPU_FL_CF = 0;
            CPU_AH = 0;
            break;
        case 8: //get drive parameters
            if (disk[drivenum].inserted) {
                CPU_FL_CF = 0;
                CPU_AH = 0;
                CPU_CH = disk[drivenum].cyls - 1;
                CPU_CL = disk[drivenum].sects & 63;
                CPU_CL = CPU_CL + (disk[drivenum].cyls / 256) * 64;
                CPU_DH = disk[drivenum].heads - 1;
                if (CPU_DL < 2) {
                    CPU_BL = 4; //else CPU_BL = 0;
                    CPU_DL = 2;
                } else
                    CPU_DL = hdcount;
            } else {
                CPU_FL_CF = 1;
                CPU_AH = 0xAA;
            }
            break;
#if 0
            case 0x15:	// get disk type
            if (disk[CPU_DL].inserted) {
                int drivenum = CPU_DL;
                printf("Requesting int 13h / function 15h for drive %02Xh\n", drivenum);
                CPU_AH = (drivenum & 0x80) ? 3 : 1;		// either "floppy without change line support" (1) or "harddisk" (3)
                CPU_CX = (disk[drivenum].filesize >> 9) >> 16;	// number of blocks, high word
                CPU_DX = (disk[drivenum].filesize >> 9) & 0xFFFF;	// number of blocks, low word
                CPU_AL = CPU_AH;
                CPU_FL_CF = 0;
            } else {
                printf("Requesting int 13h / function 15h for drive %02Xh no such device\n", CPU_DL);
                CPU_AH = 0;	// no such device
                CPU_AL = 0;
                CPU_CX = 0;
                CPU_DX = 0;
                //CPU_AX = 0x00;
//                CPU_AX = 0x0101;
                CPU_FL_CF = 1;
            }
            break;
#endif
        default:
            //printf("BIOS: unknown Int 13h service was requested: %02Xh\r\n", CPU_AH);
            CPU_FL_CF = 1;    // unknown function was requested?
            //CPU_AH = 1;
            break;
    }
    lastdiskah[CPU_DL] = CPU_AH;
    lastdiskcf[CPU_DL] = CPU_FL_CF;
    if (CPU_DL & 0x80)
        RAM[0x474] = CPU_AH;
}

#endif //TINY8086_DISK_H
