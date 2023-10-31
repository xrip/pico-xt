#include "emu.h"
#ifndef USE_ENC28J60
#include <enc28j60.h>

uint8_t net_mac[6] = { 0x90, 0xAD, 0xBE, 0xEF, 0x13, 0x37 };

extern union _bytewordregs_ regs;
extern uint16_t segregs[6];

struct netstruct {
  uint8_t enabled;
  uint8_t canrecv;
  uint16_t pktlen;
} net;

uint8_t bufSerial[1600], escape = 0;
uint16_t bufSerialLen = 0;

void outByte(uint8_t cc) {
  switch (cc) {
    case 0xFE:
      Serial.write(0xFE); //escape
      Serial.write(0x00);
      break;
    case 0xFF:
      Serial.write(0xFE);
      Serial.write(0x01);
      break;
    default:
      Serial.write(cc);
      break;
  }
}

void processSerBuf() {
  uint8_t chksum = 0;
  uint16_t i;
  bufSerialLen--; //for checksum at the end
  for (i=0; i<bufSerialLen; i++) {
    chksum += bufSerial[i];
  }
  if (chksum != bufSerial[i]) return;

  switch (bufSerial[0]) { //first byte is the command type
    case 0x00: //received ethernet packet
      //sendpkt(&bufSerial[1], bufSerialLen - 1);
      if (!net.enabled || !net.canrecv) return;
      net.canrecv = 0;
      net.pktlen = bufSerialLen - 1;
      doirq(6);
      break;
    case 0x01: //received keyboard scancode
      portram[0x60] = bufSerial[1];
      portram[0x64] |= 2;
      doirq(1);
      break;
    case 0x02: //full screen refresh request
      Serial.write(0xFF);
      Serial.write(0x02);
      Serial.write(vidmode);
      Serial.write(vidmode); //duplicate for checksum
      Serial.write(0xFE);
      Serial.write(0x02);
      portout(0x3D4, portin(0x3D4));
      portout(0x3D8, portin(0x3D8));
      portout(0x3D9, portin(0x3D9));
      for (i=0; i<16384; i++) {
        VRAM_write(i, VRAM_read(i));
      }
      break;
    case 0x03: //received disk sector
      for (i=1; i<513; i++) {
        sectorbuffer[i-1] = bufSerial[i];
      }
      sectdone = 1;
      break;
    case 0x04: //received disk write ack
      sectdone = 1;
      break;
  }
}

void addSerInBuf(uint8_t cc) {
  if (bufSerialLen == 1600) return;
  bufSerial[bufSerialLen++] = cc;
}

void bufSerIn(uint8_t cc) {
  if (cc == 0xFF) { //reset stream code
    bufSerialLen = 0;
    escape = 0;
    return;
  }

  if (escape) {
    switch (cc) {
      case 0x00: //literal 0xFE
        addSerInBuf(0xFE);
        break;
      case 0x01: //literal 0xFF
        addSerInBuf(0xFF);
        break;
      case 0x02: //end of command
        processSerBuf();
        bufSerialLen = 0;
        break;
      default:
        addSerInBuf(cc);
        break;
    }
    escape = 0;
  } else {
    if (cc == 0xFE) { //enter escape mode
      escape = 1;
    } else {
      addSerInBuf(cc);
    }
  }
}

void sendCanRecv() {
        Serial.write(0xFF);
        Serial.write(0x01);
        Serial.write(0x00);
        Serial.write(0x00);
        Serial.write(0xFE);
        Serial.write(0x02);
}

void net_handler() {
  uint32_t i;
  uint16_t j;
  uint8_t chksum, cc;
  //if (ethif==254) return; //networking not enabled
  //Serial.println("entered net_handler()");
  switch (regs.byteregs[regah]) { //function number
      case 0x00: //enable packet reception
        net.enabled = 1;
        net.canrecv = 1;
        return;
      case 0x01: //send packet of CX at DS:SI
        //if (verbose) {
            //Serial.println("Sending packet of %u bytes.", regs.wordregs[regcx]);
          //}
        //sendpkt (&RAM[ ( (uint32_t) segregs[regds] << 4) + (uint32_t) regs.wordregs[regsi]], regs.wordregs[regcx]);
        i = ( (uint32_t) segregs[regds] << 4) + (uint32_t) regs.wordregs[regsi];
        chksum = 0;
        Serial.write(0xFF); //reset stream
        Serial.write(0x00); //command type
        for (j=0; j<regs.wordregs[regcx]; j++) {
           cc = read86(i++);
           outByte(cc);
           chksum += cc;
        }
        outByte(chksum);
        Serial.write(0xFE); //stream ecsape
        Serial.write(0x02); //end of command
        return;
      case 0x02: //return packet info (packet buffer in DS:SI, length in CX)
        segregs[regds] = 0xD000;
        regs.wordregs[regsi] = 0x0000;
        regs.wordregs[regcx] = net.pktlen;
        return;
      case 0x03: //copy packet to final destination (given in ES:DI)
        //memcpy (&RAM[ ( (uint32_t) segregs[reges] << 4) + (uint32_t) regs.wordregs[regdi]], &RAM[0xD0000], net.pktlen);
        i = ( (uint32_t) segregs[reges] << 4) + (uint32_t) regs.wordregs[regdi];
        for (j=0; j<net.pktlen; j++) {
          write86(i++, bufSerial[j+1]);
        }
        net.canrecv = 1;
        net.pktlen = 0;
        //sendCanRecv();
        return;
      case 0x04: //disable packets
        net.enabled = 0;
        net.canrecv = 0;
        return;
      case 0x05: //DEBUG: dump packet (DS:SI) of CX bytes to stdout
        /*for (i=0; i<regs.wordregs[regcx]; i++) {
            printf ("%c", RAM[ ( (uint32_t) segregs[regds] << 4) + (uint32_t) regs.wordregs[regsi] + i]);
          }*/
        return;
      case 0x06: //DEBUG: print milestone string
        //print("PACKET DRIVER MILESTONE REACHED\n");
        return;
    }
}

uint8_t net_read_ram(uint32_t addr32) {
  if (addr32 < 1514) return bufSerial[addr32 + 1];
  return 0;
}

/*void net_write_ram(uint32_t addr32, uint8_t value) {
  if (addr32 < 1514) ENC28J60::buffer[addr32] = value;
}*/

void net_loop() {
  uint16_t i, len;
  uint8_t cc;
  while (Serial.available()) {
    bufSerIn(Serial.read());
  }
}

void net_init() {
}
#endif

