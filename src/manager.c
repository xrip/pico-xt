#include "manager.h"
#include "vga.h"

static void draw_window() {
    char line[80];
    for(int i = 1; i < 78; ++i) {
        line[i] = 0xCD; // ═
    }
    line[0]  = 0xC9; // ╔
    line[40] = 0xC9; // ╔
    line[39] = 0xBB; // ╗
    line[79] = 0xBB; // ╗
    draw_text(line, 0, 0, 7, 1);
    
    line[0]  = 0xC8; // ╚
    line[39] = 0xBC; // ╝
    line[40] = 0xC8; // ╚
    line[79] = 0xBC; // ╝
    draw_text(line, 0, 28, 7, 1);

    memset(line, ' ', 80);
    line[0]  = 0xBA;
    line[39] = 0xBA;
    line[40] = 0xBA;
    line[79] = 0xBA;
    for (int y = 0; y < 28; ++y) {
        draw_text(line, 0, y, 7, 1);
    }

    // TODO:
}

void work_cycle() {

    in_flash_drive();
}

void start_manager() {
    save_video_ram();
    enum graphics_mode_t ret = graphics_set_mode(TEXTMODE_80x30);
    draw_window();

    work_cycle();

    restore_video_ram();
    if (ret == TEXTMODE_80x30) {
        clrScr(1);
    }
    graphics_set_mode(ret);
}


static volatile bool backspacePressed = false;
static volatile bool enterPressed = false;
static volatile bool plusPressed = false;
static volatile bool minusPressed = false;
static volatile bool ctrlPressed = false;
static volatile bool tabPressed = false;

void handleScancode(uint32_t ps2scancode) {
    switch (ps2scancode) {
      case 0x0E:
        backspacePressed = true;
        break;
      case 0x8E:
        backspacePressed = false;
        break;
      case 0x1C:
        enterPressed = true;
        break;
      case 0x9C:
        enterPressed = false;
        break;
      case 0x4A:
        minusPressed = true;
        break;
      case 0xCA:
        minusPressed = false;
        break;
      case 0x4E:
        plusPressed = true;
        break;
      case 0xCE:
        plusPressed = false;
        break;
      case 0x1D:
        ctrlPressed = true;
        break;
      case 0x9D:
        ctrlPressed = false;
        break;
      case 0x0F:
        tabPressed = true;
        break;
      case 0x8F:
        tabPressed = false;
        break;
    }
}

#include "emulator.h"
volatile bool manager_started = false;

#include "vga.h"

int overclock() {
  if (tabPressed && ctrlPressed) {
    if (plusPressed) return 1;
    if (minusPressed) return -1;
  }
  return 0;
}

void if_usb() {
    if (manager_started) {
        return;
    }
    if (backspacePressed && enterPressed) {
        manager_started = true;
        sleep_ms(200);
        start_manager();
        manager_started = false;
    }
}

static bool already_swapped_fdds = false;
static void swap_drive_message() {
    save_video_ram();
    enum graphics_mode_t ret = graphics_set_mode(TEXTMODE_80x30);
    set_start_debug_line(0);
    clrScr(1);
    for(int i = 0; i < 10; i++) {
        logMsg("");
    }
    if (already_swapped_fdds) {
        logMsg("                      Swap FDD0 and FDD1 drive images"); logMsg("");
        logMsg("          To return images back, press Ctrl + Tab + Backspace");
    } else {
        logMsg("                    Swap FDD0 and FDD1 drive images back");
    }
    sleep_ms(3000);
    set_start_debug_line(25);
    restore_video_ram();
    if (ret == TEXTMODE_80x30) {
        clrScr(1);
    }
    graphics_set_mode(ret);
}
void if_swap_drives() { // TODO: move to the manager
    if (manager_started) {
        return;
    }
    if (backspacePressed && tabPressed && ctrlPressed) {
        if (already_swapped_fdds) {
            insertdisk(0, fdd0_sz(), fdd0_rom(), "\\XT\\fdd0.img");
            insertdisk(1, fdd1_sz(), fdd1_rom(), "\\XT\\fdd1.img");
            already_swapped_fdds = false;
            swap_drive_message();
            return;
        }
        insertdisk(1, fdd0_sz(), fdd0_rom(), "\\XT\\fdd0.img");
        insertdisk(0, fdd1_sz(), fdd1_rom(), "\\XT\\fdd1.img");
        already_swapped_fdds = true;
        swap_drive_message();
    }
}
