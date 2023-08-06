#pragma once

#include <kernel/types.h>

// start of video memory
#define VGA_MEMORY 0xB8000

// number of columns and rows in vga
#define VGA_COLS 80
#define VGA_ROWS 25

// misc
#define VGA_BLANK_CHAR ' '
#define VGA_TAB_SIZE 4

// color attribute
#define VGA_BLACK 0
#define VGA_BLUE 1
#define VGA_GREEN 2
#define VGA_CYAN 3
#define VGA_RED 4
#define VGA_MAGENTA 5
#define VGA_BROWN 6
#define VGA_GREY 7
#define VGA_DARK_GREY 8
#define VGA_BRIGHT_BLUE 9
#define VGA_BRIGHT_GREEN 10
#define VGA_BRIGHT_CYAN 11
#define VGA_BRIGHT_RED 12
#define VGA_BRIGHT_MAGENTA 13
#define VGA_YELLOW 14
#define VGA_WHITE 15

void vga_init(void);

void vga_clr(const uint8_t character);
void vga_putc(const char character);
inline void vga_backspace();
inline void vga_newline();
inline void vga_tab();
void vga_puts(char *str);

void vga_set_color(const uint8_t color);
void vga_goto_xy(const uint8_t x, const uint8_t y);
uint8_t vga_get_x();
uint8_t vga_get_y();
void vga_set_cursor(const uint8_t x, const uint8_t y);
