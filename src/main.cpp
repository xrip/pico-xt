#pragma GCC optimize("Ofast")

extern "C" {
#include "cpu8086.h"
}
#if PICO_ON_DEVICE
#include <pico/time.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>
#include <hardware/vreg.h>
#include "pico/stdio.h"
extern "C" {
#include "vga.h"
#include "ps2.h"
}
#else
#define SDL_MAIN_HANDLED

#include "SDL2/SDL.h"
#include "VGA_ROM_F16.h"

SDL_Window *window;

SDL_Surface *screen;
#endif

bool runing = true;

uint32_t dosColorPalette[16] = {
        0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
        0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
        0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
        0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF
};

#if PICO_ON_DEVICE

struct semaphore vga_start_semaphore;
/* Renderer loop on Pico's second core */
void __time_critical_func(render_core)() {
    initVGA();

    setVGAbuf(VRAM, 320, 200);

    setVGA_text_buf(VRAM);
    setVGA_bg_color(0);
    setVGAbuf_pos(0, 0);
    setVGA_color_flash_mode(true, true);
    for (int i = 0; i < 255; ++i) {
        setVGA_color_palette(i, dosColorPalette[i & 0x0F]);
    }


    setVGAmode(VGA640x480_text_80_30);

    sem_acquire_blocking(&vga_start_semaphore);

}
#else

static int RendererThread(void *ptr) {

    while (runing) {
        exec86(200);
        //tick(1000);
        //pollkb();
    }
    return 0;
}

#endif
#if !PICO_ON_DEVICE
const uint8_t cga_palette[16][3] = { //R, G, B
        { 0x00, 0x00, 0x00 }, //black
        { 0x00, 0x00, 0xAA }, //blue
        { 0x00, 0xAA, 0x00 }, //green
        { 0x00, 0xAA, 0xAA }, //cyan
        { 0xAA, 0x00, 0x00 }, //red
        { 0xAA, 0x00, 0xAA }, //magenta
        { 0xAA, 0x55, 0x00 }, //brown
        { 0xAA, 0xAA, 0xAA }, //light gray
        { 0x55, 0x55, 0x55 }, //dark gray
        { 0x55, 0x55, 0xFF }, //light blue
        { 0x55, 0xFF, 0x55 }, //light green
        { 0x55, 0xFF, 0xFF }, //light cyan
        { 0xFF, 0x55, 0x55 }, //light red
        { 0xFF, 0x55, 0xFF }, //light magenta
        { 0xFF, 0xFF, 0x55 }, //yellow
        { 0xFF, 0xFF, 0xFF }  //white
};

const uint8_t cga_gfxpal[2][2][4] = { //palettes for 320x200 graphics mode
        {
                { 0, 2, 4, 6 }, //normal palettes
                { 0, 3, 5, 7 }
        },
        {
                { 0, 10, 12, 14 }, //intense palettes
                { 0, 11, 13, 15 }
        }
};

#define cga_color(c) ((uint32_t)cga_palette[c][2] | ((uint32_t)cga_palette[c][1]<<8) | ((uint32_t)cga_palette[c][0]<<16))

#endif

int main() {
//    init86();
#if PICO_ON_DEVICE
    vreg_set_voltage(VREG_VOLTAGE_1_15);
    sleep_ms(33);
    set_sys_clock_khz(266000, true);
    sleep_ms(10);
    stdio_init_all();
    sleep_ms(10);
    Init_kbd();
    sem_init(&vga_start_semaphore, 0, 1);
    multicore_launch_core1(render_core);
    sem_release(&vga_start_semaphore);

    sleep_ms(50);

#else
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);


    window = SDL_CreateWindow("pico-xt",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              640, 400,
                              SDL_WINDOW_SHOWN);

    screen = SDL_GetWindowSurface(window);
    unsigned int *pixels = (unsigned int *) screen->pixels;

    if (!SDL_CreateThread(RendererThread, "renderer", NULL)) {
        fprintf(stderr, "Could not create the renderer thread: %s\n", SDL_GetError());
        return -1;
    }
#endif
    reset86();
    //draw_text("hello world!", 0, 0, 15, 1);

    //while (1) {};


    //draw_text("TEST", 0, 0, 166, 0);
    while (runing) {
#if !PICO_ON_DEVICE
        handleinput();
        if (videomode == 3 || videomode == 0x56) {
//            SDL_SetWindowSize(window, 640, 400);
            for (uint16_t y = 0; y < 400; y++) {
                for (uint8_t x = 0; x < 80; x++) {
                    char c = VRAM[/*0xB8000 + */(y / 16) * 160 + x * 2 + 0];
                    //printf("%c", c);
                    uint8_t glyph_row = VGA_ROM_F16[c * 16 + y % 16];
                    uint8_t color = VRAM[/*0xB8000 + */(y / 16) * 160 + x * 2 + 1];

                    for (uint8_t bit = 0; bit < 8; bit++) {
                        if ((glyph_row >> bit) & 1) {
                            pixels[y * 640 + (8 * x + bit)] = dosColorPalette[color & 0x0F];
                        } else {
                            pixels[y * 640 + (8 * x + bit)] = dosColorPalette[color >> 4];
                        }
                    }
                }
            }
        } else {
            const uint8_t vidmode = 4;

            uint32_t *pix = pixels;
            uint32_t usepal = 1 ;// (portram[0x3D9]>>5) & 1;
            uint32_t intensity = 0; // ( (portram[0x3D9]>>4) & 1) << 3;
            for (int y = 0; y < 200; y++) {
                for (int x = 0; x < 320; x++) {
                    uint32_t charx = x;
                    uint32_t chary = y;
                    uint32_t vidptr = /*0xB8000 + */( (chary>>1) * 80) + ( (chary & 1) * 8192) + (charx >> 2);
                    uint32_t curpixel = VRAM[vidptr];
                    uint32_t color;
                    switch (charx & 3) {
                        case 3:
                            curpixel = curpixel & 3;
                            break;
                        case 2:
                            curpixel = (curpixel>>2) & 3;
                            break;
                        case 1:
                            curpixel = (curpixel>>4) & 3;
                            break;
                        case 0:
                            curpixel = (curpixel>>6) & 3;
                            break;
                    }
                    if (vidmode==4) {
                        curpixel = curpixel * 2 + usepal + intensity;
                        if (curpixel == (usepal + intensity) )
                            curpixel = 0;
                        color = cga_color(curpixel);
                        //prestretch[y][x] = color;
                        *pix++ = color;
                    } else {
                        curpixel = curpixel * 63;
                        color = cga_color(curpixel);
                        //prestretch[y][x] = color;
                        *pix++ = color;
                    }
                }
                pix += 320;
            }
        }
        SDL_UpdateWindowSurface(window);
#else
        exec86(1500);
#endif
    }
    return 0;
}
