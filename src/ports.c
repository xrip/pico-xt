/* ports.c - handles port I/O for Fake86 CPU core. it's ugly, will fix up later. */

#include <stdint.h>
#include "emu.h"

volatile uint16_t pit0counter = 65535;
volatile uint32_t speakercountdown, latch42, pit0latch, pit0command, pit0divisor;
uint8_t portram[0x400];
uint8_t crt_controller_idx, crt_controller[256], port3D8 = 0, port3D9 = 0;

void portout(uint16_t portnum, uint16_t value) {
    if (portnum != 0x20)
    printf("IO: writing WORD port %Xh with data %04Xh\r\n", portnum, value);
  if (portnum < 0x400) portram[portnum] = value;
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
    case 0x3D4:
      crt_controller_idx = value;
      break;
    case 0x3D5:
      crt_controller[crt_controller_idx] = value;
      if ((crt_controller_idx == 0x0E) || (crt_controller_idx == 0x0F)) {
        //setcursor(((uint16_t)crt_controller[0x0E] << 8) | crt_controller[0x0F]);
        //Serial.write(27); Serial.write('['); Serial.print(crt_controller[0x0E] + 1); Serial.write(';'); Serial.print(crt_controller[0x0F] + 1); Serial.write('H');
      }
      break;
      case 0x3D8:
          vidmode = (value & 0x02) ? 4 : 3;
          break;
    case 0x3D9:
      port3D9 = value;
      break;
  }

#ifdef ADVANCED_CLIENT
  if ((portnum >= 0x3C0) && (portnum <= 0x3DA)) {
    uint8_t chksum;
    Serial.write(0xFF);
    Serial.write(0x04);
    outByte(portnum & 0xFF); chksum = portnum & 0xFF;
    outByte(portnum >> 8); chksum += portnum >> 8;
    outByte(value); chksum += value;
    outByte(chksum);
    Serial.write(0xFE);
    Serial.write(0x02);
  }
#endif

#ifdef VGA
  if ((portnum >= 0x3C0) && (portnum <= 0x3DA)) outVGA(portnum, value);
#endif
}

uint16_t portin(uint16_t portnum) {
    if (portnum != 0x3DA)
    printf("IO: reading WORD port %Xh with result of data %04Xh\r\n", portnum, 0);
#ifdef VGA
  if ((portnum >= 0x3C0) && (portnum <= 0x3DA)) return inVGA(portnum);
#endif
/*  uint8_t chksum;
  Serial.write(0xFF);
  Serial.write(0x07);
  outByte(portnum & 0xFF); chksum = portnum & 0xFF;
  outByte((portnum >> 8) & 0xFF); chksum += (portnum >> 8) & 0xFF;
  outByte(chksum);
  Serial.write(0xFE);
  Serial.write(0x02);*/
  switch (portnum) {
    case 0x20:
    case 0x21: //i8259
      return (in8259(portnum));
    case 0x40:
    case 0x41:
    case 0x42:
    case 0x43: //i8253
      return in8253(portnum);
    case 0x60:
    case 0x64:
      return portram[portnum];
    case 0x3D4:
      return crt_controller_idx;
    case 0x3D5:
      return crt_controller[crt_controller_idx];
      case 0x3D8:
          switch (vidmode) {
              case 0:
                  return (0x2C);
              case 1:
                  return (0x28);
              case 2:
                  return (0x2D);
              case 3:
                  return (0x29);
              case 4:
                  return (0x0E);
              case 5:
                  return (0x0A);
              case 6:
                  return (0x1E);
              default:
                  return (0x29);
          }
      case 0x3D9:
          return port3D9;
    case 0x3DA:
        port3DA ^= 1;
      if (!(port3DA & 1)) port3DA ^= 8;
      //port3da = random(256) & 9;
      return (port3DA);
    default:
      return (0xFF);
  }
}


