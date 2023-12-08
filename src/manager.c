#include "manager.h"
#include "vga.h"

static void draw_window(char *title, int x, int y, int width, int height) {
    char textline[80];
    width--;
    height--;
    // Рисуем рамки
    // ═══
    memset(textline, 0xCD, width);
    // ╔ ╗ 188 ╝ 200 ╚
    textline[0] = 0xC9;
    textline[width] = 0xBB;
    draw_text(textline, x, y, 11, 1);
    draw_text(title, (width - strlen(title)) >> 1, 0, 0, 3);
    textline[0] = 0xC8;
    textline[width] = 0xBC;
    draw_text(textline, x, height - y, 11, 1);
    memset(textline, ' ', width);
    textline[0] = textline[width] = 0xBA;
    for (int i = 1; i < height; i++) {
        draw_text(textline, x, i, 11, 1);
    }
}

void work_cycle() {
    sleep_ms(33);
    in_flash_drive();
}

void start_manager() {
    save_video_ram();
    enum graphics_mode_t ret = graphics_set_mode(TEXTMODE_80x30);

    draw_window("PICO XT internal manager", 0, 0, 80, 30);

    work_cycle();

    restore_video_ram();
    if (ret == TEXTMODE_80x30) {
        clrScr(1);
    }
    graphics_set_mode(ret);
}
