#if PICO_ON_DEVICE
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
static volatile bool aPressed = false;
static volatile bool cPressed = false;
static volatile bool gPressed = false;
static volatile bool tPressed = false;
static volatile bool dPressed = false;
static volatile bool sPressed = false;

static volatile bool xPressed = false;
static volatile bool ePressed = false;
static volatile bool uPressed = false;
static volatile bool hPressed = false;

volatile bool is_adlib_on = false;
volatile bool is_covox_on = true;
volatile bool is_game_balaster_on = true;
volatile bool is_tandy3v_on = true;
volatile bool is_dss_on = true;
volatile bool is_sound_on = true;
volatile uint8_t snd_divider = 0;
volatile uint8_t cms_divider = 0;
volatile uint8_t dss_divider = 0;
volatile uint8_t adlib_divider = 0;
volatile uint8_t tandy3v_divider = 0;
volatile uint8_t covox_divider = 0;

volatile bool is_xms_on = false;
volatile bool is_ems_on = true;
volatile bool is_umb_on = true;
volatile bool is_hma_on = true;

bool already_swapped_fdds = false;
volatile bool manager_started = false;
volatile bool usb_started = false;
static char line[81];

static volatile uint32_t lastCleanableScanCode = 0;
static uint32_t lastSavedScanCode = 0;

static const uint8_t PANEL_TOP_Y = 0;
static const uint8_t TOTAL_SCREEN_LINES = 30;
static const uint8_t F_BTN_Y_POS = TOTAL_SCREEN_LINES - 1;
static const uint8_t CMD_Y_POS = F_BTN_Y_POS - 1;
static const uint8_t PANEL_LAST_Y = CMD_Y_POS - 1;

static uint8_t FIRST_FILE_LINE_ON_PANEL_Y = PANEL_TOP_Y + 1;
static uint8_t LAST_FILE_LINE_ON_PANEL_Y = PANEL_LAST_Y - 1;

typedef struct file_panel_desc {
    int left;
    int width;
    int selected_file_idx;
    int start_file_offset;
    int files_number;
    char path[256];
} file_panel_desc_t;

static file_panel_desc_t left_panel = {
    0, 40, 1, 0, 0,
    { "\\" },
};

static file_panel_desc_t right_panel = {
    40, 40, 1, 0, 0,
    { "\\XT" },
};

static volatile bool left_panel_make_active = true;
static file_panel_desc_t* psp = &left_panel;

static inline void fill_panel(file_panel_desc_t* p);

inline static void swap_drive_message() {
    save_video_ram();
    enum graphics_mode_t ret = graphics_set_mode(TEXTMODE_80x30);
    if (ret != TEXTMODE_80x30) clrScr(1);
    if (already_swapped_fdds) {
        const line_t lns[3] = {
            { -1, "Swap FDD0 and FDD1 drive images" },
            {  0, "" },
            { -1, "To return images back, press Ctrl + Tab + Backspace"}
        };
        const lines_t lines = { 3, 2, lns };
        draw_box(10, 7, 60, 10, "Info", &lines);
    } else {
        const line_t lns[1] = {
            { -1, "Swap FDD0 and FDD1 drive images back" }
        };
        const lines_t lines = { 1, 3, lns };
        draw_box(10, 7, 60, 10, "Info", &lines);
    }
    sleep_ms(2500);
    graphics_set_mode(ret);
    restore_video_ram();
}

inline static void level_state_message(uint8_t divider, char* sys_name) {
    save_video_ram();
    enum graphics_mode_t ret = graphics_set_mode(TEXTMODE_80x30);
    if (ret != TEXTMODE_80x30) clrScr(1);
    char ln[80];
    snprintf(ln, 80, "%s volume: %d (div: 1 << %d = %d)", sys_name, 16 - divider, divider, (1 << divider));
    const line_t lns[1] = {
        { -1, ln }
    };
    const lines_t lines = { 1, 3, lns };
    draw_box(5, 7, 70, 10, "Info", &lines);
    sleep_ms(2500);
    graphics_set_mode(ret);
    restore_video_ram();
}

