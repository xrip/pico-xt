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
static volatile uint32_t lastCleanableScanCode = 0;
static uint32_t lastSavedScanCode = 0;

static uint8_t BACKGROUND_FIELD_COLOR = 1; // Blue
static uint8_t FOREGROUND_FIELD_COLOR = 7; // White

static uint8_t BACKGROUND_F1_10_COLOR = 0; // Black
static uint8_t FOREGROUND_F1_10_COLOR = 7; // White

static uint8_t BACKGROUND_F_BTN_COLOR = 3; // Green
static uint8_t FOREGROUND_F_BTN_COLOR = 0; // Black

static uint8_t BACKGROUND_CMD_COLOR = 0; // Black
static uint8_t FOREGROUND_CMD_COLOR = 7; // White

static const uint8_t PANEL_TOP_Y = 0;
static const uint8_t TOTAL_SCREEN_LINES = 30;
static const uint8_t F_BTN_Y_POS = TOTAL_SCREEN_LINES - 1;
static const uint8_t CMD_Y_POS = F_BTN_Y_POS - 1;
static const uint8_t PANEL_LAST_Y = CMD_Y_POS - 1;

static uint8_t FIRST_FILE_LINE_ON_PANEL_Y = PANEL_TOP_Y + 1;
static uint8_t LAST_FILE_LINE_ON_PANEL_Y = PANEL_LAST_Y - 1;


static void draw_window() {
    line[80] = 0;
    for(int i = 1; i < 79; ++i) {
        line[i] = 0xCD; // ═
    }
    line[0]  = 0xC9; // ╔
    line[39] = 0xBB; // ╗
    line[40] = 0xC9; // ╔
    line[79] = 0xBB; // ╗
    draw_text(line, 0, PANEL_TOP_Y, FOREGROUND_FIELD_COLOR, BACKGROUND_FIELD_COLOR);
    // TODO: center, actual drive/path
    sprintf(line, " SD:%s ", pathA);
    draw_text(line, 16, PANEL_TOP_Y, FOREGROUND_FIELD_COLOR, BACKGROUND_FIELD_COLOR);

    sprintf(line, " SD:%s ", pathB);
    draw_text(line, 57, PANEL_TOP_Y, FOREGROUND_FIELD_COLOR, BACKGROUND_FIELD_COLOR);

    memset(line, ' ', 80);
    line[0]  = 0xBA;
    line[39] = 0xBA;
    line[40] = 0xBA;
    line[79] = 0xBA;
    for (int y = PANEL_TOP_Y + 1; y < PANEL_LAST_Y; ++y) {
        draw_text(line, 0, y, FOREGROUND_FIELD_COLOR, BACKGROUND_FIELD_COLOR);
    }

    for(int i = 1; i < 79; ++i) {
        line[i] = 0xCD; // ═
    }
    line[0]  = 0xC8; // ╚
    line[39] = 0xBC; // ╝
    line[40] = 0xC8; // ╚
    line[79] = 0xBC; // ╝
    draw_text(line, 0, PANEL_LAST_Y, FOREGROUND_FIELD_COLOR, BACKGROUND_FIELD_COLOR);
}

#define BTN_WIDTH 8
typedef struct fn_1_10_tbl_rec {
    char pre_mark;
    char mark;
    char name[BTN_WIDTH];
    fn_1_10_ptr action;
} fn_1_10_tbl_rec_t;

void do_nothing(uint8_t cmd) {

}

#define BTNS_COUNT 10
typedef fn_1_10_tbl_rec_t fn_1_10_tbl_t[BTNS_COUNT];
static fn_1_10_tbl_t fn_1_10_tbl = {
    ' ', '1', " Help ", do_nothing,
    ' ', '2', " Menu ", do_nothing,
    ' ', '3', " View ", do_nothing,
    ' ', '4', " Edit ", do_nothing,
    ' ', '5', " Copy ", do_nothing,
    ' ', '6', " Move ", do_nothing,
    ' ', '7', "MkDir ", do_nothing,
    ' ', '8', " Del  ", do_nothing,
    ' ', '9', " Swap ", do_nothing,
    ' ', '0', " USB  ", do_nothing
};

static fn_1_10_tbl_t fn_1_10_tbl_alt = {
    ' ', '1', "Right ", do_nothing,
    ' ', '2', " Left ", do_nothing,
    ' ', '3', " View ", do_nothing,
    ' ', '4', " Edit ", do_nothing,
    ' ', '5', " Copy ", do_nothing,
    ' ', '6', " Move ", do_nothing,
    ' ', '7', " Find ", do_nothing,
    ' ', '8', " Del  ", do_nothing,
    ' ', '9', " UpMn ", do_nothing,
    ' ', '0', " USB  ", do_nothing
};

static fn_1_10_tbl_t fn_1_10_tbl_ctrl = {
    ' ', '1', " EjtL ", do_nothing,
    ' ', '2', " EjtR ", do_nothing,
    ' ', '3', "Debug ", do_nothing,
    ' ', '4', " Edit ", do_nothing,
    ' ', '5', " Copy ", do_nothing,
    ' ', '6', " Move ", do_nothing,
    ' ', '7', " Find ", do_nothing,
    ' ', '8', " Del  ", do_nothing,
    ' ', '9', " Swap ", do_nothing,
    ' ', '0', " USB  ", do_nothing
};

