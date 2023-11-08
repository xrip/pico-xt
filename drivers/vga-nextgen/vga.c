#include "vga.h"
#include "hardware/clocks.h"
#include "stdbool.h"
#include "hardware/structs/pll.h"
#include "hardware/structs/systick.h"

#include "hardware/dma.h"
#include "hardware/irq.h"
#include <string.h>
#include <stdio.h>
#include "hardware/pio.h"
#include "pico/stdlib.h"
#include "stdlib.h"
#include "fnt8x16.h"

#include "../../src/cga.h"

uint16_t pio_program_VGA_instructions[] = {


        //     .wrap_target
        0x6008, //  0: out    pins, 8
        //     .wrap
};

const struct pio_program pio_program_VGA = {
        .instructions = pio_program_VGA_instructions,
        .length = 1,
        .origin = -1,
};


static uint32_t *lines_pattern[4];
static uint32_t *lines_pattern_data = NULL;
static int _SM_VGA = -1;


static int N_lines_total = 525;
static int N_lines_visible = 480;
static int line_VS_begin = 490;
static int line_VS_end = 491;
static int shift_picture = 0;

static int begin_line_index = 0;
static int visible_line_size = 320;


static int dma_chan_ctrl;
static int dma_chan;

static uint8_t *g_buf;
static uint g_buf_width = 0;
static uint g_buf_height = 0;
static int g_buf_shx = 0;
static int g_buf_shy = 0;

static bool is_flash_line = false;
static bool is_flash_frame = false;

//буфер 1к графической палитры
static uint16_t palette[2][256];

static uint32_t bg_color[2];
static uint16_t palette16_mask = 0;

static uint8_t *text_buf;
static uint8_t *text_buf_color;

static uint text_buf_width = 0;
static uint text_buf_height = 0;


static uint16_t txt_palette[16];

//буфер 2К текстовой палитры для быстрой работы 
static uint16_t *txt_palette_fast = NULL;
//static uint16_t txt_palette_fast[256*4];


enum VGA_mode_t mode_VGA;

