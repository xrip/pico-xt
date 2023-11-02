extern "C" {
#include "emu.h"
}

#include "cga.h"
#include "FD0.h"

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
extern uint8_t VRAM[16384];
bool runing = true;

#if PICO_ON_DEVICE

struct semaphore vga_start_semaphore;

/* Renderer loop on Pico's second core */
void __time_critical_func(render_core)() {
    initVGA();

    setVGAbuf(VRAM, 320, 200);

    setVGA_text_buf(VRAM);
    setVGA_bg_color(0);
    setVGAbuf_pos(0, 20);
    setVGA_color_flash_mode(true, true);


    setVGAmode(VGA640x480_text_80_30);
    for (int i = 0; i < 16; ++i) {
        setVGA_color_palette(i, cga_color(i*2));
    }
    sem_acquire_blocking(&vga_start_semaphore);

    uint8_t tick50ms = 0;
    while (true) {
        //read_keyboard();
        sleep_ms(50);
        if (tick50ms == 0 || tick50ms == 10) {
            cursor_blink_state ^= 1;
        }

        if (tick50ms < 20) {
            tick50ms++;
        } else {
            doirq(0);
        }
    }

}

#else

static int RendererThread(void *ptr) {

    while (runing) {
        exec86(2000);
    }
    return 0;
}

#endif


#if !PICO_ON_DEVICE
int hijacked_input = 0;
static uint8_t keydown[0x100];

static int translatescancode_from_sdl(SDL_Keycode keyval) {
    //printf("translatekey for 0x%04X %s\n", keyval, SDL_GetKeyName(keyval));
    switch (keyval) {
        case SDLK_ESCAPE:
            return 0x01;        // escape
        case 0x30:
            return 0x0B;        // zero
        case 0x31:                    // numeric keys 1-9
        case 0x32:
        case 0x33:
        case 0x34:
        case 0x35:
        case 0x36:
        case 0x37:
        case 0x38:
        case 0x39:
            return keyval - 0x2F;
        case 0x2D:
            return 0x0C;
        case 0x3D:
            return 0x0D;
        case SDLK_BACKSPACE:
            return 0x0E;
        case SDLK_TAB:
            return 0x0F;
        case 0x71:
            return 0x10;
        case 0x77:
            return 0x11;
        case 0x65:
            return 0x12;
        case 0x72:
            return 0x13;
        case 0x74:
            return 0x14;
        case 0x79:
            return 0x15;
        case 0x75:
            return 0x16;
        case 0x69:
            return 0x17;
        case 0x6F:
            return 0x18;
        case 0x70:
            return 0x19;
        case 0x5B:
            return 0x1A;
        case 0x5D:
            return 0x1B;
        case SDLK_KP_ENTER:
        case SDLK_RETURN:
        case SDLK_RETURN2:
            return 0x1C;
        case SDLK_RCTRL:
        case SDLK_LCTRL:
            return 0x1D;
        case 0x61:
            return 0x1E;
        case 0x73:
            return 0x1F;
        case 0x64:
            return 0x20;
        case 0x66:
            return 0x21;
        case 0x67:
            return 0x22;
        case 0x68:
            return 0x23;
        case 0x6A:
            return 0x24;
        case 0x6B:
            return 0x25;
        case 0x6C:
            return 0x26;
        case 0x3B:
            return 0x27;
        case 0x27:
            return 0x28;
        case 0x60:
            return 0x29;
        case SDLK_LSHIFT:
            return 0x2A;
        case 0x5C:
            return 0x2B;
        case 0x7A:
            return 0x2C;
        case 0x78:
            return 0x2D;
        case 0x63:
            return 0x2E;
        case 0x76:
            return 0x2F;
        case 0x62:
            return 0x30;
        case 0x6E:
            return 0x31;
        case 0x6D:
            return 0x32;
        case 0x2C:
            return 0x33;
        case 0x2E:
            return 0x34;
        case 0x2F:
            return 0x35;
        case SDLK_RSHIFT:
            return 0x36;
        case SDLK_PRINTSCREEN:
            return 0x37;
        case SDLK_RALT:
        case SDLK_LALT:
            return 0x38;
        case SDLK_SPACE:
            return 0x39;
        case SDLK_CAPSLOCK:
            return 0x3A;
        case SDLK_F1:
            return 0x3B;    // F1
        case SDLK_F2:
            return 0x3C;    // F2
        case SDLK_F3:
            return 0x3D;    // F3
        case SDLK_F4:
            return 0x3E;    // F4
        case SDLK_F5:
            return 0x3F;    // F5
        case SDLK_F6:
            return 0x40;    // F6
        case SDLK_F7:
            return 0x41;    // F7
        case SDLK_F8:
            return 0x42;    // F8
        case SDLK_F9:
            return 0x43;    // F9
        case SDLK_F10:
            return 0x44;    // F10
        case SDLK_NUMLOCKCLEAR:
            return 0x45;    // numlock
        case SDLK_SCROLLLOCK:
            return 0x46;    // scroll lock
        case SDLK_KP_7:
        case SDLK_HOME:
            return 0x47;
        case SDLK_KP_8:
        case SDLK_UP:
            return 0x48;
        case SDLK_KP_9:
        case SDLK_PAGEUP:
            return 0x49;
        case SDLK_KP_MINUS:
            return 0x4A;
        case SDLK_KP_4:
        case SDLK_LEFT:
            return 0x4B;
        case SDLK_KP_5:
            return 0x4C;
        case SDLK_KP_6:
        case SDLK_RIGHT:
            return 0x4D;
        case SDLK_KP_PLUS:
            return 0x4E;
        case SDLK_KP_1:
        case SDLK_END:
            return 0x4F;
        case SDLK_KP_2:
        case SDLK_DOWN:
            return 0x50;
        case SDLK_KP_3:
        case SDLK_PAGEDOWN:
            return 0x51;
        case SDLK_KP_0:
        case SDLK_INSERT:
            return 0x52;
        case SDLK_KP_PERIOD:
        case SDLK_DELETE:
            return 0x53;
        default:
            return -1;    // *** UNSUPPORTED KEY ***
    }
}

