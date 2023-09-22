#pragma once

#include <kernel/boot.h>

#define FRAMEBUFFER_TYPE_INDEXED 0
#define FRAMEBUFFER_TYPE_RGB 1
#define FRAMEBUFFER_TYPE_EGA_TEXT 2

/// @brief Early initializes the VGA right after boot.
void vga_early_init();

/// @brief Initializes the VGA.
void vga_init(boot_info_t *boot_info);

/// @brief Finalizes the VGA.
void vga_finalize();

/// @brief Updates the video.
void vga_update();

/// @brief Print the given character on the screen.
/// @param c The character to print.
void vga_putc(int c);

/// @brief Prints the given string on the screen.
/// @param str The string to print.
void vga_puts(const char *str);

/// @brief When something is written in another position, update the cursor.
void vga_set_cursor_auto();

/// @brief Move the cursor at the position x, y on the screen.
/// @param x The x coordinate.
/// @param y The y coordinate.
void vga_move_cursor(unsigned int x, unsigned int y);

/// @brief Returns cursor's position on the screen.
/// @param x The output x coordinate.
/// @param y The output y coordinate.
void vga_get_cursor_position(unsigned int *x, unsigned int *y);

/// @brief Returns screen size.
/// @param width The screen width.
/// @param height The screen height.
void vga_get_screen_size(unsigned int *width, unsigned int *height);

/// @brief Clears the screen.
void vga_clear();

/// @brief Move to the following line (the effect of \n character).
void vga_new_line();

/// @brief Move to the up line (the effect of \n character).
void vga_cartridge_return();

/// @brief The whole screen is shifted up by one line. Used when the cursor
///        reaches the last position of the screen.
void vga_shift_one_line_up();

/// @brief The whole screen is shifted up by one page.
void vga_shift_one_page_up();

/// @brief The whole screen is shifted down by one page.
void vga_shift_one_page_down();
