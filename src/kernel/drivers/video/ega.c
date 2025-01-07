#include <kernel/drivers/video/ega.h>
#include <arch/i386/ports.h>
#include <arch/i386/cpu.h>
#include <kernel/ctype.h>
#include <kernel/stdio.h>
#include <kernel/string.h>

#define HEIGHT 25 ///< The height of the
#define WIDTH 80 ///< The width of the
#define W2 (WIDTH * 2) ///< The width of the
#define TOTAL_SIZE (HEIGHT * WIDTH * 2) ///< The total size of the screen.
#define ADDR (char *)0xB8000U ///< The address of the
#define STORED_PAGES 3 ///< The number of stored pages.

/// @brief Stores the association between ANSI colors and pure VIDEO colors.
struct ansi_color_map_t {
  /// The ANSI color number.
  uint8_t ansi_color;
  /// The VIDEO color number.
  uint8_t ega_color;
}

// clang-format off
/// @brief The mapping.
ansi_color_map[] = {
  { 30, 0 },
  { 31, 4 },
  { 32, 2 },
  { 33, 6 },
  { 34, 1 },
  { 35, 5 },
  { 36, 3 },
  { 37, 7 },

  { 90, 8 },
  { 91, 12 },
  { 92, 10 },
  { 93, 14 },
  { 94, 9 },
  { 95, 13 },
  { 96, 11 },
  { 97, 15 },

  { 40, 0 },
  { 41, 4 },
  { 42, 2 },
  { 43, 6 },
  { 44, 1 },
  { 45, 5 },
  { 46, 3 },
  { 47, 7 },

  { 100, 8 },
  { 101, 12 },
  { 102, 10 },
  { 103, 14 },
  { 104, 9 },
  { 105, 13 },
  { 106, 11 },
  { 107, 15 },

  { 0, 0 }
};
// clang-format on

/// VIDEO_MEMORY ADDRESS BASE
char *video_base = ADDR;
/// Pointer to a position of the screen writer.
char *pointer = ADDR;
/// The current color.
unsigned char color = 7;
/// Used to write on the escape_buffer. If -1, we are not parsing an escape sequence.
int escape_index = -1;
/// Used to store an escape sequence.
char escape_buffer[256];
/// Buffer where we store the upper scroll history.
char upper_buffer[STORED_PAGES * TOTAL_SIZE] = { 0 };
/// Buffer where we store the lower scroll history.
char original_page[TOTAL_SIZE] = { 0 };
/// Determines if the screen is currently scrolled.
int scrolled_page = 0;

/// @brief Get the current column number.
/// @return The column number.
static inline unsigned __get_x() {
  return ((pointer - video_base) % (WIDTH * 2)) / 2;
}

/// @brief Get the current row number.
/// @return The row number.
static inline unsigned __get_y() {
  return (pointer - video_base) / (WIDTH * 2);
}

/// @brief Draws the given character.
/// @param c The character to draw.
static inline void __draw_char(char c) {
  // for (char *ptr = (video_base + TOTAL_SIZE + (WIDTH * 2)); ptr > pointer;
  //      ptr -= 2) {
  //   *(ptr) = *(ptr - 2);
  //   *(ptr + 1) = *(ptr - 1);
  // }
  *(pointer++) = c;
  *(pointer++) = color;
}

/// @brief Sets the provided ansi code.
/// @param ansi_code The ansi code describing background and foreground color.
static inline void __set_color(uint8_t ansi_code) {
  struct ansi_color_map_t *it = ansi_color_map;
  while (it->ansi_color != 0) {
    if (ansi_code == it->ansi_color) {
      if (((ansi_code >= 30) && (ansi_code <= 37)) ||
          ((ansi_code >= 90) && (ansi_code <= 97))) {
        color = (color & 0xF0U) | it->ega_color;
      } else {
        color = (color & 0x0FU) | (it->ega_color << 4U);
      }
      break;
    }
    ++it;
  }
}

/// @brief Moves the cursor backward.
/// @param erase  If 1 also erase the character.
/// @param amount How many times we move backward.
static inline void __move_cursor_backward(int erase, int amount) {
  for (int i = 0; i < amount; ++i) {
    // Bring back the pointer.
    pointer -= 2;
    if (erase) {
      strcpy(pointer, pointer + 2);
    }
  }
  ega_set_cursor_auto();
}