void __not_in_flash_func(dma_handler_VGA)() {

    dma_hw->ints0 = 1u << dma_chan_ctrl;
    static uint32_t frame_i = 0;
    static uint32_t line_active = 0;
    static uint8_t *vbuf = NULL;
    line_active++;
    if (line_active == N_lines_total) {
        line_active = 0;
        frame_i++;
        vbuf = g_buf;
    }

    if (line_active >= N_lines_visible) {
        //заполнение цветом фона
        if ((line_active == N_lines_visible) | (line_active == (N_lines_visible + 3))) {
            uint32_t * ptr_vbuf_OUT = lines_pattern[2 + ((line_active) & 1)];
            ptr_vbuf_OUT += shift_picture / 4;
            int p_i = ((line_active & is_flash_line) + (frame_i & is_flash_frame)) & 1;
            uint32_t color32 = bg_color[p_i];
            for (int i = visible_line_size / 2; i--;) {
                *ptr_vbuf_OUT++ = color32;
            }


        }
        //синхросигналы
        if ((line_active >= line_VS_begin) && (line_active <= line_VS_end))
            dma_channel_set_read_addr(dma_chan_ctrl, &lines_pattern[1], false);//VS SYNC
        else
            dma_channel_set_read_addr(dma_chan_ctrl, &lines_pattern[0], false);
        return;
    }

    if (!(vbuf)) {
        dma_channel_set_read_addr(dma_chan_ctrl, &lines_pattern[0], false);
        return;
    }//если нет видеобуфера - рисуем пустую строку
    int line = 0;
    int l_inx = 0;

    uint32_t * *ptr_vbuf_OUT;
    uint div_factor = 2;
    switch (mode_VGA) {
        case CGA_640x200x2:
        case CGA_320x200x4:
        case CGA_160x200x16:
        case VGA640x480div2:
            l_inx = line_active / 2;
            if (line_active % 2) return;
            line = (line_active) / 2 - g_buf_shy;


            break;
        case VGA640x480div3:
            l_inx = line_active / 2;
            if (line_active % 3) return;
            div_factor = 3;
            line = (line_active) / 3 - g_buf_shy;


            break;
        case VGA640x480_text_40_30:
        case VGA640x480_text_80_30:

            ptr_vbuf_OUT = &lines_pattern[2 + ((line_active) & 1)];
            // uint8_t* vbuf_OUT=(uint8_t*)(*ptr_vbuf_OUT);
            // vbuf_OUT+=shift_picture;
            uint16_t *vbuf_OUT16 = (uint16_t *) (*ptr_vbuf_OUT);
            vbuf_OUT16 += shift_picture / 2;
            const uint font_W = 8;
            const uint font_H = 16;
            uint chrs_in_line = text_buf_width;//количество символов в строке

            int i_ch = line_active / font_H; //номер символа в буфере в начале отображаемой строки
            int sh_ch = line_active % font_H;//"слой" символа


            uint8_t *line_tex_buf = &text_buf[i_ch * chrs_in_line * 2]; //указатель откуда начать считывать символы
            uint8_t *line_tex_buf_color =
                    &text_buf[i_ch * chrs_in_line] + 1;//указатель откуда начать считывать цвета символов


            uint8_t col[2];

            for (int i = 0; i < chrs_in_line; i++) {
                uint8_t d = fnt8x16[(*line_tex_buf++) * font_H +
                                    sh_ch];//из таблицы символов получаем "срез" текущего символа

                //выводим по 2 пиксела из доп. буфера текстовой палитры
                //достаточно быстро , но тратим 2к на буфер

                //считываем из быстрой палитры начало таблицы быстрого преобразования 2-битных комбинаций цветов пикселей
                uint16_t *fast_color = &txt_palette_fast[4 * (*line_tex_buf++)];

                if (cursor_blink_state && (line_active >> 4 == CURY && i == CURX && sh_ch >= 11 && sh_ch <= 13)) {
                    *vbuf_OUT16++ = fast_color[3];
                    *vbuf_OUT16++ = fast_color[3];
                    *vbuf_OUT16++ = fast_color[3];
                    *vbuf_OUT16++ = fast_color[3];
                } else {
                    *vbuf_OUT16++ = fast_color[d & 3];
                    d >>= 2;
                    *vbuf_OUT16++ = fast_color[d & 3];
                    d >>= 2;
                    *vbuf_OUT16++ = fast_color[d & 3];
                    d >>= 2;
                    *vbuf_OUT16++ = fast_color[d & 3];
                }
                //  continue;

                //по 1 пикселу очень медленно, но не надо доп буфера палитры
                //   col[1]=txt_palette[(*line_tex_buf_color)&0xf];
                //   col[0]=txt_palette[(*line_tex_buf_color++)>>4];

                //     *vbuf_OUT++=col[d&1];
                //     d>>=1;
                //     *vbuf_OUT++=col[d&1];
                //     d>>=1;
                //     *vbuf_OUT++=col[d&1];
                //     d>>=1;
                //     *vbuf_OUT++=col[d&1];
                //     d>>=1;
                //     *vbuf_OUT++=col[d&1];
                //     d>>=1;
                //     *vbuf_OUT++=col[d&1];
                //     d>>=1;
                //     *vbuf_OUT++=col[d&1];
                //     d>>=1;
                //     *vbuf_OUT++=col[d&1];



            }

            dma_channel_set_read_addr(dma_chan_ctrl, ptr_vbuf_OUT, false);
            return;
            break;

        default:
            return;
    }


    dma_channel_set_read_addr(dma_chan_ctrl, &lines_pattern[2 + ((l_inx) & 1)], false);

    if (line < 0) return;
    if (line >= g_buf_height) {
        // заполнение линии цветом фона

        if ((line == g_buf_height) | (line == (g_buf_height + 1)) | (line == (g_buf_height + 2))) {

            uint32_t * ptr_vbuf_OUT = lines_pattern[2 + ((l_inx) & 1)];
            int p_i = ((l_inx & is_flash_line) + (frame_i & is_flash_frame)) & 1;
            uint32_t color32 = bg_color[p_i];

            ptr_vbuf_OUT += shift_picture / 4;
            for (int i = visible_line_size / 2; i--;) {
                *ptr_vbuf_OUT++ = color32;
            }
        }


        return;
    };

    //зона прорисовки изображения
    //начальные точки буферов
    // uint8_t* vbuf8=vbuf+line*g_buf_width; //8bit buf
    // uint8_t* vbuf8=vbuf+(line*g_buf_width/2); //4bit buf
    //uint8_t* vbuf8=vbuf+(line*g_buf_width/4); //2bit buf
    //uint8_t* vbuf8=vbuf+((line&1)*8192+(line>>1)*g_buf_width/4);
    uint8_t *vbuf8 = vbuf + ((line >> 1) * 80) + ((line & 1) * 8192);
    ptr_vbuf_OUT = &lines_pattern[2 + ((l_inx) & 1)];


    uint16_t *vbuf_OUT = (uint16_t *) (*ptr_vbuf_OUT);
    vbuf_OUT += shift_picture / 2; //смещение началы вывода на размер синхросигнала


//    g_buf_shx&=0xfffffffe;//4bit buf
    if (mode_VGA == CGA_640x200x2) {
        g_buf_shx &= 0xfffffff1;//1bit buf
    } else {
        g_buf_shx &= 0xfffffff2;//2bit buf
    }

    //для div_factor 2
    int max_loop = g_buf_width;
    if (g_buf_shx < 0) {
        //vbuf8-=g_buf_shx; //8bit buf
        if (mode_VGA == CGA_640x200x2) {
            vbuf8 -= g_buf_shx / 8;//1bit buf
        } else {
            vbuf8 -= g_buf_shx / 4;//2bit buf
        }
        max_loop += g_buf_shx;
    } else
        vbuf_OUT += g_buf_shx * 2 / div_factor;


    int x_pixels = MIN((visible_line_size - ((g_buf_shx > 0) ? (g_buf_shx) : 0)), max_loop);
    if (x_pixels < 0) return;

    int p_i = ((line & is_flash_line) + (frame_i & is_flash_frame)) & 1;

    uint16_t *pal = palette[p_i];


    uint8_t *vbuf_OUT8;
    switch (mode_VGA) {
        case CGA_640x200x2:
            vbuf_OUT8 = (uint8_t *) vbuf_OUT;
            //1bit buf
            for (int i = x_pixels / 4; i--;) {

                *vbuf_OUT8++ = pal[(*vbuf8 >> 7) & 1];
                *vbuf_OUT8++ = pal[(*vbuf8 >> 6) & 1];
                *vbuf_OUT8++ = pal[(*vbuf8 >> 5) & 1];
                *vbuf_OUT8++ = pal[(*vbuf8 >> 4) & 1];
                *vbuf_OUT8++ = pal[(*vbuf8 >> 3) & 1];
                *vbuf_OUT8++ = pal[(*vbuf8 >> 2) & 1];
                *vbuf_OUT8++ = pal[(*vbuf8 >> 1) & 1];
                *vbuf_OUT8++ = pal[(*vbuf8 >> 0) & 1];
                vbuf8++;
            }
            break;
        case CGA_320x200x4:
            //2bit buf
            for (int i = x_pixels / 4; i--;) {
                *vbuf_OUT++ = pal[(*vbuf8 >> 6) & 3];
                *vbuf_OUT++ = pal[(*vbuf8 >> 4) & 3];
                *vbuf_OUT++ = pal[(*vbuf8 >> 2) & 3];
                *vbuf_OUT++ = pal[(*vbuf8 >> 0) & 3];
                vbuf8++;
            }
            break;
        case CGA_160x200x16:
            //4bit buf
            for (int i = x_pixels / 2; i--;) {
                //поменять местами, если надо дугое чередование
                *vbuf_OUT++ = pal[(*vbuf8) & 0xf];
                *vbuf_OUT++ = pal[(*vbuf8) & 0xf];
                *vbuf_OUT++ = pal[(*vbuf8) & 0xf];
                *vbuf_OUT++ = pal[(*vbuf8) & 0xf];
                *vbuf_OUT++ = pal[(*vbuf8 >> 4) & 0xf];
                *vbuf_OUT++ = pal[(*vbuf8 >> 4) & 0xf];
                *vbuf_OUT++ = pal[(*vbuf8 >> 4) & 0xf];
                *vbuf_OUT++ = pal[(*vbuf8 >> 4) & 0xf];

                vbuf8++;
            }
            break;

        case VGA640x480div2:
            //8bit buf
            // for(int i=x_pixels;i--;)
            //     {
            //             *vbuf_OUT++=pal[*vbuf8++];

            //     }   
            //4bit buf 
            for (int i = x_pixels / 2; i--;) {
                //поменять местами, если надо дугое чередование
                *vbuf_OUT++ = pal[(*vbuf8) & 0xf];
                *vbuf_OUT++ = pal[(*vbuf8 >> 4) & 0xf];

                vbuf8++;
            }
            break;
        case VGA640x480div3:
            vbuf_OUT8 = (uint8_t *) vbuf_OUT;
            for (int i = x_pixels; i--;) {
                *vbuf_OUT8++ = pal[*vbuf8];
                *vbuf_OUT8++ = pal[*vbuf8] >> 8;
                *vbuf_OUT8++ = pal[*vbuf8++];

            }
            break;

        default:
            break;
    }


    dma_channel_set_read_addr(dma_chan_ctrl, ptr_vbuf_OUT, false);


}


