#include <kernel/drivers/video.h>
#include <kernel/drivers/video/ega.h>
#include <kernel/drivers/video/vga.h>
#include <kernel/drivers/video/fb.h>

#include <kernel/printf.h>

typedef void (*video_init_t)(boot_info_t *boot_info);
typedef void (*video_finalize_t)();
typedef void (*video_update_t)();
typedef void (*video_putc_t)(int c);
typedef void (*video_puts_t)(const char *str);
typedef void (*video_set_cursor_auto_t)();
typedef void (*video_move_cursor_t)(unsigned int x, unsigned int y);
typedef void (*video_get_cursor_position_t)(unsigned int *x, unsigned int *y);
typedef void (*video_get_screen_size_t)(unsigned int *width,
                                        unsigned int *height);
typedef void (*video_clear_t)();
typedef void (*video_new_line_t)();
typedef void (*video_cartridge_return_t)();
typedef void (*video_shift_one_line_up_t)();
typedef void (*video_shift_one_page_up_t)();
typedef void (*video_shift_one_page_down_t)();

typedef struct {
  bool is_init;

  /* VGA operations */
  video_init_t init;
  video_finalize_t finalize;
  video_update_t update;
  video_putc_t putc;
  video_puts_t puts;
  // video_set_cursor_auto_t set_cursor_auto;
  video_move_cursor_t move_cursor;
  video_get_cursor_position_t get_cursor_position;
  video_get_screen_size_t get_screen_size;
  video_clear_t clear;
  video_new_line_t new_line;
  // video_cartridge_return_t cartridge_return;
  // video_shift_one_line_up_t shift_one_line_up;
  // video_shift_one_page_up_t shift_one_page_up;
  // video_shift_one_page_down_t shift_one_page_down;

} video_driver_t;

void __init(boot_info_t *boot_info) {
}
void __finalize() {
}
void __update() {
}
void __putc(int c) {
}
void __puts(const char *str) {
}
void __set_cursor_auto() {
}
void __move_cursor(unsigned int x, unsigned int y) {
}
void __get_cursor_position(unsigned int *x, unsigned int *y) {
}
void __get_screen_size(unsigned int *width, unsigned int *height) {
}
void __clear() {
}
void __new_line() {
}
void __cartridge_return() {
}
void __shift_one_line_up() {
}
void __shift_one_page_up() {
}
void __shift_one_page_down() {
}

static video_driver_t *driver = NULL;

static video_driver_t driver_none = {
  .is_init             = false,
  .init                = __init,
  .finalize            = __finalize,
  .update              = __update,
  .putc                = __putc,
  .puts                = __puts,
  .move_cursor         = __move_cursor,
  .get_cursor_position = __get_cursor_position,
  .get_screen_size     = __get_screen_size,
  .clear               = __clear,
  .new_line            = __new_line,
};

static video_driver_t driver_ega = {
  .is_init             = false,
  .init                = ega_init,
  .finalize            = ega_finalize,
  .update              = ega_update,
  .putc                = ega_putc,
  .puts                = ega_puts,
  .move_cursor         = ega_move_cursor,
  .get_cursor_position = ega_get_cursor_position,
  .get_screen_size     = ega_get_screen_size,
  .clear               = ega_clear,
  .new_line            = ega_new_line,
};

static video_driver_t driver_vga = {
  .is_init             = false,
  .init                = vga_init,
  .finalize            = vga_finalize,
  .update              = vga_update,
  .putc                = vga_putc,
  .puts                = vga_puts,
  .move_cursor         = vga_move_cursor,
  .get_cursor_position = vga_get_cursor_position,
  .get_screen_size     = vga_get_screen_size,
  .clear               = vga_clear,
  .new_line            = vga_new_line,
};

static video_driver_t driver_fb = {
  .is_init             = false,
  .init                = fb_init,
  .finalize            = fb_finalize,
  .update              = fb_update,
  .putc                = fb_putc,
  .puts                = fb_puts,
  .move_cursor         = fb_move_cursor,
  .get_cursor_position = fb_get_cursor_position,
  .get_screen_size     = fb_get_screen_size,
  .clear               = fb_clear,
  .new_line            = fb_new_line,
};

void video_early_init() {
  // TODO: change to console writer
  driver = &driver_ega;
  driver->clear();
}

void video_init(boot_info_t *boot_info) {
  if (boot_info->video_type == VIDEO_TYPE_EGA_TEXT) {
    // EGA 80x25 text mode
    driver = &driver_ega;
  } else if (boot_info->video_type == VIDEO_TYPE_VGA) {
    // vga supported
    driver = &driver_vga;
  } else if (boot_info->video_type == VIDEO_TYPE_FRAMEBUFFER) {
    // framebuffer supported
    driver = &driver_fb;
  } else {
    // no video driver
    driver = &driver_none;
  }

  driver->init(boot_info);
}

void video_finalize() {
  driver->finalize();
}

void video_update() {
  driver->update();
}

void video_putc(int c) {
  driver->putc(c);
}

void video_puts(const char *str) {
  driver->puts(str);
}

void video_move_cursor(unsigned int x, unsigned int y) {
  driver->move_cursor(x, y);
}

void video_get_cursor_position(unsigned int *x, unsigned int *y) {
  driver->get_cursor_position(x, y);
}

void video_get_screen_size(unsigned int *width, unsigned int *height) {
  driver->get_screen_size(width, height);
}

void video_clear() {
  driver->clear();
}

void video_new_line() {
  driver->new_line();
}

// void video_cartridge_return() {
//   driver->video_cartridge_return();
// }

// void video_shift_one_line_up(void) {
//   driver->shift_one_line_up();
// }

// void video_shift_one_page_up() {
//   driver->shift_one_page_up();
// }

// void video_shift_one_page_down(void) {
//   driver->shift_one_page_down();
// }