inline static void swap_sound_state_message(volatile bool* p_state, char* sys_name, char switch_char) {
    save_video_ram();
    enum graphics_mode_t ret = graphics_set_mode(TEXTMODE_80x30);
    if (ret != TEXTMODE_80x30) clrScr(1);
    char ln[80];
    snprintf(ln, 80, "Turn %s %s", sys_name, *p_state ? "OFF" : "ON");
    if (*p_state) {
        char ln3[42];
        snprintf(ln3, 42, "To turn it ON back, press Ctrl + Tab + %c", switch_char);
        const line_t lns[3] = {
            { -1, ln },
            {  0, "" },
            { -1, ln3}
        };
        const lines_t lines = { 3, 2, lns };
        draw_box(10, 7, 60, 10, "Info", &lines);
    } else {
        const line_t lns[1] = {
            { -1, ln }
        };
        const lines_t lines = { 1, 3, lns };
        draw_box(10, 7, 60, 10, "Info", &lines);
    }
    *p_state = !*p_state;
    sleep_ms(2500);
    graphics_set_mode(ret);
    restore_video_ram();
}

typedef struct drive_state {
    char* path;
    char* lbl;
} drive_state_t;

static drive_state_t drives_states[3] = {
    0, "FDD0",
    0, "FDD1",
    0, "HDD0"
};

void notify_image_insert_action(uint8_t drivenum, char *pathname) {
    drives_states[drivenum].path = pathname;
}

static void swap_drives(uint8_t cmd) {
    sprintf(line, "F%d pressed - swap FDD images", cmd + 1);
    draw_cmd_line(0, CMD_Y_POS, line);
    if (already_swapped_fdds) {
        insertdisk(0, fdd0_sz(), fdd0_rom(), "\\XT\\fdd0.img");
        insertdisk(1, fdd1_sz(), fdd1_rom(), "\\XT\\fdd1.img");
    } else {
        insertdisk(1, fdd0_sz(), fdd0_rom(), "\\XT\\fdd0.img");
        insertdisk(0, fdd1_sz(), fdd1_rom(), "\\XT\\fdd1.img");
    }
    already_swapped_fdds = !already_swapped_fdds;
    swap_drive_message();
}

inline static void if_swap_drives() {
    if (backspacePressed && tabPressed && ctrlPressed) {
        swap_drives(8);
    }
}

static void draw_window() {
    sprintf(line, "SD:%s", left_panel.path);
    draw_panel( 0, PANEL_TOP_Y, 40, PANEL_LAST_Y + 1, line, 0);
    sprintf(line, "SD:%s", right_panel.path);
    draw_panel(40, PANEL_TOP_Y, 40, PANEL_LAST_Y + 1, line, 0);
}

void do_nothing(uint8_t cmd) {
    sprintf(line, "F%d pressed - not yet implemnted", cmd + 1);
    draw_cmd_line(0, CMD_Y_POS, line);
}

static bool mark_to_exit_flag = false;
void mark_to_exit(uint8_t cmd) {
    mark_to_exit_flag = true;
}

static void turn_usb_on(uint8_t cmd);

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

static inline void turn_usb_off(uint8_t cmd) { // TODO: support multiple enter for USB mount
    set_tud_msc_ejected(true);
    usb_started = false;
    // Alt + F10 no more actions
    memset(fn_1_10_tbl_alt[9].name, ' ', BTN_WIDTH);
    fn_1_10_tbl_alt[9].action = do_nothing;
    // Ctrl + F10 - Exit
    sprintf(fn_1_10_tbl_ctrl[9].name, " Exit ");
    fn_1_10_tbl_ctrl[9].action = mark_to_exit;

    fill_panel(&left_panel);
    fill_panel(&right_panel);
    bottom_line();
}

