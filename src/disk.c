//
// Created by xrip on 23.10.2023.
//
#include "emulator.h"

#ifdef DEBUG_DISK
#define DBG_PRINTF(...) printf(__VA_ARGS__)
#else
#define DBG_PRINTF(...)
#endif

#if PICO_ON_DEVICE

#define _FILE FIL
_FILE fileA;
_FILE fileB;
_FILE fileC;
_FILE * getFileA() { return &fileA; }
_FILE * getFileB() { return &fileB; }
_FILE * getFileC() { return &fileC; }
size_t getFileA_sz() { return fileA.obj.fs ? f_size(&fileA) : 0; }
size_t getFileB_sz() { return fileB.obj.fs ? f_size(&fileB) : 0; }
size_t getFileC_sz() { return fileC.obj.fs ? f_size(&fileC) : 0; }
#ifdef BOOT_DEBUG
_FILE fileD; // for debug output
#endif
#else
#include <SDL2/SDL.h>
#define _FILE SDL_RWops
_FILE * file;
#endif
struct struct_drive {
    _FILE *diskfile;
    size_t filesize;
    uint16_t cyls;
    uint16_t sects;
    uint16_t heads;
    uint8_t inserted;
    uint8_t readonly;
    char *data;
};

struct struct_drive disk[4];

static uint8_t sectorbuffer[512];

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

#if PICO_ON_DEVICE

static _FILE* actualDrive(uint8_t drivenum) {
    return (drivenum > 1) ? &fileC : ( drivenum == 0 ? &fileA : &fileB );
}

static _FILE* tryFlushROM(uint8_t drivenum, size_t size, char *ROM, char *path) {
    _FILE *pFile = actualDrive(drivenum);
    gpio_put(PICO_DEFAULT_LED_PIN, true);
    FRESULT result = f_open(pFile, path, FA_WRITE | FA_CREATE_ALWAYS);
    if (result != FR_OK) {
        return NULL;
    }
    UINT bw;
    result = f_write(pFile, ROM, size, &bw);
    if (result != FR_OK) {
        return NULL;
    }
    f_close(pFile);
    sleep_ms(33); // TODO: ensure
    result = f_open(pFile, path, FA_READ | FA_WRITE);
    if (FR_OK != result) {
        return NULL;
    }
    gpio_put(PICO_DEFAULT_LED_PIN, false);
    return pFile;
}

#include "disk_c.h"
#include "fat12.h"

static _FILE* tryDefaultDrive(uint8_t drivenum, size_t size, char *path) {
    char tmp[80];
    snprintf(tmp, 80, "Drive 0x%02X not found. Will try to init %s by size: %f MB...", drivenum, path, (size / 1024 / 1024.0f));
    logMsg(tmp);
    _FILE *pFile = actualDrive(drivenum);
    FRESULT result = f_open(pFile, path, FA_WRITE | FA_CREATE_ALWAYS);
    if (result != FR_OK) {
        return NULL;
    }
    result = f_lseek(pFile, size);
    if (result != FR_OK) {
        return NULL;
    }
    f_close(pFile);
    sleep_ms(33); // TODO: ensure
    result = f_open(pFile, path, FA_READ | FA_WRITE);
    if (FR_OK != result) {
        return NULL;
    }
    if (drivenum > 1) {
        UINT bw;
        f_write(pFile, drive_c_0000000, sizeof(drive_c_0000000), &bw);
        f_lseek(pFile, 0x0007E00);
        f_write(pFile, drive_c_0007E00, sizeof(drive_c_0007E00), &bw);
    } else {
        UINT bw;
        f_write(pFile, drive_b_0000000, sizeof(drive_b_0000000), &bw);     
    }
    gpio_put(PICO_DEFAULT_LED_PIN, false);
    return pFile;
}
// manager.h
void notify_image_insert_action(uint8_t drivenum, char *pathname);
#endif