void setVGAmode(enum VGA_mode_t modeVGA) {
    if (modeVGA == VGA640x480_text_40_30) {
        text_buf_width = 40;
    } else {
        text_buf_width = 80;
    }
    if (_SM_VGA < 0) return;//если  VGA не инициализирована -
    //pio_sm_set_enabled(PIO_VGA, _SM_VGA, false);
    //sleep_ms(10);
    if ((txt_palette_fast) && (lines_pattern_data)) {
        mode_VGA = modeVGA;
        return;
    };
    uint8_t TMPL_VHS8 = 0;
    uint8_t TMPL_VS8 = 0;
    uint8_t TMPL_HS8 = 0;
    uint8_t TMPL_LINE8 = 0;

    int line_size;
    double fdiv = 100;
    int HS_SIZE = 4;
    int HS_SHIFT = 100;
    //  irq_remove_handler(VGA_DMA_IRQ,irq_get_exclusive_handler(VGA_DMA_IRQ));

    // if (txt_palette_fast) {
    //                 free(txt_palette_fast);
    //                 txt_palette_fast=NULL;
    //                     }

    switch (modeVGA) {
        case VGA640x480_text_40_30:
        case VGA640x480_text_80_30:
            text_buf_width = (modeVGA == VGA640x480_text_40_30 ? 40 : 80);
            text_buf_height = 30;
            //текстовая палитра

            for (int i = 0; i < 16; i++) {

                txt_palette[i] = (txt_palette[i] & 0x3f) | (palette16_mask >> 8);
            }

            if (!(txt_palette_fast)) {

                txt_palette_fast = (uint16_t *) calloc(256 * 4, sizeof(uint16_t));;
                for (int i = 0; i < 256; i++) {
                    uint8_t c1 = txt_palette[i & 0xf];
                    uint8_t c0 = txt_palette[i >> 4];

                    txt_palette_fast[i * 4 + 0] = (c0) | (c0 << 8);
                    txt_palette_fast[i * 4 + 1] = (c1) | (c0 << 8);
                    txt_palette_fast[i * 4 + 2] = (c0) | (c1 << 8);
                    txt_palette_fast[i * 4 + 3] = (c1) | (c1 << 8);
                }


            }


        case CGA_640x200x2:
        case CGA_320x200x4:
        case CGA_160x200x16:
        case VGA640x480div3:
        case VGA640x480div2:

            TMPL_LINE8 = 0b11000000;
            HS_SHIFT = 328 * 2;
            HS_SIZE = 48 * 2;


            line_size = 400 * 2;

            shift_picture = line_size - HS_SHIFT;

            palette16_mask = 0xc0c0;

            visible_line_size = 320;

            N_lines_total = 525;
            N_lines_visible = 480;
            line_VS_begin = 490;
            line_VS_end = 491;


            fdiv = clock_get_hz(clk_sys) / (25175000.0);//частота пиксельклока
            //   irq_set_exclusive_handler(VGA_DMA_IRQ, dma_handler_VGA);

            break;


        default:
            return;
            break;
    }


    //корректировка  палитры по маске бит синхры
    bg_color[0] = (bg_color[0] & 0x3f3f3f3f) | palette16_mask | (palette16_mask << 16);
    bg_color[1] = (bg_color[1] & 0x3f3f3f3f) | palette16_mask | (palette16_mask << 16);
    for (int i = 0; i < 256; i++) {
        palette[0][i] = (palette[0][i] & 0x3f3f) | palette16_mask;
        palette[1][i] = (palette[1][i] & 0x3f3f) | palette16_mask;
    }






    //line_size=4*(line_size>>2)+4;
    //инициализация шаблонов строк и синхросигнала



    // if (lines_pattern_data) 
    //     {
    //     free(lines_pattern_data);
    //     lines_pattern_data=NULL;
    //     }
    if (!(lines_pattern_data))//выделение памяти, если не выделено
    {
        uint32_t div32 = (uint32_t) (fdiv * (1 << 16) + 0.0);
        PIO_VGA->sm[_SM_VGA].clkdiv = div32 & 0xfffff000; //делитель для конкретной sm
        dma_channel_set_trans_count(dma_chan, line_size / 4, false);


        lines_pattern_data = (uint32_t *) calloc(line_size * 4 / 4, sizeof(uint32_t));;


        for (int i = 0; i < 4; i++) {
            lines_pattern[i] = &lines_pattern_data[i * (line_size / 4)];
        }
        // memset(lines_pattern_data,N_TMPLS*1200,0);
        TMPL_VHS8 = TMPL_LINE8 ^ 0b11000000;
        TMPL_VS8 = TMPL_LINE8 ^ 0b10000000;
        TMPL_HS8 = TMPL_LINE8 ^ 0b01000000;

        uint8_t *base_ptr = (uint8_t *) lines_pattern[0];
        //пустая строка
        memset(base_ptr, TMPL_LINE8, line_size);
        //memset(base_ptr+HS_SHIFT,TMPL_HS8,HS_SIZE);
        //выровненная синхра вначале
        memset(base_ptr, TMPL_HS8, HS_SIZE);


        // кадровая синхра
        base_ptr = (uint8_t *) lines_pattern[1];
        memset(base_ptr, TMPL_VS8, line_size);
        //memset(base_ptr+HS_SHIFT,TMPL_VHS8,HS_SIZE);
        //выровненная синхра вначале
        memset(base_ptr, TMPL_VHS8, HS_SIZE);

        //заготовки для строк с изображением
        base_ptr = (uint8_t *) lines_pattern[2];
        memcpy(base_ptr, lines_pattern[0], line_size);
        base_ptr = (uint8_t *) lines_pattern[3];
        memcpy(base_ptr, lines_pattern[0], line_size);


    }


    mode_VGA = modeVGA;


    //pio_sm_set_enabled(PIO_VGA, _SM_VGA, true);


};