static void turn_usb_on(uint8_t cmd) {
    init_pico_usb_drive();
    usb_started = true;
    // do not USB after it was turned on
    memset(fn_1_10_tbl[9].name, ' ', BTN_WIDTH);
    fn_1_10_tbl[9].action = do_nothing;
    // do not Exit in usb mode
    memset(fn_1_10_tbl_ctrl[9].name, ' ', BTN_WIDTH);
    fn_1_10_tbl_ctrl[9].action = do_nothing;
    // Alt + F10 - force unmount usb
    sprintf(fn_1_10_tbl_alt[9].name, " UnUSB");
    fn_1_10_tbl_alt[9].action = turn_usb_off;

    bottom_line();
}

static inline void fill_panel(file_panel_desc_t* p) {
    DIR dir;
    if (f_opendir(&dir, p->path) != FR_OK) {
        const line_t lns[1] = {
            { -1, "It is not a folder!" }
        };
        const lines_t lines = { 1, 4, lns };
        draw_box(10, 7, 60, 10, "Warning", &lines);
        sleep_ms(1500);
        return;
    }
    FILINFO fileInfo;
    int y = 1;
    p->files_number = 0;
    if (p->start_file_offset == 0 && strlen(p->path) > 1) {
        draw_label(p->left + 1, y, p->width - 2, "..", p == psp && p->selected_file_idx == y);
        y++;
        p->files_number++;
    }
    while(f_readdir(&dir, &fileInfo) == FR_OK &&
          fileInfo.fname[0] != '\0'
    ) {
        if (p->start_file_offset <= p->files_number && y <= LAST_FILE_LINE_ON_PANEL_Y) {
            char* name = fileInfo.fname;
            snprintf(line, 80, "%s\\%s", p->path, fileInfo.fname);
            for (int i = 0; i < 3; ++i) {
                if (drives_states[i].path && strcmp(drives_states[i].path, line) == 0) {
                    snprintf(line, p->width, "%s", name);
                    for (int j = strlen(name); j < p->width - 6; ++j) {
                        line[j] = ' ';
                    }
                    snprintf(line + p->width - 6, 6, "%s", drives_states[i].lbl);
                    name = line;
                    break;
                }
            }
            draw_label(p->left + 1, y, p->width - 2, name, p == psp && p->selected_file_idx == y);
            y++;
        }
        p->files_number++;
    }
    f_closedir(&dir);
    for (; y <= LAST_FILE_LINE_ON_PANEL_Y; ++y) {
        draw_label(p->left + 1, y, p->width - 2, "", false);
    }
}

inline static void select_right_panel() {
    psp = &right_panel;
    fill_panel(&left_panel);
    fill_panel(&right_panel);
}

inline static void select_left_panel() {
    psp = &left_panel;
    fill_panel(&left_panel);
    fill_panel(&right_panel);
}

inline static void scan_code_processed() {
  if (lastCleanableScanCode) {
      lastSavedScanCode = lastCleanableScanCode;
  }
  lastCleanableScanCode = 0;
}

inline static fn_1_10_btn_pressed(uint8_t fn_idx) {
    sprintf(line, "F%d pressed", fn_idx + 1);
    draw_cmd_line(0, CMD_Y_POS, line);
    (*actual_fn_1_10_tbl())[fn_idx].action(fn_idx);
}

inline static void handle_down_pressed() {
    if (psp->selected_file_idx < LAST_FILE_LINE_ON_PANEL_Y &&
        psp->start_file_offset + psp->selected_file_idx < psp->files_number
    ) {
        psp->selected_file_idx++;
        fill_panel(psp);
    } else if (
        psp->selected_file_idx == LAST_FILE_LINE_ON_PANEL_Y &&
        psp->start_file_offset + psp->selected_file_idx < psp->files_number
    ) {
        psp->selected_file_idx -= 5;
        psp->start_file_offset += 5;
        fill_panel(psp);    
    }
    scan_code_processed();
}

inline static void handle_up_pressed() {
    if (psp->selected_file_idx > FIRST_FILE_LINE_ON_PANEL_Y) {
        psp->selected_file_idx--;
        fill_panel(psp);
    } else if (psp->selected_file_idx == FIRST_FILE_LINE_ON_PANEL_Y && psp->start_file_offset > 0) {
        psp->selected_file_idx += 5;
        psp->start_file_offset -= 5;
        fill_panel(psp);       
    }
    scan_code_processed();
}

