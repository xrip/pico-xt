#ifndef PICO_USB_DRIVE
#define PICO_USB_DRIVE

#include "ff.h"
#include "diskio.h"

void in_flash_drive();

char* fdd0_rom();
char* fdd1_rom();
size_t fdd0_sz();
size_t fdd1_sz();

// from vga.h
void logMsg(char * msg);

// msc_disk.c
_Bool tud_msc_test_ejected();
enum {
  DISK_BLOCK_SIZE = 512,
  FAT_OFFSET = 0x1000
};
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize);
void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size);

#endif