uint8_t insertdisk(uint8_t drivenum, size_t size, char *ROM, char *pathname) {
    if (drivenum & 0x80) drivenum -= 126;
    _FILE *pFile = NULL;
    if (pathname != NULL) {
#if PICO_ON_DEVICE
        pFile = actualDrive(drivenum);
        FRESULT result = f_open(pFile, pathname, FA_READ | FA_WRITE);
        if (FR_OK != result) {
            if (size != 0 && ROM != NULL) {
                pFile = tryFlushROM(drivenum, size, ROM, pathname);
                if (!pFile)
                    pathname = NULL;
            } else {
                pFile = tryDefaultDrive(drivenum, drivenum > 1 ? 0xFFF0000 : 1228800, pathname);
                if (!pFile)
                    return 1;
            }
        } else {
            size = f_size(pFile);
        }
        notify_image_insert_action(drivenum, pathname);
#else
        file = SDL_RWFromFile(pathname, "r+w");

        if (!file) {
            return 1;
        }
        size = SDL_RWsize(file);
#endif
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
    } else {   // it's a floppy image (1.44 МВ)
        cyls = 80;
        sects = 18;
        heads = 2;
        if (size <= 1228800) // 1.2 MB
            sects = 15;
        if (size <= 737280) // 720 KB
            sects = 9;
        if (size <= 368640) { // 360 KB
            cyls = 40;
            sects = 9;
        }
        if (size <= 163840) { // 160 KB
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
#if PICO_ON_DEVICE
    disk[drivenum].diskfile = pFile;
#else
    disk[drivenum].diskfile = file;
#endif
    disk[drivenum].data = pFile == NULL ? ROM : NULL;
    disk[drivenum].filesize = size;
    disk[drivenum].inserted = true;
    disk[drivenum].readonly = disk[drivenum].data ? true : false;
    disk[drivenum].cyls = cyls;
    disk[drivenum].heads = heads;
    disk[drivenum].sects = sects;

    if (drivenum >= 2)
        hdcount++;
    else
        fdcount++;
    char tmp[80];
#ifdef BOOT_DEBUG
    snprintf(tmp, 80,
            "DISK: Disk 0%02Xh has been attached %s from file %s size=%luK, CHS=%d,%d,%d",
            drivenum,
            disk[drivenum].readonly ? "R/O" : "R/W",
            pathname ? pathname : "ROM",
            (unsigned long) (size >> 10),
            cyls,
            heads,
            sects
    ); logMsg(tmp);
#endif
    return 0;
error:
    snprintf(tmp, 80, stderr, "DISK: ERROR: disk 0%02Xh as %s err: %s", drivenum, pathname ? pathname : "ROM", err);
    logMsg(tmp);
    return 1;
}

// Call this ONLY if all parameters are valid! There is no check here!
static size_t chs2ofs(int drivenum, int cyl, int head, int sect) {
    return (
        ((size_t)cyl * (size_t)disk[drivenum].heads + (size_t)head) * (size_t)disk[drivenum].sects + (size_t) sect - 1
    ) * 512UL;
}

#if PICO_ON_DEVICE

bool img_disk_read_sec(int drv, BYTE * buffer, LBA_t lba) {
    _FILE * pFile = actualDrive(drv);
    if(FR_OK != f_lseek(pFile, lba * 512)) {
        return false;
    }
    UINT br;
    if(FR_OK != f_read(pFile, buffer, 512, &br)) {
        return false;
    }
    return true;
}

bool img_disk_write_sec(int drv, BYTE * buffer, LBA_t lba) {
    _FILE * pFile = actualDrive(drv);
    if(FR_OK != f_lseek(pFile, lba * 512)) {
        return false;
    }
    UINT bw;
    if(FR_OK != f_write(pFile, buffer, 512, &bw)) {
        return false;
    }
    return true;
}
#endif

#ifdef BOOT_DEBUG
void logFile(char* msg) {
    f_open(&fileD, "\\XT\\boot.log", FA_WRITE | FA_OPEN_APPEND);
    UINT bw;
    f_write(&fileD, msg, strlen(msg), &bw);
    f_close(&fileD);
}
#endif

static void
bios_readdisk(uint8_t drivenum,
              uint16_t dstseg, uint16_t dstoff,
              uint16_t cyl, uint16_t sect, uint16_t head,
              uint16_t sectcount, int is_verify
) {
    uint32_t cursect, memdest, lba;
    size_t fileoffset;
#if PICO_ON_DEVICE
    FRESULT result;
#endif
    if (!disk[drivenum].inserted) {
        DBG_PRINTF("no media %i\r\n", drivenum);
        CPU_AH = 0x31;    // no media in drive
        goto error;
    }
    if (!sect || sect > disk[drivenum].sects || cyl >= disk[drivenum].cyls || head >= disk[drivenum].heads) {
        DBG_PRINTF("sector no found\r\n");
        CPU_AH = 0x04;    // sector not found
        goto error;
    }
    fileoffset = chs2ofs(drivenum, cyl, head, sect);
#ifdef BOOT_DISK_RW
    char tmp[80]; UINT bw;
    f_open(&fileD, "\\XT\\boot.log", FA_WRITE | FA_OPEN_APPEND);
#endif
    if (disk[drivenum].data == NULL && fileoffset >= 0) {
#if PICO_ON_DEVICE
        result = f_lseek(disk[drivenum].diskfile, fileoffset);
#ifdef BOOT_DISK_RW
        sprintf(tmp, "\ndrv%i f_lseek(0x%X) result: %s\n", drivenum, fileoffset, FRESULT_str(result));
        f_write(&fileD, tmp, strlen(tmp), &bw);
#endif
        if (result != FR_OK) {
            CPU_AH = 0x04;    // sector not found
#ifdef BOOT_DISK_RW
            f_close(&fileD);
#endif
            goto error;            
        }
#else
        SDL_RWseek(disk[drivenum].diskfile, fileoffset, RW_SEEK_SET);
#endif
    }
    if (fileoffset > disk[drivenum].filesize) {
        DBG_PRINTF("sector no found\r\n");
        CPU_AH = 0x04;    // sector not found
        goto error;
    }
    memdest = ((uint32_t) dstseg << 4) + (uint32_t) dstoff;
    // for the readdisk function, we need to use write86 instead of directly fread'ing into
    // the RAM array, so that read-only flags are honored. otherwise, a program could load
    // data from a disk over BIOS or other ROM code that it shouldn't be able to.
    for (cursect = 0; cursect < sectcount; cursect++) {
        if (disk[drivenum].data != NULL) {
            memcpy(sectorbuffer, &disk[drivenum].data[fileoffset], 512);
#ifdef BOOT_DISK_RW
            sprintf(tmp, "\ndrv%i f_read(0x%X) result: %s (%d)\n", drivenum, fileoffset, FRESULT_str(FR_OK), 512); // fake read
            f_write(&fileD, tmp, strlen(tmp), &bw);
            for (int i = 0; i < 512; ++i) {
                if (i != 0 && i % 32 == 0) {
                    f_write(&fileD, " \n", 2, &bw);
                }
                sprintf(tmp, " %02X", sectorbuffer[i]);
                f_write(&fileD, tmp, strlen(tmp), &bw);
            }
#endif
        } else {
#if PICO_ON_DEVICE
            gpio_put(PICO_DEFAULT_LED_PIN, true);
            UINT bytes_read;
            result = f_read(disk[drivenum].diskfile, sectorbuffer, 512, &bytes_read);
#ifdef BOOT_DISK_RW
            sprintf(tmp, "\ndrv%i f_read(0x%X) result: %s (%d)\n", drivenum, fileoffset, FRESULT_str(result), bytes_read);
            f_write(&fileD, tmp, strlen(tmp), &bw);
            for (int i = 0; i < bytes_read; ++i) {
                if (i != 0 && i % 32 == 0) {
                    f_write(&fileD, " \n", 2, &bw);
                }
                sprintf(tmp, " %02X", sectorbuffer[i]);
                f_write(&fileD, tmp, strlen(tmp), &bw);
            }
#endif
            if (FR_OK != result)
                break;
            gpio_put(PICO_DEFAULT_LED_PIN, false);
#else
            SDL_RWread(disk[drivenum].diskfile, &sectorbuffer[0], 512,1);
#endif
        }
        fileoffset += 512;
        if (is_verify) {
            for (int sectoffset = 0; sectoffset < 512; sectoffset++) {
                // FIXME: segment overflow condition?
                if (read86(memdest++) != sectorbuffer[sectoffset]) {
                    // sector verify failed!
#ifdef BOOT_DISK_RW
                    sprintf(tmp, "drv%i sector verify failed!\n", drivenum);
                    f_write(&fileD, tmp, strlen(tmp), &bw);
                    f_close(&fileD);
#endif
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
#ifdef BOOT_DISK_RW
    f_close(&fileD);
#endif
    if (sectcount && !cursect) {
        CPU_AH = 0x04;    // sector not found
        goto error;       // not even one sector could be read?
    }
    CPU_AL = cursect;
    CPU_FL_CF = 0;
    CPU_AH = 0;
    return;
    error:
    DBG_PRINTF("DISK ERROR %i 0x%x\r\n", drivenum, CPU_AH);
    // AH must be set with the error code
    CPU_AL = 0;
    CPU_FL_CF = 1;
}

void bios_read_boot_sector(int drive, uint16_t dstseg, uint16_t dstoff) {
    bios_readdisk(drive, dstseg, dstoff, 0, 1, 0, 1, 0);
}

static void
bios_writedisk(uint8_t drivenum,
               uint16_t dstseg, uint16_t dstoff,
               uint16_t cyl, uint16_t sect, uint16_t head,
               uint16_t sectcount
 ) {
    uint32_t cursect, memdest, lba;
    size_t fileoffset;
#if PICO_ON_DEVICE
    FRESULT result;
#endif
    //DBG_PRINTF("bios_writedisk\r\n");
    if (!disk[drivenum].inserted) {
        CPU_AH = 0x31;    // no media in drive
        goto error;
    }
    if (!sect || sect > disk[drivenum].sects || cyl >= disk[drivenum].cyls || head >= disk[drivenum].heads) {
        CPU_AH = 0x04;    // sector not found
        goto error;
    }
    /*
    lba = ((uint32_t) cyl * (uint32_t) disk[drivenum].heads + (uint32_t) head) * (uint32_t) disk[drivenum].sects +
          (uint32_t) sect - 1;
    fileoffset = lba * 512;
    */
    fileoffset = chs2ofs(drivenum, cyl, head, sect);
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
#if PICO_ON_DEVICE
    if (disk[drivenum].diskfile != NULL && f_lseek(disk[drivenum].diskfile, fileoffset) != FR_OK) {
        //DBG_PRINTF("Write seek error");
        CPU_AH = 0x04;    // sector not found
        goto error;
    }
    // result = f_lseek(disk[drivenum].diskfile, fileoffset);
#else
    SDL_RWseek(disk[drivenum].diskfile, fileoffset, RW_SEEK_SET);
#endif
    memdest = ((uint32_t) dstseg << 4) + (uint32_t) dstoff;
    for (cursect = 0; cursect < sectcount; cursect++) {
        for (int sectoffset = 0; sectoffset < 512; sectoffset++) {
            // FIXME: segment overflow condition?
            sectorbuffer[sectoffset] = read86(memdest++);
        }

        if (disk[drivenum].data == NULL) {
#if PICO_ON_DEVICE
            UINT writen_bytes;
            result = f_write(disk[drivenum].diskfile, sectorbuffer, 512, &writen_bytes);
//            DBG_PRINTF("drivenum %i :: f_write result: %s (%d)\r\n", drivenum, FRESULT_str(result), writen_bytes);
#else
            SDL_RWwrite(disk[drivenum].diskfile, sectorbuffer, 512, 1);
#endif
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

    //DBG_PRINTF("DISK interrupt function %02Xh\r\n", CPU_AH);
    switch (CPU_AH) {
        case 0: //reset disk system
            //DBG_PRINTF("Disk reset %i\r\n", CPU_DL);
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
/*            DBG_PRINTF("bios_readdisk %i %i %i %i %i %i %i\r\n",
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
/*            DBG_PRINTF("bios_writedisk %i %i %i %i %i %i %i\r\n",   CPU_DL,                // drivenum
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
/*            DBG_PRINTF("bios_verify %i %i %i %i %i %i %i\r\n",
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
                DBG_PRINTF("Requesting int 13h / function 15h for drive %02Xh\n", drivenum);
                CPU_AH = (drivenum & 0x80) ? 3 : 1;		// either "floppy without change line support" (1) or "harddisk" (3)
                CPU_CX = (disk[drivenum].filesize >> 9) >> 16;	// number of blocks, high word
                CPU_DX = (disk[drivenum].filesize >> 9) & 0xFFFF;	// number of blocks, low word
                CPU_AL = CPU_AH;
                CPU_FL_CF = 0;
            } else {
                DBG_PRINTF("Requesting int 13h / function 15h for drive %02Xh no such device\n", CPU_DL);
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
            //DBG_PRINTF("BIOS: unknown Int 13h service was requested: %02Xh\r\n", CPU_AH);
            CPU_FL_CF = 1;    // unknown function was requested?
            //CPU_AH = 1;
            break;
    }
    lastdiskah[CPU_DL] = CPU_AH;
    lastdiskcf[CPU_DL] = CPU_FL_CF;
    if (CPU_DL & 0x80)
        RAM[0x474] = CPU_AH;
}