static inline void redraw_current_panel() {
    psp->selected_file_idx = 1;
    psp->start_file_offset = 0;
    sprintf(line, "SD:%s", psp->path);
    draw_panel(psp->left, PANEL_TOP_Y, psp->width, PANEL_LAST_Y + 1, line, 0);
    fill_panel(psp);
    draw_cmd_line(0, CMD_Y_POS, line);
}

static inline void enter_pressed() {
    if (psp->selected_file_idx == 1 && psp->start_file_offset == 0 && strlen(psp->path) > 1) {
        int i = strlen(psp->path);
        while(--i > 0) {
            if (psp->path[i] == '\\') {
                psp->path[i] = 0;
                redraw_current_panel();
                return;
            }
        }
        psp->path[0] = '\\';
        psp->path[1] = 0;
        redraw_current_panel();
        return;
    }
    DIR dir;
    if (f_opendir(&dir, psp->path) != FR_OK) {
        const line_t lns[1] = {
            { -1, "It is not a folder!" }
        };
        const lines_t lines = { 1, 4, lns };
        draw_box(10, 7, 60, 10, "Warning", &lines);
        sleep_ms(1500);
        return;
    }
    FILINFO fileInfo;
    int y = 1;
    if (psp->start_file_offset == 0 && strlen(psp->path) > 1) {
        y++;
    }
    while(f_readdir(&dir, &fileInfo) == FR_OK && fileInfo.fname[0] != '\0') {
        if (psp->start_file_offset <= psp->files_number && y <= LAST_FILE_LINE_ON_PANEL_Y) {
            if (psp->selected_file_idx == y) {
                sprintf(line, "fn: %s afn: %s sz: %d attr: %03oo date: %04Xh time: %04Xh",
                              fileInfo.fname, fileInfo.altname, fileInfo.fsize, fileInfo.fattrib, fileInfo.fdate, fileInfo.ftime);
                draw_cmd_line(0, CMD_Y_POS, line);
                if (fileInfo.fattrib & AM_DIR) {
                    f_closedir(&dir);
                    if (strlen(psp->path) > 1) {
                        sprintf(line, "%s\\%s", psp->path, fileInfo.fname);
                    } else {
                        sprintf(line, "\\%s", fileInfo.fname);
                    }
                    if (f_opendir(&dir, line) != FR_OK) {
                        const line_t lns[1] = {
                            { -1, "It is not a folder!" }
                        };
                        const lines_t lines = { 1, 4, lns };
                        draw_box(10, 7, 60, 10, "Warning", &lines);
                        sleep_ms(1500);
                    } else {
                        f_closedir(&dir);
                        strcpy(psp->path, line);
                        redraw_current_panel();
                    }
                    return;
                }
            }
            y++;
        }
    }
    f_closedir(&dir);
}

const static char* adlib_name = "AdLib emulation";
const static char* covox_name = "COVOX on LPT2";
const static char* dss_name = "Digital Sound Source on LPT1";
const static char* tandy3v_name = "Tandy 3-voices music device";
const static char* ss_name = "Whole Sound System";
const static char* cms_name = "Game Blaster (Creative Music System)";

