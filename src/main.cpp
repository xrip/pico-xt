#include <cstdlib>
#include <cstdio>

extern "C" {
#include "emu.h"
}
#if PICO_ON_DEVICE
#include <pico/time.h>
#include <pico/multicore.h>
#include "pico/stdio.h"
extern "C" {
#include "vga.h"
}
#else
#define SDL_MAIN_HANDLED

#include "SDL2/SDL.h"
#include "VGA_ROM_F16.h"

SDL_Window *window;

SDL_Surface *screen;

#endif
extern uint8_t videomode;
bool runing = true;

#if PICO_ON_DEVICE
struct semaphore vga_start_semaphore;
/* Renderer loop on Pico's second core */
void __time_critical_func(render_core)() {
    initVGA();

    setVGAbuf(VRAM, 320, 240);

    setVGA_text_buf(VRAM);
    setVGA_bg_color(0);
    setVGAbuf_pos(0, 0);
    setVGA_color_flash_mode(true, true);

    setVGAmode(VGA640x480_text_80_30);

    sem_acquire_blocking(&vga_start_semaphore);
}
#else

static int RendererThread(void *ptr) {

    while (runing) {
        loop();
    }
    return 0;
}

#endif

uint32_t dosColorPalette[16] = {
        0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
        0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
        0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
        0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF
};

int main() {
    setup();
    //reset86();
//    init86();
#if PICO_ON_DEVICE
    stdio_init_all();
    sleep_ms(10);


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
        printf("Could not create the renderer thread: %s\n", SDL_GetError());
        return -1;
    }
#endif
    //draw_text("hello world!", 0, 0, 15, 1);

    //while (1) {};



    while (runing) {
        SDL_Event event;
        SDL_PollEvent(&event);
        if (event.type == SDL_QUIT) {
            if (window != NULL) {
                SDL_DestroyWindow(window);
            }

            SDL_Quit();
            return 0;
        }


        if (videomode == 3) {
            for (uint16_t y = 0; y < 400; y++) {
                for (uint8_t x = 0; x < 80; x++) {
                    char c = screenmem[/*0xB8000 + */(y / 16) * 160 + x * 2 + 0];
                    //printf("%c", c);
                    uint8_t glyph_row = VGA_ROM_F16[c * 16 + y % 16];
                    uint8_t color = screenmem[/*0xB8000 + */(y / 16) * 160 + x * 2 + 1];

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
            for (int y = 0; y < 200; ++y)
            for (int x = 0; x < 320; ++x) {
                pixels[y*640+x] = dosColorPalette[screenmem[y*(320/4)+(x / 4)]];
            }
        }

        SDL_UpdateWindowSurface(window);

    }
    return 0;
}
