#ifndef _ANSI_H
#define _ANSI_H

#include <stdint.h>
#include <stdbool.h>

#define ANSI_STATE_NONE 0
#define ANSI_STATE_EXTENDED_FG 1
#define ANSI_STATE_EXTENDED_BG 2
#define ANSI_STATE_EXTENDED_FG_256 3
#define ANSI_STATE_EXTENDED_BG_256 4

void begin_escape_sequence();
void handle_escape_sequence_color(uint32_t color);
void parse_escape_sequence(char *seq);
void handle_escape_sequence_char(char c);

extern bool in_escape_sequence;
extern char *escape_sequence;

#endif