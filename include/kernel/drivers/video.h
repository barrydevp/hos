#pragma once

#include <kernel/boot.h>

#define VIDEO_TYPE_NONE 0
#define VIDEO_TYPE_VGA 1
#define VIDEO_TYPE_FRAMEBUFFER 2
#define VIDEO_TYPE_EGA_TEXT 3

/// @brief Early initializes the VGA right after boot.
void video_early_init();

/// @brief Initializes the VGA.
void video_init(boot_info_t *boot_info);

/// @brief Finalizes the VGA.
void video_finalize();

/// @brief Updates the video.
void video_update();

/// @brief Print the given character on the screen.
/// @param c The character to print.
void video_putc(int c);

/// @brief Prints the given string on the screen.
/// @param str The string to print.
void video_puts(const char *str);

/// @brief When something is written in another position, update the cursor.
void video_set_cursor_auto();

/// @brief Move the cursor at the position x, y on the screen.
/// @param x The x coordinate.
/// @param y The y coordinate.
void video_move_cursor(unsigned int x, unsigned int y);

/// @brief Returns cursor's position on the screen.
/// @param x The output x coordinate.
/// @param y The output y coordinate.
void video_get_cursor_position(unsigned int *x, unsigned int *y);

/// @brief Returns screen size.
/// @param width The screen width.
/// @param height The screen height.
void video_get_screen_size(unsigned int *width, unsigned int *height);

/// @brief Clears the screen.
void video_clear();

/// @brief Move to the following line (the effect of \n character).
void video_new_line();

/// @brief Move to the up line (the effect of \n character).
void video_cartridge_return();

/// @brief The whole screen is shifted up by one line. Used when the cursor
///        reaches the last position of the screen.
void video_shift_one_line_up();

/// @brief The whole screen is shifted up by one page.
void video_shift_one_page_up();

/// @brief The whole screen is shifted down by one page.
void video_shift_one_page_down();
