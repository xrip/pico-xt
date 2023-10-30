#pragma GCC optimize("Ofast")

extern "C" {
#include "cpu8086.h"
}
#include "cga.h"
#if PICO_ON_DEVICE
#ifndef OVERCLOCKING
#define OVERCLOCKING 270
#endif

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
#include "../drivers/vga-nextgen/fnt8x16.h"

SDL_Window *window;

SDL_Surface *screen;
#endif

bool runing = true;

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


    setVGAmode(VGA640x480_text_80_30);
    for (int i = 0; i < 16; ++i) {
        setVGA_color_palette(i, cga_color(i*2));
    }
    sem_acquire_blocking(&vga_start_semaphore);

    uint8_t tick50ms = 0;
    while (true) {
        read_keyboard();
        sleep_ms(50);
        if (tick50ms == 0 || tick50ms == 10) {
            cursor_blink_state ^= 1;
        }

        if (tick50ms < 20) {
            tick50ms++;
        } else {
            tick50ms = 0;
        }
    }

}

#else
extern uint16_t pr3D9;

static int RendererThread(void *ptr) {

    while (runing) {
        exec86(200);
    }
    return 0;
}

#endif

int main() {
#if PICO_ON_DEVICE
#if (OVERCLOCKING > 270)
    hw_set_bits(&vreg_and_chip_reset_hw->vreg, VREG_AND_CHIP_RESET_VREG_VSEL_BITS);
    sleep_ms(33);

    set_sys_clock_khz(OVERCLOCKING * 1000, true);
#else
    vreg_set_voltage(VREG_VOLTAGE_1_15);
    sleep_ms(33);
    set_sys_clock_khz(270000, true);
#endif

    stdio_init_all();

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);


    for (int i = 0; i < 6; i++) {
        sleep_ms(33);
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        sleep_ms(33);
        gpio_put(PICO_DEFAULT_LED_PIN, false);
    }

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
                    uint8_t glyph_row = fnt8x16[c * 16 + y % 16];
                    uint8_t color = VRAM[/*0xB8000 + */(y / 16) * 160 + x * 2 + 1];

                    for (uint8_t bit = 0; bit < 8; bit++) {
                        if (cursor_blink_state && (y >> 4 == CURY && x == CURX && (y % 16) >= 12 && (y % 16) <= 13)) {
                            pixels[y * 640 + (8 * x + bit)] = dosColorPalette[color & 0x0F];
                        } else {
                            if ((glyph_row >> bit) & 1) {
                                pixels[y * 640 + (8 * x + bit)] = dosColorPalette[color & 0x0F];
                            } else {
                                pixels[y * 640 + (8 * x + bit)] = dosColorPalette[color >> 4];
                            }
                        }
                    }
                }
            }
        } else {
            const uint8_t vidmode = 4;

            uint32_t *pix = pixels;
            uint32_t usepal = (pr3D9 >> 5) & 1;
            uint32_t intensity = ((pr3D9 >> 4) & 1) << 3;
            for (int y = 0; y < 200; y++) {
                for (int x = 0; x < 320; x++) {
                    uint32_t charx = x;
                    uint32_t chary = y;
                    uint32_t vidptr = /*0xB8000 + */((chary >> 1) * 80) + ((chary & 1) * 8192) + (charx >> 2);
                    uint32_t curpixel = VRAM[vidptr];
                    uint32_t color;
                    switch (charx & 3) {
                        case 3:
                            curpixel = curpixel & 3;
                            break;
                        case 2:
                            curpixel = (curpixel >> 2) & 3;
                            break;
                        case 1:
                            curpixel = (curpixel >> 4) & 3;
                            break;
                        case 0:
                            curpixel = (curpixel >> 6) & 3;
                            break;
                    }
                    if (vidmode == 4) {
                        curpixel = curpixel * 2 + usepal + intensity;
                        if (curpixel == (usepal + intensity))
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
        exec86(200);
#endif
    }
    return 0;
}
