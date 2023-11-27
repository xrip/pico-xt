#include "ram_page.h"
#include "f_util.h"
#include "ff.h"
#include "emm.h"
#include <pico.h>
#include <pico/stdlib.h>

uint16_t RAM_PAGES[RAM_BLOCKS] = { 0 };

static uint32_t get_ram_page_for(const uint32_t addr32);

#if BOOT_DEBUG_ACC
void logMsg(char* tmp);
#endif

uint8_t ram_page_read(uint32_t addr32) {
    const register uint32_t ram_page = get_ram_page_for(addr32);
    const register uint32_t addr_in_page = addr32 & RAM_IN_PAGE_ADDR_MASK;
#if BOOT_DEBUG_ACC
    uint8_t res = RAM[(ram_page * RAM_PAGE_SIZE) + addr_in_page];
    if (addr32 >= 0x800000) {
        char tmp[40]; sprintf(tmp, "R 8 %X: %02Xh", addr32, res); logMsg(tmp);
    }
    return res;
#else
    return RAM[(ram_page * RAM_PAGE_SIZE) + addr_in_page];
#endif
}

inline static uint16_t read16arr(uint8_t* arr, uint32_t base_addr, uint32_t addr32) {
    register uint8_t* ptr = arr + addr32 - base_addr;
    register uint16_t b1 = *ptr++;
    register uint16_t b0 = *ptr;
    return b1 | (b0 << 8);
}

uint16_t ram_page_read16(uint32_t addr32) {
    const register uint32_t ram_page = get_ram_page_for(addr32);
    const register uint32_t addr_in_page = addr32 & RAM_IN_PAGE_ADDR_MASK;
#if BOOT_DEBUG_ACC
    uint16_t res = read16arr(RAM, 0, (ram_page * RAM_PAGE_SIZE) + addr_in_page);
    if (addr32 >= BOOT_DEBUG_ACC) {
        char tmp[40]; sprintf(tmp, "R16 %X: %04Xh", addr32, res); logMsg(tmp);
    }
    return res;
#else
    return read16arr(RAM, 0, (ram_page * RAM_PAGE_SIZE) + addr_in_page);
#endif
}

void ram_page_write(uint32_t addr32, uint8_t value) {
    register uint32_t ram_page = get_ram_page_for(addr32);
    register uint32_t addr_in_page = addr32 & RAM_IN_PAGE_ADDR_MASK;
    RAM[(ram_page * RAM_PAGE_SIZE) + addr_in_page] = value;
    register uint16_t ram_page_desc = RAM_PAGES[ram_page];
    if (!(ram_page_desc & 0x8000)) {
        // if higest (15) bit is set, it means - the page has changes
        RAM_PAGES[ram_page] = ram_page_desc | 0x8000; // mark it as changed - bit 15
    }
#if BOOT_DEBUG_ACC
    if (addr32 >= BOOT_DEBUG_ACC) {
        char tmp[40]; sprintf(tmp, "W 8 %X: %02Xh", addr32, value); logMsg(tmp);
    }
#endif
}

void ram_page_write16(uint32_t addr32, uint16_t value) {
    register uint32_t ram_page = get_ram_page_for(addr32);
    register uint32_t addr_in_page = addr32 & RAM_IN_PAGE_ADDR_MASK;
    register uint8_t* addr_in_ram = RAM + ram_page * RAM_PAGE_SIZE + addr_in_page;
    *addr_in_ram++     = (uint8_t) value;
    *addr_in_ram       = (uint8_t)(value >> 8);
    register uint16_t ram_page_desc = RAM_PAGES[ram_page];
    if (!(ram_page_desc & 0x8000)) {
        // if higest (15) bit is set, it means - the page has changes
        RAM_PAGES[ram_page] = ram_page_desc | 0x8000; // mark it as changed - bit 15
    }
#if BOOT_DEBUG_ACC
    if (addr32 >= BOOT_DEBUG_ACC) {
        char tmp[40]; sprintf(tmp, "R16 %X: %04Xh", addr32, value); logMsg(tmp);
    }
#endif
}

static uint16_t oldest_ram_page = 1;
static uint16_t last_ram_page = 0;
static uint32_t last_lba_page = 0;