void setVGAbuf(uint8_t *buf, uint16_t w, uint16_t h) {
    g_buf = buf;
    g_buf_width = w;
    g_buf_height = h;
};


void setVGAbuf_pos(int x, int y) {
    g_buf_shx = x;
    g_buf_shy = y;


};

void setVGA_color_flash_mode(bool flash_line, bool flash_frame) {
    is_flash_frame = flash_frame;
    is_flash_line = flash_line;
};

void setVGA_text_buf(uint8_t *buf_text) {
    text_buf = buf_text;
};

void clrScr(uint8_t color) {
    memset(text_buf, 0, text_buf_height * text_buf_width);
    memset(text_buf_color, (color << 4), text_buf_height * text_buf_width);

};

void draw_text(char *string, int x, int y, uint8_t color, uint8_t bgcolor) {
    if ((y < 0) | (y >= text_buf_height)) return;
    int len = strlen(string);
    if (x < 0) {
        if ((len + x) > 0) {
            string += -x;
            x = 0;

        } else return;
    }
    if (x >= text_buf_width) return;

    uint8_t *t_buf = text_buf + (text_buf_width * y) + x;
    //uint8_t* t_buf_c=text_buf+(text_buf_width*y)+x+1;
    for (int xi = x; xi < text_buf_width; xi++) {
        if (!(*string)) break;
        *t_buf++ = *string++;
        *t_buf++ = (bgcolor << 4) | (color & 0xF);

    }


};


