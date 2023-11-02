#include "emu.h"
#include "FD0.h"

#define FDD_144M

struct struct_drive {
    uint16_t cyls;
    uint16_t sects;
    uint16_t heads;
    uint8_t inserted;
} disk[4];
uint8_t sectorbuffer[512];

extern uint8_t hdcount, fdcount;

uint8_t insertdisk(uint8_t drivenum) {
    if (drivenum & 0x80) {
        drivenum -= 126;
        disk[drivenum].sects = 63;
        disk[drivenum].heads = 16;
        disk[drivenum].cyls = 1023; //up to 512 MB
        hdcount++;
    } else {
#ifdef FDD_144M
        printf("FDD\r\n");
        disk[drivenum].cyls = 80;
        disk[drivenum].sects = 18;
        disk[drivenum].heads = 2;
#endif
#ifdef FDD_122M
        disk[drivenum].cyls = 80;
        disk[drivenum].sects = 15;
        disk[drivenum].heads = 2;
#endif
#ifdef FDD_720K
        disk[drivenum].cyls = 80;
        disk[drivenum].sects = 9;
        disk[drivenum].heads = 2;
#endif
#ifdef FDD_360K
        disk[drivenum].cyls = 40;
        disk[drivenum].sects = 9;
        disk[drivenum].heads = 2;
#endif
#ifdef FDD_320K
        disk[drivenum].cyls = 40;
        disk[drivenum].sects = 8;
        disk[drivenum].heads = 2;
#endif
#ifdef FDD_180K
        disk[drivenum].cyls = 40;
        disk[drivenum].sects = 9;
        disk[drivenum].heads = 1;
#endif
        fdcount++;
    }
    disk[drivenum].inserted = 1;
    return 0;
}

void ejectdisk(uint8_t drivenum) {
    if (drivenum & 0x80) drivenum -= 126;
    disk[drivenum].inserted = 0;
}

extern uint16_t ramseg;

uint8_t sectdone;

void getsect(uint32_t lba, uint8_t *dst) {
    memcpy(dst, &FD0[lba], 512);
}

void putsect(uint32_t lba, uint8_t *src) {
    //card.writeBlock(lba, src);
}

// Call this ONLY if all parameters are valid! There is no check here!
static size_t chs2ofs(int drivenum, int cyl, int head, int sect) {
    return (((size_t) cyl * (size_t) disk[drivenum].heads + (size_t) head) * (size_t) disk[drivenum].sects +
            (size_t) sect - 1) * 512UL;
}