void bottom_line() {
    const fn_1_10_tbl_t * ptbl = &fn_1_10_tbl;
    if (altPressed) {
        ptbl = &fn_1_10_tbl_alt;
    } else if (ctrlPressed) {
        ptbl = &fn_1_10_tbl_ctrl;
    }
    sprintf(line, "1      "); 
    for (int i = 0; i < BTNS_COUNT; ++i) {
        const fn_1_10_tbl_rec_t* rec = &(*ptbl)[i];
        // 1, 2, 3... button mark
        line[0] = rec->pre_mark;
        line[1] = rec->mark;
        draw_text(line, i * BTN_WIDTH, F_BTN_Y_POS, FOREGROUND_F1_10_COLOR, BACKGROUND_F1_10_COLOR);
        // button
        sprintf(line, rec->name);
        draw_text(line, i * BTN_WIDTH + 2, F_BTN_Y_POS, FOREGROUND_F_BTN_COLOR, BACKGROUND_F_BTN_COLOR);
    }
    
    memset(line, ' ', 80); line[0] = '>'; line[80] = 0;
    draw_text(line, 0, CMD_Y_POS, FOREGROUND_CMD_COLOR, BACKGROUND_CMD_COLOR); // status/command line
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
        int bgc = left_panel_is_selected && left_panel_selected_file == y ? 11 : BACKGROUND_FIELD_COLOR;
        draw_text(line, x, y++, FOREGROUND_FIELD_COLOR, bgc);
    }

    while(f_readdir(&dir, &fileInfo) == FR_OK && fileInfo.fname[0] != '\0' && y <= LAST_FILE_LINE_ON_PANEL_Y) {
        sprintf(line, fileInfo.fname);
        for(int l = strlen(line); l < 38; ++l) {
           line[l] = ' ';
        }
        line[38] = 0;
        int bgc = left_panel_is_selected && left_panel_selected_file == y ? 11 : BACKGROUND_FIELD_COLOR;
        draw_text(line, x, y++, FOREGROUND_FIELD_COLOR, bgc);
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
        int bgc = !left_panel_is_selected && right_panel_selected_file == y ? 11 : BACKGROUND_FIELD_COLOR;
        draw_text(line, x, y++, FOREGROUND_FIELD_COLOR, bgc);
    }

    while(f_readdir(&dir, &fileInfo) == FR_OK && fileInfo.fname[0] != '\0' && y <= LAST_FILE_LINE_ON_PANEL_Y) {
        sprintf(line, fileInfo.fname);
        for(int l = strlen(line); l < 38; ++l) {
           line[l] = ' ';
        }
        line[38] = 0;
        int bgc = !left_panel_is_selected && right_panel_selected_file == y ? 11 : BACKGROUND_FIELD_COLOR;
        draw_text(line, x, y++, FOREGROUND_FIELD_COLOR, bgc);
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

inline static void scan_code_processed() {
  if (lastCleanableScanCode)
    lastSavedScanCode = lastCleanableScanCode;
  lastCleanableScanCode = 0;
}

static void work_cycle() {
    while(1) {
        if (left_panel_is_selected && !left_panel_make_active) {
            select_right_panel();
        }
        if (!left_panel_is_selected && left_panel_make_active) {
            select_left_panel();
        }
        switch(lastCleanableScanCode) {
          case 0x1D: // Ctrl down
          case 0x9D: // Ctrl up
          case 0x38: // ALT down
          case 0xB8: // ALT up
            bottom_line();
            scan_code_processed();
            break;
          case 0x50: // down arr down
            scan_code_processed();
            break;
          case 0xD0: // down arr up
            if (lastSavedScanCode != 0x50) {
                break;
            }
            if (left_panel_is_selected &&
                left_panel_selected_file < LAST_FILE_LINE_ON_PANEL_Y
              ) {
                left_panel_selected_file++;
                fill_left();
            }
            else if(
                right_panel_selected_file < LAST_FILE_LINE_ON_PANEL_Y
            ) {
                right_panel_selected_file++;
                fill_right();
            }
            scan_code_processed();
            break;
          case 0x48: // up arr down
            scan_code_processed();
            break;
          case 0xC8: // up arr up
            if (lastSavedScanCode != 0x48) {
                break;
            }
            if (left_panel_is_selected &&
                left_panel_selected_file > FIRST_FILE_LINE_ON_PANEL_Y
            ) {
                left_panel_selected_file--;
                fill_left();
            }
            else if(
                right_panel_selected_file > FIRST_FILE_LINE_ON_PANEL_Y
            ) {
                right_panel_selected_file--;
                fill_right();
            }
            scan_code_processed();
            break;
          case 0xCB: // left
            break;
          case 0xCD: // right
            break;
        }
    //    sleep_ms(33);
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
    bottom_line();

    work_cycle();
    
    set_start_debug_line(25);
    restore_video_ram();
    if (ret == TEXTMODE_80x30) {
        clrScr(1);
    }
    graphics_set_mode(ret);
}

bool handleScancode(uint32_t ps2scancode) { // core 1
    lastCleanableScanCode = ps2scancode;
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
        if (manager_started)
            left_panel_make_active = !left_panel_make_active; // TODO: combinations?
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
