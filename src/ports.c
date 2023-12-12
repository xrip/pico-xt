/* ports.c - handles port I/O for Fake86 CPU core. it's ugly, will fix up later. */
#include "emulator.h"

#if PICO_ON_DEVICE

#include "ps2.h"
#include "vga.h"
#include "nespad.h"
#endif
uint16_t portram[256];
uint8_t VGA_CRT_index, VGA_CRT[25]; // CRT controller
uint8_t VGA_ATTR_index, VGA_ATTR[21]; // VGA attribute regisers
uint8_t VGA_SC_index, VGA_SC[5]; // VGA Sequencer Registers
uint8_t VGA_GC_index, VGA_GC[9]; // VGA Other Graphics Registers

uint16_t port378, port37A, port3D8, port3D9, port3DA, port201;
volatile uint16_t port379;

void ports_reboot() {
    notify_a20_line_state_changed(false);
    memset(portram, 0, sizeof(portram));
    port378, port379, port37A, port3D8, port3D9, port3DA, port201 = 0;
}

static uint8_t vga_palette_index = 0;
static uint8_t vga_color_index = 0;
static uint8_t dac_state = 0;
static uint8_t latchReadRGB = 0, latchReadPal = 0;

extern volatile uint16_t true_covox;

uint32_t ega_plane_offset = 0;
bool vga_planar_mode = false;

static bool flip3C0 = false;

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
        //
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
            out8237(portnum, value & 255);
            return;
#endif
        case 0x20:
        case 0x21: //i8259
            out8259(portnum, value & 255);
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
        case 0xC0:
        case 0xC1:
        case 0xC2:
        case 0xC3:
        case 0xC4:
        case 0xC5:
        case 0xC6:
        case 0xC7:
            sn76489_out(value);
        break;
#if SOUND_BLASTER
        //
        case 0x220:
        case 0x221:
        case 0x222:
        case 0x223:
        case 0x224:
        case 0x225:
        case 0x226:
        case 0x227:
        case 0x228:
        case 0x229:
        case 0x22a:
        case 0x22b:
        case 0x22c:
        case 0x22d:
        case 0x22e:
        //case 0x22f:
            outBlaster(portnum, value);
            break;
#endif
#if DSS
        case 0x378: // ssData
        case 0x37A: // ssControl
            dss_out(portnum, value);
            break;
        case 0x3BD: // covox data port
            true_covox = value;
            break;
#endif
#if SOUND_BLASTER || ADLIB
        case 0x388: // adlib
        case 0x389:
            outadlib(portnum, value);
            break;
