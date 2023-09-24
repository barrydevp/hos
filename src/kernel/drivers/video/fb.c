#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/font.h>

#include <arch/i386/ports.h>
#include <arch/i386/cpu.h>
#include <kernel/types.h>
#include <kernel/string.h>
#include <kernel/math.h>

/// Counts the number of elements of an array.
#define COUNT_OF(x) \
  ((sizeof(x) / sizeof(0 [x])) / ((size_t)(!(sizeof(x) % sizeof(0 [x])))))

#define IS_MODE(_width, _height, _colors) \
  if (width == _width && height == _height && colors == _colors)

/// Framebuffer driver details.
typedef struct {
  uint32_t width; ///< Screen's width.
  uint32_t height; ///< Screen's height.
  uint8_t bpp; ///< Bits per pixel (bpp).
  uint32_t pitch; ///< Bits per pixel (bpp).
  uint8_t *address; ///< Starting address of the screen.
  video_font_t *font; ///< The current font.

  // state
  int x;
  int y;
  uint32_t fg;
  uint32_t bg;
  int cursor_state;

} fb_driver_t;

/// Is Framebuffer enabled.
static bool fb_enable = false;
/// A buffer for storing a copy of the video memory.
// char vidmem[262144];
/// Current driver.
static fb_driver_t *driver = NULL;

static fb_driver_t driver0 = {
  .width = 0,
  .height = 0,
  .bpp = 0,
  .pitch = 0,
  .address = NULL,
  .font = &font_8x16,
  .x = 0,
  .y = 0,
  .fg = 0,
  .bg = 0,
  .cursor_state = 0,
};

/// @brief Reads from the video memory.
/// @param offset where we are going to read.
/// @return the value we read.
static inline unsigned char __read_byte(unsigned int offset) {
  return (unsigned char)(*(driver->address + offset));
}

/// @brief Writes onto the video memory.
/// @param offset where we are going to write.
static inline void __write_byte(unsigned int offset, unsigned char value) {
  *(char *)(driver->address + offset) = value;
}

int fb_is_enabled() {
  return fb_enable;
}

int fb_width() {
  if (fb_enable)
    return driver->width;
  return 0;
}

int fb_height() {
  if (fb_enable)
    return driver->height;
  return 0;
}

static inline void __draw_pixel(int x, int y, uint32_t color) {
  switch (driver->bpp) {
    case 8: {
      uint8_t *pixel = driver->address + driver->pitch * y + x;
      *pixel = color;
    } break;
    case 15:
    case 16: {
      uint16_t *pixel = driver->address + driver->pitch * y + 2 * x;
      *pixel = color;
    } break;
    case 24: {
      uint32_t *pixel = driver->address + driver->pitch * y + 3 * x;
      *pixel = (color & 0xffffff) | (*pixel & 0xff000000);
    } break;
    case 32: {
      uint32_t *pixel = driver->address + driver->pitch * y + 4 * x;
      *pixel = color;
    } break;
  }
}

void fb_draw_pixel(int x, int y, uint32_t color) {
  __draw_pixel(x, y, color);
}

/// TODO: move to font writer
void fb_draw_char(int x, int y, unsigned char c, uint32_t fg, uint32_t bg) {
  static unsigned mask[] = {
    1u << 0u, //            1
    1u << 1u, //            2
    1u << 2u, //            4
    1u << 3u, //            8
    1u << 4u, //           16
    1u << 5u, //           32
    1u << 6u, //           64
    1u << 7u, //          128
    1u << 8u, //          256
  };
  const unsigned char *glyph = driver->font->font + c * driver->font->height;
  for (unsigned iy = 0; iy < driver->font->height; ++iy) {
    for (unsigned ix = 0; ix < driver->font->width; ++ix) {
      __draw_pixel(x + (driver->font->width - ix), y + iy,
                   glyph[iy] & mask[ix] ? fg : bg);
    }
  }
}

void fb_draw_string(int x, int y, const char *str, uint32_t fg, uint32_t bg) {
  char i = 0;
  while (*str != '\0') {
    fb_draw_char(x + i * 8, y, *str, fg, bg);
    str++;
    i++;
  }
}

void fb_draw_line(int x0, int y0, int x1, int y1, unsigned char color) {
  int dx = abs(x1 - x0), sx = sign(x1 - x0);
  int dy = abs(y1 - y0), sy = sign(y1 - y0);
  int err = (dx > dy ? dx : -dy) / 2;
  while (true) {
    fb_draw_pixel(x0, y0, color);
    if ((x0 == x1) && (y0 == y1))
      break;
    if (dx > dy) {
      x0 += sx;
      err -= dy;
      if (err < 0)
        err += dx;
      y0 += sy;
    } else {
      y0 += sy;
      err -= dx;
      if (err < 0)
        err += dy;
      x0 += sx;
    }
  }
}

