#pragma once
#include <inttypes.h>

void if_manager();
void if_swap_drives();
int overclock();
void notify_image_insert_action(uint8_t drivenum, char *pathname);
void start_manager();
