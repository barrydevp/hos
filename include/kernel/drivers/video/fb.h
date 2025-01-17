#pragma once

#include <kernel/boot.h>

/// @brief Initializes the Framebuffer.
void fb_init(boot_info_t *boot_info);

/// @brief Finalizes the Framebuffer.
void fb_finalize();

/// @brief Updates the graphic elements.
void fb_update();

/// @brief Checks if the Framebuffer is enabled.
/// @return 1 if enabled, 0 otherwise.
int fb_is_enabled();

/// @brief Returns the width of the screen.
/// @return the width of the screen.
int fb_width();

/// @brief Returns the height of the screen.
/// @return the height of the screen.
int fb_height();

/// @brief Clears the screen.
void fb_clear();

/// @param x x-axis position.
/// @brief Draws a pixel at the given position.
/// @param y y-axis position.
/// @param color color of the character.
void fb_draw_pixel(int x, int y, uint32_t color);

/// @brief Draws a character at the given position.
/// @param x x-axis position.
/// @param y y-axis position.
/// @param c character to draw.
/// @param color color of the character.
void fb_draw_char(int x, int y, unsigned char c, uint32_t fg, uint32_t bg);

/// @brief Draws a string at the given position.
/// @param x x-axis position.
/// @param y y-axis position.
/// @param str string to draw.
/// @param color color of the character.
void fb_draw_string(int x, int y, const char *str, uint32_t fg, uint32_t bg);

/// @brief Draws a line from point 1 to point 2.
/// @param x0 point 1 x-axis position.
/// @param y0 point 1 y-axis position.
/// @param x1 point 2 x-axis position.
/// @param y1 point 2 y-axis position.
/// @param color color of the line.
void fb_draw_line(int x0, int y0, int x1, int y1, unsigned char color);

/// @brief Draws a rectangle provided the position of the starting corner and the ending corner.
/// @param sx top-left corner x-axis position.
/// @param sy top-left corner y-axis position.
/// @param w width.
/// @param h height.
/// @param color color of the rectangle.
void fb_draw_rectangle(int sx, int sy, int w, int h, unsigned char color);

/// @brief Draws a circle provided the position of the center and the radius.
/// @param xc x-axis position.
/// @param yc y-axis position.
/// @param r radius.
/// @param color used to draw the circle.
void fb_draw_circle(int xc, int yc, int r, unsigned char color);

/// @brief Draws a triangle.
/// @param x1 1st point x-axis position.
/// @param y1 1st point y-axis position.
/// @param x2 2nd point x-axis position.
/// @param y2 2nd point y-axis position.
/// @param x3 3rd point x-axis position.
/// @param y3 3rd point y-axis position.
/// @param color used to draw the triangle.
void fb_draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3,
                      unsigned char color);

/// @brief Print the given character on the screen.
/// @param c The character to print.
void fb_putc(int c);

/// @brief Prints the given string on the screen.
/// @param str The string to print.
void fb_puts(const char *str);

/// @brief Move the cursor at the position x, y on the screen.
/// @param x The x coordinate.
/// @param y The y coordinate.
void fb_move_cursor(unsigned int x, unsigned int y);

/// @brief Returns cursor's position on the screen.
/// @param x The output x coordinate.
/// @param y The output y coordinate.
void fb_get_cursor_position(unsigned int *x, unsigned int *y);

/// @brief Returns screen size.
/// @param width The screen width.
/// @param height The screen height.
void fb_get_screen_size(unsigned int *width, unsigned int *height);

/// @brief Move to the following line (the effect of \n character).
void fb_new_line();

/// @brief Change the color.
void fb_set_color(uint32_t fg, uint32_t bg);

/// @brief Shift the screen one line up.
void fb_shift_one_line_up();
