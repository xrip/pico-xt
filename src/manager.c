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
static volatile bool upPressed = false;
static volatile bool downPressed = false;

bool already_swapped_fdds = false;
volatile bool manager_started = false;

static char line[81];
static char pathA[256] = { "\\XT" };
static char pathB[256] = { "\\XT" };
static volatile bool left_panel_make_active = true;
static bool left_panel_is_selected = true;
static int left_panel_selected_file = 1;
static int right_panel_selected_file = 1;
static volatile uint32_t lastScanCode = 0;
static uint32_t prevProcessedScanCode = 0;

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
    sprintf(line, " SD:%s ", pathA);
    draw_text(line, 16, 0, 7, 1);

    sprintf(line, " SD:%s ", pathB);
    draw_text(line, 57, 0, 7, 1);

    memset(line, ' ', 80);
    line[0]  = 0xBA;
    line[39] = 0xBA;
    line[40] = 0xBA;
    line[79] = 0xBA;
    for (int y = 1; y < 27; ++y) {
        draw_text(line, 0, y, 7, 1);
    }

    for(int i = 1; i < 79; ++i) {
        line[i] = 0xCD; // ═
    }
    line[0]  = 0xC8; // ╚
    line[39] = 0xBC; // ╝
    line[40] = 0xC8; // ╚
    line[79] = 0xBC; // ╝
    draw_text(line, 0, 27, 7, 1);
}

void bottom_line() {
    sprintf(line, "1       2       3       4       5       6       7       8       9       10      ");
    draw_text(line, 0, 29, 7, 0);
    
    memset(line, ' ', 80); line[0] = '>';
    draw_text(line, 0, 28, 7, 0); // status line ?

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
    FIL file;
    if (f_stat(pathA, &file) != FR_OK/* || !(file.fattrib & AM_DIR)*/) { // TODO:
        // TODO: Error dialog
        return;
    }
    DIR dir;
    if (f_opendir(&dir, pathA) != FR_OK) {
        // TODO: Error dialog
        return;
    }
    FILINFO fileInfo;
    int y = 1;
    const int x = 1;
    if (strlen(pathA) > 1) {
        sprintf(line, "..");
        for(int l = strlen(line); l < 38; ++l) {
            line[l] = ' ';
        }
        line[38] = 0;
        int bgc = left_panel_is_selected && left_panel_selected_file == y ? 11 : 1;
        draw_text(line, x, y++, 7, bgc);
    }

    while(f_readdir(&dir, &fileInfo) == FR_OK && fileInfo.fname[0] != '\0' && y < 28) {
        sprintf(line, fileInfo.fname);
        for(int l = strlen(line); l < 38; ++l) {
           line[l] = ' ';
        }
        line[38] = 0;
        int bgc = left_panel_is_selected && left_panel_selected_file == y ? 11 : 1;
        draw_text(line, x, y++, 7, bgc);
    }
    f_closedir(&dir);
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
    if (strlen(pathB) > 1) {
        sprintf(line, "..");
        for(int l = strlen(line); l < 38; ++l) {
            line[l] = ' ';
        }
        line[38] = 0;
        int bgc = !left_panel_is_selected && right_panel_selected_file == y ? 11 : 1;
        draw_text(line, x, y++, 7, bgc);
    }

    while(f_readdir(&dir, &fileInfo) == FR_OK && fileInfo.fname[0] != '\0' && y < 28) {
        sprintf(line, fileInfo.fname);
        for(int l = strlen(line); l < 38; ++l) {
           line[l] = ' ';
        }
        line[38] = 0;
        int bgc = !left_panel_is_selected && right_panel_selected_file == y ? 11 : 1;
        draw_text(line, x, y++, 7, bgc);
    }
    f_closedir(&dir);
}

static void select_right_panel() {
    left_panel_is_selected = false;
    fill_right();
    fill_left();
}

static void select_left_panel() {
    left_panel_is_selected = true;
    fill_right();
    fill_left();
}

static void work_cycle() {
    while(1) {
        bottom_line();
        if (left_panel_is_selected && !left_panel_make_active) {
            select_right_panel();
        }
        if (!left_panel_is_selected && left_panel_make_active) {
            select_left_panel();
        }
        switch(lastScanCode) {
          case 0xD0: // down
            if (prevProcessedScanCode != 0x50) {
                break;
            }
            if (left_panel_is_selected) {
                left_panel_selected_file++;
                fill_left();
            }
            else {
                right_panel_selected_file++;
                fill_right();
            }
            break;
          case 0xC8: // up
            if (prevProcessedScanCode != 0x48) {
                break;
            }
            if (left_panel_is_selected) {
                left_panel_selected_file--;
                fill_left();
            }
            else {
                right_panel_selected_file--;
                fill_right();
            }
            break;
          case 0xCB: // left
            break;
          case 0xCD: // right
            break;
        }
        prevProcessedScanCode = lastScanCode;
        lastScanCode = 0;
        sleep_ms(33);
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
    lastScanCode = ps2scancode;
    switch (ps2scancode) {
      case 0x48:
        upPressed = true;
        break;
      case 0xC8:
        upPressed = false;
        break;
      case 0x50:
        downPressed = true;
        break;
      case 0xD0:
        downPressed = false;
        break;
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
        if (manager_started) left_panel_make_active = !left_panel_make_active; // TODO: combinations?
        tabPressed = false;
        break;
      default: {
        char tmp[40];
        sprintf(tmp, "Scan code: %02X", ps2scancode);
        draw_text(tmp, 0, 0, 0, 3);
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