static inline void if_sound_control() { // core #0
    if (ctrlPressed && tabPressed) {
        if (aPressed) {
            swap_sound_state_message(&is_adlib_on, adlib_name, 'A');
        } else if (cPressed) {
            swap_sound_state_message(&is_covox_on, covox_name, 'C');
        } else if (dPressed) {
            swap_sound_state_message(&is_dss_on, dss_name, 'D');
        } else if (tPressed) {
            swap_sound_state_message(&is_tandy3v_on, tandy3v_name, 'T');
        } else if (gPressed) {
            swap_sound_state_message(&is_game_balaster_on, cms_name, 'G');
        } else if (sPressed) {
            swap_sound_state_message(&is_sound_on, ss_name, 'S');
        } else if (xPressed) {
            swap_sound_state_message(&is_xms_on, "Extended Memory Manager (XMS)", 'X');
        } else if (ePressed) {
            swap_sound_state_message(&is_ems_on, "Expanded Memory Manager (EMS)", 'E');
        } else if (uPressed) {
            swap_sound_state_message(&is_umb_on, "Upper Memory Blocks Manager (UMB)", 'U');
        } else if (hPressed) {
            swap_sound_state_message(&is_hma_on, "Hight Memory Address Manager (HMA)", 'H');
        }
    } else if (ctrlPressed && plusPressed) {
        if (aPressed) {
            adlib_divider -= adlib_divider == 0 ? 0 : 1;
            plusPressed = false;
            level_state_message(adlib_divider, adlib_name);
        } else if (sPressed) {
            snd_divider -= snd_divider == 0 ? 0 : 1;
            plusPressed = false;
            level_state_message(snd_divider, ss_name);
        } else if (tPressed) {
            tandy3v_divider -= tandy3v_divider == 0 ? 0 : 1;
            plusPressed = false;
            level_state_message(tandy3v_divider, tandy3v_name);
        } else if (cPressed) {
            covox_divider -= covox_divider == 0 ? 0 : 1;
            plusPressed = false;
            level_state_message(covox_divider, covox_name);
        } else if (dPressed) {
            dss_divider -= dss_divider == 0 ? 0 : 1;
            plusPressed = false;
            level_state_message(dss_divider, dss_name);
        } else if (gPressed) {
            cms_divider -= cms_divider == 0 ? 0 : 1;
            plusPressed = false;
            level_state_message(cms_divider, cms_name);
        }
    } else if (ctrlPressed && minusPressed) {
        if (aPressed) {
            adlib_divider += adlib_divider >= 16 ? 0 : 1;
            minusPressed = false;
            level_state_message(adlib_divider, adlib_name);
        } else if (sPressed) {
            snd_divider += snd_divider >= 16 ? 0 : 1;
            minusPressed = false;
            level_state_message(snd_divider, ss_name);
        } else if (tPressed) {
            tandy3v_divider += tandy3v_divider >= 16 ? 0 : 1;
            minusPressed = false;
            level_state_message(tandy3v_divider, tandy3v_name);
        } else if (cPressed) {
            covox_divider += covox_divider >= 16 ? 0 : 1;
            minusPressed = false;
            level_state_message(covox_divider, covox_name);
        } else if (dPressed) {
            dss_divider += dss_divider >= 16 ? 0 : 1;
            minusPressed = false;
            level_state_message(dss_divider, dss_name);
        } else if (gPressed) {
            cms_divider += cms_divider >= 16 ? 0 : 1;
            minusPressed = false;
            level_state_message(cms_divider, cms_name);
        }
    }
}

static uint8_t repeat_cnt = 0;

static inline void work_cycle() {
    while(1) {
        if_swap_drives();
        if_overclock();
        if_sound_control();
        if (psp == &left_panel && !left_panel_make_active) {
            select_right_panel();
        }
        if (psp != &left_panel && left_panel_make_active) {
            select_left_panel();
        }
        if (lastSavedScanCode != lastCleanableScanCode && lastSavedScanCode > 0x80) {
            repeat_cnt = 0;
        } else {
            repeat_cnt++;
            if (repeat_cnt > 0xFE && lastSavedScanCode < 0x80) {
               lastCleanableScanCode = lastSavedScanCode + 0x80;
               repeat_cnt = 0;
            }
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
            handle_down_pressed();
            break;
          case 0x48: // up arr down
            scan_code_processed();
            break;
          case 0xC8: // up arr up
            if (lastSavedScanCode != 0x48) {
                break;
            }
            handle_up_pressed();
            break;
          case 0xCB: // left
            break;
          case 0xCD: // right
            break;
          case 0x1C: // Enter down
            scan_code_processed();
            break;
          case 0x9C: // Enter up
            if (lastSavedScanCode != 0x1C) {
                break;
            }
            enter_pressed();
            scan_code_processed();
            break;
        }
        if (usb_started && tud_msc_ejected()) {
            turn_usb_off(0);
        }
        if (usb_started) {
            pico_usb_drive_heartbeat();
        } else if(mark_to_exit_flag) {
            return;
        }
        //sprintf(line, "scan-code: %02Xh / saved scan-code: %02Xh", lastCleanableScanCode, lastSavedScanCode);
        //draw_cmd_line(0, CMD_Y_POS, line);
    }
}

