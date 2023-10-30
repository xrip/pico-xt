/* ports.c - handles port I/O for Fake86 CPU core. it's ugly, will fix up later. */
#pragma once
#ifndef TINY8086_PORTS_H
#define TINY8086_PORTS_H

#include <stdint.h>
#include "i8259.h"
#include "vga.h"
#include "cga.h"

uint16_t pit0counter = 65535;
uint32_t speakercountdown, latch42, pit0latch, pit0command, pit0divisor;
uint16_t portram[256];
uint8_t crt_controller_idx, crt_controller[256];
uint16_t port3da;
uint16_t pr3D9;

void portout(uint16_t portnum, uint16_t value) {
    switch (portnum) {
        case 0x20:
        case 0x21: //i8259
            out8259(portnum, value);
            return;
        case 0x40: //pit 0 data port
            switch (pit0command) {
                case 0xB6:
                case 0x34:
                case 0x36:
                    if (pit0latch == 0) {
                        pit0divisor = (pit0divisor & 0xFF00) + (value & 0xFF);
                        pit0latch = 1;
                        return;
                    } else {
                        pit0divisor = (pit0divisor & 0xFF) + (value & 0xFF) * 256;
                        pit0latch = 0;
                        if (pit0divisor == 0) pit0divisor = 65536;
                        return;
                    }
            }
            break;
        case 0x42: //speaker countdown
            if (latch42 == 0) {
                speakercountdown = (speakercountdown & 0xFF00) + value;
                latch42 = 1;
            } else {
                speakercountdown = (speakercountdown & 0xFF) + value * 256;
                latch42 = 0;
            }
            break;
        case 0x43: //pit 0 command port
            pit0command = value;
            switch (pit0command) {
                case 0x34:
                case 0x36: //reprogram pit 0 divisor
                    pit0latch = 0;
                    break;
                default:
                    latch42 = 0;
                    break;
            }
            break;
        case 0x3D4:
            crt_controller_idx = value;
            break;
        case 0x3D5:
            crt_controller[crt_controller_idx] = value;
            if ((crt_controller_idx == 0x0E) || (crt_controller_idx == 0x0F)) {
                //setcursor(((uint16_t)crt_controller[0x0E] << 8) | crt_controller[0x0F]);
            }
            break;
        case 0x3DA:
            break;

        case 0x3D9:
            pr3D9 = value;
            uint32_t usepal = (value>>5) & 1;
            uint32_t intensity = ( (value>>4) & 1) << 3;
            for (int i = 0; i < 16; ++i) {
                setVGA_color_palette(i, cga_color(i*2+usepal+intensity));
            }
            break;
        default:
            if (portnum < 256) portram[portnum] = value;
    }
}

uint16_t portin(uint16_t portnum) {
    switch (portnum) {
        case 0x20:
        case 0x21: //i8259
            return (in8259(portnum));
        case 0x40:
            if (pit0latch == 0) {
                pit0counter = (millis() % 55) * 1192;
                pit0latch = 1;
                return (pit0counter & 0xFF);
            } else {
                pit0latch = 0;
                return ((pit0counter >> 8) & 0xFF);
            }
        case 0x43:
            return (pit0command);
        case 0x60:
        case 0x64:
            return portram[portnum];
        case 0x3D4:
            return crt_controller_idx;
        case 0x3D5:
            return crt_controller[crt_controller_idx];
        case 0x3D8:
            switch (videomode) {
                case 0: return (0x2C);
                case 1: return (0x28);
                case 2: return (0x2D);
                case 3: return (0x29);
                case 4: return (0x0E);
                case 5: return (0x0A);
                case 6: return (0x1E);
                default: return (0x29);
            }

        case 0x3D9:
            return pr3D9;

        case 0x3DA:
            port3da ^= 1;
            if (!(port3da & 1)) port3da ^= 8;
            //port3da = random(256);
            return (port3da);
        default:
            return (0xFF);
    }
}

static void portout16(uint16_t portnum, uint16_t value) {
#ifdef DEBUG_PORT_TRAFFIC
    printf("IO: writing WORD port %Xh with data %04Xh\n", portnum, value);
#endif
    portout(portnum, (uint8_t) value);
    portout(portnum + 1, (uint8_t) (value >> 8));
}

static uint16_t portin16(uint16_t portnum) {
    uint16_t ret = (uint16_t) portin(portnum);
    ret |= ((uint16_t) portin(portnum + 1) << 8);
#ifdef DEBUG_PORT_TRAFFIC
    printf("IO: reading WORD port %Xh with result of data %04Xh\n", portnum, ret);
#endif
    return ret;
}

#endif