/// @brief Moves the cursor forward.
/// @param erase  If 1 also erase the character.
/// @param amount How many times we move forward.
static inline void __move_cursor_forward(int erase, int amount) {
  for (int i = 0; i < amount; ++i) {
    // Bring forward the pointer.
    if (erase) {
      __draw_char(' ');
    } else {
      pointer += 2;
    }
  }
  ega_set_cursor_auto();
}

/// @brief Issue the vide to move the cursor to the given position.
/// @param x The x coordinate.
/// @param y The y coordinate.
static inline void __ega_set_cursor(unsigned int x, unsigned int y) {
  uint32_t position = x * WIDTH + y;
  // Cursor LOW port to vga INDEX register.
  outportb(0x3D4, 0x0F);
  outportb(0x3D5, (uint8_t)(position & 0xFFU));
  // Cursor HIGH port to vga INDEX register.
  outportb(0x3D4, 0x0E);
  outportb(0x3D5, (uint8_t)((position >> 8U) & 0xFFU));
}

void ega_init(boot_info_t *boot_info) {
  video_base = (char *)boot_info->video_start;
  // pointer = pointer - ADDR + video_base;
  pointer = video_base;
  ega_clear();
  ega_set_cursor_auto();
}

void ega_finalize() {
  ega_clear();
}

void ega_update() {
}

void ega_putc(int c) {
  // ESCAPE SEQUENCES
  if (c == '\033') {
    escape_index = 0;
    return;
  }
  if (escape_index >= 0) {
    if ((escape_index == 0) && (c == '[')) {
      return;
    }
    escape_buffer[escape_index++] = c;
    escape_buffer[escape_index] = 0;
    if (isalpha(c)) {
      escape_buffer[--escape_index] = 0;
      if (c == 'C') {
        __move_cursor_forward(false, atoi(escape_buffer));
      } else if (c == 'D') {
        __move_cursor_backward(false, atoi(escape_buffer));
      } else if (c == 'm') {
        __set_color(atoi(escape_buffer));
      } else if (c == 'J') {
        ega_clear();
      }
      escape_index = -1;
    }
    return;
  }

  // == NORMAL CHARACTERS =======================================================================
  // If the character is '\n' go the new line.
  if (c == '\n') {
    ega_new_line();
    //ega_shift_one_line_down();
  } else if (c == '\b') {
    __move_cursor_backward(true, 1);
  } else if (c == '\r') {
    ega_cartridge_return();
  } else if (c == 127) {
    strcpy(pointer, pointer + 2);
  } else if ((c >= 0x20) && (c <= 0x7E)) {
    __draw_char(c);
  } else {
    return;
  }

  ega_shift_one_line_up();
  ega_set_cursor_auto();
}

void ega_puts(const char *str) {
  // #ifndef VGA_TEXT_MODE
  //   if (vga_is_enabled()) {
  //     vga_puts(str);
  //     return;
  //   }
  // #endif
  while ((*str) != 0) {
    ega_putc((*str++));
  }
}

void ega_set_cursor_auto() {
  // #ifndef VGA_TEXT_MODE
  //   if (vga_is_enabled())
  //     return;
  // #endif
  __ega_set_cursor(((pointer - video_base) / 2U) / WIDTH,
                   ((pointer - video_base) / 2U) % WIDTH);
}

void ega_move_cursor(unsigned int x, unsigned int y) {
  // #ifndef VGA_TEXT_MODE
  //   if (vga_is_enabled()) {
  //     vga_move_cursor(x, y);
  //     return;
  //   }
  // #endif
  pointer = video_base + ((y * WIDTH * 2) + (x * 2));
  ega_set_cursor_auto();
}

void ega_get_cursor_position(unsigned int *x, unsigned int *y) {
  // #ifndef VGA_TEXT_MODE
  //   if (vga_is_enabled()) {
  //     vga_get_cursor_position(x, y);
  //     return;
  //   }
  // #endif
  if (x)
    *x = __get_x();
  if (y)
    *y = __get_y();
}

void ega_get_screen_size(unsigned int *width, unsigned int *height) {
  // #ifndef VGA_TEXT_MODE
  //   if (vga_is_enabled()) {
  //     vga_get_screen_size(width, height);
  //     return;
  //   }
  // #endif
  if (width)
    *width = WIDTH;
  if (height)
    *height = HEIGHT;
}

