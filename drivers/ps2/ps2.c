#include "ps2.h"
#include <pico/stdlib.h>
#include <stdbool.h>
#include "string.h"
#include "hardware/irq.h"


volatile int bitcount;
static uint8_t ps2bufsize = 0;
uint8_t ps2buffer[KBD_BUFFER_SIZE];
uint8_t kbloop = 0;

void ps2poll();

uint8_t ps2_to_xt_1(uint32_t val) {
    uint8_t i;
    for (i = 0; i < 85; i++) {
        if (ps2_group1[i].make == val) return ps2_group1[i].xt_make;
    }
    return 0;
}

uint8_t ps2_to_xt_2(uint32_t val) {
    uint8_t i;
    for (i = 0; i < 16; i++) {
        if (ps2_group2[i].xt_make == val) return ps2_group2[i].make;
    }
    return 0;
}

uint32_t ps2getcode() {
    uint32_t retval, i, len;
    if (!ps2bufsize) return 0;
    switch (ps2buffer[0]) {
        case 0xF0:
        case 0xE0:
        case 0xE1:
            len = 2;
            break;
        default:
            len = 1;
            break;
    }
    if (ps2bufsize < len) return 0;
    if (ps2buffer[0] == 0xE0) {
        if (ps2buffer[1] == 0xF0) len = 3;
    }
    if (ps2bufsize < len) return 0;
    retval = 0;

    //for (i=0; i<len; i++) {
    //    Serial.write(8);
    //    Serial.write(ps2buffer[i]);
    //}

    //translate code
    if (len == 1) {
        retval = ps2_to_xt_1(ps2buffer[0]);
    }
    if (len == 2) {
        if (ps2buffer[0] == 0xF0) retval = ps2_to_xt_1(ps2buffer[1]) | 0x80;
        if (ps2buffer[0] == 0xE0) retval = ps2_to_xt_2(ps2buffer[1]);
    }
    if (len == 3) {
        if ((ps2buffer[0] == 0xE0) && (ps2buffer[1] == 0xF0)) retval = ps2_to_xt_2(ps2buffer[2]) | 0x80;
    }
    //end translate code

    for (i = len; i < KBD_BUFFER_SIZE; i++) {
        ps2buffer[i - len] = ps2buffer[i];
    }
    ps2bufsize -= len;
    return retval;
}

void KeyboardHandler(void) {
    static uint8_t incoming = 0;
    static uint32_t prev_ms = 0;
    uint32_t now_ms;
    uint8_t n, val;

    val = gpio_get(KBD_DATA_PIN);
    now_ms = time_us_64();
    if (now_ms - prev_ms > 250) {
        bitcount = 0;
        incoming = 0;
    }
    prev_ms = now_ms;
    n = bitcount - 1;
    if (n <= 7) {
        incoming |= (val << n);
    }
    bitcount++;
    if (bitcount == 11) {
        if (ps2bufsize < KBD_BUFFER_SIZE) {
            ps2buffer[ps2bufsize++] = incoming;
            ps2poll();
        }

        bitcount = 0;
        incoming = 0;
    }
    kbloop = 1;
}

void Init_kbd(void) {
    bitcount = 0;
    memset(ps2buffer, 0, KBD_BUFFER_SIZE);

    gpio_init(KBD_CLOCK_PIN);
    gpio_disable_pulls(KBD_CLOCK_PIN);
    gpio_set_dir(KBD_CLOCK_PIN, GPIO_IN);
    gpio_init(KBD_DATA_PIN);
    gpio_disable_pulls(KBD_DATA_PIN);
    gpio_set_dir(KBD_DATA_PIN, GPIO_IN);

    gpio_set_irq_enabled_with_callback(KBD_CLOCK_PIN, GPIO_IRQ_EDGE_FALL, true,
                                       (gpio_irq_callback_t) &KeyboardHandler); //
}

extern uint16_t portram[256];
extern void doirq(uint8_t irqnum);

void ps2poll() {
    uint32_t ps2scancode;
    ps2scancode = ps2getcode();
    if (!ps2scancode){
        return;
    }
    portram[0x60] = ps2scancode;
    portram[0x64] |= 2;
    doirq(1);
}