uint32_t get_ram_page_for(const uint32_t addr32) {
    const register uint32_t lba_page = addr32 / RAM_PAGE_SIZE; // 4KB page idx
    if (last_lba_page == lba_page) {
        return last_ram_page;
    }
    last_lba_page = lba_page;
    for (register uint32_t ram_page = 1; ram_page < RAM_BLOCKS; ++ram_page) {
        register uint16_t ram_page_desc = RAM_PAGES[ram_page];
        register uint16_t lba_page_in_ram = ram_page_desc & 0x7FFF; // 14-0 - max 32k keys for 4K LBA bloks
        if (lba_page_in_ram == lba_page) {
            last_ram_page = ram_page;
            return ram_page;
        }
    }
    // char tmp[40]; sprintf(tmp, "VRAM page: 0x%X", lba_page); logMsg(tmp);
    // rolling page usage
    uint16_t ram_page = oldest_ram_page++;
    if (oldest_ram_page >= RAM_BLOCKS - 1) oldest_ram_page = 1; // do not use first page (id == 0)
    uint16_t ram_page_desc = RAM_PAGES[ram_page];
    bool ro_page_was_found = !(ram_page_desc & 0x8000);
    // higest (15) bit is set, it means - the page has changes (RW page)
    uint32_t old_lba_page = ram_page_desc & 0x7FFF; // 14-0 - max 32k keys for 4K LBA bloks
    RAM_PAGES[ram_page] = lba_page;
    if (ro_page_was_found) {
        // just replace RO page (faster than RW flush to flash)
        // sprintf(tmp, "1 RAM page 0x%X / VRAM page: 0x%X", ram_page, lba_page); logMsg(tmp);
        uint32_t ram_page_offset = ((uint32_t)ram_page) * RAM_PAGE_SIZE;
        uint32_t lba_page_offset = lba_page * RAM_PAGE_SIZE;
        read_vram_block(RAM + ram_page_offset, lba_page_offset, RAM_PAGE_SIZE);
        last_ram_page = ram_page;
        return ram_page;
    }
    // Lets flush found RW page to flash
    // sprintf(tmp, "2 RAM page 0x%X / VRAM page: 0x%X", ram_page, lba_page); logMsg(tmp);
    uint32_t ram_page_offset = ram_page * RAM_PAGE_SIZE;
    uint32_t lba_page_offset = old_lba_page * RAM_PAGE_SIZE;
    // sprintf(tmp, "2 RAM offs 0x%X / VRAM offs: 0x%X", ram_page_offset, lba_page_offset); logMsg(tmp);
    flush_vram_block(RAM + ram_page_offset, lba_page_offset, RAM_PAGE_SIZE);
    // use new page:
    lba_page_offset = lba_page * RAM_PAGE_SIZE;
    read_vram_block(RAM + ram_page_offset, lba_page_offset, RAM_PAGE_SIZE);
    last_ram_page = ram_page;
    return ram_page;
}

static const char* path = "\\XT\\pagefile.sys";
static FIL file;

bool init_vram() {
    logMsg((char *)"Create <SD-card>\\XT\\pagefile.sys");
    FRESULT result = f_open(&file, path, FA_READ | FA_WRITE | FA_CREATE_ALWAYS);
    if (result == FR_OK) {
        result = f_lseek(&file, TOTAL_VIRTUAL_MEMORY_KBS * 1024);
        if (result != FR_OK) {
            logMsg((char *)"Unable to init <SD-card>\\XT\\pagefile.sys");
            return false;
        }
    } else {
        logMsg((char *)"Unable to create <SD-card>\\XT\\pagefile.sys");
        return false;
    }
    f_close(&file);
    result = f_open(&file, path, FA_READ | FA_WRITE);
    if (result != FR_OK) {
        logMsg((char *)"Unable to open <SD-card>\\XT\\pagefile.sys");
    }
    logMsg((char *)"pagefile.sys is initialized");
    return true;
}

FRESULT vram_seek(FIL* fp, uint32_t file_offset) {
    FRESULT result = f_lseek(&file, file_offset);
    if (result != FR_OK) {
        char tmp[40];
        result = f_open(&file, path, FA_READ | FA_WRITE);
        if (result != FR_OK) {
            sprintf(tmp, "Unable to open pagefile.sys: %s (%d)", FRESULT_str(result), result);
            logMsg(tmp);
            return result;
        }
        sprintf(tmp, "Failed to f_lseek: %s (%d)", FRESULT_str(result), result);
        logMsg(tmp);
    }
    return result;
}

void read_vram_block(char* dst, uint32_t file_offset, uint32_t sz) {
    gpio_put(PICO_DEFAULT_LED_PIN, true);
    char tmp[40];
    if (file_offset >= 0x100000) {
        sprintf(tmp, "Read  pagefile 0x%X<-0x%X", dst, file_offset);
        logMsg(tmp);
    }
    FRESULT result = vram_seek(&file, file_offset);
    if (result != FR_OK) {
        return;
    }
    UINT br;
    result = f_read(&file, dst, sz, &br);
    if (result != FR_OK) {
        sprintf(tmp, "Failed to f_read: %s (%d)", FRESULT_str(result), result);
        logMsg(tmp);
    }
    gpio_put(PICO_DEFAULT_LED_PIN, false);
}

void flush_vram_block(const char* src, uint32_t file_offset, uint32_t sz) {
    gpio_put(PICO_DEFAULT_LED_PIN, true);
    char tmp[40];
    if (file_offset >= 0x100000) {
        sprintf(tmp, "Flush pagefile 0x%X->0x%X", src, file_offset);
        logMsg(tmp);
    }
    FRESULT result = vram_seek(&file, file_offset);
    if (result != FR_OK) {
        return;
    }
    UINT bw;
    result = f_write(&file, src, sz, &bw);
    if (result != FR_OK) {
        sprintf(tmp, "Failed to f_write: %s (%d)", FRESULT_str(result), result);
        logMsg(tmp);
    }
    gpio_put(PICO_DEFAULT_LED_PIN, false);
}