void ega_clear() {
  // #ifndef VGA_TEXT_MODE
  //   if (vga_is_enabled()) {
  //     vga_clear_screen();
  //     return;
  //   }
  // #endif
  memset(upper_buffer, 0, STORED_PAGES * TOTAL_SIZE);
  memset(video_base, 0, TOTAL_SIZE);
}

void ega_new_line() {
  // #ifndef VGA_TEXT_MODE
  //   if (vga_is_enabled()) {
  //     vga_new_line();
  //     return;
  //   }
  // #endif
  pointer = video_base + ((pointer - video_base) / W2 + 1) * W2;
  ega_shift_one_line_up();
  ega_set_cursor_auto();
}

void ega_cartridge_return() {
  // #ifndef VGA_TEXT_MODE
  //   if (vga_is_enabled()) {
  //     vga_new_line();
  //     return;
  //   }
  // #endif
  pointer = video_base + ((pointer - video_base) / W2 - 1) * W2;
  ega_new_line();
  ega_shift_one_line_up();
  ega_set_cursor_auto();
}

void ega_shift_one_line_up(void) {
  if (pointer >= video_base + TOTAL_SIZE) {
    // Move the upper buffer up by one line.
    for (int row = 0; row < (STORED_PAGES * HEIGHT); ++row) {
      memcpy(upper_buffer + W2 * row, upper_buffer + W2 * (row + 1), 2 * WIDTH);
    }
    // Copy the first line on the screen inside the last line of the upper buffer.
    memcpy(upper_buffer + (TOTAL_SIZE * STORED_PAGES - W2), video_base,
           2 * WIDTH);
    // Move the screen up by one line.
    // for (int row = 0; row < HEIGHT; ++row) {
    //   memcpy(video_base + (W2 * row), video_base + (W2 * (row + 1)), 2 * WIDTH);
    // }
    for (int row = 1; row < HEIGHT; ++row) {
      memcpy(video_base + (W2 * (row - 1)), video_base + (W2 * row), 2 * WIDTH);
    }
    // Clear last row
    memset(video_base + (W2 * (HEIGHT - 1)), 0x0, 2 * WIDTH);
    // Update the pointer.
    pointer = video_base + ((pointer - video_base) / W2 - 1) * W2;
  }
}

void ega_shift_one_page_up() {
  if (scrolled_page > 0) {
    // Decrese the number of scrolled pages, and compute which page must be loaded.
    int page_to_load = (STORED_PAGES - (--scrolled_page));
    // If we have reached 0, restore the original page.
    if (scrolled_page == 0)
      memcpy(video_base, original_page, TOTAL_SIZE);
    else
      memcpy(video_base, upper_buffer + (page_to_load * TOTAL_SIZE),
             TOTAL_SIZE);
  }
}

void ega_shift_one_page_down(void) {
  if (scrolled_page < STORED_PAGES) {
    // Increase the number of scrolled pages, and compute which page must be loaded.
    int page_to_load = (STORED_PAGES - (++scrolled_page));
    // If we are loading the first history page, save the original.
    if (scrolled_page == 1)
      memcpy(original_page, video_base, TOTAL_SIZE);
    // Load the specific page.
    memcpy(video_base, upper_buffer + (page_to_load * TOTAL_SIZE), TOTAL_SIZE);
  }
}

#if 0
void ega_scroll_up()
{
    if (is_scrolled)
        return;
    char *ptr = memory = pointer;
    for (unsigned int it = 0; it < TOTAL_SIZE; it++) {
        downbuffer[y][x] = *ptr;
        *ptr++           = upbuffer[y][x];
    }

    is_scrolled = true;

    stored_x = ega_get_column();

    stored_y = ega_get_line();

    ega_move_cursor(width, height);
}

void ega_scroll_down()
{
    char *ptr = memory;

    // If PAGEUP hasn't been pressed, it's useless to go down, there is nothing.
    if (!is_scrolled) {
        return;
    }
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width * 2; x++) {
            *ptr++ = downbuffer[y][x];
        }
    }

    is_scrolled = false;

    ega_move_cursor(stored_x, stored_y);

    stored_x = 0;

    stored_y = 0;
}

#endif
