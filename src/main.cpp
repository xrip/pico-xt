extern "C" {
#include "emulator.h"
}

#if PICO_ON_DEVICE
#include <pico/time.h>
#include <pico/multicore.h>
#include <hardware/pwm.h>
#include "hardware/clocks.h"
#include <pico/stdlib.h>
#include <hardware/vreg.h>
#include <pico/stdio.h>

#include "psram_spi.h"
#include "nespad.h"

extern "C" {
#include "vga.h"
#include "ps2.h"
#include "usb.h"
#include "manager.h"
}
#if I2S_SOUND
#include "audio.h"
#endif
#else
#define SDL_MAIN_HANDLED

#include "SDL2/SDL.h"
#include "../drivers/vga-nextgen/fnt8x16.h"

SDL_Window* window;

SDL_Surface* screen;
uint16_t true_covox = 0;
#endif

bool PSRAM_AVAILABLE = false;
bool SD_CARD_AVAILABLE = false;
uint32_t DIRECT_RAM_BORDER = PSRAM_AVAILABLE ? RAM_SIZE : (SD_CARD_AVAILABLE ? RAM_PAGE_SIZE : RAM_SIZE);
bool runing = true;
static int16_t last_dss_sample = 0;
#if PICO_ON_DEVICE
pwm_config config = pwm_get_default_config();
#define PWM_PIN0 (26)
#define PWM_PIN1 (27)

#ifdef I2S_SOUND
i2s_config_t i2s_config = i2s_get_default_config();
static int16_t samples[2][441*2] = { 0 };
static int active_buffer = 0;
static int sample_index = 0;

#endif


uint16_t true_covox = 0;

struct semaphore vga_start_semaphore;
/* Renderer loop on Pico's second core */
void __scratch_y("second_core") second_core() {
#ifdef SOUND_ENABLED
#ifdef I2S_SOUND
    i2s_config.sample_freq = SOUND_FREQUENCY;
    i2s_config.dma_trans_count = SOUND_FREQUENCY / 100;
    i2s_volume(&i2s_config, 0);
    i2s_init(&i2s_config);
    sleep_ms(100);
#else
    gpio_set_function(BEEPER_PIN, GPIO_FUNC_PWM);
    pwm_init(pwm_gpio_to_slice_num(BEEPER_PIN), &config, true);

    gpio_set_function(PWM_PIN0, GPIO_FUNC_PWM);
    gpio_set_function(PWM_PIN1, GPIO_FUNC_PWM);

    pwm_config_set_clkdiv(&config, 1.0);
    pwm_config_set_wrap(&config, (1 << 12) - 1); // MAX PWM value

    pwm_init(pwm_gpio_to_slice_num(PWM_PIN0), &config, true);
    pwm_init(pwm_gpio_to_slice_num(PWM_PIN1), &config, true);
#endif
#endif
    graphics_init();
    graphics_set_buffer(VIDEORAM, 320, 200);
    graphics_set_textbuffer(VIDEORAM + 32768);
    graphics_set_bgcolor(0);
    graphics_set_offset(0, 0);
    graphics_set_flashmode(true, true);

    for (int i = 0; i < 16; ++i) {
        graphics_set_palette(i, cga_palette[i]);
    }


    sem_acquire_blocking(&vga_start_semaphore);

    uint64_t tick = time_us_64();
    uint64_t last_timer_tick = tick, last_cursor_blink = tick, last_sound_tick = tick, last_dss_tick = tick;

    while (true) {
        if (tick >= last_timer_tick + timer_period) {
            doirq(0);
            last_timer_tick = tick;
        }

        if (tick >= last_cursor_blink + 500000) {
            cursor_blink_state ^= 1;
            last_cursor_blink = tick;
        }

#ifdef SOUND_ENABLED
#ifdef DSS
        // (1000000 / 7000) ~= 140
        if (tick >= last_dss_tick + 140) {
            last_dss_sample = dss_sample();
            if (last_dss_sample)
                last_dss_sample = (int16_t)(((int32_t)last_dss_sample - (int32_t)0x0080) << 7);
            // 8 unsigned on LPT1 mix to signed 16
            last_dss_tick = tick;
        }
#endif

        // Sound frequency 44100
        if (tick >= last_sound_tick + (1000000 / SOUND_FREQUENCY)) {
            int16_t sample = 0;

#ifdef COVOX
            if (true_covox) {
                sample += (int16_t)(((int32_t)true_covox - (int32_t)0x0080) << 7);
                // 8 unsigned on LPT2 mix to signed 16
            }
#endif

#ifdef TANDY3V
            sample += sn76489_sample();
#endif

#ifdef DSS
            sample += last_dss_sample;
#endif

#ifdef I2S_SOUND
            if (speakerenabled) {
                sample += speaker_sample();
            }

            samples[active_buffer][sample_index * 2] = sample;
            samples[active_buffer][sample_index * 2 + 1] = sample;

#ifdef CMS
            cms_samples(&samples[active_buffer][sample_index * 2]);
#endif

            if (sample_index++ >= i2s_config.dma_trans_count) {
                sample_index = 0;
                i2s_dma_write(&i2s_config, samples[active_buffer]);
                active_buffer ^= 1;
            }
#else
            // register uint8_t r_divider = snd_divider + 4; // TODO: tume up divider per channel
            uint16_t corrected_sample = (uint16_t)((int32_t)sample + 0x8000L) >> 4;
            // register uint8_t l_divider = snd_divider + 4;
            //register uint16_t l_v = (uint16_t)((int32_t)sample + 0x8000L) >> 4;
            pwm_set_gpio_level(PWM_PIN0, corrected_sample);
            pwm_set_gpio_level(PWM_PIN1, corrected_sample);
#endif
            last_sound_tick = tick;
        }
#endif

        tick = time_us_64();
        tight_loop_contents();
    }
}

