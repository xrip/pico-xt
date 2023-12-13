#pragma once
#include <inttypes.h>
#include <stdbool.h>

void if_manager();
void notify_image_insert_action(uint8_t drivenum, char *pathname);
bool handleScancode(uint32_t ps2scancode);