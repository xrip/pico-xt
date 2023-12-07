#include "hma.h"
#include "ram.h"

void map_hma_ram_pages() {
    for (uint8_t ba = (HMA_START_ADDRESS >> 15); ba <= (BASE_XMS_ADDR >> 15); ++ba) {
        update_segment_map_hma(ba,
            PSRAM_AVAILABLE ? write8psram  : ram_page_write ,
            PSRAM_AVAILABLE ? write16psram : ram_page_write16,
            PSRAM_AVAILABLE ? read8psram   : ram_page_read  ,
            PSRAM_AVAILABLE ? read16psram  : ram_page_read16
        );
    }
}

void unmap_hma_ram_pages() {
    for (uint8_t ba = (HMA_START_ADDRESS >> 15); ba <= (BASE_XMS_ADDR >> 15); ++ba) {
        update_segment_map_hma(ba, 0, 0, 0, 0);
    }
}
