#include <kernel/drivers/vga.h>
#include <kernel/drivers/vga/ega.h>
#include <kernel/drivers/vga/fb.h>

#include <kernel/printf.h>

typedef void (*vga_init_t)(boot_info_t *boot_info);
typedef void (*vga_finalize_t)();
typedef void (*vga_update_t)();
typedef void (*vga_putc_t)(int c);
typedef void (*vga_puts_t)(const char *str);
typedef void (*vga_set_cursor_auto_t)();
typedef void (*vga_move_cursor_t)(unsigned int x, unsigned int y);
typedef void (*vga_get_cursor_position_t)(unsigned int *x, unsigned int *y);
typedef void (*vga_get_screen_size_t)(unsigned int *width,
                                      unsigned int *height);
typedef void (*vga_clear_t)();
typedef void (*vga_new_line_t)();
typedef void (*vga_cartridge_return_t)();
typedef void (*vga_shift_one_line_up_t)();
typedef void (*vga_shift_one_page_up_t)();
typedef void (*vga_shift_one_page_down_t)();

typedef struct {
  bool is_init;

  /* VGA operations */
  vga_init_t init;
  vga_finalize_t finalize;
  vga_update_t update;
  vga_putc_t putc;
  vga_puts_t puts;
  // vga_set_cursor_auto_t set_cursor_auto;
  vga_move_cursor_t move_cursor;
  vga_get_cursor_position_t get_cursor_position;
  vga_get_screen_size_t get_screen_size;
  vga_clear_t clear;
  vga_new_line_t new_line;
  // vga_cartridge_return_t cartridge_return;
  // vga_shift_one_line_up_t shift_one_line_up;
  // vga_shift_one_page_up_t shift_one_page_up;
  // vga_shift_one_page_down_t shift_one_page_down;

} vga_driver_t;

static vga_driver_t *driver = NULL;

static vga_driver_t driver_ega = {
  .is_init = false,
  .init = ega_init,
  .finalize = ega_finalize,
  .update = ega_update,
  .putc = ega_putc,
  .puts = ega_puts,
  .move_cursor = ega_move_cursor,
  .get_cursor_position = ega_get_cursor_position,
  .get_screen_size = ega_get_screen_size,
  .clear = ega_clear,
  .new_line = ega_new_line,
};

static vga_driver_t driver_fb = {
  .is_init = false,
  .init = fb_init,
  .finalize = fb_finalize,
  .update = fb_update,
  .putc = fb_putc,
  .puts = fb_puts,
  .move_cursor = fb_move_cursor,
  .get_cursor_position = fb_get_cursor_position,
  .get_screen_size = fb_get_screen_size,
  .clear = fb_clear,
  .new_line = fb_new_line,
};

void vga_early_init() {
  // TODO: change to console writer
  driver = &driver_ega;
  driver->clear();
}

void vga_init(boot_info_t *boot_info) {
  if (boot_info->fb_type == FRAMEBUFFER_TYPE_EGA_TEXT) {
    dprintf("here--->");
    // EGA 80x25 text mode
    driver = &driver_ega;
  } else {
    // framebuffer supported
    driver = &driver_fb;
  }

  driver->init(boot_info);
}

void vga_finalize() {
  driver->finalize();
}

void vga_update() {
  driver->update();
}

void vga_putc(int c) {
  driver->putc(c);
}

void vga_puts(const char *str) {
  driver->puts(str);
}

void vga_move_cursor(unsigned int x, unsigned int y) {
  driver->move_cursor(x, y);
}

void vga_get_cursor_position(unsigned int *x, unsigned int *y) {
  driver->get_cursor_position(x, y);
}

void vga_get_screen_size(unsigned int *width, unsigned int *height) {
  driver->get_screen_size(width, height);
}

void vga_clear() {
  driver->clear();
}

void vga_new_line() {
  driver->new_line();
}

// void vga_cartridge_return() {
//   driver->vga_cartridge_return();
// }

// void vga_shift_one_line_up(void) {
//   driver->shift_one_line_up();
// }

// void vga_shift_one_page_up() {
//   driver->shift_one_page_up();
// }

// void vga_shift_one_page_down(void) {
//   driver->shift_one_page_down();
// }
