extern "C" {
#include "emulator.h"
}

#if PICO_ON_DEVICE
#ifndef OVERCLOCKING
#define OVERCLOCKING 270
#endif

#include <pico/time.h>
#include <pico/multicore.h>
#include <hardware/pwm.h>
#include "hardware/clocks.h"
#include <pico/stdlib.h>
#include <hardware/vreg.h>
#include <pico/stdio.h>
#include "f_util.h"
#include "ff.h"
#include "psram_spi.h"

static FATFS fs;
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
bool PSRAM_AVAILABLE = false;
bool runing = true;

#if PICO_ON_DEVICE

struct semaphore vga_start_semaphore;

/* Renderer loop on Pico's second core */
void __time_critical_func(render_core)() {
    graphics_init();

    graphics_set_buffer(VRAM, 320, 200);
    graphics_set_textbuffer(VRAM);

    graphics_set_bgcolor(0);
    graphics_set_offset(0, 0);
    graphics_set_flashmode(true, true);


    //graphics_set_mode(TEXTMODE_80x30);

    for (int i = 0; i < 16; ++i) {
        graphics_set_palette(i, cga_palette[i]);
    }
    sem_acquire_blocking(&vga_start_semaphore);

    uint8_t tick50ms = 0;
    while (true) {
        doirq(0);
        sleep_ms(timer_period);
        if (tick50ms == 0 || tick50ms == 10) {
            cursor_blink_state ^= 1;
        }

        if (tick50ms < 20) {
            tick50ms++;
        } else {
            tick50ms = 0;
        }
//        tickssource();
    }

}

#else


static int RendererThread(void *ptr) {
    while (runing) {
        exec86(2000);
#if !PICO_ON_DEVICE
        //SDL_Delay(1);
#endif
    }
    return 0;
}

#endif

#if PICO_ON_DEVICE
#define  PWM_PIN0 26
pwm_config config = pwm_get_default_config();
psram_spi_inst_t psram_spi;
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
    set_sys_clock_khz(200000, true);
#endif

    stdio_init_all();

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    gpio_set_function(PWM_PIN0, GPIO_FUNC_PWM);
    gpio_set_function(PWM_PIN0+1, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(PWM_PIN0+1);
    pwm_set_chan_level(slice_num, PWM_CHAN_A, 1);
    pwm_set_chan_level(slice_num, PWM_CHAN_B, 3);
    pwm_config_set_wrap(&config, 261);
    pwm_config_set_clkdiv(&config, 127);
    pwm_init(slice_num, &config, true);

    for (int i = 0; i < 6; i++) {
        sleep_ms(23);
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        sleep_ms(23);
        gpio_put(PICO_DEFAULT_LED_PIN, false);
    }

    //nespad_begin(clock_get_hz(clk_sys) / 1000, NES_GPIO_CLK, NES_GPIO_DATA, NES_GPIO_LAT);
    keyboard_init();



    sem_init(&vga_start_semaphore, 0, 1);
    multicore_launch_core1(render_core);
    sem_release(&vga_start_semaphore);

    sleep_ms(50);

#if 1

    // TODO: сделать нормально
    psram_spi = psram_spi_init(pio0, -1);
    psram_write32(&psram_spi, 0x313373, 0xDEADBEEF);
    PSRAM_AVAILABLE = 0xDEADBEEF == psram_read32(&psram_spi, 0x313373);
#endif

#else
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);


    window = SDL_CreateWindow("pico-xt",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              640, 400,
                              SDL_WINDOW_SHOWN);

    screen = SDL_GetWindowSurface(window);
    auto *pixels = (unsigned int *) screen->pixels;

    SDL_PauseAudio(0);
    if (!SDL_CreateThread(RendererThread, "renderer", nullptr)) {
        fprintf(stderr, "Could not create the renderer thread: %s\n", SDL_GetError());
        return -1;
    }