void fb_draw_rectangle(int sx, int sy, int w, int h, unsigned char color) {
  fb_draw_line(sx, sy, sx + w, sy, color);
  fb_draw_line(sx, sy, sx, sy + h, color);
  fb_draw_line(sx, sy + h, sx + w, sy + h, color);
  fb_draw_line(sx + w, sy, sx + w, sy + h, color);
}

void fb_draw_circle(int xc, int yc, int r, unsigned char color) {
  int x = 0;
  int y = r;
  int p = 3 - 2 * r;
  if (!r)
    return;
  while (y >= x) // only formulate 1/8 of circle
  {
    fb_draw_pixel(xc - x, yc - y, color); //upper left left
    fb_draw_pixel(xc - y, yc - x, color); //upper upper left
    fb_draw_pixel(xc + y, yc - x, color); //upper upper right
    fb_draw_pixel(xc + x, yc - y, color); //upper right right
    fb_draw_pixel(xc - x, yc + y, color); //lower left left
    fb_draw_pixel(xc - y, yc + x, color); //lower lower left
    fb_draw_pixel(xc + y, yc + x, color); //lower lower right
    fb_draw_pixel(xc + x, yc + y, color); //lower right right
    if (p < 0)
      p += 4 * x++ + 6;
    else
      p += 4 * (x++ - y--) + 10;
  }
}

void fb_draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3,
                      unsigned char color) {
  fb_draw_line(x1, y1, x2, y2, color);
  fb_draw_line(x2, y2, x3, y3, color);
  fb_draw_line(x3, y3, x1, y1, color);
}

inline static void __fb_clear_cursor() {
  for (unsigned cy = 0; cy < driver->font->height; ++cy)
    for (unsigned cx = 0; cx < driver->font->width; ++cx)
      fb_draw_pixel(driver->x + cx, driver->y + cy, 0);
}

inline static void __fb_draw_cursor() {
  uint32_t color =
    (driver->cursor_state = (driver->cursor_state == 0)) * driver->fg;
  for (unsigned cy = 0; cy < driver->font->height; ++cy)
    for (unsigned cx = 0; cx < driver->font->width; ++cx)
      fb_draw_pixel(driver->x + cx, driver->y + cy, color);
}

void fb_putc(int c) {
  if (driver->cursor_state)
    __fb_clear_cursor();
  // If the character is '\n' go the new line.
  if (c == '\n') {
    fb_new_line();
  } else if ((c >= 0x20) && (c <= 0x7E)) {
    fb_draw_char(driver->x, driver->y, c, driver->fg, driver->bg);
    if ((uint32_t)(driver->x += driver->font->width) >= driver->width)
      fb_new_line();
  } else {
    return;
  }
}

void fb_puts(const char *str) {
  while ((*str) != 0) {
    fb_putc((*str++));
  }
}

void fb_move_cursor(unsigned int x, unsigned int y) {
  driver->x = x * driver->font->width;
  driver->y = y * driver->font->height;
  __fb_draw_cursor();
}

void fb_get_cursor_position(unsigned int *x, unsigned int *y) {
  if (x)
    *x = driver->x / driver->font->width;
  if (y)
    *y = driver->y / driver->font->height;
}

void fb_get_screen_size(unsigned int *width, unsigned int *height) {
  if (width)
    *width = driver->width / driver->font->width;
  if (height)
    *height = driver->height / driver->font->height;
}

void fb_clear() {
  memset(driver->address, 0, 64 * 1024);
  driver->x = 0;
  driver->y = 0;
}

void fb_new_line() {
  // Just the 5x6 font needs some space.
  const unsigned int vertical_space = (driver->font == &font_5x6);
  // Go back at the beginning of the line.
  driver->x = 0;
  if ((uint32_t)(driver->y += driver->font->height + vertical_space) >=
      (driver->height - driver->font->height)) {
    driver->y = 0;
    fb_clear();
  }
}

void fb_update() {
  // if ((timer_get_ticks() % (TICKS_PER_SECOND / 2)) == 0) {
  //   __fb_draw_cursor();
  // }
}

void fb_set_color(uint32_t fg, uint32_t bg) {
  driver->fg = fg;
  driver->bg = bg;
}

void fb_init(boot_info_t *boot_info) {
  struct multiboot_tag_framebuffer *mfb =
    boot_info->multiboot_header->multiboot_framebuffer;

  // framebuffer un-supported
  if (!mfb) {
    // EGA 80x25 text mode
    return;
  }

  driver = &driver0;

  // Set fb info
  driver->width = mfb->common.framebuffer_width;
  driver->height = mfb->common.framebuffer_height;
  driver->bpp = mfb->common.framebuffer_bpp;
  driver->pitch = mfb->common.framebuffer_pitch;
  // Set the address.
  driver->address = (uint8_t *)boot_info->video_start;
  // Set the state
  driver->x = 0;
  driver->y = 0;
  driver->fg = 0xffffffff;
  driver->bg = 0x0;
  driver->cursor_state = 0;
  // Clears the screen.
  fb_clear();
  // Set the Framebuffer as enabled.
  fb_enable = true;
}

void fb_finalize() {
  fb_enable = false;
}