void readdisk(uint8_t drivenum, uint16_t dstseg, uint16_t dstoff, uint16_t cyl, uint16_t sect, uint16_t head,
                     uint16_t sectcount) {
    uint32_t cursect, memdest, lba;
    size_t fileoffset;

    if (!disk[drivenum].inserted) {
        CPU_AH = 0x31;    // no media in drive
        goto error;
    }
    if (!sect || sect > disk[drivenum].sects || cyl >= disk[drivenum].cyls || head >= disk[drivenum].heads) {
        CPU_AH = 0x04;    // sector not found
        goto error;
    }

    fileoffset = chs2ofs(drivenum, cyl, head, sect);

/*
    if (fileoffset > disk[drivenum].filesize) {
        CPU_AH = 0x04;    // sector not found
        goto error;
    }*/

    memdest = ((uint32_t) dstseg << 4) + (uint32_t) dstoff;
    // for the readdisk function, we need to use write86 instead of directly fread'ing into
    // the RAM array, so that read-only flags are honored. otherwise, a program could load
    // data from a disk over BIOS or other ROM code that it shouldn't be able to.

    for (cursect = 0; cursect < sectcount; cursect++) {
        memcpy(sectorbuffer, &FD0[fileoffset], 512);

        fileoffset += 512;

        for (int sectoffset = 0; sectoffset < 512; sectoffset++) {
            // FIXME: segment overflow condition?
            write86(memdest++, sectorbuffer[sectoffset]);
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

void readdisk1(uint8_t drivenum, uint16_t dstseg, uint16_t dstoff, uint16_t cyl, uint16_t sect, uint16_t head,
               uint16_t sectcount) {
    uint32_t memdest, goodsects, sectoffset, lba;
    if ((sect == 0) || !disk[drivenum].inserted) return;

    lba = chs2ofs(drivenum, cyl, head, sect);
    memdest = ((uint32_t) dstseg << 4) + (uint32_t) dstoff;

    for (goodsects = 0; goodsects < sectcount; goodsects++) {
        memcpy(sectorbuffer, &FD0[lba], 512);
        //getsect(lba * 512, sectorbuffer);
        //memdest = (uint32_t) dstseg * 16 + (uint32_t) dstoff;
        for (sectoffset = 0; sectoffset < 512; sectoffset++) {
            write86(memdest++, sectorbuffer[sectoffset]);
            //Serial.write(sectorbuffer[dummy]);
        }
        lba += 512;
    }
    cf = 0;
    regs.byteregs[regah] = 0;
    regs.byteregs[regal] = sectcount;
}

void writedisk(uint8_t drivenum, uint16_t dstseg, uint16_t dstoff, uint16_t cyl, uint16_t sect, uint16_t head,
               uint16_t sectcount) {
    uint32_t memdest, goodsects, dummy, lba;
    if ((sect == 0) || !disk[drivenum].inserted) return;
#ifdef MEGA
    SPI.setClockDivider(SPI_CLOCK_SDCARD);
#endif
    lba = ((long) cyl * (long) disk[drivenum].heads + (long) head) * (long) disk[drivenum].sects + (long) sect - 1;
    for (goodsects = 0; goodsects < sectcount; goodsects++) {
        memdest = (uint32_t) dstseg * 16 + (uint32_t) dstoff;
        for (dummy = 0; dummy < 512; dummy++) {
            sectorbuffer[dummy] = read86(memdest++);
        }
        //card.erase(lba, lba);
        putsect(lba, sectorbuffer);
        dstoff += 512;
        lba++;
    }
    cf = 0;
    regs.byteregs[regah] = 0;
    regs.byteregs[regal] = sectcount;
#ifdef MEGA
    SPI.setClockDivider(SPI_CLOCK_SPIRAM);
#endif
}

void diskhandler() {
    static uint8_t lastdiskah[4], lastdiskcf[4];
    uint8_t drivenum;
    drivenum = regs.byteregs[regdl];
    if (drivenum & 0x80) drivenum -= 126;
    switch (regs.byteregs[regah]) {
        case 0: //reset disk system
            regs.byteregs[regah] = 0;
            cf = 0; //useless function in an emulator. say success and return.
            break;
        case 1: //return last status
            regs.byteregs[regah] = lastdiskah[drivenum];
            cf = lastdiskcf[drivenum];
            return;
        case 2: //read sector(s) into memory
            if (disk[drivenum].inserted) {
                printf("DISK %i read ", drivenum);
                readdisk(drivenum, segregs[reges], getreg16(regbx),
                         (uint16_t) regs.byteregs[regch] + ((uint16_t) regs.byteregs[regcl] / 64) * 256,
                         regs.byteregs[regcl] & 63, regs.byteregs[regdh], regs.byteregs[regal]);
                cf = 0;
                regs.byteregs[regah] = 0;
            } else {
                cf = 1;
                regs.byteregs[regah] = 1;
            }
            break;
        case 3: //write sector(s) from memory
            if (disk[drivenum].inserted) {
                writedisk(drivenum, segregs[reges], getreg16(regbx),
                          regs.byteregs[regch] + (regs.byteregs[regcl] / 64) * 256, regs.byteregs[regcl] & 63,
                          regs.byteregs[regdh], regs.byteregs[regal]);
                cf = 0;
                regs.byteregs[regah] = 0;
            } else {
                cf = 1;
                regs.byteregs[regah] = 1;
            }
            break;
        case 4:
        case 5: //format track
            cf = 0;
            regs.byteregs[regah] = 0;
            break;
        case 8: //get drive parameters
            if (disk[drivenum].inserted) {
                cf = 0;
                regs.byteregs[regah] = 0;
                regs.byteregs[regch] = disk[drivenum].cyls - 1;
                regs.byteregs[regcl] = disk[drivenum].sects & 63;
                regs.byteregs[regcl] = regs.byteregs[regcl] + (disk[drivenum].cyls / 256) * 64;
                regs.byteregs[regdh] = disk[drivenum].heads - 1;
                //segregs[reges] = 0; regs.wordregs[regdi] = 0x7C0B; //floppy parameter table
                if (drivenum < 2) {
                    regs.byteregs[regbl] = 4; //else regs.byteregs[regbl] = 0;
                    regs.byteregs[regdl] = 2;
                } else regs.byteregs[regdl] = hdcount;
            } else {
                cf = 1;
                regs.byteregs[regah] = 0xAA;
            }
            break;
        default:
            cf = 1;
    }
    lastdiskah[drivenum] = regs.byteregs[regah];
    lastdiskcf[drivenum] = cf;
    if (regs.byteregs[regdl] & 0x80) write86(0x474, regs.byteregs[regah]);
}