#endif

    reset86();
    while (runing) {
#if !PICO_ON_DEVICE
        handleinput();
        uint8_t mode = videomode; // & 0x0F;
        if (mode == 0x11) mode = 1;
        if (mode <= 3 || mode == 0x56) {
            uint8_t cols = videomode <= 1 ? 40 : 80;
//            SDL_SetWindowSize(window, 640, 400);
            for (uint16_t y = 0; y < 400; y++) {
                for (uint8_t x = 0; x < cols; x++) {
                    uint8_t c = VRAM[/*0xB8000 + */(y / 16) * (cols * 2) + x * 2 + 0];
                    uint8_t glyph_row = font_8x16[c * 16 + y % 16];
                    uint8_t color = VRAM[/*0xB8000 + */(y / 16) * (cols * 2) + x * 2 + 1];

                    for (uint8_t bit = 0; bit < 8; bit++) {
                        if (cursor_blink_state && (y >> 4 == CURSOR_Y && x == CURSOR_X && (y % 16) >= 12 && (y % 16) <= 13)) {
                            pixels[y * 640 + (8 * x + bit)] = cga_palette[color & 0x0F];
                        } else {
                            if ((glyph_row >> bit) & 1) {
                                pixels[y * 640 + (8 * x + bit)] = cga_palette[color & 0x0F];
                            } else {
                                pixels[y * 640 + (8 * x + bit)] = cga_palette[color >> 4];
                            }
                        }
                    }
                }
            }
        } else if (mode < 6) {
            uint32_t *pix = pixels;
            uint32_t usepal = cga_colorset;
            uint32_t intensity = cga_intensity << 3;
            for (int y = 0; y < 400; y++) {
                for (int x = 0; x < 320; x++) {
                    uint32_t vidptr = /*0xB8000 + */(((y / 2) >> 1) * 80) + (((y / 2) & 1) * 8192) + (x >> 2);
                    uint32_t curpixel = VRAM[vidptr];
                    uint32_t color;
                    switch (x & 3) {
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
                    if (mode == 4) {
                        curpixel = curpixel * 2 + usepal + intensity;
                        if (curpixel == (usepal + intensity))
                            curpixel = 0;
                        color = cga_palette[curpixel];
                        *pix++ = color;
                        *pix++ = color;
                    } else {
                        curpixel = curpixel * 63;
                        color = cga_palette[curpixel];
                        *pix++ = color;
                        *pix++ = color;
                    }
                }
                //pix += 320;
            }
        } else if (mode == 6) {
            uint32_t *pix = pixels;
            for (int y = 0; y < 400; y++) {
                for (int x = 0; x < 640; x++) {

                    uint32_t vidptr = /*0xB8000 + */(((y /2) >> 1) * 80) + (((y / 2) & 1) * 8192) + (x >> 3);
                    uint32_t curpixel = (VRAM[vidptr] >> (7 - (x & 7))) & 1;
                    *pix++ = cga_palette[curpixel * 15];
                }
            }
        } else if (mode == 66 || mode == 8 || mode == 64) {
            uint32_t intensity = mode == 66 ? 0 : 1+cga_intensity;
            uint32_t *pix = pixels;
            for (int y = 0; y < 200; y++) {
                for (int x = 0; x < 160; x++) {
                    uint32_t vidptr = /*0xB8000 + */((y  >> 1) * 80) + ((y  & 1) * 8192) + x;
                    uint32_t curpixel = (VRAM[vidptr] >> 4) & 15;
                    *pix++ = cga_composite_palette[intensity][curpixel];
                    *pix++ = cga_composite_palette[intensity][curpixel];
                    *pix++ = cga_composite_palette[intensity][curpixel];
                    *pix++ = cga_composite_palette[intensity][curpixel];
                    curpixel = (VRAM[vidptr]) & 15;
                    *pix++ = cga_composite_palette[intensity][curpixel];
                    *pix++ = cga_composite_palette[intensity][curpixel];
                    *pix++ = cga_composite_palette[intensity][curpixel];
                    *pix++ = cga_composite_palette[intensity][curpixel];
                }
            }
        } else if (mode == 76) {
            uint8_t cols = 80;
//            SDL_SetWindowSize(window, 640, 400);
            for (uint16_t y = 0; y < 400; y++) {
                for (uint8_t x = 0; x < cols; x++) {
                    uint8_t c = VRAM[(y / 4) * (cols * 2) + x * 2 + 0];
                    uint8_t glyph_row = font_8x16[c * 16 + y % 16];
                    uint8_t color = VRAM[(y / 4) * (cols * 2) + x * 2 + 1];

                    for (uint8_t bit = 0; bit < 8; bit++) {
                        if (cursor_blink_state && (y >> 4 == CURSOR_Y && x == CURSOR_X && (y % 16) >= 12 && (y % 16) <= 13)) {
                            pixels[y * 640 + (8 * x + bit)] = cga_palette[color & 0x0F];
                        } else {
                            if ((glyph_row >> bit) & 1) {
                                pixels[y * 640 + (8 * x + bit)] = cga_palette[color & 0x0F];
                            } else {
                                pixels[y * 640 + (8 * x + bit)] = cga_palette[color >> 4];
                            }
                        }
                    }
                }
            }
        } else if (mode == 66 ) {
            uint32_t *pix = pixels;
            for (int y = 0; y < 200; y++) {
                for (int x = 0; x < 160; x++) {
                    uint32_t charx = x;
                    uint32_t chary = y;
                    uint32_t vidptr = /*0xB8000 + */((chary >> 1) * 80) + ((chary & 1) * 8192) + (charx >> 1);
                    uint32_t curpixel = VRAM[vidptr];
                    //*vbuf_OUT++ = pal[(*vbuf8) & 0xf];
                    //*vbuf_OUT++ = pal[(*vbuf8 >> 4) & 0xf];
                    *pix++ = cga_palette[(curpixel >> 4) & 0xf];
                    *pix++ = cga_palette[curpixel & 0xf];
                }
            }
        } else if (mode == 9) {
            uint32_t *pix = pixels;
            for (int y = 0; y < 400; y++) {
                for (int x = 0; x < 640; x++) {
                    uint32_t vidptr = (y>>3) *160 + (x>>2) + ( (y>>1) &3) *8192;
                    uint32_t color;
                    if ( ( (x>>1) &1) ==0)
                        color = cga_palette[VRAM[vidptr] >> 4];
                    else
                        color = cga_palette[VRAM[vidptr] & 15];
                    //prestretch[y][x] = color;
                    *pix++ = color;
                }
                //pix += 320;
            }
        }
        SDL_UpdateWindowSurface(window);
#else
        exec86(340);
#endif
    }
    return 0;
}
