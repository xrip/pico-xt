/* ports.c - handles port I/O for Fake86 CPU core. it's ugly, will fix up later. */
#include "emulator.h"

#if PICO_ON_DEVICE

#include "ps2.h"
#include "vga.h"

#endif
uint16_t portram[256];
uint8_t crt_controller_idx, crt_controller[256];
uint16_t port3D8, port3D9, port3DA, port201;

void portout(uint16_t portnum, uint16_t value) {
    if (portnum == 0x80) {
        printf("Diagnostic port out: %04X\r\n", value);
    }
    switch (portnum) {
        case 0x20:
        case 0x21: //i8259
            out8259(portnum, value);
            return;
        case 0x40:
        case 0x41:
        case 0x42:
        case 0x43: //i8253
            out8253(portnum, value);
            break;

#if PICO_ON_DEVICE
        case 0x64: // Passthrought all
            portram[portnum] = value;
            ps2_send(value);
#endif
        case 0x61: // 061H  PPI port B.
            portram[portnum] = value;
#if PICO_ON_DEVICE
            if ((value & 3) == 3) {
                pwm_set_gpio_level(26, 127);
                pwm_set_gpio_level(27, 127);
            } else {
                pwm_set_gpio_level(26, 0);
                pwm_set_gpio_level(27, 0);
            }
#endif
            break;

/*        case 0x201:
            port201 = value;
            break;*/
        case 0x3D4:
            // http://www.techhelpmanual.com/901-color_graphics_adapter_i_o_ports.html
            crt_controller_idx = value;
            break;
        case 0x3D5:
            //printf("port3d5 0x%x\r\n", value);
            crt_controller[crt_controller_idx] = value;
            if ((crt_controller_idx == 0x0E) || (crt_controller_idx == 0x0F)) {
                //setcursor(((uint16_t)crt_controller[0x0E] << 8) | crt_controller[0x0F]);
            }

            break;
        case 0x3D8: // CGA Mode control register
            // https://www.seasip.info/VintagePC/cga.html
            port3D8 = value;

            //printf("port3D8 0x%x\r\n", value);
            // third cga palette (black/red/cyan/white)
            if (videomode == 5 && (port3D8 >> 2) & 1) {
#if PICO_ON_DEVICE
                graphics_set_palette(0, cga_palette[0]);
                graphics_set_palette(1, cga_palette[4]);
                graphics_set_palette(2, cga_palette[3]);
                graphics_set_palette(3, cga_palette[15]);
#endif
            }

            // 160x100x16
            if ((videomode == 2 || videomode == 3) && (port3D8 & 0x0f) == 0b0001) {
                printf("160x100x16");
#if PICO_ON_DEVICE
                graphics_set_mode(TEXTMODE_160x100);
#else
                videomode = 76;
#endif
            }

            // 160x200x16
            if (videomode == 6 && (port3D8 & 0x0f) == 0b1010) {
                printf("160x200x16");
#if PICO_ON_DEVICE
                    for (int i = 0; i < 16; i++) {
                        graphics_set_palette(i, cga_composite_palette[0][i]);
                    }
                graphics_set_mode(CGA_160x200x16);
#else
                videomode = 66;
#endif
            }
            break;
        case 0x3D9:
            port3D9 = value;
#if PICO_ON_DEVICE
            if ((videomode == 6 || videomode == 8) && (port3D8 & 0x0f) == 0b1010) {
                break;
            }
            cga_colorset = (value >> 5) & 1;
            cga_intensity = ((value >> 4) & 1);
            printf("colorset %i, int %i\r\n", cga_colorset, cga_intensity);
            for (int i = 0; i < 4; i++) {
                graphics_set_palette(i, cga_palette[cga_gfxpal[cga_intensity][cga_colorset][i]]);
            }
            //setVGA_color_palette(0, cga_palette[0]);
#endif
            break;
        case 0x3DA:
            break;
        default:
            if (portnum < 256) portram[portnum] = value;
    }
}

uint16_t portin(uint16_t portnum) {
    switch (portnum) {
        case 0x20:
        case 0x21: //i8259
            return in8259(portnum);
        case 0x40:
        case 0x41:
        case 0x42:
        case 0x43: //i8253
            return in8253(portnum);
        case 0x60:
        case 0x61:
        case 0x64:
            return portram[portnum];
/*        case 0x201:
            return port201;*/
        case 0x3D4:
            return crt_controller_idx;
        case 0x3D5:
            return crt_controller[crt_controller_idx];
        case 0x3D8:
            return port3D8;
        case 0x3D9:
            return port3D9;
        case 0x3DA:
            port3DA ^= 1;
            if (!(port3DA & 1)) port3DA ^= 8;
            //port3da = random(256);
            return port3DA;
        default:
            return 0xFF;
    }
}

__inline void portout16(uint16_t portnum, uint16_t value) {
#ifdef DEBUG_PORT_TRAFFIC
    printf("IO: writing WORD port %Xh with data %04Xh\n", portnum, value);
#endif
    portout(portnum, (uint8_t) value);
    portout(portnum + 1, (uint8_t) (value >> 8));
}

__inline uint16_t portin16(uint16_t portnum) {
    uint16_t ret = (uint16_t) portin(portnum);
    ret |= ((uint16_t) portin(portnum + 1) << 8);
#ifdef DEBUG_PORT_TRAFFIC
    printf("IO: reading WORD port %Xh with result of data %04Xh\n", portnum, ret);
#endif
    return ret;
}