void translate_scancode(uint8_t code,bool is_press, bool is_e0,bool is_e1){
    if (is_e1){
        if (code==0x14) {if (is_press) kbd_statuses[2]|=KB_U2_PAUSE_BREAK; else kbd_statuses[2]&=~KB_U2_PAUSE_BREAK;}
        return;
    }

    if (!is_e0)
        switch (code){
            //0
            case 0x1C: if (is_press) kbd_statuses[0]|=KB_U0_A; else kbd_statuses[0]&=~KB_U0_A; break;
            case 0x32: if (is_press) kbd_statuses[0]|=KB_U0_B; else kbd_statuses[0]&=~KB_U0_B; break;
            case 0x21: if (is_press) kbd_statuses[0]|=KB_U0_C; else kbd_statuses[0]&=~KB_U0_C; break;
            case 0x23: if (is_press) kbd_statuses[0]|=KB_U0_D; else kbd_statuses[0]&=~KB_U0_D; break;
            case 0x24: if (is_press) kbd_statuses[0]|=KB_U0_E; else kbd_statuses[0]&=~KB_U0_E; break;
            case 0x2B: if (is_press) kbd_statuses[0]|=KB_U0_F; else kbd_statuses[0]&=~KB_U0_F; break;
            case 0x34: if (is_press) kbd_statuses[0]|=KB_U0_G; else kbd_statuses[0]&=~KB_U0_G; break;
            case 0x33: if (is_press) kbd_statuses[0]|=KB_U0_H; else kbd_statuses[0]&=~KB_U0_H; break;
            case 0x43: if (is_press) kbd_statuses[0]|=KB_U0_I; else kbd_statuses[0]&=~KB_U0_I; break;
            case 0x3B: if (is_press) kbd_statuses[0]|=KB_U0_J; else kbd_statuses[0]&=~KB_U0_J; break;

            case 0x42: if (is_press) kbd_statuses[0]|=KB_U0_K; else kbd_statuses[0]&=~KB_U0_K; break;
            case 0x4B: if (is_press) kbd_statuses[0]|=KB_U0_L; else kbd_statuses[0]&=~KB_U0_L; break;
            case 0x3A: if (is_press) kbd_statuses[0]|=KB_U0_M; else kbd_statuses[0]&=~KB_U0_M; break;
            case 0x31: if (is_press) kbd_statuses[0]|=KB_U0_N; else kbd_statuses[0]&=~KB_U0_N; break;
            case 0x44: if (is_press) kbd_statuses[0]|=KB_U0_O; else kbd_statuses[0]&=~KB_U0_O; break;
            case 0x4D: if (is_press) kbd_statuses[0]|=KB_U0_P; else kbd_statuses[0]&=~KB_U0_P; break;
            case 0x15: if (is_press) kbd_statuses[0]|=KB_U0_Q; else kbd_statuses[0]&=~KB_U0_Q; break;
            case 0x2D: if (is_press) kbd_statuses[0]|=KB_U0_R; else kbd_statuses[0]&=~KB_U0_R; break;
            case 0x1B: if (is_press) kbd_statuses[0]|=KB_U0_S; else kbd_statuses[0]&=~KB_U0_S; break;
            case 0x2C: if (is_press) kbd_statuses[0]|=KB_U0_T; else kbd_statuses[0]&=~KB_U0_T; break;

            case 0x3C: if (is_press) kbd_statuses[0]|=KB_U0_U; else kbd_statuses[0]&=~KB_U0_U; break;
            case 0x2A: if (is_press) kbd_statuses[0]|=KB_U0_V; else kbd_statuses[0]&=~KB_U0_V; break;
            case 0x1D: if (is_press) kbd_statuses[0]|=KB_U0_W; else kbd_statuses[0]&=~KB_U0_W; break;
            case 0x22: if (is_press) kbd_statuses[0]|=KB_U0_X; else kbd_statuses[0]&=~KB_U0_X; break;
            case 0x35: if (is_press) kbd_statuses[0]|=KB_U0_Y; else kbd_statuses[0]&=~KB_U0_Y; break;
            case 0x1A: if (is_press) kbd_statuses[0]|=KB_U0_Z; else kbd_statuses[0]&=~KB_U0_Z; break;

            case 0x54: if (is_press) kbd_statuses[0]|=KB_U0_LEFT_BR; else kbd_statuses[0]&=~KB_U0_LEFT_BR; break;
            case 0x5B: if (is_press) kbd_statuses[0]|=KB_U0_RIGHT_BR; else kbd_statuses[0]&=~KB_U0_RIGHT_BR; break;
            case 0x4C: if (is_press) kbd_statuses[0]|=KB_U0_SEMICOLON; else kbd_statuses[0]&=~KB_U0_SEMICOLON; break;
            case 0x52: if (is_press) kbd_statuses[0]|=KB_U0_QUOTE; else kbd_statuses[0]&=~KB_U0_QUOTE; break;
            case 0x41: if (is_press) kbd_statuses[0]|=KB_U0_COMMA; else kbd_statuses[0]&=~KB_U0_COMMA; break;
            case 0x49: if (is_press) kbd_statuses[0]|=KB_U0_PERIOD; else kbd_statuses[0]&=~KB_U0_PERIOD; break;

                //1 -----------
            case 0x45: if (is_press) kbd_statuses[1]|=KB_U1_0; else kbd_statuses[1]&=~KB_U1_0; break;
            case 0x16: if (is_press) kbd_statuses[1]|=KB_U1_1; else kbd_statuses[1]&=~KB_U1_1; break;
            case 0x1E: if (is_press) kbd_statuses[1]|=KB_U1_2; else kbd_statuses[1]&=~KB_U1_2; break;
            case 0x26: if (is_press) kbd_statuses[1]|=KB_U1_3; else kbd_statuses[1]&=~KB_U1_3; break;
            case 0x25: if (is_press) kbd_statuses[1]|=KB_U1_4; else kbd_statuses[1]&=~KB_U1_4; break;
            case 0x2E: if (is_press) kbd_statuses[1]|=KB_U1_5; else kbd_statuses[1]&=~KB_U1_5; break;
            case 0x36: if (is_press) kbd_statuses[1]|=KB_U1_6; else kbd_statuses[1]&=~KB_U1_6; break;
            case 0x3D: if (is_press) kbd_statuses[1]|=KB_U1_7; else kbd_statuses[1]&=~KB_U1_7; break;
            case 0x3E: if (is_press) kbd_statuses[1]|=KB_U1_8; else kbd_statuses[1]&=~KB_U1_8; break;
            case 0x46: if (is_press) kbd_statuses[1]|=KB_U1_9; else kbd_statuses[1]&=~KB_U1_9; break;

            case 0x4E: if (is_press) kbd_statuses[1]|=KB_U1_MINUS; else kbd_statuses[1]&=~KB_U1_MINUS; break;
            case 0x55: if (is_press) kbd_statuses[1]|=KB_U1_EQUALS; else kbd_statuses[1]&=~KB_U1_EQUALS; break;
            case 0x5D: if (is_press) kbd_statuses[1]|=KB_U1_BACKSLASH; else kbd_statuses[1]&=~KB_U1_BACKSLASH; break;
            case 0x66: if (is_press) kbd_statuses[1]|=KB_U1_BACK_SPACE; else kbd_statuses[1]&=~KB_U1_BACK_SPACE; break;
            case 0x5A: if (is_press) kbd_statuses[1]|=KB_U1_ENTER; else kbd_statuses[1]&=~KB_U1_ENTER; break;
            case 0x4A: if (is_press) kbd_statuses[1]|=KB_U1_SLASH; else kbd_statuses[1]&=~KB_U1_SLASH; break;
            case 0x0E: if (is_press) kbd_statuses[1]|=KB_U1_TILDE; else kbd_statuses[1]&=~KB_U1_TILDE; break;
            case 0x0D: if (is_press) kbd_statuses[1]|=KB_U1_TAB; else kbd_statuses[1]&=~KB_U1_TAB; break;
            case 0x58: if (is_press) kbd_statuses[1]|=KB_U1_CAPS_LOCK; else kbd_statuses[1]&=~KB_U1_CAPS_LOCK; break;
            case 0x76: if (is_press) kbd_statuses[1]|=KB_U1_ESC; else kbd_statuses[1]&=~KB_U1_ESC; break;

            case 0x12: if (is_press) kbd_statuses[1]|=KB_U1_L_SHIFT; else kbd_statuses[1]&=~KB_U1_L_SHIFT; break;
            case 0x14: if (is_press) kbd_statuses[1]|=KB_U1_L_CTRL; else kbd_statuses[1]&=~KB_U1_L_CTRL; break;
            case 0x11: if (is_press) kbd_statuses[1]|=KB_U1_L_ALT; else kbd_statuses[1]&=~KB_U1_L_ALT; break;
            case 0x59: if (is_press) kbd_statuses[1]|=KB_U1_R_SHIFT; else kbd_statuses[1]&=~KB_U1_R_SHIFT; break;

            case 0x29: if (is_press) kbd_statuses[1]|=KB_U1_SPACE; else kbd_statuses[1]&=~KB_U1_SPACE; break;
                //2 -----------
            case 0x70: if (is_press) kbd_statuses[2]|=KB_U2_NUM_0; else kbd_statuses[2]&=~KB_U2_NUM_0; break;
            case 0x69: if (is_press) kbd_statuses[2]|=KB_U2_NUM_1; else kbd_statuses[2]&=~KB_U2_NUM_1; break;
            case 0x72: if (is_press) kbd_statuses[2]|=KB_U2_NUM_2; else kbd_statuses[2]&=~KB_U2_NUM_2; break;
            case 0x7A: if (is_press) kbd_statuses[2]|=KB_U2_NUM_3; else kbd_statuses[2]&=~KB_U2_NUM_3; break;
            case 0x6B: if (is_press) kbd_statuses[2]|=KB_U2_NUM_4; else kbd_statuses[2]&=~KB_U2_NUM_4; break;
            case 0x73: if (is_press) kbd_statuses[2]|=KB_U2_NUM_5; else kbd_statuses[2]&=~KB_U2_NUM_5; break;
            case 0x74: if (is_press) kbd_statuses[2]|=KB_U2_NUM_6; else kbd_statuses[2]&=~KB_U2_NUM_6; break;
            case 0x6C: if (is_press) kbd_statuses[2]|=KB_U2_NUM_7; else kbd_statuses[2]&=~KB_U2_NUM_7; break;
            case 0x75: if (is_press) kbd_statuses[2]|=KB_U2_NUM_8; else kbd_statuses[2]&=~KB_U2_NUM_8; break;
            case 0x7D: if (is_press) kbd_statuses[2]|=KB_U2_NUM_9; else kbd_statuses[2]&=~KB_U2_NUM_9; break;

            case 0x77: if (is_press) kbd_statuses[2]|=KB_U2_NUM_LOCK; else kbd_statuses[2]&=~KB_U2_NUM_LOCK; break;
            case 0x7C: if (is_press) kbd_statuses[2]|=KB_U2_NUM_MULT; else kbd_statuses[2]&=~KB_U2_NUM_MULT; break;
            case 0x7B: if (is_press) kbd_statuses[2]|=KB_U2_NUM_MINUS; else kbd_statuses[2]&=~KB_U2_NUM_MINUS; break;
            case 0x79: if (is_press) kbd_statuses[2]|=KB_U2_NUM_PLUS; else kbd_statuses[2]&=~KB_U2_NUM_PLUS; break;
            case 0x71: if (is_press) kbd_statuses[2]|=KB_U2_NUM_PERIOD; else kbd_statuses[2]&=~KB_U2_NUM_PERIOD; break;
            case 0x7E: if (is_press) kbd_statuses[2]|=KB_U2_SCROLL_LOCK; else kbd_statuses[2]&=~KB_U2_SCROLL_LOCK; break;
                //3 -----------
            case 0x05: if (is_press) kbd_statuses[3]|=KB_U3_F1; else kbd_statuses[3]&=~KB_U3_F1; break;
            case 0x06: if (is_press) kbd_statuses[3]|=KB_U3_F2; else kbd_statuses[3]&=~KB_U3_F2; break;
            case 0x04: if (is_press) kbd_statuses[3]|=KB_U3_F3; else kbd_statuses[3]&=~KB_U3_F3; break;
            case 0x0C: if (is_press) kbd_statuses[3]|=KB_U3_F4; else kbd_statuses[3]&=~KB_U3_F4; break;
            case 0x03: if (is_press) kbd_statuses[3]|=KB_U3_F5; else kbd_statuses[3]&=~KB_U3_F5; break;
            case 0x0B: if (is_press) kbd_statuses[3]|=KB_U3_F6; else kbd_statuses[3]&=~KB_U3_F6; break;
            case 0x83: if (is_press) kbd_statuses[3]|=KB_U3_F7; else kbd_statuses[3]&=~KB_U3_F7; break;
            case 0x0A: if (is_press) kbd_statuses[3]|=KB_U3_F8; else kbd_statuses[3]&=~KB_U3_F8; break;
            case 0x01: if (is_press) kbd_statuses[3]|=KB_U3_F9; else kbd_statuses[3]&=~KB_U3_F9; break;
            case 0x09: if (is_press) kbd_statuses[3]|=KB_U3_F10; else kbd_statuses[3]&=~KB_U3_F10; break;

            case 0x78: if (is_press) kbd_statuses[3]|=KB_U3_F11; else kbd_statuses[3]&=~KB_U3_F11; break;
            case 0x07: if (is_press) kbd_statuses[3]|=KB_U3_F12; else kbd_statuses[3]&=~KB_U3_F12; break;



            default:
                break;
        }
    if (is_e0)
        switch (code){
            //1----------------
            case 0x1F: if (is_press) kbd_statuses[1]|=KB_U1_L_WIN; else kbd_statuses[1]&=~KB_U1_L_WIN; break;
            case 0x14: if (is_press) kbd_statuses[1]|=KB_U1_R_CTRL; else kbd_statuses[1]&=~KB_U1_R_CTRL; break;
            case 0x11: if (is_press) kbd_statuses[1]|=KB_U1_R_ALT; else kbd_statuses[1]&=~KB_U1_R_ALT; break;
            case 0x27: if (is_press) kbd_statuses[1]|=KB_U1_R_WIN; else kbd_statuses[1]&=~KB_U1_R_WIN; break;
            case 0x2F: if (is_press) kbd_statuses[1]|=KB_U1_MENU; else kbd_statuses[1]&=~KB_U1_MENU; break;
                //2------------------
                //для принт скрин обработаем только 1 код

            case 0x7C: if (is_press) kbd_statuses[2]|=KB_U2_PRT_SCR; break;
            case 0x12: if (!is_press) kbd_statuses[2]&=~KB_U2_PRT_SCR; break;

            case 0x4A: if (is_press) kbd_statuses[2]|=KB_U2_NUM_SLASH; else kbd_statuses[2]&=~KB_U2_NUM_SLASH; break;
            case 0x5A: if (is_press) kbd_statuses[2]|=KB_U2_NUM_ENTER; else kbd_statuses[2]&=~KB_U2_NUM_ENTER; break;
            case 0x75: if (is_press) kbd_statuses[2]|=KB_U2_UP; else kbd_statuses[2]&=~KB_U2_UP; break;
            case 0x72: if (is_press) kbd_statuses[2]|=KB_U2_DOWN; else kbd_statuses[2]&=~KB_U2_DOWN; break;
            case 0x74: if (is_press) kbd_statuses[2]|=KB_U2_RIGHT; else kbd_statuses[2]&=~KB_U2_RIGHT; break;
            case 0x6B: if (is_press) kbd_statuses[2]|=KB_U2_LEFT; else kbd_statuses[2]&=~KB_U2_LEFT; break;
            case 0x71: if (is_press) kbd_statuses[2]|=KB_U2_DELETE; else kbd_statuses[2]&=~KB_U2_DELETE; break;
            case 0x69: if (is_press) kbd_statuses[2]|=KB_U2_END; else kbd_statuses[2]&=~KB_U2_END; break;
            case 0x7A: if (is_press) kbd_statuses[2]|=KB_U2_PAGE_DOWN; else kbd_statuses[2]&=~KB_U2_PAGE_DOWN; break;
            case 0x7D: if (is_press) kbd_statuses[2]|=KB_U2_PAGE_UP; else kbd_statuses[2]&=~KB_U2_PAGE_UP; break;

            case 0x6C: if (is_press) kbd_statuses[2]|=KB_U2_HOME; else kbd_statuses[2]&=~KB_U2_HOME; break;
            case 0x70: if (is_press) kbd_statuses[2]|=KB_U2_INSERT; else kbd_statuses[2]&=~KB_U2_INSERT; break;
        }
}
uint8_t __not_in_flash_func(get_scan_code)(void){ //__not_in_flash_func()
    uint8_t i = ps2bufsize;
    if (++i >= KBD_BUFFER_SIZE) i = 0;
    return ps2buffer[i];
}

bool decode_kbd(){
    static bool is_e0=false;
    static bool is_e1=false;
    static bool is_f0=false;

    //char test_str[128];
    uint8_t scancode=get_scan_code();
    if (scancode==0xe0) {is_e0=true;return false;} //ps2Flags.
    if (scancode==0xe1) {is_e1=true;return false;}
    if (scancode==0xf0) {is_f0=true;return false;}
    if (scancode){
        //сканкод
        //получение универсальных кодов из сканкодов PS/2
        translate_scancode(scancode,!is_f0,is_e0,is_e1);
        //keys_to_str(test_str,' ');
        //DEH_printf("is_e0=%d, is_f0=%d, code=0x%02x test_str=%s\n",is_e0,is_f0,scancode,test_str);

        is_e0=false;
        if (is_f0) is_e1=false;
        is_f0=false;
        //преобразование из универсальных сканкодов в матрицу бытрого преобразования кодов для zx клавиатуры
        //zx_kb_decode(zx_keyboard_state);

        return true;//произошли изменения

    }
    /*if((time_us_32()-last_key)>500){
        bitcount = 0;
    }*/
    return false;
}
