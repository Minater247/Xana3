#ifndef _KEYBOARD_H
#define _KEYBOARD_H

#include <stdint.h>

void keyboard_install();
uint8_t keyboard_getcode();
char scancode_to_char(uint8_t scancode);
void keyboard_addcode(uint8_t scancode);
void keyboard_addchar(char c);
void keyboard_addstring(char *string);

#endif