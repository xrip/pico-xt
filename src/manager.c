#include "manager.h"
#include "emulator.h"
#include "vga.h"
#include "usb.h"
#include "pico-vision.h"

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
volatile bool usb_started = false;

inline static void swap_drive_message() {
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

inline static void swap_drives(uint8_t cmd) {
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

void if_swap_drives() {
    if (manager_started) {
        return;
    }
    swap_drives(7);
}

static char line[81];
static char pathA[256] = { "\\XT" };
static char pathB[256] = { "\\XT" };
static volatile bool left_panel_make_active = true;
static bool left_panel_is_selected = true;

static int left_panel_selected_file_idx = 1;
static int right_panel_selected_file_idx = 1;
static volatile uint16_t left_start_file_offset = 0;
static volatile uint16_t right_start_file_offset = 0;
static volatile uint16_t left_files_number = 0;
static volatile uint16_t right_files_number = 0;

static volatile uint32_t lastCleanableScanCode = 0;
static uint32_t lastSavedScanCode = 0;

static const uint8_t PANEL_TOP_Y = 0;
static const uint8_t TOTAL_SCREEN_LINES = 30;
static const uint8_t F_BTN_Y_POS = TOTAL_SCREEN_LINES - 1;
static const uint8_t CMD_Y_POS = F_BTN_Y_POS - 1;
static const uint8_t PANEL_LAST_Y = CMD_Y_POS - 1;

static uint8_t FIRST_FILE_LINE_ON_PANEL_Y = PANEL_TOP_Y + 1;
static uint8_t LAST_FILE_LINE_ON_PANEL_Y = PANEL_LAST_Y - 1;

static void draw_window() {
    sprintf(line, "SD:%s", pathA);
    draw_panel( 0, PANEL_TOP_Y, 40, PANEL_LAST_Y + 1, line, 0);
    draw_panel(40, PANEL_TOP_Y, 40, PANEL_LAST_Y + 1, line, 0);
}

void do_nothing(uint8_t cmd) {
}

static bool mark_to_exit_flag = false;
void mark_to_exit(uint8_t cmd) {
    mark_to_exit_flag = true;
}

void turn_usb_on(uint8_t cmd) {
    init_pico_usb_drive();
    usb_started = true;
}

static fn_1_10_tbl_t fn_1_10_tbl = {
    ' ', '1', " Help ", do_nothing,
    ' ', '2', " Menu ", do_nothing,
    ' ', '3', " View ", do_nothing,
    ' ', '4', " Edit ", do_nothing,
    ' ', '5', " Copy ", do_nothing,
    ' ', '6', " Move ", do_nothing,
    ' ', '7', "MkDir ", do_nothing,
    ' ', '8', " Del  ", do_nothing,
    ' ', '9', " Swap ", swap_drives,
    ' ', '0', " USB  ", turn_usb_on
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
    ' ', '0', " USB  ", turn_usb_on
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
    ' ', '9', " Swap ", swap_drives,
    ' ', '0', " Exit ", mark_to_exit
};

static inline fn_1_10_tbl_t* actual_fn_1_10_tbl() {
    const fn_1_10_tbl_t * ptbl = &fn_1_10_tbl;
    if (altPressed) {
        ptbl = &fn_1_10_tbl_alt;
    } else if (ctrlPressed) {
        ptbl = &fn_1_10_tbl_ctrl;
    }
    return ptbl;
}

static inline void bottom_line() {
    for (int i = 0; i < BTNS_COUNT; ++i) {
        const fn_1_10_tbl_rec_t* rec = &(*actual_fn_1_10_tbl())[i];
        draw_fn_btn(rec, i * BTN_WIDTH, F_BTN_Y_POS);
    }
    draw_cmd_line(0, CMD_Y_POS, 0);
}

static inline void fill_left() {
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
    left_files_number = 0;
    if (strlen(pathA) > 1) {
        draw_label(1, y, 38, "..", left_panel_is_selected && left_panel_selected_file_idx == y);
        y++;
        left_files_number++;
    }
    while(f_readdir(&dir, &fileInfo) == FR_OK && fileInfo.fname[0] != '\0' && y <= LAST_FILE_LINE_ON_PANEL_Y) {
        draw_label(1, y, 38, fileInfo.fname, left_panel_is_selected && left_panel_selected_file_idx == y);
        y++;
        left_files_number++;
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
    right_files_number = 0;
    if (strlen(pathA) > 1) {
        draw_label(41, y, 38, "..", !left_panel_is_selected && right_panel_selected_file_idx == y);
        y++;
        right_files_number++;
    }
    while(f_readdir(&dir, &fileInfo) == FR_OK && fileInfo.fname[0] != '\0' && y <= LAST_FILE_LINE_ON_PANEL_Y) {
        draw_label(41, y, 38, fileInfo.fname, !left_panel_is_selected && right_panel_selected_file_idx == y);
        y++;
        right_files_number++;
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

inline static fn_1_10_btn_pressed(uint8_t fn_idx) {
    (*actual_fn_1_10_tbl())[fn_idx].action(fn_idx);
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
          case 0x01: // Esc down
          case 0x81: // Esc up
            break;
          case 0x3B: // F1..10 down
          case 0x3C: // F2
          case 0x3D: // F3
          case 0x3E: // F4
          case 0x3F: // F5
          case 0x40: // F6
          case 0x41: // F7
          case 0x42: // F8
          case 0x43: // F9
          case 0x44: // F10
            scan_code_processed();
            break;
          case 0xBB: // F1..10 up
          case 0xBC: // F2
          case 0xBD: // F3
          case 0xBE: // F4
          case 0xBF: // F5
          case 0xC0: // F6
          case 0xC1: // F7
          case 0xC2: // F8
          case 0xC3: // F9
          case 0xC4: // F10
            if (lastSavedScanCode != lastCleanableScanCode - 0x80) {
                break;
            }
            fn_1_10_btn_pressed(lastCleanableScanCode - 0xBB);
            scan_code_processed();
            break;
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
                left_panel_selected_file_idx < LAST_FILE_LINE_ON_PANEL_Y &&
                left_panel_selected_file_idx < left_files_number
              ) {
                left_panel_selected_file_idx++;
                fill_left();
            }
            else if(
                right_panel_selected_file_idx < LAST_FILE_LINE_ON_PANEL_Y &&
                right_panel_selected_file_idx < right_files_number
            ) {
                right_panel_selected_file_idx++;
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
                left_panel_selected_file_idx > FIRST_FILE_LINE_ON_PANEL_Y
            ) {
                left_panel_selected_file_idx--;
                fill_left();
            }
            else if(
                right_panel_selected_file_idx > FIRST_FILE_LINE_ON_PANEL_Y
            ) {
                right_panel_selected_file_idx--;
                fill_right();
            }
            scan_code_processed();
            break;
          case 0xCB: // left
            break;
          case 0xCD: // right
            break;
        }
        if (usb_started && tud_msc_ejected()) {
            usb_started = false;
        }
        if (usb_started) {
            pico_usb_drive_heartbeat();
        } else if(mark_to_exit_flag) {
            return;
        }
    }
}

void start_manager() {
    mark_to_exit_flag = false;
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

void if_manager() {
    if (manager_started) {
        return;
    }
    if (backspacePressed && enterPressed) {
        manager_started = true;
        start_manager();
        manager_started = false;
    }
}
