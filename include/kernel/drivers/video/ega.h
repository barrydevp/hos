#pragma once

#include <kernel/types.h>
#include <kernel/boot.h>

#define FG_RESET "\033[0m" ///< ANSI code for resetting.

#define FG_BLACK   "\033[30m" ///< ANSI code for setting a BLACK foreground.
#define FG_RED     "\033[31m" ///< ANSI code for setting a RED foreground.
#define FG_GREEN   "\033[32m" ///< ANSI code for setting a GREEN foreground.
#define FG_YELLOW  "\033[33m" ///< ANSI code for setting a YELLOW foreground.
#define FG_BLUE    "\033[34m" ///< ANSI code for setting a BLUE foreground.
#define FG_MAGENTA "\033[35m" ///< ANSI code for setting a MAGENTA foreground.
#define FG_CYAN    "\033[36m" ///< ANSI code for setting a CYAN foreground.
#define FG_WHITE   "\033[37m" ///< ANSI code for setting a WHITE foreground.

#define FG_BLACK_BOLD   "\033[1;30m" ///< ANSI code for setting a BLACK foreground.
#define FG_RED_BOLD     "\033[1;31m" ///< ANSI code for setting a RED foreground.
#define FG_GREEN_BOLD   "\033[1;32m" ///< ANSI code for setting a GREEN foreground.
#define FG_YELLOW_BOLD  "\033[1;33m" ///< ANSI code for setting a YELLOW foreground.
#define FG_BLUE_BOLD    "\033[1;34m" ///< ANSI code for setting a BLUE foreground.
#define FG_MAGENTA_BOLD "\033[1;35m" ///< ANSI code for setting a MAGENTA foreground.
#define FG_CYAN_BOLD    "\033[1;36m" ///< ANSI code for setting a CYAN foreground.
#define FG_WHITE_BOLD   "\033[1;37m" ///< ANSI code for setting a WHITE foreground.

#define FG_BLACK_BRIGHT   "\033[90m" ///< ANSI code for setting a BRIGHT_BLACK foreground.
#define FG_RED_BRIGHT     "\033[91m" ///< ANSI code for setting a BRIGHT_RED foreground.
#define FG_GREEN_BRIGHT   "\033[92m" ///< ANSI code for setting a BRIGHT_GREEN foreground.
#define FG_YELLOW_BRIGHT  "\033[93m" ///< ANSI code for setting a BRIGHT_YELLOW foreground.
#define FG_BLUE_BRIGHT    "\033[94m" ///< ANSI code for setting a BRIGHT_BLUE foreground.
#define FG_MAGENTA_BRIGHT "\033[95m" ///< ANSI code for setting a BRIGHT_MAGENTA foreground.
#define FG_CYAN_BRIGHT    "\033[96m" ///< ANSI code for setting a BRIGHT_CYAN foreground.
#define FG_WHITE_BRIGHT   "\033[97m" ///< ANSI code for setting a BRIGHT_WHITE foreground.

#define FG_BLACK_BRIGHT_BOLD   "\033[1;90m" ///< ANSI code for setting a BRIGHT_BLACK foreground.
#define FG_RED_BRIGHT_BOLD     "\033[1;91m" ///< ANSI code for setting a BRIGHT_RED foreground.
#define FG_GREEN_BRIGHT_BOLD   "\033[1;92m" ///< ANSI code for setting a BRIGHT_GREEN foreground.
#define FG_YELLOW_BRIGHT_BOLD  "\033[1;93m" ///< ANSI code for setting a BRIGHT_YELLOW foreground.
#define FG_BLUE_BRIGHT_BOLD    "\033[1;94m" ///< ANSI code for setting a BRIGHT_BLUE foreground.
#define FG_MAGENTA_BRIGHT_BOLD "\033[1;95m" ///< ANSI code for setting a BRIGHT_MAGENTA foreground.
#define FG_CYAN_BRIGHT_BOLD    "\033[1;96m" ///< ANSI code for setting a BRIGHT_CYAN foreground.
#define FG_WHITE_BRIGHT_BOLD   "\033[1;97m" ///< ANSI code for setting a BRIGHT_WHITE foreground.

#define BG_BLACK          "\033[40m"  ///< ANSI code for setting a BLACK background.
#define BG_RED            "\033[41m"  ///< ANSI code for setting a RED background.
#define BG_GREEN          "\033[42m"  ///< ANSI code for setting a GREEN background.
#define BG_YELLOW         "\033[43m"  ///< ANSI code for setting a YELLOW background.
#define BG_BLUE           "\033[44m"  ///< ANSI code for setting a BLUE background.
#define BG_MAGENTA        "\033[45m"  ///< ANSI code for setting a MAGENTA background.
#define BG_CYAN           "\033[46m"  ///< ANSI code for setting a CYAN background.
#define BG_WHITE          "\033[47m"  ///< ANSI code for setting a WHITE background.

#define BG_BRIGHT_BLACK   "\033[100m" ///< ANSI code for setting a BRIGHT_BLACK background.
#define BG_BRIGHT_RED     "\033[101m" ///< ANSI code for setting a BRIGHT_RED background.
#define BG_BRIGHT_GREEN   "\033[102m" ///< ANSI code for setting a BRIGHT_GREEN background.
#define BG_BRIGHT_YELLOW  "\033[103m" ///< ANSI code for setting a BRIGHT_YELLOW background.
#define BG_BRIGHT_BLUE    "\033[104m" ///< ANSI code for setting a BRIGHT_BLUE background.
#define BG_BRIGHT_MAGENTA "\033[105m" ///< ANSI code for setting a BRIGHT_MAGENTA background.
#define BG_BRIGHT_CYAN    "\033[106m" ///< ANSI code for setting a BRIGHT_CYAN background.
#define BG_BRIGHT_WHITE   "\033[107m" ///< ANSI code for setting a BRIGHT_WHITE background.

/// @brief Initialize the video.
void ega_init(boot_info_t* boot_info);

/// @brief Finalizes the EGA.
void ega_finalize();

/// @brief Updates the video.
void ega_update();

/// @brief Print the given character on the screen.
/// @param c The character to print.
void ega_putc(int c);

/// @brief Prints the given string on the screen.
/// @param str The string to print.
void ega_puts(const char *str);

/// @brief When something is written in another position, update the cursor.
void ega_set_cursor_auto();

/// @brief Move the cursor at the position x, y on the screen.
/// @param x The x coordinate.
/// @param y The y coordinate.
void ega_move_cursor(unsigned int x, unsigned int y);

/// @brief Returns cursor's position on the screen.
/// @param x The output x coordinate.
/// @param y The output y coordinate.
void ega_get_cursor_position(unsigned int * x, unsigned int * y);

/// @brief Returns screen size.
/// @param width The screen width.
/// @param height The screen height.
void ega_get_screen_size(unsigned int * width, unsigned int * height);

/// @brief Clears the screen.
void ega_clear();

/// @brief Move to the following line (the effect of \n character).
void ega_new_line();

/// @brief Move to the up line (the effect of \n character).
void ega_cartridge_return();

/// @brief The whole screen is shifted up by one line. Used when the cursor
///        reaches the last position of the screen.
void ega_shift_one_line_up();

/// @brief The whole screen is shifted up by one page.
void ega_shift_one_page_up();

/// @brief The whole screen is shifted down by one page.
void ega_shift_one_page_down();
