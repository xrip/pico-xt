#include "manager.h"
#include "emulator.h"
#include "vga.h"

static volatile bool backspacePressed = false;
static volatile bool enterPressed = false;
static volatile bool plusPressed = false;
static volatile bool minusPressed = false;
static volatile bool ctrlPressed = false;
static volatile bool altPressed = false;
static volatile bool tabPressed = false;

volatile bool manager_started = false;

static char line[81];
static char pathA[256] = { "C:\\" };
static char pathB[256] = { "\\XT" };

static void draw_window() {
    line[80] = 0;
    for(int i = 1; i < 79; ++i) {
        line[i] = 0xCD; // ═
    }
    line[0]  = 0xC9; // ╔
    line[39] = 0xBB; // ╗
    line[40] = 0xC9; // ╔
    line[79] = 0xBB; // ╗
    draw_text(line, 0, 0, 7, 1);
    // TODO: center, actual drive/path
    sprintf(line, " C:\\ ");
    draw_text(line, 16, 0, 7, 1);

    sprintf(line, " SD:\\XT ");
    draw_text(line, 57, 0, 7, 1);

    memset(line, ' ', 80);
    line[0]  = 0xBA;
    line[39] = 0xBA;
    line[40] = 0xBA;
    line[79] = 0xBA;
    for (int y = 1; y < 28; ++y) {
        draw_text(line, 0, y, 7, 1);
    }

    for(int i = 1; i < 79; ++i) {
        line[i] = 0xCD; // ═
    }
    line[0]  = 0xC8; // ╚
    line[39] = 0xBC; // ╝
    line[40] = 0xC8; // ╚
    line[79] = 0xBC; // ╝
    draw_text(line, 0, 28, 7, 1);
}

void bottom_line() {
    sprintf(line, "1       2       3       4       5       6       7       8       9       10      ");
    draw_text(line, 0, 29, 7, 0);

    if (altPressed) {
        sprintf(line, " Left "); draw_text(line,  1, 29, 0, 3);
        sprintf(line, "Right "); draw_text(line,  9, 29, 0, 3);
        sprintf(line, " View "); draw_text(line, 17, 29, 0, 3);
        sprintf(line, " Edit "); draw_text(line, 25, 29, 0, 3);
        sprintf(line, " Copy "); draw_text(line, 33, 29, 0, 3);
        sprintf(line, " Move "); draw_text(line, 41, 29, 0, 3);
        sprintf(line, " Find "); draw_text(line, 49, 29, 0, 3);
        sprintf(line, " Del  "); draw_text(line, 57, 29, 0, 3);
        sprintf(line, " Swap "); draw_text(line, 65, 29, 0, 3);
        sprintf(line, " USB ");  draw_text(line, 74, 29, 0, 3);
    } else if (ctrlPressed) {
        sprintf(line, "Eject "); draw_text(line,  1, 29, 0, 3);
        sprintf(line, " Menu "); draw_text(line,  9, 29, 0, 3);
        sprintf(line, " View "); draw_text(line, 17, 29, 0, 3);
        sprintf(line, " Edit "); draw_text(line, 25, 29, 0, 3);
        sprintf(line, " Copy "); draw_text(line, 33, 29, 0, 3);
        sprintf(line, " Move "); draw_text(line, 41, 29, 0, 3);
        sprintf(line, " Dir  "); draw_text(line, 49, 29, 0, 3);
        sprintf(line, " Del  "); draw_text(line, 57, 29, 0, 3);
        sprintf(line, " UpMn "); draw_text(line, 65, 29, 0, 3);
        sprintf(line, " USB ");  draw_text(line, 74, 29, 0, 3);
    } else {
        sprintf(line, " Help "); draw_text(line,  1, 29, 0, 3);
        sprintf(line, " Menu "); draw_text(line,  9, 29, 0, 3);
        sprintf(line, " View "); draw_text(line, 17, 29, 0, 3);
        sprintf(line, " Edit "); draw_text(line, 25, 29, 0, 3);
        sprintf(line, " Copy "); draw_text(line, 33, 29, 0, 3);
        sprintf(line, " Move "); draw_text(line, 41, 29, 0, 3);
        sprintf(line, " Dir  "); draw_text(line, 49, 29, 0, 3);
        sprintf(line, " Del  "); draw_text(line, 57, 29, 0, 3);
        sprintf(line, " UpMn "); draw_text(line, 65, 29, 0, 3);
        sprintf(line, " USB ");  draw_text(line, 74, 29, 0, 3);
    }
}

void fill_left() {

}

void fill_right() {
    FIL file;
    if (f_stat(pathB, &file) != FR_OK/* || !(file.fattrib & AM_DIR)*/) { // TODO:
        // TODO: Error dialog
        return;
    }
    DIR dir;
    if (f_opendir(&dir, pathB) != FR_OK) {
        // TODO: Error dialog
        return;
    }
    FILINFO fileInfo;
    int y = 1;
    const int x = 41;
    sprintf(line, ".."); draw_text(line, x, y++, 7, 1);
    while(f_readdir(&dir, &fileInfo) == FR_OK && fileInfo.fname[0] != '\0' && y < 28) {
       sprintf(line, fileInfo.fname); draw_text(line, x, y++, 7, 1);
    }
    f_closedir(&dir);
}

void work_cycle() {
    while(1) {
        bottom_line();
        sleep_ms(200);
    }
    
    //in_flash_drive();
}

void start_manager() {
    save_video_ram();
    enum graphics_mode_t ret = graphics_set_mode(TEXTMODE_80x30);
    set_start_debug_line(30);
    draw_window();
    fill_left();
    fill_right();

    work_cycle();
    
    set_start_debug_line(25);
    restore_video_ram();
    if (ret == TEXTMODE_80x30) {
        clrScr(1);
    }
    graphics_set_mode(ret);
}

bool handleScancode(uint32_t ps2scancode) { // core 1
    switch (ps2scancode) {
      case 0x38:
        altPressed = true;
        break;
      case 0xB8:
        altPressed = false;
        break;
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
      default: {
     //   char tmp[40];
      //  sprintf(tmp, "Scan code: %02X", ps2scancode);
      //  draw_text(tmp, 0, 0, 0, 3);
      }
    }
    return manager_started;
}

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