#else

bool is_adlib_on = true;
bool is_covox_on = true;
bool is_game_balaster_on = true;
bool is_tandy3v_on = true;
bool is_dss_on = true;
bool is_sound_on = true;
bool covox_lpt2 = true;

volatile bool is_xms_on = false;
volatile bool is_ems_on = true;
volatile bool is_umb_on = true;
volatile bool is_hma_on = true;

static uint8_t dss_tick = 0;
static void fill_audio(void* udata, uint8_t* stream, int len) { // for SDL mode only
    int16_t *output = (int16_t*)stream;
    output[0] = 0;
#ifdef COVOX
    if (true_covox) {
        output[0] += (int16_t)(((int32_t)true_covox - (int32_t)0x0080) << 7);
        // 8 unsigned on LPT2 mix to signed 16
    }
#endif
#ifdef DSS
    if (dss_tick++ >= 5) {
        last_dss_sample = dss_sample();
        dss_tick = 0;
    }
    output[0] +=  (int16_t) last_dss_sample;
#endif
#ifdef TANDY3V
    output[0] += sn76489_sample();
#endif
#ifdef ADLIB
    output[0] = adlibgensample();
#endif

    if (speakerenabled) {
        output[0] += speaker_sample();
    }

    output[1] = output[0];
#if CMS
    cms_samples(output);
#endif

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

    //stdio_init_all();

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    for (int i = 0; i < 6; i++) {
        sleep_ms(23);
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        sleep_ms(23);
        gpio_put(PICO_DEFAULT_LED_PIN, false);
    }

    nespad_begin(clock_get_hz(clk_sys) / 1000, NES_GPIO_CLK, NES_GPIO_DATA, NES_GPIO_LAT);
    keyboard_init();

    sem_init(&vga_start_semaphore, 0, 1);
    multicore_launch_core1(second_core);
    sem_release(&vga_start_semaphore);

    sleep_ms(50);

    graphics_set_mode(TEXTMODE_80x30);

    init_psram();

    FRESULT result = f_mount(&fs, "", 1);
    if (FR_OK != result) {
        char tmp[80];
        sprintf(tmp, "Unable to mount SD-card: %s (%d)", FRESULT_str(result), result);
        logMsg(tmp);
    }
    else {
        SD_CARD_AVAILABLE = true;
    }

    DIRECT_RAM_BORDER = PSRAM_AVAILABLE ? RAM_SIZE : (SD_CARD_AVAILABLE ? RAM_PAGE_SIZE : RAM_SIZE);

    if (!PSRAM_AVAILABLE && !SD_CARD_AVAILABLE) {
        logMsg((char *)"Mo PSRAM or SD CARD available. Only 160Kb RAM will be usable...");
        sleep_ms(3000);
    }

#else
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO);


    window = SDL_CreateWindow("pico-xt",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              640 * 2, 400 * 2,
                              SDL_WINDOW_SHOWN);
    screen = SDL_GetWindowSurface(window);
    auto drawsurface = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 400, screen->format->BitsPerPixel,
                                            screen->format->Rmask, screen->format->Gmask, screen->format->Bmask,
                                            screen->format->Amask);
    auto* pixels = (unsigned int *)drawsurface->pixels;
