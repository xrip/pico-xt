#if PICO_ON_DEVICE
#include <stdio.h>
#include <string.h>
#include "pico-vision.h"
#include "vga.h"

static const color_schema_t color_schema = {
   /*BACKGROUND_FIELD_COLOR =*/ 1, // Blue
   /*FOREGROUND_FIELD_COLOR =*/ 7, // White

   /*BACKGROUND_F1_10_COLOR =*/ 0, // Black
   /*FOREGROUND_F1_10_COLOR =*/ 7, // White

   /*BACKGROUND_F_BTN_COLOR =*/ 3, // Green
   /*FOREGROUND_F_BTN_COLOR =*/ 0, // Black

   /*BACKGROUND_CMD_COLOR =*/ 0, // Black
   /*FOREGROUND_CMD_COLOR =*/ 7, // White

   /*FOREGROUND_SELECTED_COLOR =*/ 0, // Black
   /*BACKGROUND_SELECTED_COLOR =*/ 11, //
};

static color_schema_t* pcs = &color_schema;

void set_color_schems(color_schema_t* pschema) {
    pcs = pschema;
}

void draw_panel(int left, int top, int width, int height, char* title, char* bottom) {
    char line[82];
    // top line
    for(int i = 1; i < width - 1; ++i) {
        line[i] = 0xCD; // ═
    }
    line[0]         = 0xC9; // ╔
    line[width - 1] = 0xBB; // ╗
    line[width]     = 0;
    draw_text(line, left, top, pcs->FOREGROUND_FIELD_COLOR, pcs->BACKGROUND_FIELD_COLOR); 
    if (title) {
        int sl = strlen(title);
        if (width - 4 < sl) {
            title -= width + 4; // cat title
            sl -= width + 4;
        }
        int title_left = left + (width - sl) / 2;
        sprintf(line, " %s ", title);
        draw_text(line, title_left, top, pcs->FOREGROUND_FIELD_COLOR, pcs->BACKGROUND_FIELD_COLOR);
    }
    // middle lines
    memset(line, ' ', width);
    line[0]         = 0xBA; // ║
    line[width - 1] = 0xBA;
    line[width]     = 0;
    for (int y = top + 1; y < top + height - 1; ++y) {
        draw_text(line, left, y, pcs->FOREGROUND_FIELD_COLOR, pcs->BACKGROUND_FIELD_COLOR);
    }
    // bottom line
    for(int i = 1; i < width - 1; ++i) {
        line[i] = 0xCD; // ═
    }
    line[0]         = 0xC8; // ╚
    line[width - 1] = 0xBC; // ╝
    line[width]     = 0;
    draw_text(line, left, top + height - 1, pcs->FOREGROUND_FIELD_COLOR, pcs->BACKGROUND_FIELD_COLOR);
    if (bottom) {
        int sl = strlen(bottom);
        if (width - 4 < sl) {
            bottom -= width + 4; // cat bottom
            sl -= width + 4;
        } 
        int bottom_left = (width - sl) / 2;
        sprintf(line, " %s ", bottom);
        draw_text(line, bottom_left, top + height - 1, pcs->FOREGROUND_FIELD_COLOR, pcs->BACKGROUND_FIELD_COLOR);
    }
}

void draw_box(int left, int top, int width, int height, char* title, char* txt) {
    draw_panel(left, top, width, height, title, 0);
    char line[80] = {0};
    int i = 0;
    int y = top + 1;
    while (*txt != 0 && i < 80) {
        if (*txt == '\n') {
            draw_label(left + 1, y, width - 2, line, false);
            y++;
            i = 0;
        } else {
            line[i++] = *txt;
        }
        txt++;
    }
    if (line[0] && i > 0) {
        draw_label(left + 1, y, width - 2, line, false);
    }
    for (int i = y; y < top + height - 1; ++y) {
        draw_label(left + 1, y, width - 2, "", false);
    }
}

void draw_fn_btn(fn_1_10_tbl_rec_t* prec, int left, int top) {
    char line[10];
    sprintf(line, "       ");
    // 1, 2, 3... button mark
    line[0] = prec->pre_mark;
    line[1] = prec->mark;
    draw_text(line, left, top, pcs->FOREGROUND_F1_10_COLOR, pcs->BACKGROUND_F1_10_COLOR);
    // button
    sprintf(line, prec->name);
    draw_text(line, left + 2, top, pcs->FOREGROUND_F_BTN_COLOR, pcs->BACKGROUND_F_BTN_COLOR);
}

void draw_cmd_line(int left, int top, char* cmd) { // TODO: cmd
    char line[82];
    if (cmd) {
        int sl = strlen(cmd);
        snprintf(line, 80, ">%s", cmd);
        memset(line + sl + 1, ' ', 80 - sl);
    } else {
        memset(line, ' ', 80); line[0] = '>';
    }
    line[80] = 0;
    draw_text(line, left, top, pcs->FOREGROUND_CMD_COLOR, pcs->BACKGROUND_CMD_COLOR);
}

void draw_label(int left, int top, int width, char* txt, bool selected) {
    char line[82];
    bool fin = false;
    for (int i = 0; i < width; ++i) {
        if (!fin) {
            if (!txt[i]) {
                fin = true;
                line[i] = ' ';
            } else {
                line[i] = txt[i];
            }
        } else {
            line[i] = ' ';
        }
    }
    line[width] = 0;
    int fgc = selected ? pcs->FOREGROUND_SELECTED_COLOR : pcs->FOREGROUND_FIELD_COLOR;
    int bgc = selected ? pcs->BACKGROUND_SELECTED_COLOR : pcs->BACKGROUND_FIELD_COLOR;
    draw_text(line, left, top, fgc, bgc);
}
#endif