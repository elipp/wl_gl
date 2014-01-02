#ifndef KEYBOARD_H
#define KEYBOARD_H

extern bool keys[256];

#ifdef _WIN32
#include <Windows.h>

#define KAR_KB_UP VK_UP
#define KAR_KB_DOWN VK_DOWN
#define KAR_KB_LEFT VK_LEFT
#define KAR_KB_RIGHT VK_RIGHT

#elif __linux__
#include <linux/input.h>

#include "linux_input_h_keychar_map.h"

#define KAR_KB_UP KEY_UP
#define KAR_KB_DOWN KEY_DOWN
#define KAR_KB_LEFT KEY_LEFT
#define KAR_KB_RIGHT KEY_RIGHT

#endif

#endif
