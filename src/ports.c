/* ports.c - handles port I/O for Fake86 CPU core. it's ugly, will fix up later. */
#include "emulator.h"

#if PICO_ON_DEVICE

#include "ps2.h"
#include "vga.h"
#include "nespad.h"

#endif
uint16_t portram[256];
uint8_t crt_controller_idx, crt_controller[18];
uint16_t port378, port379, port37A, port3D8, port3D9, port3DA, port201;

void ports_reboot() {
    notify_a20_line_state_changed(false);
    memset(portram, 0, sizeof(portram));
    port378, port379, port37A, port3D8, port3D9, port3DA, port201 = 0;
}

static uint8_t vga_palette_index = 0;
static uint8_t vga_color_index = 0;
static uint8_t dac_state = 0;
static uint8_t latchReadRGB = 0, latchReadPal = 0;

#define ega_plane_size 16000
uint16_t ega_plane_offset = 0;
static uint16_t port3C4 = 0;
static uint16_t port3C5 = 0;
static uint8_t port3C0 = 0xff;
static bool port3C0_flipflop = false;

void portout(uint16_t portnum, uint16_t value) {
    //if (portnum == 0x80) {
    //    printf("Diagnostic port out: %04X\r\n", value);
    //}
    switch (portnum) {
#ifdef DMA_8237
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
        case 0x0C:
        case 0x0D:
        case 0x0E:
        case 0x0F:
        case 0x80:
        case 0x81:
        case 0x82:
        case 0x83:
        case 0x84:
        case 0x85:
        case 0x86:
        case 0x87:
        case 0x88:
        case 0x89:
        case 0x8A:
        case 0x8B:
        case 0x8C:
        case 0x8D:
        case 0x8E:
        case 0x8F:
            out8237(portnum, value);
            return;
#endif
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
        case 0x60:
            if (portnum < 256) portram[portnum] = value;
            break;
#ifdef XMS_DRIVER
        case PORT_A20:
            portram[portnum] = value;
            if (value & A20_ENABLE_BIT) {
                notify_a20_line_state_changed(true);
            }
            else {
                notify_a20_line_state_changed(false);
            }
            break;
#endif
        case 0x64: // Passthrought all
#if PICO_ON_DEVICE
            keyboard_send(value);
            portram[portnum] = value;
#endif
        case 0x61: // 061H  PPI port B.
            portram[portnum] = value;
#if PICO_ON_DEVICE
            if ((value & 3) == 3) {
                pwm_set_gpio_level(BEEPER_PIN, 127);
            }
            else {
                pwm_set_gpio_level(BEEPER_PIN, 0);
            }
#endif
            break;
#ifdef SOUND_SYSTEM
        case 0x378:
        case 0x37A:
            outsoundsource(portnum, value);
            break;
        case 0x388: // adlib
        case 0x389:
            outadlib(portnum, value);
            break;
#endif
        case 0x3C0:
            ///printf("EGA control register 3c0 0x%x\r\n", value);
            if (port3C0_flipflop && port3C4 <= 0xf) {
                printf("3c0 COLOR %i 0x%x\r\n", port3C4, value);
                const uint8_t b = (value & 0b001 ? 2 : 0) + (value & 0b111000 ? 1 : 0);
                const uint8_t g = (value & 0b010 ? 2 : 0) + (value & 0b111000 ? 1 : 0);
                const uint8_t r = (value & 0b100 ? 2 : 0) + (value & 0b111000 ? 1 : 0);
                vga_palette[port3C4] = rgb(r * 85, g * 85, b * 85);
            }
            port3C4 = value;
            port3C0_flipflop ^= true;

            break;
        case 0x3C4: //sequence controller index
            // TODO: implement other EGA sequences
            port3C4 = value & 255;
            break;
        case 0x3C5: //sequence controller data
            // TODO: Это грязный хак, сделать нормально
            port3C5 = value & 255;
            if ((port3C4 & 0x1F) == 0x02) {
                switch (value) {
                    case 0x02: ega_plane_offset = ega_plane_size * 1;
                        break;
                    case 0x04: ega_plane_offset = ega_plane_size * 2;
                        break;
                    case 0x08: ega_plane_offset = ega_plane_size * 3;
                        break;
                    default:
                        ega_plane_offset = 0;
                }
            }
            break;
        case 0x3C7: //color index register (read operations)
            //printf("W 0x%x : 0x%x\r\n", portnum, value);
            dac_state = 0;
            latchReadRGB = 0;
            break;
        case 0x3C8: //color index register (write operations)
            //printf("W 0x%x : 0x%x\r\n", portnum, value);
            vga_color_index = 0;
            dac_state = 3;
            vga_palette_index = value & 255;
            break;
        case 0x3C9: //RGB data register
        {
            //printf("W 0x%x : 0x%x\r\n", portnum, value);
            static uint32_t color;
            //value = value & 63;
            //value = value; // & 63;
            switch (vga_color_index) {
                case 0: //red
                    color = value << 16;
                    break;
                case 1: //green
                    color |= value << 8;
                    break;
                case 2: //blue
                    color |= value;
                    vga_palette[vga_palette_index] = color << 2;
#if PICO_ON_DEVICE
                    graphics_set_palette(vga_palette_index, vga_palette[vga_palette_index]);
#endif
                //printf("RGB#%i %x\r\n", vga_palette_index, vga_palette[vga_palette_index]);
                    vga_palette_index++;
                    break;
            }
            vga_color_index = (vga_color_index + 1) % 3;

            break;
        }
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
        case 0x3D9:
            port3D9 = value;
            if (videomode >= 0xd) return;
            uint8_t bg_color = value & 0xf;
            cga_colorset = value >> 5 & 1;
            cga_intensity = value >> 4 & 1;
            char tmp[80];
            sprintf(tmp, "colorset %i, int %i value  %x", cga_colorset, cga_intensity, value);
            logMsg(tmp);

#if PICO_ON_DEVICE
            graphics_set_palette(0, bg_color != 0xf ? cga_palette[bg_color] : 0);

            if ((videomode == 6 && (port3D8 & 0x0f) == 0b1010) || videomode >= 8) {
                break;
            }

            for (int i = 1; i < 4; i++) {
                graphics_set_palette(i, cga_palette[cga_gfxpal[cga_intensity][cga_colorset][i]]);
            }
        //setVGA_color_palette(0, cga_palette[0]);
#else

        if (bg_color != 0xf) {
            cga_composite_palette[0][0] = cga_palette[bg_color];
            tandy_palette[0] = cga_palette[bg_color];
            printf("setting cga color\r\n");
        } else {
            cga_composite_palette[0][0] = 0;
            tandy_palette[0] = 0;
        }

           //cga_composite_palette[0][0] = cga_palette[bg_color];

#endif
            break;
        case 0x3D8: // CGA Mode control register
            // https://www.seasip.info/VintagePC/cga.html
            port3D8 = value;
            if (videomode >= 0xd) return;
        // third cga palette (black/red/cyan/white)
            if (videomode == 5 && (port3D8 >> 2) & 1) {
                logMsg("the unofficial Mode 5 palette, accessed by disabling ColorBurst\n");
                cga_colorset = 2;
#if PICO_ON_DEVICE
                for (int i = 0; i < 4; i++) {
                    graphics_set_palette(i, cga_palette[cga_gfxpal[cga_intensity][cga_colorset][i]]);
                }
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
        // TODO: Включение/выключение глобального композитного режима по хоткеямы
            if ((videomode == 6 /*|| videomode == 4*/) && (port3D8 & 0x0f) == 0b1010) {
                printf("160x200x16");
#if PICO_ON_DEVICE
                for (int i = 0; i < 16; i++) {
                    graphics_set_palette(i, cga_composite_palette[0][i]);
                }
                graphics_set_mode(CGA_160x200x16);
#else
                videomode = videomode == 4 ? 64 : 66;
#endif
            }
            break;
        case 0x3DA: // EGA control register
            printf("Tandy control register 3dA 0x%x\r\n", value);
            break;
        case 0x3DE: // tandy register
            printf("Tandy control register 3dE 0x%x\r\n", value);
            break;
        case 0x3DF: // tandy CRT/Processor Page Register
            printf("Tandy control register 3df 0x%x\r\n", value);
        /*
        CRT/Processor Page Register
        This 8-bit (write-only) register is addressed at 3DF. The descriptions below are
        of the register functions:
        Bit	Description
        0	CRT Page 0
        1	CRT Page 1
        2	CRT Page 2
        3	Processor Page 0
        4	Processor Page 1
        5	Processor Page 2
        6	Video Address Mode 0
        7	Video Address Mode 1
        */
            break;
        case 0x3F8:
        case 0x3F9:
        case 0x3FA:
        case 0x3FB:
        case 0x3FC:
        case 0x3FD:
        case 0x3FE:
        case 0x3FF:
            outsermouse(portnum, value);
            break;
        default:
            if (portnum < 256) portram[portnum] = value;
    }
}

uint16_t portin(uint16_t portnum) {
    switch (portnum) {
#ifdef DMA_8237
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
        case 0x0C:
        case 0x0D:
        case 0x0E:
        case 0x0F:
        case 0x80:
        case 0x81:
        case 0x82:
        case 0x83:
        case 0x84:
        case 0x85:
        case 0x86:
        case 0x87:
        case 0x88:
        case 0x89:
        case 0x8A:
        case 0x8B:
        case 0x8C:
        case 0x8D:
        case 0x8E:
        case 0x8F:
            return in8237(portnum);
#endif
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
#ifdef XMS_DRIVER
        case PORT_A20:
            return is_a20_line_open() ? (portram[portnum] | A20_ENABLE_BIT) : (portram[portnum] & !A20_ENABLE_BIT);
#endif
        /*case 0x201: // joystick
            return 0b11110000;*/
#ifdef SOUND_SYSTEM
        case 0x379:
            return insoundsource(portnum);
        case 0x388: // adlib
        case 0x389:
            return inadlib(portnum);
#endif
        case 0x3C0:
            return port3C4;
        case 0x3C4: //sequence controller index
            // https://wiki.osdev.org/VGA_Hardware#Port_0x3C0
            // https://files.osdev.org/mirrors/geezer/osd/graphics/modes.c
            // https://vtda.org/books/Computing/Programming/EGA-VGA-ProgrammersReferenceGuide2ndEd_BradleyDyckKliewer.pdf
            // TODO: implement other EGA sequences
            return port3C4;
        case 0x3C5:
            if (port3C4 == 0x02) return port3C5;
            return 0xFF;
        case 0x3C7: //DAC state
            return dac_state;
        case 0x3C8: //palette index
            return latchReadPal;
        case 0x3C9: //RGB data register
            switch (latchReadRGB++) {
                case 0: //blue
                    return (vga_palette[latchReadPal] >> 2) & 63;
                case 1: //green
                    return (vga_palette[latchReadPal] >> 2) & 63;
                case 2: //red
                    latchReadRGB = 0;
                    return (vga_palette[latchReadPal++] >> 2) & 63;
            }
            break;
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
        case 0x3F8:
        case 0x3F9:
        case 0x3FA:
        case 0x3FB:
        case 0x3FC:
        case 0x3FD:
        case 0x3FE:
        case 0x3FF:
            return insermouse(portnum);
        default:
            return 0xFF;
    }
}

void portout16(uint16_t portnum, uint16_t value) {
#ifdef DEBUG_PORT_TRAFFIC
    printf("IO: writing WORD port %Xh with data %04Xh\n", portnum, value);
#endif
    portout(portnum, (uint8_t)value);
    portout(portnum + 1, (uint8_t)(value >> 8));
}

uint16_t portin16(uint16_t portnum) {
    uint16_t ret = (uint16_t)portin(portnum);
    ret |= ((uint16_t)portin(portnum + 1) << 8);
#ifdef DEBUG_PORT_TRAFFIC
    printf("IO: reading WORD port %Xh with result of data %04Xh\n", portnum, ret);
#endif
    return ret;
}