void handleinput(void) {
    SDL_Event event;
    int mx = 0, my = 0;
    uint8_t tempbuttons;
    int translated_key;
    if (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_KEYDOWN:
                translated_key = translatescancode_from_sdl(event.key.keysym.sym);
                if (translated_key >= 0) {
                    if (!hijacked_input) {
                        portram[0x60] = translated_key;
                        portram[0x64] |= 2;
                        doirq(1);
                    }
                    //printf("%02X\n", translatescancode_from_sdl(event.key.keysym.sym));
                    keydown[translated_key] = 1;
                } else if (!hijacked_input)
                    printf("INPUT: Unsupported key: %s [%d]\n", SDL_GetKeyName(event.key.keysym.sym),
                           event.key.keysym.sym);
                break;
            case SDL_KEYUP:
                translated_key = translatescancode_from_sdl(event.key.keysym.sym);
                if (translated_key >= 0) {
                    if (!hijacked_input) {
                        portram[0x60] = translated_key | 0x80;
                        portram[0x64] |= 2;
                        doirq(1);
                    }
                    keydown[translated_key] = 0;
                }
                break;
            case SDL_QUIT:
                SDL_Quit();
                break;
            default:
                break;
        }
    }
}

uint32_t ClockTick(uint32_t interval, void *name) {
    doirq(0);
    return interval;
}

uint32_t BlinkTimer(uint32_t interval, void *name) {
    cursor_blink_state ^= 1;
    return interval;
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

//    SDL_AddTimer(55, ClockTick, (void *) "clock");
    SDL_AddTimer(500, BlinkTimer, (void *) "blink");
#endif
    insertdisk(0);
    init8253();
    init8259();
    reset86();
    //draw_text("hello world!", 0, 0, 15, 1);

    //while (1) {};


    //draw_text("TEST", 0, 0, 166, 0);
    while (runing) {
#if !PICO_ON_DEVICE
        handleinput();
        if (vidmode == 3 || vidmode == 0x56) {
//            SDL_SetWindowSize(window, 640, 400);
            for (uint16_t y = 0; y < 400; y++) {
                for (uint8_t x = 0; x < 80; x++) {
                    uint8_t c = VRAM[/*0xB8000 + */(y / 16) * 160 + x * 2 + 0];
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
            uint32_t usepal = (port3D9 >> 5) & 1;
            uint32_t intensity = ((port3D9 >> 4) & 1) << 3;
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