inline static void start_manager() {
    mark_to_exit_flag = false;
    save_video_ram();
    enum graphics_mode_t ret = graphics_set_mode(TEXTMODE_80x30);
    set_start_debug_line(30);
    draw_window();
    select_left_panel();
    bottom_line();

    work_cycle();
    
    set_start_debug_line(25);
    graphics_set_mode(ret);
    restore_video_ram();
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
      case 0x20:
        dPressed = true;
        break;
      case 0xA0:
        dPressed = false;
        break;
      case 0x2E:
        cPressed = true;
        break;
      case 0xAE:
        cPressed = false;
        break;
      case 0x14:
        tPressed = true;
        break;
      case 0x94:
        tPressed = false;
        break;
      case 0x22:
        gPressed = true;
        break;
      case 0xA2:
        gPressed = false;
        break;
      case 0x1E:
        aPressed = true;
        break;
      case 0x9E:
        aPressed = false;
        break;
      case 0x1F:
        sPressed = true;
        break;
      case 0x9F:
        sPressed = false;
        break;
      case 0x2D:
        xPressed = true;
        break;
      case 0xAD:
        xPressed = false;
        break;
      case 0x12:
        ePressed = true;
        break;
      case 0x92:
        ePressed = false;
        break;
      case 0x16:
        uPressed = true;
        break;
      case 0x96:
        uPressed = false;
        break;
      case 0x23:
        hPressed = true;
        break;
      case 0xA3:
        hPressed = false;
        break;
      case 0x0F:
        tabPressed = true;
        break;
      case 0x8F:
        if (manager_started)
            left_panel_make_active = !left_panel_make_active; // TODO: combinations?
        tabPressed = false;
        break;
      default:
        //snprintf(line, 80, "Scan-code: %02Xh", ps2scancode);
        //draw_cmd_line(0, CMD_Y_POS, line);
        break;
    }
    return manager_started;
}

inline static int overclock() {
  if (tabPressed && ctrlPressed) {
    if (plusPressed) return 1;
    if (minusPressed) return -1;
  }
  return 0;
}

uint32_t overcloking_khz = OVERCLOCKING * 1000;

inline void if_overclock() {
    int oc = overclock();
    if (oc > 0) overcloking_khz += 1000;
    if (oc < 0) overcloking_khz -= 1000;
    if (oc != 0) {
    save_video_ram();
        enum graphics_mode_t ret = graphics_set_mode(TEXTMODE_80x30);
        if (ret != TEXTMODE_80x30) clrScr(1);
        uint vco, postdiv1, postdiv2;
        if (check_sys_clock_khz(overcloking_khz, &vco, &postdiv1, &postdiv2)) {
            set_sys_clock_pll(vco, postdiv1, postdiv2);
            sprintf(line, "overcloking_khz: %u kHz", overcloking_khz);
            line_t lns[1] = {
                { -1, line }
            };
            lines_t lines = { 1, 3, lns };
            draw_box(10, 7, 60, 10, "Info", &lines);
        }
        else {
            sprintf(line, "System clock of %u kHz cannot be achieved", overcloking_khz);
            line_t lns[1] = {
                { -1, line }
            };
            lines_t lines = { 1, 3, lns };
            draw_box(10, 7, 60, 10, "Warning", &lines);
        }
        sleep_ms(2500);
        graphics_set_mode(ret);
        restore_video_ram();
    }
}

void if_manager() {
    if_swap_drives();
    if_overclock();
    if_sound_control();
    if (manager_started) {
        return;
    }
    if (backspacePressed && enterPressed) {
        manager_started = true;
        start_manager();
        manager_started = false;
    }
}

#endif
