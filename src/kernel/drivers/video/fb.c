#include <kernel/multiboot2.h>
#include <kernel/printf.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/font.h>
#include <kernel/drivers/video/palette.h>

#include <arch/i386/ports.h>
#include <arch/i386/cpu.h>
#include <arch/i386/timer.h>
#include <kernel/types.h>
#include <kernel/string.h>
#include <kernel/math.h>

/// Counts the number of elements of an array.
#define COUNT_OF(x)                                                            \
  ((sizeof(x) / sizeof(0 [x])) / ((size_t)(!(sizeof(x) % sizeof(0 [x])))))

#define IS_MODE(_width, _height, _colors)                                      \
  if (width == _width && height == _height && colors == _colors)

/// Framebuffer driver details.
// https://wiki.osdev.org/Drawing_In_a_Linear_Framebuffer
typedef struct {
  uint32_t width;  ///< Screen's width.
  uint32_t height; ///< Screen's height.
  uint8_t bpp;     ///< Bits per pixel (bpp).
  uint32_t
    pitch; ///< how many bytes of VRAM you should skip to go one pixel down (width * bpp/8).
  uint8_t *address;   ///< Starting address of the screen.
  video_font_t *font; ///< The current font.
  palette_entry_t *palette;
  struct multiboot_tag_framebuffer *mfb;

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
static fb_driver_t *fb_driver = NULL;

static fb_driver_t fb_driver_640x480_16 = {
  .address      = NULL,
  .font         = &font_8x16,
  .palette      = rgb565_16_palette,
  .x            = 0,
  .y            = 0,
  .fg           = 0,
  .bg           = 0,
  .cursor_state = 0,
};

static fb_driver_t fb_driver_720x480_16 = {
  .address      = NULL,
  .font         = &font_8x16,
  .palette      = rgb565_16_palette,
  .x            = 0,
  .y            = 0,
  .fg           = 0,
  .bg           = 0,
  .cursor_state = 0,
};

int fb_is_enabled() {
  return fb_enable;
}

int fb_width() {
  if (fb_enable)
    return fb_driver->width;
  return 0;
}

int fb_height() {
  if (fb_enable)
    return fb_driver->height;
  return 0;
}

static inline uint32_t __get_color(palette_entry_t palette) {
  struct multiboot_tag_framebuffer *mfb = fb_driver->mfb;
  uint32_t color                        = 0x0;
  color |= (((1 << mfb->framebuffer_red_mask_size) - 1) & palette.red)
        << mfb->framebuffer_red_field_position;
  color |= (((1 << mfb->framebuffer_green_mask_size) - 1) & palette.green)
        << mfb->framebuffer_green_field_position;
  color |= (((1 << mfb->framebuffer_blue_mask_size) - 1) & palette.blue)
        << mfb->framebuffer_blue_field_position;
  return color;
}

static inline void __draw_pixel(int x, int y, uint32_t color) {
  switch (fb_driver->bpp) {
    case 8: {
      uint8_t *pixel = fb_driver->address + fb_driver->pitch * y + x;
      *pixel         = color;
    } break;
    case 15:
    case 16: {
      uint16_t *pixel =
        (uint16_t *)(fb_driver->address + fb_driver->pitch * y + 2 * x);
      *pixel = color;
    } break;
    case 24: {
      uint32_t *pixel =
        (uint32_t *)(fb_driver->address + fb_driver->pitch * y + 3 * x);
      *pixel = (color & 0xffffff) | (*pixel & 0xff000000);
    } break;
    case 32: {
      uint32_t *pixel =
        (uint32_t *)(fb_driver->address + fb_driver->pitch * y + 4 * x);
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
  const unsigned char *glyph =
    fb_driver->font->font + c * fb_driver->font->height;
  for (unsigned iy = 0; iy < fb_driver->font->height; ++iy) {
    for (unsigned ix = 0; ix < fb_driver->font->width; ++ix) {
      __draw_pixel(x + (fb_driver->font->width - ix), y + iy,
                   glyph[iy] & mask[ix] ? fg : bg);
    }
  }
}

void fb_draw_string(int x, int y, const char *str, uint32_t fg, uint32_t bg) {
  char i = 0;
  while (*str != '\0') {
    fb_draw_char(x + i * fb_driver->font->width, y, *str, fg, bg);
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
  for (unsigned cy = 0; cy < fb_driver->font->height; ++cy)
    for (unsigned cx = 0; cx < fb_driver->font->width; ++cx)
      fb_draw_pixel(fb_driver->x + cx, fb_driver->y + cy, 0);
}

inline static void __fb_draw_cursor() {
  uint32_t color =
    (fb_driver->cursor_state = (fb_driver->cursor_state == 0)) * fb_driver->fg;
  for (unsigned cy = 0; cy < fb_driver->font->height; ++cy)
    for (unsigned cx = 0; cx < fb_driver->font->width; ++cx)
      fb_draw_pixel(fb_driver->x + cx, fb_driver->y + cy, color);
}

void fb_putc(int c) {
  if (fb_driver->cursor_state)
    __fb_clear_cursor();
  // If the character is '\n' go the new line.
  if (c == '\n') {
    fb_new_line();
  } else if ((c >= 0x20) && (c <= 0x7E)) {
    fb_draw_char(fb_driver->x, fb_driver->y, c, fb_driver->fg, fb_driver->bg);
    if ((uint32_t)(fb_driver->x += fb_driver->font->width) >= fb_driver->width)
      fb_new_line();
  } else {
    return;
  }
}

void fb_puts(const char *str) {
  while ((*str) != 0) { fb_putc((*str++)); }
}

void fb_move_cursor(unsigned int x, unsigned int y) {
  fb_driver->x = x * fb_driver->font->width;
  fb_driver->y = y * fb_driver->font->height;
  __fb_draw_cursor();
}

void fb_get_cursor_position(unsigned int *x, unsigned int *y) {
  if (x)
    *x = fb_driver->x / fb_driver->font->width;
  if (y)
    *y = fb_driver->y / fb_driver->font->height;
}

void fb_get_screen_size(unsigned int *width, unsigned int *height) {
  if (width)
    *width = fb_driver->width / fb_driver->font->width;
  if (height)
    *height = fb_driver->height / fb_driver->font->height;
}

void fb_clear() {
  memset(fb_driver->address, fb_driver->bg,
         fb_driver->height * fb_driver->pitch);
  fb_driver->x = 0;
  fb_driver->y = 0;
}

inline unsigned int __line_height() {
  // Just the 5x6 font needs some space.
  const unsigned int vertical_space = (fb_driver->font == &font_5x6);
  const unsigned int line_height    = fb_driver->font->height + vertical_space;

  return line_height;
}

void fb_shift_one_line_up() {
  // Calculate number of lines
  const unsigned int line_height = __line_height();
  const unsigned int max_lines   = (fb_driver->height) / line_height;

  // Move the screen up by one line.
  for (uint32_t i = 0; i < max_lines - 1; ++i) {
    memcpy(fb_driver->address + (fb_driver->pitch * line_height * i),
           fb_driver->address + (fb_driver->pitch * line_height * (i + 1)),
           fb_driver->pitch * line_height);
  }

  // Blank the last line.
  memset(fb_driver->address + fb_driver->pitch * line_height * (max_lines - 1),
         fb_driver->bg,
         fb_driver->pitch *
           (fb_driver->height - (max_lines - 1) * line_height));

  // Shift the cursor one line
  fb_driver->y -= line_height;
}

void fb_new_line() {
  const unsigned int line_height = __line_height();

  // Go back at the beginning of the line.
  fb_driver->x = 0;
  fb_driver->y += line_height;
  if ((uint32_t)(fb_driver->y + line_height) > (fb_driver->height)) {
    /* fb_driver->y = 0; */
    /* fb_clear(); */
    fb_shift_one_line_up();
  }
}

void fb_update() {
  if ((timer_get_ticks() % (TICKS_PER_SECOND / 2)) == 0) {
    __fb_draw_cursor();
  }
}

void fb_set_color(uint32_t fg, uint32_t bg) {
  fb_driver->fg = fg;
  fb_driver->bg = bg;
}

void fb_init(boot_info_t *boot_info) {
  struct multiboot_tag_framebuffer *mfb =
    boot_info->multiboot_header->multiboot_framebuffer;

  // framebuffer un-supported
  if (!mfb) {
    // EGA 80x25 text mode
    dprintf("[WARN]: Unsupported EGA 80x25 text mode in framebuffer driver!\n");
    return;
  }

  if (mfb->common.framebuffer_type != MULTIBOOT_FRAMEBUFFER_TYPE_RGB) {
    dprintf("[WARN]: Unsupported non-RGB in framebuffer driver!\n");
    return;
  }

  struct multiboot_tag_framebuffer_common *fb = &mfb->common;

  if (fb->framebuffer_bpp == 16) {
    if (fb->framebuffer_width == 720 && fb->framebuffer_height == 480) {
      fb_driver      = &fb_driver_720x480_16;
      fb_driver->mfb = mfb;
      fb_driver->fg  = __get_color(fb_driver->palette[White_16]);
      fb_driver->bg  = __get_color(fb_driver->palette[Black_16]);
    } else if (fb->framebuffer_width == 640 && fb->framebuffer_height == 480) {
      fb_driver      = &fb_driver_640x480_16;
      fb_driver->mfb = mfb;
      /* dprintf(">>>>>>>>>>>>>>>>>> 0x%X\n", __get_color(fb_driver->palette[Blue_16_1])); */
      fb_driver->fg = __get_color(fb_driver->palette[White_16]);
      fb_driver->bg = __get_color(fb_driver->palette[Black_16]);
    }
  }

  if (!fb_driver) {
    dprintf("[WARN]: Unsupported framebuffer driver!\n");
    return;
  }

  // Set fb info
  fb_driver->width  = fb->framebuffer_width;
  fb_driver->height = fb->framebuffer_height;
  fb_driver->bpp    = fb->framebuffer_bpp;
  fb_driver->pitch  = fb->framebuffer_pitch;
  // Set the address.
  fb_driver->address = (uint8_t *)boot_info->video_start;
  // Set the state
  fb_driver->x            = 0;
  fb_driver->y            = 0;
  fb_driver->cursor_state = 0;
  // Clears the screen.
  fb_clear();
  // Set the Framebuffer as enabled.
  fb_enable = true;
}

void fb_finalize() {
  fb_enable = false;
}
