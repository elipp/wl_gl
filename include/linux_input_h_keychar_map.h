#ifndef LINUX_INPUT_H_KEYCHAR_MAP_H
#define LINUX_INPUT_H_KEYCHAR_MAP_H

#include <cstdint>
#include <cstring>
#include <linux/input.h>

extern char linux_input_h_keymap[];
void setup_keymap(); 
int maps_to_regular_char(int keycode);

#endif
