#include "linux_input_h_keychar_map.h"

char linux_input_h_keymap[256];

int maps_to_regular_char(int keycode) {
	if (keycode > 255 || keycode < 0) { return 0; }
	char c = linux_input_h_keymap[keycode];
	return (c != '?') ? 1 : 0;
}

void setup_keymap() {

	memset(linux_input_h_keymap, '?', sizeof(linux_input_h_keymap));
	// this is admittedly pretty crappy D:DD. replace this with a static table, or better yet, a proper input library.
	linux_input_h_keymap[0] = '\0';
	linux_input_h_keymap[KEY_Q] = 'q';
	linux_input_h_keymap[KEY_W] = 'w';
	linux_input_h_keymap[KEY_E] = 'e';
	linux_input_h_keymap[KEY_R] = 'r';
	linux_input_h_keymap[KEY_T] = 't';
	linux_input_h_keymap[KEY_Y] = 'y';
	linux_input_h_keymap[KEY_U] = 'u';
	linux_input_h_keymap[KEY_I] = 'i';
	linux_input_h_keymap[KEY_O] = 'o';
	linux_input_h_keymap[KEY_P] = 'p';
	linux_input_h_keymap[KEY_A] = 'a';
	linux_input_h_keymap[KEY_S] = 's';
	linux_input_h_keymap[KEY_D] = 'd';
	linux_input_h_keymap[KEY_F] = 'f';
	linux_input_h_keymap[KEY_G] = 'g';
	linux_input_h_keymap[KEY_H] = 'h';
	linux_input_h_keymap[KEY_J] = 'j';
	linux_input_h_keymap[KEY_K] = 'k';
	linux_input_h_keymap[KEY_L] = 'l';
	linux_input_h_keymap[KEY_Z] = 'z';
	linux_input_h_keymap[KEY_X] = 'x';
	linux_input_h_keymap[KEY_C] = 'c';
	linux_input_h_keymap[KEY_V] = 'v';
	linux_input_h_keymap[KEY_B] = 'b';
	linux_input_h_keymap[KEY_N] = 'n';
	linux_input_h_keymap[KEY_M] = 'm';

	linux_input_h_keymap[KEY_1] = '1';
	linux_input_h_keymap[KEY_2] = '2';
	linux_input_h_keymap[KEY_3] = '3';
	linux_input_h_keymap[KEY_4] = '4';
	linux_input_h_keymap[KEY_5] = '5';
	linux_input_h_keymap[KEY_6] = '6';
	linux_input_h_keymap[KEY_7] = '7';
	linux_input_h_keymap[KEY_8] = '8';
	linux_input_h_keymap[KEY_9] = '9';
	linux_input_h_keymap[KEY_0] = '0';

	linux_input_h_keymap[KEY_MINUS] = '-';
	linux_input_h_keymap[KEY_EQUAL] = '=';
//	linux_input_h_keymap[KEY_LEFTSHIFT] = '<';
	linux_input_h_keymap[KEY_102ND] = '<';	// hmm, this is quite weird though
//	linux_input_h_keymap[KEY_KPPLUS] = '+';
//	linux_input_h_keymap[KEY_KPASTERISK] = '*';
	linux_input_h_keymap[KEY_COMMA] = ',';
	linux_input_h_keymap[KEY_SEMICOLON] = ';';
	linux_input_h_keymap[KEY_APOSTROPHE] = '\'';
	linux_input_h_keymap[KEY_DOT] = '.';
	linux_input_h_keymap[KEY_SLASH] = '/';
	linux_input_h_keymap[KEY_GRAVE] = '`';
	linux_input_h_keymap[KEY_BACKSLASH] = '\\';
	linux_input_h_keymap[KEY_LEFTBRACE] = '[';
	linux_input_h_keymap[KEY_RIGHTBRACE] = ']';

	linux_input_h_keymap[KEY_SPACE] = ' ';
	
}