void setVGA_bg_color(uint32_t color888) {
    uint8_t conv0[] = { 0b00, 0b00, 0b01, 0b10, 0b10, 0b10, 0b11, 0b11 };
    uint8_t conv1[] = { 0b00, 0b01, 0b01, 0b01, 0b10, 0b11, 0b11, 0b11 };

    uint8_t b = ((color888 & 0xff) / 42);

    uint8_t r = (((color888 >> 16) & 0xff) / 42);
    uint8_t g = (((color888 >> 8) & 0xff) / 42);

    uint8_t c_hi = (conv0[r] << 4) | (conv0[g] << 2) | conv0[b];
    uint8_t c_lo = (conv1[r] << 4) | (conv1[g] << 2) | conv1[b];
    bg_color[0] = (((((c_hi << 8) | c_lo) & 0x3f3f) | palette16_mask) << 16) |
                  ((((c_hi << 8) | c_lo) & 0x3f3f) | palette16_mask);
    bg_color[1] = (((((c_lo << 8) | c_hi) & 0x3f3f) | palette16_mask) << 16) |
                  ((((c_lo << 8) | c_hi) & 0x3f3f) | palette16_mask);

};

void setVGA_color_palette(uint8_t i_color, uint32_t color888) {
    uint8_t conv0[] = { 0b00, 0b00, 0b01, 0b10, 0b10, 0b10, 0b11, 0b11 };
    uint8_t conv1[] = { 0b00, 0b01, 0b01, 0b01, 0b10, 0b11, 0b11, 0b11 };

    uint8_t b = ((color888 & 0xff) / 42);

    uint8_t r = (((color888 >> 16) & 0xff) / 42);
    uint8_t g = (((color888 >> 8) & 0xff) / 42);

    uint8_t c_hi = (conv0[r] << 4) | (conv0[g] << 2) | conv0[b];
    uint8_t c_lo = (conv1[r] << 4) | (conv1[g] << 2) | conv1[b];

    palette[0][i_color] = (((c_hi << 8) | c_lo) & 0x3f3f) | palette16_mask;
    palette[1][i_color] = (((c_lo << 8) | c_hi) & 0x3f3f) | palette16_mask;

};

