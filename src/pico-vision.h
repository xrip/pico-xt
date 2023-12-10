#pragma once
#include <stdbool.h>
#include <inttypes.h>

typedef struct color_schema {
    uint8_t BACKGROUND_FIELD_COLOR;
    uint8_t FOREGROUND_FIELD_COLOR;
    uint8_t BACKGROUND_F1_10_COLOR;
    uint8_t FOREGROUND_F1_10_COLOR;
    uint8_t BACKGROUND_F_BTN_COLOR;
    uint8_t FOREGROUND_F_BTN_COLOR;
    uint8_t BACKGROUND_CMD_COLOR;
    uint8_t FOREGROUND_CMD_COLOR;
    uint8_t FOREGROUND_SELECTED_COLOR;
    uint8_t BACKGROUND_SELECTED_COLOR;
} color_schema_t;

// type of F1-F10 function pointer
typedef void (*fn_1_10_ptr)(uint8_t);

#define BTN_WIDTH 8
typedef struct fn_1_10_tbl_rec {
    char pre_mark;
    char mark;
    char name[BTN_WIDTH];
    fn_1_10_ptr action;
} fn_1_10_tbl_rec_t;

#define BTNS_COUNT 10
typedef fn_1_10_tbl_rec_t fn_1_10_tbl_t[BTNS_COUNT];

void set_color_schems(color_schema_t* pschema);

void draw_panel(int left, int top, int width, int height, char* title, char* bottom);

void draw_fn_btn(fn_1_10_tbl_rec_t* prec, int left, int top);

void draw_cmd_line(int left, int top, char* cmd);

void draw_label(int left, int top, int width, char* txt, bool selected);
