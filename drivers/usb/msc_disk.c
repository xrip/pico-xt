/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "bsp/board_api.h"
#include "tusb.h"
#include "usb.h"
#include "emulator.h"

#include "fdd.h"
#include "startup_disk.h"

char* fdd0_rom() {
  return FDD0;
}

char* fdd1_rom() {
#if ROM_DRIVE_B
  return FDD1;
#else
  return NULL;
#endif
}

size_t fdd0_sz() {
  return sizeof FDD0;
}

size_t fdd1_sz() {
#if ROM_DRIVE_B
  return sizeof FDD1;
#else
  return 0;
#endif
}

#if CFG_TUD_MSC

// whether host does safe-eject
static bool ejectedDrv[4] = { false, false, false, false };

const uint32_t min_rom_block = 4096;
const uint32_t sec_per_block = min_rom_block / DISK_BLOCK_SIZE;

// Invoked when received SCSI_CMD_INQUIRY
// Application fill vendor id, product id and revision with string up to 8, 16, 4 characters respectively
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4]) {
  // char tmp[81]; sprintf(tmp, "tud_msc_inquiry_cb: %d", lun); logMsg(tmp);
  switch(lun) {
	case 0: {
        const char vid[] = "Pico-XT A:";
        memcpy(vendor_id, vid, strlen(vid));
	}
	break;
#if 1
	case 1: {
        const char vid[] = "Pico-XT B:";
        memcpy(vendor_id, vid, strlen(vid));
	}
	break;
	case 2: {
        const char vid[] = "Pico-XT C:";
        memcpy(vendor_id, vid, strlen(vid));
	}
	break;
	case 3: {
        const char vid[] = "Pico-XT SD-Card";
        memcpy(vendor_id, vid, strlen(vid));
	}
	break;
  }
  const char pid[] = "Mass Storage";
  const char rev[] = "1.0";
  memcpy(product_id , pid, strlen(pid));
  memcpy(product_rev, rev, strlen(rev));
}

// Invoked when received Test Unit Ready command.
// return true allowing host to read/write this LUN e.g SD card inserted
bool tud_msc_test_unit_ready_cb(uint8_t lun) {
  // char tmp[80]; sprintf(tmp, "tud_msc_test_unit_ready_cb(%d)", lun); logMsg(tmp);
  // RAM disk is ready until ejected
  if (ejectedDrv[lun]) {
    // Additional Sense 3A-00 is NOT_FOUND
    tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
    return false;
  }
  return true;
}
bool tud_msc_ejected() {
  int ejected_cnt = 0;
  for (int i = 0; i < sizeof(ejectedDrv); ++i) {
    if (ejectedDrv[i])
      ejected_cnt++;
  }
  // char tmp[80]; sprintf(tmp, "tud_msc_ejected: %d", ejected_cnt); logMsg(tmp);
  if(ejected_cnt >= 2) {       // fdd may not be proper unmount, so it is enought to eject 2 from 4 devices to eject others
    set_tud_msc_ejected(true); // force eject remaining drive
    return true;
  }
  return false;
}
void set_tud_msc_ejected(bool v) {
  // char tmp[80]; sprintf(tmp, "set_tud_msc_ejected: %s", v ? "true" : "false"); logMsg(tmp);
  for (int i = 0; i < sizeof(ejectedDrv); ++i)
    ejectedDrv[i] = v;
}

extern FIL * getFileC();

// Invoked when received SCSI_CMD_READ_CAPACITY_10 and SCSI_CMD_READ_FORMAT_CAPACITY to determine the disk size
// Application update block count and block size
void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size) {
  // char tmp[80]; sprintf(tmp, "tud_msc_capacity_cb(%d) block_count: %d block_size: %d r: %d", lun, block_count, block_size); logMsg(tmp);
  switch(lun) {
	case 0: {
      int r = getFileA_sz();
      *block_count = (r ? r : sizeof(FDD0)) / DISK_BLOCK_SIZE;
      *block_size  = DISK_BLOCK_SIZE;
	}
	break;
	case 1: {
      int r = getFileB_sz();
  #if ROM_DRIVE_B
      *block_count = (r ? r : sizeof(FDD1)) / DISK_BLOCK_SIZE;
  #else
      *block_count = r / DISK_BLOCK_SIZE;
  #endif
      *block_size  = DISK_BLOCK_SIZE;
	}
	break;
	case 2: {
      *block_count = getFileC_sz() / DISK_BLOCK_SIZE;
      *block_size  = DISK_BLOCK_SIZE;
	}
	break;
	case 3: {
      DWORD dw;
      DRESULT dio = disk_ioctl(0, GET_SECTOR_COUNT, &dw);
      if (dio == RES_OK) {
        *block_count = dw;
      } else {
        char tmp[80]; sprintf(tmp, "disk_ioctl(GET_SECTOR_COUNT) failed: %d", dio); logMsg(tmp);
        *block_count = 0;
        return;
      }
      *block_size = FF_MAX_SS;
	}
	break;
  }
}