#endif
#endif
/*
        case 0x3B8: // TODO: hercules support
            break;
*/
        case 0x3C0:
            ///printf("EGA control register 3c0 0x%x\r\n", value);
            if (flip3C0 && VGA_ATTR_index <= 0xf) {
                printf("3c0 COLOR %i 0x%x\r\n", VGA_ATTR_index, value);

                const uint8_t r = (value & 0b001 ? 2 : 0) + (value & 0b111000 ? 1 : 0);
                const uint8_t g = (value & 0b010 ? 2 : 0) + (value & 0b111000 ? 1 : 0);
                const uint8_t b = (value & 0b100 ? 2 : 0) + (value & 0b111000 ? 1 : 0);

                vga_palette[VGA_ATTR_index] = rgb(b * 85, g * 85, r * 85);
            } else if (flip3C0) {
                VGA_ATTR[VGA_ATTR_index] = value & 255;
            }
            VGA_ATTR_index = value;
            flip3C0 ^= true;

            break;
        case 0x3C4: //sequence controller index
            // TODO: implement other EGA sequences
            VGA_SC_index = value & 255;
            break;
        case 0x3C5: //sequence controller data
            VGA_SC[VGA_SC_index] = value & 255;
            if (VGA_SC_index == 0) {
                ega_plane_offset = 0;
                vga_planar_mode = 0;
            }
            if ((VGA_SC_index & 0x1F) == 0x02) {
                switch (value) {
                    case 0x02: ega_plane_offset = (VGA_plane_size * 1);
                        break;
                    case 0x04: ega_plane_offset = (VGA_plane_size * 2);
                        break;
                    case 0x08: ega_plane_offset = (VGA_plane_size * 3);
                        break;
                    default:
                        ega_plane_offset = 0;
                }
            }
            if (VGA_SC_index == 0x04) {
                vga_planar_mode = (value & 6) ? true : false;
#if  PICO_ON_DEVICE
                if (vga_planar_mode && videomode == 0x13) {
                    graphics_set_mode(VGA_320x200x256x4);
                } else {
                    graphics_set_mode(VGA_320x200x256);
                }
#endif
            }
            break;
        case 0x3C7: //color index register (read operations)
            //printf("W 0x%x : 0x%x\r\n", portnum, value);
            latchReadPal = value % 255;
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
            static uint8_t r,g,b;
            value = value & 63;

            switch (vga_color_index) {
                case 0: //red
                    r = value;
                    break;
                case 1: //green
                    g = value;
                    break;
                case 2: //blue
                    b =  value;
                    vga_palette[vga_palette_index] = rgb(r << 2, g << 2, b << 2);
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
        case 0x3CE:
            VGA_GC_index = value & 255;
        break;
        // https://frolov-lib.ru/books/bsp.old/v03/ch7.htm
        // https://swag.outpostbbs.net/EGAVGA/0222.PAS.html
        case 0x3CF:
            VGA_GC[VGA_GC_index]= value & 255;
        case 0x3D4:
            // http://www.techhelpmanual.com/901-color_graphics_adapter_i_o_ports.html
            VGA_CRT_index = value;
            break;
        case 0x3D5:
            //printf("port3d5 0x%x\r\n", value);
            VGA_CRT[VGA_CRT_index] = value;
            if ((VGA_CRT_index == 0x0E) || (VGA_CRT_index == 0x0F)) {
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
#if DSS
        case 0x378: return port378;
        case 0x379: // ssStatus
            return dss_in(portnum);
        case 0x3BE: // LPT2 status (covox is always ready)
            return 0;
#endif
#if SOUND_BLASTER || ADLIB
        case 0x388: // adlib
        case 0x389:
            return inadlib(portnum);
#endif
#if SOUND_BLASTER
        case 0x220:
        case 0x221:
        case 0x222:
        case 0x223:
        case 0x224:
        case 0x225:
        case 0x226:
        case 0x227:
        case 0x228:
        case 0x229:
        case 0x22a:
        case 0x22b:
        case 0x22c:
        case 0x22d:
        case 0x22e:
            return inBlaster(portnum);
#endif
        #endif
        // http://www.techhelpmanual.com/900-video_graphics_array_i_o_ports.html
        // https://wiki.osdev.org/VGA_Hardware#Port_0x3C0
        // https://files.osdev.org/mirrors/geezer/osd/graphics/modes.c
        // https://vtda.org/books/Computing/Programming/EGA-VGA-ProgrammersReferenceGuide2ndEd_BradleyDyckKliewer.pdf
        case 0x3C0: // Attribute Address Register (read/write)
            return VGA_ATTR_index;
        case 0x3C1: // Palette Registers / Other Attribute Registers (write)
            return VGA_ATTR[VGA_ATTR_index];
        case 0x3C4: // Sequencer Address Register (read/write)
            return VGA_SC_index;
        case 0x3C5:
            return VGA_SC[VGA_SC_index];
        case 0x3C7: //DAC state
            return dac_state;
        case 0x3C8: //palette index
            return latchReadPal;
        case 0x3C9: //RGB data register
            //printf("latchReadRGB %i\r\n", latchReadPal);
            switch (latchReadRGB++) {
                case 0: //blue
                    return ((vga_palette[latchReadPal] >> 16) >> 2) & 63;
                case 1: //green
                    return ((vga_palette[latchReadPal] >> 8) >> 2) & 63;
                case 2: //red
                    latchReadRGB = 0;
                    return (vga_palette[latchReadPal++] >> 2) & 63;
            }
            break;
        case 0x3D4: // CRT controller address
            return VGA_CRT_index;
        case 0x3D5: // CRT controller internal registers
            return VGA_CRT[VGA_CRT_index];
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