void initVGA() {
    //инициализация палитры по умолчанию
#if 1
    uint8_t conv0[] = { 0b00, 0b00, 0b01, 0b10, 0b10, 0b10, 0b11, 0b11 };
    uint8_t conv1[] = { 0b00, 0b01, 0b01, 0b01, 0b10, 0b11, 0b11, 0b11 };
    for (int i = 0; i < 256; i++) {
        uint8_t b = (i & 0b11);

        uint8_t r = ((i >> 5) & 0b111);
        uint8_t g = ((i >> 2) & 0b111);

        uint8_t c_hi = 0xc0 | (conv0[r] << 4) | (conv0[g] << 2) | b;
        uint8_t c_lo = 0xc0 | (conv1[r] << 4) | (conv1[g] << 2) | b;


        palette[0][i] = (c_hi << 8) | c_lo;
        palette[1][i] = (c_lo << 8) | c_hi;
    }
#endif
    //текстовая палитра
    for (int i = 0; i < 16; i++) {
        uint8_t b = (i & 1) ? ((i >> 3) ? 3 : 2) : 0;
        uint8_t r = (i & 4) ? ((i >> 3) ? 3 : 2) : 0;
        uint8_t g = (i & 2) ? ((i >> 3) ? 3 : 2) : 0;

        uint8_t c = (r << 4) | (g << 2) | b;

        txt_palette[i] = (c & 0x3f) | 0xc0;
    }



    //инициализация PIO
    //загрузка программы в один из PIO
    uint offset = pio_add_program(PIO_VGA, &pio_program_VGA);
    _SM_VGA = pio_claim_unused_sm(PIO_VGA, true);
    uint sm = _SM_VGA;

    for (int i = 0; i < 8; i++) {
        gpio_init(beginVGA_PIN + i);
        gpio_set_dir(beginVGA_PIN + i, GPIO_OUT);
        pio_gpio_init(PIO_VGA, beginVGA_PIN + i);
    };//резервируем под выход PIO

    //pio_sm_config c = pio_vga_program_get_default_config(offset); 

    pio_sm_set_consecutive_pindirs(PIO_VGA, sm, beginVGA_PIN, 8, true);//конфигурация пинов на выход

    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + 0, offset + (pio_program_VGA.length - 1));


    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);//увеличение буфера TX за счёт RX до 8-ми 
    sm_config_set_out_shift(&c, true, true, 32);
    sm_config_set_out_pins(&c, beginVGA_PIN, 8);
    pio_sm_init(PIO_VGA, sm, offset, &c);


    pio_sm_set_enabled(PIO_VGA, sm, true);



    //инициализация DMA


    dma_chan_ctrl = dma_claim_unused_channel(true);
    dma_chan = dma_claim_unused_channel(true);
    //основной ДМА канал для данных
    dma_channel_config c0 = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&c0, DMA_SIZE_32);

    channel_config_set_read_increment(&c0, true);
    channel_config_set_write_increment(&c0, false);

    uint dreq = DREQ_PIO1_TX0 + sm;
    if (PIO_VGA == pio0) dreq = DREQ_PIO0_TX0 + sm;

    channel_config_set_dreq(&c0, dreq);
    channel_config_set_chain_to(&c0, dma_chan_ctrl);                        // chain to other channel

    dma_channel_configure(
            dma_chan,
            &c0,
            &PIO_VGA->txf[sm], // Write address
            lines_pattern[0],             // read address
            600 / 4, //
            false             // Don't start yet
    );
    //канал DMA для контроля основного канала
    dma_channel_config c1 = dma_channel_get_default_config(dma_chan_ctrl);
    channel_config_set_transfer_data_size(&c1, DMA_SIZE_32);

    channel_config_set_read_increment(&c1, false);
    channel_config_set_write_increment(&c1, false);
    channel_config_set_chain_to(&c1, dma_chan);                         // chain to other channel
    //channel_config_set_dreq(&c1, DREQ_PIO0_TX0);



    dma_channel_configure(
            dma_chan_ctrl,
            &c1,
            &dma_hw->ch[dma_chan].read_addr, // Write address
            &lines_pattern[0],             // read address
            1, //
            false             // Don't start yet
    );
    //dma_channel_set_read_addr(dma_chan, &DMA_BUF_ADDR[0], false);




    setVGAmode(VGA640x480div2);


    irq_set_exclusive_handler(VGA_DMA_IRQ, dma_handler_VGA);

    dma_channel_set_irq0_enabled(dma_chan_ctrl, true);

    irq_set_enabled(VGA_DMA_IRQ, true);
    dma_start_channel_mask((1u << dma_chan));


};