// Invoked when received Start Stop Unit command
// - Start = 0 : stopped power mode, if load_eject = 1 : unload disk storage
// - Start = 1 : active mode, if load_eject = 1 : load disk storage
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject) {
  //char tmp[81]; sprintf(tmp, "power_condition: 0x%X start: %d load_eject: %d", power_condition, start, load_eject); logMsg(tmp);
  (void) lun;
  (void) power_condition;
  if ( load_eject ) {
    if (start) {
      // load disk storage
    } else {
      // unload disk storage
      ejectedDrv[lun] = true;
    }
  }
  return true;
}

extern bool img_disk_read_sec(int drv, BYTE * buffer, LBA_t lba);
// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and return number of copied bytes.
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
  // char tmp[80]; sprintf(tmp, "tud_msc_read10_cb(%d, %d, %d, %d)", lun, lba, offset, bufsize); logMsg(tmp);
  uint8_t* rom = 0;
  size_t rom_sz;
  switch(lun) {
	case 0: {
    if (getFileA_sz()) {
		  return img_disk_read_sec(0, buffer, lba) ? bufsize : -1;
    }
		rom = FDD0;
		rom_sz = sizeof(FDD0);
 	}
	break;
	case 1: {
  #if ROM_DRIVE_B
    if (getFileB_sz()) {
		  return img_disk_read_sec(1, buffer, lba) ? bufsize : -1;
    }
		rom = FDD1;
		rom_sz = sizeof(FDD1);
  #else
    return img_disk_read_sec(1, buffer, lba) ? bufsize : -1;
  #endif
	}
	break;
#endif

	case 2: {
		  return img_disk_read_sec(2, buffer, lba) ? bufsize : -1;
  	}
	break;
	case 3: {
      return disk_read(0, buffer, lba, 1) == RES_OK ? bufsize : -1;
	  }
  }
  if ( lba >= rom_sz / DISK_BLOCK_SIZE || !rom) {
	    return -1;
  }
  char* rom_lba = rom + lba * DISK_BLOCK_SIZE + offset;
  memcpy(buffer, rom_lba, bufsize);
  if (ejectedDrv[lun]) { // read action means - it is mount back
    ejectedDrv[lun] = false;
  }
  return (int32_t) bufsize;
}

bool sd_card_writable() {
    DSTATUS ds = disk_status(0);
    DSTATUS rs = ds & 0x04/*STA_PROTECT*/;
    // char tmp[80]; sprintf(tmp, "tud_msc_is_writable_cb(1) ds: %d rs: %d r: %d", ds, rs, !rs); logMsg(tmp);
    return !rs; // TODO: sd-card write protected ioctl?
}

bool tud_msc_is_writable_cb (uint8_t lun) {
  switch(lun) {
	case 0:
	  return getFileA_sz() ? sd_card_writable() : false;
	case 1:
	  return getFileB_sz() ? sd_card_writable() : false;
	case 2:
	  return getFileC_sz() ? sd_card_writable() : false;    
  }
  return sd_card_writable();
}

extern bool img_disk_write_sec(int drv, BYTE * buffer, LBA_t lba);

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and return number of written bytes
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
  // char tmp[80]; sprintf(tmp, "tud_msc_write10_cb(%d, %d, %d, %d)", lun, lba, offset, bufsize); logMsg(tmp);
  switch(lun) {
	case 0:
	    return img_disk_write_sec(0, buffer, lba) ? bufsize : -1;
	case 1:
	    return img_disk_write_sec(1, buffer, lba) ? bufsize : -1;
	case 2:
	    return img_disk_write_sec(2, buffer, lba) ? bufsize : -1;
	case 3: {
      return disk_write(0, buffer, lba, 1) == 0 ? bufsize : -1;
	}
  }
  return (int32_t) bufsize;
}

// Callback invoked when received an SCSI command not in built-in list below
// - READ_CAPACITY10, READ_FORMAT_CAPACITY, INQUIRY, MODE_SENSE6, REQUEST_SENSE
// - READ10 and WRITE10 has their own callbacks
int32_t tud_msc_scsi_cb (uint8_t lun, uint8_t const scsi_cmd[16], void* buffer, uint16_t bufsize) {
  // char tmp[81]; sprintf(tmp, "scsi_cmd0(%d) 0x%X 1: 0x%X 2: 0x%X 3: 0x%X ...", lun, scsi_cmd[0], scsi_cmd[1], scsi_cmd[2], scsi_cmd[3]); logMsg(tmp);
  // read10 & write10 has their own callback and MUST not be handled here
  void const* response = NULL;
  int32_t resplen = 0;
  // most scsi handled is input
  bool in_xfer = true;
  switch (scsi_cmd[0]) {
    default:
      // Set Sense = Invalid Command Operation
      tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);
      // negative means error -> tinyusb could stall and/or response with failed status
      resplen = -1;
    break;
  }
  // return resplen must not larger than bufsize
  if ( resplen > bufsize ) resplen = bufsize;
  if ( response && (resplen > 0) ) {
    if(in_xfer) {
      memcpy(buffer, response, (size_t) resplen);
    } else {
      // SCSI output
    }
  }
  return (int32_t) resplen;
}

#endif
