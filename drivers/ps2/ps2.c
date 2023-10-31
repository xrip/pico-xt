#include "ps2.h"
#include <pico/stdlib.h>
#include <stdbool.h>
#include "string.h"
#include "hardware/irq.h"


volatile int bitcount;
volatile uint16_t data;
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
    now_ms = time_us_32();
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
    data = 0;
    memset(ps2buffer, 0, KBD_BUFFER_SIZE);

    //buffer = (uint8_t *)&this->kbd_buffer;
    //printf("[%08X][%08X]\n",*&buffer,&this->kbd_buffer);
    gpio_init(KBD_CLOCK_PIN);
    gpio_disable_pulls(KBD_CLOCK_PIN);
    gpio_set_dir(KBD_CLOCK_PIN, GPIO_IN);
    gpio_init(KBD_DATA_PIN);
    gpio_disable_pulls(KBD_DATA_PIN);
    gpio_set_dir(KBD_DATA_PIN, GPIO_IN);

    //gpio_init(KBD_MIRROR_PIN);
    //gpio_set_dir(KBD_MIRROR_PIN,GPIO_OUT);

    gpio_init(25);
    gpio_set_dir(25, GPIO_OUT);

    gpio_set_irq_enabled_with_callback(KBD_CLOCK_PIN, GPIO_IRQ_EDGE_FALL, true,
                                       (gpio_irq_callback_t) &KeyboardHandler); //
}

extern uint8_t portram[0x400];

extern void doirq(uint8_t irqnum);

void ps2poll() {
    uint32_t ps2scancode;
    ps2scancode = ps2getcode();
    if (!ps2scancode) return;
    portram[0x60] = ps2scancode;
    portram[0x64] |= 2;
    doirq(1);
}