#if SOUND_ENABLED
    SDL_AudioSpec wanted;
    wanted.freq = SOUND_FREQUENCY;
    wanted.format = AUDIO_S16;
    wanted.channels = 2;
    wanted.samples = 1;
    wanted.callback = fill_audio;
    wanted.userdata = NULL;

    if (SDL_OpenAudio(&wanted, NULL) < 0) {
        printf("Error: %s\n", SDL_GetError());
    }
    SDL_PauseAudio(0);

#endif
    /*if (!SDL_CreateThread(RendererThread, "renderer", nullptr)) {
        fprintf(stderr, "Could not create the renderer thread: %s\n", SDL_GetError());
        return -1;
    }*/
#endif

    vga_palette[0] = rgb(0, 0, 0);
    vga_palette[1] = rgb(0, 0, 169);
    vga_palette[2] = rgb(0, 169, 0);
    vga_palette[3] = rgb(0, 169, 169);
    vga_palette[4] = rgb(169, 0, 0);
    vga_palette[5] = rgb(169, 0, 169);
    vga_palette[6] = rgb(169, 169, 0);
    vga_palette[7] = rgb(169, 169, 169);
    vga_palette[8] = rgb(0, 0, 84);
    vga_palette[9] = rgb(0, 0, 255);
    vga_palette[10] = rgb(0, 169, 84);
    vga_palette[11] = rgb(0, 169, 255);
    vga_palette[12] = rgb(169, 0, 84);
    vga_palette[13] = rgb(169, 0, 255);
    vga_palette[14] = rgb(169, 169, 84);
    vga_palette[15] = rgb(169, 169, 255);
    vga_palette[16] = rgb(0, 84, 0);
    vga_palette[17] = rgb(0, 84, 169);
    vga_palette[18] = rgb(0, 255, 0);
    vga_palette[19] = rgb(0, 255, 169);
    vga_palette[20] = rgb(169, 84, 0);
    vga_palette[21] = rgb(169, 84, 169);
    vga_palette[22] = rgb(169, 255, 0);
    vga_palette[23] = rgb(169, 255, 169);
    vga_palette[24] = rgb(0, 84, 84);
    vga_palette[25] = rgb(0, 84, 255);
    vga_palette[26] = rgb(0, 255, 84);
    vga_palette[27] = rgb(0, 255, 255);
    vga_palette[28] = rgb(169, 84, 84);
    vga_palette[29] = rgb(169, 84, 255);
    vga_palette[30] = rgb(169, 255, 84);
    vga_palette[31] = rgb(169, 255, 255);
    vga_palette[32] = rgb(84, 0, 0);
    vga_palette[33] = rgb(84, 0, 169);
    vga_palette[34] = rgb(84, 169, 0);
    vga_palette[35] = rgb(84, 169, 169);
    vga_palette[36] = rgb(255, 0, 0);
    vga_palette[37] = rgb(255, 0, 169);
    vga_palette[38] = rgb(255, 169, 0);
    vga_palette[39] = rgb(255, 169, 169);
    vga_palette[40] = rgb(84, 0, 84);
    vga_palette[41] = rgb(84, 0, 255);
    vga_palette[42] = rgb(84, 169, 84);
    vga_palette[43] = rgb(84, 169, 255);
    vga_palette[44] = rgb(255, 0, 84);
    vga_palette[45] = rgb(255, 0, 255);
    vga_palette[46] = rgb(255, 169, 84);
    vga_palette[47] = rgb(255, 169, 255);
    vga_palette[48] = rgb(84, 84, 0);
    vga_palette[49] = rgb(84, 84, 169);
    vga_palette[50] = rgb(84, 255, 0);
    vga_palette[51] = rgb(84, 255, 169);
    vga_palette[52] = rgb(255, 84, 0);
    vga_palette[53] = rgb(255, 84, 169);
    vga_palette[54] = rgb(255, 255, 0);
    vga_palette[55] = rgb(255, 255, 169);
    vga_palette[56] = rgb(84, 84, 84);
    vga_palette[57] = rgb(84, 84, 255);
    vga_palette[58] = rgb(84, 255, 84);
    vga_palette[59] = rgb(84, 255, 255);
    vga_palette[60] = rgb(255, 84, 84);
    vga_palette[61] = rgb(255, 84, 255);
    vga_palette[62] = rgb(255, 255, 84);
    vga_palette[63] = rgb(255, 255, 255);
    vga_palette[64] = rgb(255, 125, 125);
    vga_palette[65] = rgb(255, 157, 125);
    vga_palette[66] = rgb(255, 190, 125);
    vga_palette[67] = rgb(255, 222, 125);
    vga_palette[68] = rgb(255, 255, 125);
    vga_palette[69] = rgb(222, 255, 125);
    vga_palette[70] = rgb(190, 255, 125);
    vga_palette[71] = rgb(157, 255, 125);
    vga_palette[72] = rgb(125, 255, 125);
    vga_palette[73] = rgb(125, 255, 157);
    vga_palette[74] = rgb(125, 255, 190);
    vga_palette[75] = rgb(125, 255, 222);
    vga_palette[76] = rgb(125, 255, 255);
    vga_palette[77] = rgb(125, 222, 255);
    vga_palette[78] = rgb(125, 190, 255);
    vga_palette[79] = rgb(125, 157, 255);
    vga_palette[80] = rgb(182, 182, 255);
    vga_palette[81] = rgb(198, 182, 255);
    vga_palette[82] = rgb(218, 182, 255);
    vga_palette[83] = rgb(234, 182, 255);
    vga_palette[84] = rgb(255, 182, 255);
    vga_palette[85] = rgb(255, 182, 234);
    vga_palette[86] = rgb(255, 182, 218);
    vga_palette[87] = rgb(255, 182, 198);
    vga_palette[88] = rgb(255, 182, 182);
    vga_palette[89] = rgb(255, 198, 182);
    vga_palette[90] = rgb(255, 218, 182);
    vga_palette[91] = rgb(255, 234, 182);
    vga_palette[92] = rgb(255, 255, 182);
    vga_palette[93] = rgb(234, 255, 182);
    vga_palette[94] = rgb(218, 255, 182);
    vga_palette[95] = rgb(198, 255, 182);
    vga_palette[96] = rgb(182, 255, 182);
    vga_palette[97] = rgb(182, 255, 198);
    vga_palette[98] = rgb(182, 255, 218);
    vga_palette[99] = rgb(182, 255, 234);
    vga_palette[100] = rgb(182, 255, 255);
    vga_palette[101] = rgb(182, 234, 255);
    vga_palette[102] = rgb(182, 218, 255);
    vga_palette[103] = rgb(182, 198, 255);
    vga_palette[104] = rgb(0, 0, 113);
    vga_palette[105] = rgb(28, 0, 113);
    vga_palette[106] = rgb(56, 0, 113);
    vga_palette[107] = rgb(84, 0, 113);
    vga_palette[108] = rgb(113, 0, 113);
    vga_palette[109] = rgb(113, 0, 84);
    vga_palette[110] = rgb(113, 0, 56);
    vga_palette[111] = rgb(113, 0, 28);
    vga_palette[112] = rgb(113, 0, 0);
    vga_palette[113] = rgb(113, 28, 0);
    vga_palette[114] = rgb(113, 56, 0);
    vga_palette[115] = rgb(113, 84, 0);
    vga_palette[116] = rgb(113, 113, 0);
    vga_palette[117] = rgb(84, 113, 0);
    vga_palette[118] = rgb(56, 113, 0);
    vga_palette[119] = rgb(28, 113, 0);
    vga_palette[120] = rgb(0, 113, 0);
    vga_palette[121] = rgb(0, 113, 28);
    vga_palette[122] = rgb(0, 113, 56);
    vga_palette[123] = rgb(0, 113, 84);
    vga_palette[124] = rgb(0, 113, 113);
    vga_palette[125] = rgb(0, 84, 113);
    vga_palette[126] = rgb(0, 56, 113);
    vga_palette[127] = rgb(0, 28, 113);
    vga_palette[128] = rgb(56, 56, 113);
    vga_palette[129] = rgb(68, 56, 113);
    vga_palette[130] = rgb(84, 56, 113);
    vga_palette[131] = rgb(97, 56, 113);
    vga_palette[132] = rgb(113, 56, 113);
    vga_palette[133] = rgb(113, 56, 97);
    vga_palette[134] = rgb(113, 56, 84);
    vga_palette[135] = rgb(113, 56, 68);
    vga_palette[136] = rgb(113, 56, 56);
    vga_palette[137] = rgb(113, 68, 56);
    vga_palette[138] = rgb(113, 84, 56);
    vga_palette[139] = rgb(113, 97, 56);
    vga_palette[140] = rgb(113, 113, 56);
    vga_palette[141] = rgb(97, 113, 56);
    vga_palette[142] = rgb(84, 113, 56);
    vga_palette[143] = rgb(68, 113, 56);
    vga_palette[144] = rgb(56, 113, 56);
    vga_palette[145] = rgb(56, 113, 68);
    vga_palette[146] = rgb(56, 113, 84);
    vga_palette[147] = rgb(56, 113, 97);
    vga_palette[148] = rgb(56, 113, 113);
    vga_palette[149] = rgb(56, 97, 113);
    vga_palette[150] = rgb(56, 84, 113);
    vga_palette[151] = rgb(56, 68, 113);
    vga_palette[152] = rgb(80, 80, 113);
    vga_palette[153] = rgb(89, 80, 113);
    vga_palette[154] = rgb(97, 80, 113);
    vga_palette[155] = rgb(105, 80, 113);
    vga_palette[156] = rgb(113, 80, 113);
    vga_palette[157] = rgb(113, 80, 105);
    vga_palette[158] = rgb(113, 80, 97);
    vga_palette[159] = rgb(113, 80, 89);
    vga_palette[160] = rgb(113, 80, 80);
    vga_palette[161] = rgb(113, 89, 80);
    vga_palette[162] = rgb(113, 97, 80);
    vga_palette[163] = rgb(113, 105, 80);
    vga_palette[164] = rgb(113, 113, 80);
    vga_palette[165] = rgb(105, 113, 80);
    vga_palette[166] = rgb(97, 113, 80);
    vga_palette[167] = rgb(89, 113, 80);
    vga_palette[168] = rgb(80, 113, 80);
    vga_palette[169] = rgb(80, 113, 89);
    vga_palette[170] = rgb(80, 113, 97);
    vga_palette[171] = rgb(80, 113, 105);
    vga_palette[172] = rgb(80, 113, 113);
    vga_palette[173] = rgb(80, 105, 113);
    vga_palette[174] = rgb(80, 97, 113);
    vga_palette[175] = rgb(80, 89, 113);
    vga_palette[176] = rgb(0, 0, 64);
    vga_palette[177] = rgb(16, 0, 64);
    vga_palette[178] = rgb(32, 0, 64);
    vga_palette[179] = rgb(48, 0, 64);
    vga_palette[180] = rgb(64, 0, 64);
    vga_palette[181] = rgb(64, 0, 48);
    vga_palette[182] = rgb(64, 0, 32);
    vga_palette[183] = rgb(64, 0, 16);
    vga_palette[184] = rgb(64, 0, 0);
    vga_palette[185] = rgb(64, 16, 0);
    vga_palette[186] = rgb(64, 32, 0);
    vga_palette[187] = rgb(64, 48, 0);
    vga_palette[188] = rgb(64, 64, 0);
    vga_palette[189] = rgb(48, 64, 0);
    vga_palette[190] = rgb(32, 64, 0);
    vga_palette[191] = rgb(16, 64, 0);
    vga_palette[192] = rgb(0, 64, 0);
    vga_palette[193] = rgb(0, 64, 16);
    vga_palette[194] = rgb(0, 64, 32);
    vga_palette[195] = rgb(0, 64, 48);
    vga_palette[196] = rgb(0, 64, 64);
    vga_palette[197] = rgb(0, 48, 64);
    vga_palette[198] = rgb(0, 32, 64);
    vga_palette[199] = rgb(0, 16, 64);
    vga_palette[200] = rgb(32, 32, 64);
    vga_palette[201] = rgb(40, 32, 64);
    vga_palette[202] = rgb(48, 32, 64);
    vga_palette[203] = rgb(56, 32, 64);
    vga_palette[204] = rgb(64, 32, 64);
    vga_palette[205] = rgb(64, 32, 56);
    vga_palette[206] = rgb(64, 32, 48);
    vga_palette[207] = rgb(64, 32, 40);
    vga_palette[208] = rgb(64, 32, 32);
    vga_palette[209] = rgb(64, 40, 32);
    vga_palette[210] = rgb(64, 48, 32);
    vga_palette[211] = rgb(64, 56, 32);
    vga_palette[212] = rgb(64, 64, 32);
    vga_palette[213] = rgb(56, 64, 32);
    vga_palette[214] = rgb(48, 64, 32);
    vga_palette[215] = rgb(40, 64, 32);
    vga_palette[216] = rgb(32, 64, 32);
    vga_palette[217] = rgb(32, 64, 40);
    vga_palette[218] = rgb(32, 64, 48);
    vga_palette[219] = rgb(32, 64, 56);
    vga_palette[220] = rgb(32, 64, 64);
    vga_palette[221] = rgb(32, 56, 64);
    vga_palette[222] = rgb(32, 48, 64);
    vga_palette[223] = rgb(32, 40, 64);
    vga_palette[224] = rgb(44, 44, 64);
    vga_palette[225] = rgb(48, 44, 64);
    vga_palette[226] = rgb(52, 44, 64);
    vga_palette[227] = rgb(60, 44, 64);
    vga_palette[228] = rgb(64, 44, 64);
    vga_palette[229] = rgb(64, 44, 60);
    vga_palette[230] = rgb(64, 44, 52);
    vga_palette[231] = rgb(64, 44, 48);
    vga_palette[232] = rgb(64, 44, 44);
    vga_palette[233] = rgb(64, 48, 44);
    vga_palette[234] = rgb(64, 52, 44);
    vga_palette[235] = rgb(64, 60, 44);
    vga_palette[236] = rgb(64, 64, 44);
    vga_palette[237] = rgb(60, 64, 44);
    vga_palette[238] = rgb(52, 64, 44);
    vga_palette[239] = rgb(48, 64, 44);
    vga_palette[240] = rgb(44, 64, 44);
    vga_palette[241] = rgb(44, 64, 48);
    vga_palette[242] = rgb(44, 64, 52);
    vga_palette[243] = rgb(44, 64, 60);
    vga_palette[244] = rgb(44, 64, 64);
    vga_palette[245] = rgb(44, 60, 64);
    vga_palette[246] = rgb(44, 52, 64);
    vga_palette[247] = rgb(44, 48, 64);
    vga_palette[248] = rgb(0, 0, 0);
    vga_palette[249] = rgb(0, 0, 0);
    vga_palette[250] = rgb(0, 0, 0);
    vga_palette[251] = rgb(0, 0, 0);
    vga_palette[252] = rgb(0, 0, 0);
    vga_palette[253] = rgb(0, 0, 0);
    vga_palette[254] = rgb(0, 0, 0);
    vga_palette[255] = rgb(0, 0, 0);

    reset86();
    while (runing) {
#if !PICO_ON_DEVICE
        handleinput();
        uint8_t mode = videomode; // & 0x0F;
        uint8_t* vidramptr = VIDEORAM + 32768;
        if (mode == 0x11) mode = 1;
        if (mode <= 3 || mode == 0x56) {
            uint8_t cols = videomode <= 1 ? 40 : 80;
            //            SDL_SetWindowSize(window, 640, 400);
            for (uint16_t y = 0; y < 400; y++) {
                for (uint8_t x = 0; x < cols; x++) {
                    uint8_t c = vidramptr[/*0xB8000 + */(y / 16) * (cols * 2) + x * 2 + 0];
                    uint8_t glyph_row = font_8x16[c * 16 + y % 16];
                    uint8_t color = vidramptr[/*0xB8000 + */(y / 16) * (cols * 2) + x * 2 + 1];

                    for (uint8_t bit = 0; bit < 8; bit++) {
                        if (cursor_blink_state && (
                                y >> 4 == CURSOR_Y && x == CURSOR_X && (y % 16) >= 12 && (y % 16) <= 13)) {
                            if (videomode <= 1) pixels[y * 640 + (8 * x + bit)] = cga_palette[color & 0x0F];
                            pixels[y * 640 + (8 * x + bit)] = cga_palette[color & 0x0F];
                        }
                        else {
                            if ((glyph_row >> bit) & 1) {
                                if (videomode <= 1) pixels[y * 640 + (8 * x + bit)] = cga_palette[color & 0x0F];
                                pixels[y * 640 + (8 * x + bit)] = cga_palette[color & 0x0F];
                            }
                            else {
                                if (videomode <= 1) pixels[y * 640 + (8 * x + bit)] = cga_palette[color >> 4];
                                pixels[y * 640 + (8 * x + bit)] = cga_palette[color >> 4];
                            }
                        }
                    }
                }
            }
        }
        else if (mode < 6) {
            uint32_t* pix = pixels;
            uint32_t usepal = cga_colorset;
            uint32_t intensity = cga_intensity << 3;
            for (int y = 0; y < 400; y++) {
                for (int x = 0; x < 320; x++) {
                    uint32_t vidptr = /*0xB8000 + */(((y / 2) >> 1) * 80) + (((y / 2) & 1) * 8192) + (x >> 2);
                    uint32_t curpixel = vidramptr[vidptr];
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
                    if (mode == 4 || mode == 5) {
                        color = cga_palette[cga_gfxpal[cga_intensity][cga_colorset][curpixel]];
                        *pix++ = color;
                        *pix++ = color;
                    }
                    else {
                        curpixel = curpixel * 63;
                        color = cga_palette[curpixel];
                        *pix++ = color;
                        *pix++ = color;
                    }
                }
                //pix += 320;
            }
        }
        else if (mode == 6) {
            uint32_t* pix = pixels;
            for (int y = 0; y < 400; y++) {
                for (int x = 0; x < 640; x++) {
                    uint32_t vidptr = /*0xB8000 + */(((y / 2) >> 1) * 80) + (((y / 2) & 1) * 8192) + (x >> 3);
                    uint32_t curpixel = (vidramptr[vidptr] >> (7 - (x & 7))) & 1;
                    *pix++ = cga_palette[curpixel * 15];
                }
            }
        }
        else if (mode == 66 || mode == 64) {
            // composite / tandy 160x200
            uint32_t intensity = mode == 66 ? 0 : 1 + cga_intensity;
            uint32_t* pix = pixels;
            for (int y = 0; y < 200; y++) {
                for (int x = 0; x < 160; x++) {
                    uint32_t vidptr = /*0xB8000 + */((y >> 1) * 80) + ((y & 1) * 8192) + x;
                    uint32_t curpixel = (vidramptr[vidptr] >> 4) & 15;
                    *pix++ = cga_composite_palette[intensity][curpixel];
                    *pix++ = cga_composite_palette[intensity][curpixel];
                    *pix++ = cga_composite_palette[intensity][curpixel];
                    *pix++ = cga_composite_palette[intensity][curpixel];
                    curpixel = (vidramptr[vidptr]) & 15;
                    *pix++ = cga_composite_palette[intensity][curpixel];
                    *pix++ = cga_composite_palette[intensity][curpixel];
                    *pix++ = cga_composite_palette[intensity][curpixel];
                    *pix++ = cga_composite_palette[intensity][curpixel];
                }
            }
        }
        if (mode == 8) {
            // composite / tandy 160x200
            uint32_t intensity = 0;
            uint32_t* pix = pixels;
            for (int y = 0; y < 200; y++) {
                for (int x = 0; x < 160; x++) {
                    uint32_t vidptr = /*0xB8000 + */((y >> 1) * 80) + ((y & 1) * 8192) + x;
                    uint32_t curpixel = (vidramptr[vidptr] >> 4) & 15;
                    *pix++ = tandy_palette[curpixel];
                    *pix++ = tandy_palette[curpixel];
                    *pix++ = tandy_palette[curpixel];
                    *pix++ = tandy_palette[curpixel];
                    curpixel = (vidramptr[vidptr]) & 15;
                    *pix++ = tandy_palette[curpixel];
                    *pix++ = tandy_palette[curpixel];
                    *pix++ = tandy_palette[curpixel];
                    *pix++ = tandy_palette[curpixel];
                }
            }
        }
        else if (mode == 76) {
            uint8_t cols = 80;
            //            SDL_SetWindowSize(window, 640, 400);
            for (uint16_t y = 0; y < 400; y++) {
                for (uint8_t x = 0; x < cols; x++) {
                    uint8_t c = vidramptr[(y / 4) * (cols * 2) + x * 2 + 0];
                    uint8_t glyph_row = font_8x16[c * 16 + y % 16];
                    uint8_t color = vidramptr[(y / 4) * (cols * 2) + x * 2 + 1];

                    for (uint8_t bit = 0; bit < 8; bit++) {
                        if (cursor_blink_state && (
                                y >> 4 == CURSOR_Y && x == CURSOR_X && (y % 16) >= 12 && (y % 16) <= 13)) {
                            pixels[y * 640 + (8 * x + bit)] = cga_palette[color & 0x0F];
                        }
                        else {
                            if ((glyph_row >> bit) & 1) {
                                pixels[y * 640 + (8 * x + bit)] = cga_palette[color & 0x0F];
                            }
                            else {
                                pixels[y * 640 + (8 * x + bit)] = cga_palette[color >> 4];
                            }
                        }
                    }
                }
            }
        }
        else if (mode == 66) {
            uint32_t* pix = pixels;
            for (int y = 0; y < 200; y++) {
                for (int x = 0; x < 160; x++) {
                    uint32_t charx = x;
                    uint32_t chary = y;
                    uint32_t vidptr = /*0xB8000 + */((chary >> 1) * 80) + ((chary & 1) * 8192) + (charx >> 1);
                    uint32_t curpixel = vidramptr[vidptr];
                    //*vbuf_OUT++ = pal[(*vbuf8) & 0xf];
                    //*vbuf_OUT++ = pal[(*vbuf8 >> 4) & 0xf];
                    *pix++ = cga_palette[(curpixel >> 4) & 0xf];
                    *pix++ = cga_palette[curpixel & 0xf];
                }
            }
        }
        else if (mode == 9) {
            uint32_t* pix = pixels;
            for (int y = 0; y < 400; y++) {
                for (int x = 0; x < 640; x++) {
                    uint32_t vidptr = ((y / 2) & 3) * 8192 + (y / 8) * 160 + (x / 4);
                    uint32_t color;
                    if (((x >> 1) & 1) == 0)
                        color = tandy_palette[vidramptr[vidptr] >> 4];
                    else
                        color = tandy_palette[vidramptr[vidptr] & 15];
                    //prestretch[y][x] = color;
                    *pix++ = color;
                }
                //pix += 320;
            }
        }
        else if (mode == 0x13) {
            uint32_t* pix = pixels;
            vidramptr = VIDEORAM;
            for (int y = 0; y < 400; y++) {
                for (int x = 0; x < 640; x++) {
                    uint32_t vidptr = (x >> 1) + (y >> 1) * 320;
                    uint32_t color = vga_palette[vidramptr[vidptr]];
                    *pix++ = color;
                }
                //pix += 320;
            }
        }
        else if (mode == 0x011d) {
            uint32_t* pix = pixels;
            vidramptr = VIDEORAM;
            for (int y = 0; y < 400; y++) {
                for (int x = 0; x < 320 / 4; x++) {
                    uint32_t vidptr = (x >> 1) + (y >> 1) * 320 / 8;
                    uint8_t pixel = vidramptr[vidptr];
                    *pix++ = vga_palette[(pixel >> 7) & 1];
                    *pix++ = vga_palette[(pixel >> 6) & 1];
                    *pix++ = vga_palette[(pixel >> 5) & 1];
                    *pix++ = vga_palette[(pixel >> 4) & 1];
                    *pix++ = vga_palette[(pixel >> 3) & 1];
                    *pix++ = vga_palette[(pixel >> 2) & 1];
                    *pix++ = vga_palette[(pixel >> 1) & 1];
                    *pix++ = vga_palette[(pixel >> 0) & 1];
                }
            }
        }
        else if (mode == 0x0d) {
            uint32_t* pix = pixels;
            vidramptr = VIDEORAM;
            const uint32_t planesize = 16000;
            for (int y = 0; y < 400; y++) {
                for (int x = 0; x < 640; x++) {
                    uint32_t divx = x >> 1;
                    uint32_t divy = y >> 1;
                    uint32_t vidptr = divy * 40 + (divx >> 3);
                    int x1 = 7 - (divx & 7);
                    uint32_t color = (vidramptr[vidptr] >> x1) & 1;
                    color |= (((vidramptr[planesize + vidptr] >> x1) & 1) << 1);
                    color |= (((vidramptr[planesize * 2 + vidptr] >> x1) & 1) << 2);
                    color |= (((vidramptr[planesize * 3 + vidptr] >> x1) & 1) << 3);
                    //prestretch[y][x] = color;
                    *pix++ = vga_palette[color];;
                }
                // pix += pia.tail;
            }
        }
        else if (mode == 0x0e) {
            uint32_t* pix = pixels;
            vidramptr = VIDEORAM;
            const uint32_t planesize = 16000;
            for (int y = 0; y < 400; y++) {
                for (int x = 0; x < 640; x++) {
                    uint32_t divx = x;
                    uint32_t divy = y >> 1;
                    uint32_t vidptr = divy * 80 + (divx >> 3);
                    int x1 = 7 - (divx & 7);
                    uint32_t color = (vidramptr[vidptr] >> x1) & 1;
                    color |= (((vidramptr[planesize + vidptr] >> x1) & 1) << 1);
                    color |= (((vidramptr[planesize * 2 + vidptr] >> x1) & 1) << 2);
                    color |= (((vidramptr[planesize * 3 + vidptr] >> x1) & 1) << 3);
                    //prestretch[y][x] = color;
                    *pix++ = vga_palette[color];;
                }
                // pix += pia.tail;
            }
        }
        SDL_BlitScaled(drawsurface, NULL, screen, NULL);
        SDL_UpdateWindowSurface(window);
#else
        if_manager();
#endif
        exec86(2000);
    }
    return 0;
}
