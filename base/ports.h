//
// Created by xrip on 23.10.2023.
//
#pragma once
#ifndef TINY8086_PORTS_H
#define TINY8086_PORTS_H

#include <stdint.h>
#include <stdio.h>
#include "i8259.h"

#include <time.h>

uint8_t random_uint8() {
    // Seed the random number generator with current time
    srand(time(NULL));

    // Generate a random uint8_t value
    uint8_t random_value = rand() % 256;

    return random_value;
}

extern uint8_t videomode;
uint16_t portram[256];

uint16_t pit0counter = 65535;
uint32_t speakercountdown = 1320;                                   // random value to start with
uint32_t latch42, pit0latch, pit0command, pit0divisor;

uint8_t crt_controller_idx, crt_controller[256];
uint16_t divisorLatchWord;

uint8_t pr3FArst = 0;
uint16_t pr3F8;
uint16_t pr3F9;
uint16_t pr3FA;
uint16_t pr3FB;
uint16_t pr3FC;
uint16_t pr3FD;
uint16_t pr3FE;
uint16_t pr3FF;
uint8_t pr3D9;
uint8_t port3da;

uint8_t COM1OUT;
uint8_t COM1IN = 0;

void portout(uint16_t portnum, uint16_t value) {
    if (portnum < 256) portram[portnum] = value;

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
            } break;
        case 0x42: //speaker countdown
            if (latch42 == 0) {
                speakercountdown = (speakercountdown & 0xFF00) + value;
                latch42 = 1;
            } else {
                speakercountdown = (speakercountdown & 0xFF) + value * 256;
                latch42 = 0;
                speakercountdown = speakercountdown /10;

                if (speakercountdown == 0) {
                    /* FIXME!!
                    analogWrite(SPK_PIN, 0);                      // set 0% (0) duty clcle ==> Sound output off
                    digitalWrite(SPK_PIN, LOW);                   // set the pin LOW to avoid current flowing through speaker.
                     */
                }
                else {
                    /* FIXME!!
                    SPK_FREQ = (238500/speakercountdown);             // Calculate the resulting frequency of the counter output
                    //  SPK_FREQ = (477000/speakercountdown);             // Calculate the resulting frequency of the counter output --- wrong frequency?? 2x!

                    if (SPK_FREQ < 20000) {                           // Human audible frequency
                        analogWriteFreq(SPK_FREQ);                    // set PWM frequency.
                        SPK_FREQ = 1;
                    } else {
                        analogWrite(SPK_PIN, 0);                      // set 0% (0) duty clcle ==> Sound output off
                        digitalWrite(SPK_PIN, LOW);                   // set the pin LOW to avoid current flowing through speaker.
                        SPK_FREQ = 1;
                    }
                    if (portram[0x61]&2){
                        analogWrite(SPK_PIN, 127);                    // set 50% (127) duty cycle ==> Sound output on
                    } else {
                        analogWrite(SPK_PIN, 0);                      // set 0% (0) duty clcle ==> Sound output off
                        digitalWrite(SPK_PIN, LOW);                   // set the pin LOW to avoid current flowing through speaker.
                    }
                     */
                }
            }
            break;

        case 0x43: //pit 0 command port
            pit0command = value;
            switch (pit0command) {
                case 0x34:
                case 0x36: //reprogram pit 0 divisor
                    pit0latch = 0; break;
                default:
                    latch42 = 0; break;
            } break;

        case 0x61:
            portram[0x61] = value;
            if ((value&2) && (speakercountdown >0)) {
                // FIXME!!
                //analogWrite(SPK_PIN, 127);                    // set 50% (127) duty cycle ==> Sound output on
            } else {
                // FIXME!!
                //analogWrite(SPK_PIN, 0);                      // set 0% (0) duty clcle ==> Sound output off
                //digitalWrite(SPK_PIN, LOW);                   // set the pin LOW to avoid current flowing through speaker.
            }
            break;

        case 0x64:
            break;

        case 0x3D4:
            crt_controller_idx = value;
            break;

        case 0x3D5:
            break;

        case 0x3D9:
            pr3D9 = value;
            break;

        case 0x3F8:               ////////////////////// WS: comm 1
            pr3F8 = value;
            if (pr3FB & 0x80) {
                divisorLatchWord |= value;                    // Divisor Latch Low Byte
                /* FIXME!!
switch (divisorLatchWord) {

case 0x900: COM1baud = 50;  break;
case 0x417: COM1baud = 110; break;
case 0x20C: COM1baud = 220; break;
case 0x180: COM1baud = 300; break;
case 0xC0:  COM1baud = 600; break;
case 0x60:  COM1baud = 1200; break;
case 0x30:  COM1baud = 2400; break;
case 0x18:  COM1baud = 4800; break;
case 0x0C:  COM1baud = 9600; break;
case 0x06:  COM1baud = 19200; break;
case 0x03:  COM1baud = 38400; break;
case 0x02:  COM1baud = 57600; break;
case 0x01:  COM1baud = 115200; break;
default:    COM1baud = 2400; break;
}
swSer1.end();
swSer1.begin(COM1baud, COM1Config, SWSER_RX, SWSER_TX, false, SWSERBUFCAP, SWSERISRCAP);
 */

            } else {
                // FIXME!!
                COM1OUT = value;
                pr3FA = 1;
            }
            break;

        case 0x3F9:
            if (pr3FB & 0x80) {
                divisorLatchWord = (value<<8);               // Divisor Latch High Byte
            } else {
                pr3F9 = value;
            }
            break;

        case 0x3FA:
            pr3FA = value;
            break;

        case 0x3FB:
            pr3FB = value;
            /* FIXME!!
            switch (pr3FB & 0x3F) {
                case 0x00:  COM1Config = SWSERIAL_5N1; break;
                case 0x01:  COM1Config = SWSERIAL_6N1; break;
                case 0x02:  COM1Config = SWSERIAL_7N1; break;
                case 0x03:  COM1Config = SWSERIAL_8N1; break;
                case 0x04:  COM1Config = SWSERIAL_5N2; break;
                case 0x05:  COM1Config = SWSERIAL_6N2; break;
                case 0x06:  COM1Config = SWSERIAL_7N2; break;
                case 0x07:  COM1Config = SWSERIAL_8N2; break;
                case 0x08:  COM1Config = SWSERIAL_5O1; break;
                case 0x09:  COM1Config = SWSERIAL_6O1; break;
                case 0x0A:  COM1Config = SWSERIAL_7O1; break;
                case 0x0B:  COM1Config = SWSERIAL_8O1; break;
                case 0x0C:  COM1Config = SWSERIAL_5O2; break;
                case 0x0D:  COM1Config = SWSERIAL_6O2; break;
                case 0x0E:  COM1Config = SWSERIAL_7O2; break;
                case 0x0F:  COM1Config = SWSERIAL_8O2; break;
                case 0x18:  COM1Config = SWSERIAL_5E1; break;
                case 0x19:  COM1Config = SWSERIAL_6E1; break;
                case 0x1A:  COM1Config = SWSERIAL_7E1; break;
                case 0x1B:  COM1Config = SWSERIAL_8E1; break;
                case 0x1C:  COM1Config = SWSERIAL_5E2; break;
                case 0x1D:  COM1Config = SWSERIAL_6E2; break;
                case 0x1E:  COM1Config = SWSERIAL_7E2; break;
                case 0x1F:  COM1Config = SWSERIAL_8E2; break;
                case 0x28:  COM1Config = SWSERIAL_5M1; break;
                case 0x29:  COM1Config = SWSERIAL_6M1; break;
                case 0x2A:  COM1Config = SWSERIAL_7M1; break;
                case 0x2B:  COM1Config = SWSERIAL_8M1; break;
                case 0x2C:  COM1Config = SWSERIAL_5M2; break;
                case 0x2D:  COM1Config = SWSERIAL_6M2; break;
                case 0x2E:  COM1Config = SWSERIAL_7M2; break;
                case 0x2F:  COM1Config = SWSERIAL_8M2; break;
                case 0x38:  COM1Config = SWSERIAL_5S1; break;
                case 0x39:  COM1Config = SWSERIAL_6S1; break;
                case 0x3A:  COM1Config = SWSERIAL_7S1; break;
                case 0x3B:  COM1Config = SWSERIAL_8S1; break;
                case 0x3C:  COM1Config = SWSERIAL_5S2; break;
                case 0x3D:  COM1Config = SWSERIAL_6S2; break;
                case 0x3E:  COM1Config = SWSERIAL_7S2; break;
                case 0x3F:  COM1Config = SWSERIAL_8S2; break;
                default: COM1Config = SWSERIAL_8N1; break;
            }
            swSer1.end();
            swSer1.begin(COM1baud, COM1Config, SWSER_RX, SWSER_TX, false, SWSERBUFCAP, SWSERISRCAP);
            */
            break;

        case 0x3FC:
            pr3FC = value;
            break;

        case 0x3FD:
            pr3FD = value;
            break;

        case 0x3FE:
            pr3FE = value;
            break;

        case 0x3FF:
            pr3FF = value;
            break;
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
            return portram[portnum];

        case 0x61:
            return portram[portnum];
            break;

        case 0x64:
            return portram[portnum];

        case 0x3D4:
            return crt_controller_idx;
            break;

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
            break;

        case 0x3D9:
            return pr3D9;
            break;

        case 0x3F8:
            return 0x00;
            if (COM1IN) {
                pr3F8 = COM1IN;
                COM1IN = 0;
                pr3FA = 1;
                return (pr3F8);
            }
            else {
                return (pr3F8);
            }
            break;

        case 0x3F9:
            return (pr3F9);

        case 0x3FA:
            if (pr3FArst == 1) {
                pr3FA =1;
                pr3FArst=0;
            }

            if (pr3FA !=1) pr3FArst = 1;
            return (pr3FA);

        case 0x3FB:
            return (pr3FB);

        case 0x3FC:
            return (pr3FC);

        case 0x3FD:
            return (pr3FD);

        case 0x3FE:
            return (0xBB);

        case 0x3FF:
            return (pr3FF);

        case 0x3D5:
            return crt_controller[crt_controller_idx];
            break;

        case 0x3DA:
            port3da = random_uint8(256);
            return (port3da);
        default:
            return (0xFF);
    }
}

static void portout16( uint16_t portnum, uint16_t value )
{
#ifdef DEBUG_PORT_TRAFFIC
    printf("IO: writing WORD port %Xh with data %04Xh\n", portnum, value);
#endif
    portout(portnum, (uint8_t)value);
    portout(portnum + 1, (uint8_t)(value >> 8));
}

static uint16_t portin16( uint16_t portnum )
{
    uint16_t ret = (uint16_t)portin(portnum);
    ret |= ((uint16_t)portin(portnum + 1) << 8);
#ifdef DEBUG_PORT_TRAFFIC
    printf("IO: reading WORD port %Xh with result of data %04Xh\n", portnum, ret);
#endif
    return ret;
}
#endif //TINY8086_PORTS_H
