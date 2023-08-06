#include <kernel/drivers/vga.h>
#include <arch/i386/ports.h>
#include <arch/i386/cpu.h>
#include <kernel/string.h>

// misc
#define VIDEO_INDEX(x, y) (x) * 2 + (y)*VGA_COLS * 2

// keep track of cursor positions
static uint8_t x_pos = 0, y_pos = 0;
static uint8_t screen_color = 0;
static uint8_t saved_x_pos = 0;

void vga_clr(const uint8_t c) {
  uint8_t *video = (unsigned char *)VGA_MEMORY;

  // clear out each index with our character and attribute byte
  for (uint32_t i = 0; i < VGA_COLS * VGA_ROWS * 2; i += 2) {
    video[i] = c;
    video[i + 1] = screen_color;
  }

  // reset cursor
  x_pos = 0;
  y_pos = 0;

  vga_set_cursor(x_pos, y_pos);
}

void vga_scroll() {
  uint8_t *video = (unsigned char *)VGA_MEMORY;

  // move VGA_COLS up
  for (uint32_t i = 1; i < VGA_ROWS; i++) {
    for (uint32_t j = 0; j < VGA_COLS; j++) {
      video[VIDEO_INDEX(j, i - 1)] = video[VIDEO_INDEX(j, i)];
      video[VIDEO_INDEX(j, i - 1) + 1] = video[VIDEO_INDEX(j, i) + 1];
    }
  }

  // now clear the last line
  for (uint32_t i = 0; i < VGA_COLS; i++) {
    video[VIDEO_INDEX(i, VGA_ROWS - 1)] = VGA_BLANK_CHAR;
    video[VIDEO_INDEX(i, VGA_ROWS - 1) + 1] = screen_color;
  }

  // FIXME: if y_pos > VGA_ROWS, what now?
  y_pos = y_pos - 1;
}

/** adjust the position when it go beyond the frame */
inline void vga_adjust_position() {
  if (x_pos >= VGA_COLS) {
    x_pos = 0;
    y_pos++;
  }

  if (y_pos >= VGA_ROWS) {
    vga_scroll();
  }
}

void vga_putc(const char c) {
  if (c == '\0') {
    return;
  }

  // encounter newline, move to update cursor
  if (c == '\n' || c == '\r') {
    vga_newline();
  } else if (c == '\t') {
    vga_tab();
  } else if (c == '\b') {
    vga_backspace();
  } else {
    // offset to the correct memory address and
    // set the character and the attribute byte
    uint8_t *video = (unsigned char *)VGA_MEMORY;
    video[VIDEO_INDEX(x_pos, y_pos)] = (uint8_t)c;
    video[VIDEO_INDEX(x_pos, y_pos) + 1] = screen_color;

    x_pos++;
    vga_adjust_position();
  }

  vga_set_cursor(x_pos, y_pos);
}

inline void vga_newline() {
  uint8_t *video = (unsigned char *)VGA_MEMORY;

  // fill blank to the end of line
  for (uint32_t i = 0; i < VGA_COLS - x_pos; i++) {
    video[VIDEO_INDEX(x_pos + i, y_pos)] = VGA_BLANK_CHAR;
    video[VIDEO_INDEX(x_pos + i, y_pos) + 1] = screen_color;
  }

  // reset cursor to next line
  x_pos = 0;
  y_pos++;
  vga_adjust_position();
}

inline void vga_tab() {
  uint8_t *video = (unsigned char *)VGA_MEMORY;

  // if the middle of a tab reach to the end of line => need a newline
  if ((VGA_COLS <= x_pos + VGA_TAB_SIZE) && (VGA_ROWS <= y_pos + 1)) {
    // save x_pos because vga_newline() may modify it
    uint8_t prv_x = x_pos;

    // the start part of a tab
    for (uint8_t i = 0; i < VGA_COLS - prv_x; i += 1) {
      video[VIDEO_INDEX(x_pos + i, y_pos)] = VGA_BLANK_CHAR;
      video[VIDEO_INDEX(x_pos + i, y_pos) + 1] = screen_color;
    }

    x_pos = 0;
    y_pos++;
    vga_adjust_position();

    // the end part of a tab
    for (uint8_t i = 0; i < VGA_TAB_SIZE - VGA_COLS + prv_x; i += 1) {
      video[VIDEO_INDEX(i, y_pos)] = VGA_BLANK_CHAR;
      video[VIDEO_INDEX(i, y_pos) + 1] = screen_color;
    }

    x_pos = VGA_TAB_SIZE - VGA_COLS + prv_x;
    y_pos++;
  } else {
    // otherwise just go ahead filling a tab
    for (uint8_t i = 0; i < VGA_TAB_SIZE; i += 1) {
      video[VIDEO_INDEX(x_pos + i, y_pos)] = VGA_BLANK_CHAR;
      video[VIDEO_INDEX(x_pos + i, y_pos) + 1] = screen_color;
    }

    x_pos += VGA_TAB_SIZE;
    vga_adjust_position();
  }
}

inline void vga_backspace() {
  uint8_t x = vga_get_x();
  uint8_t y = vga_get_y();

  //go to previous line
  if (x == 0) {
    /*
		uint8_t y = vga_get_y();
		if(y == 0)
		{
			//do nothing if we are at top
		}
		else
		{
			//go to previous last line and find the a character that is not a space
			x = VGA_COLS - 1;
			y--;
    			uint8_t* video = (uint8_t*)VGA_MEMORY;
	    		video += x * 2 + y * VGA_COLS * 2;
			while(x > 0 && *video == ' ')
			{
				x--;
				video -= 2;
			}
			x++;

			vga_go_to_pixel(x, y);
		}
		*/
  }
  //move back a space
  else {
    vga_goto_xy((uint8_t)((int)x - 1), y);
  }

  //get updated values
  x = vga_get_x();
  y = vga_get_y();

  //print a space at the current location
  uint8_t *video = (unsigned char *)VGA_MEMORY;
  video += x * 2 + y * VGA_COLS * 2;
  *video++ = ' ';
  *video = screen_color;
}

void vga_puts(char *str) {
  for (char *c = str; *c != '\0'; c++) {
    vga_putc(*c);
  }
}

// int vga_printf(const char *str, ...) {
//   va_list list;
//   va_start(list, str);
//
//   for (size_t i = 0; i < strlen(str); i++) {
//     switch (str[i]) {
//       case '%': {
//         switch (str[i + 1]) {
//           case 'c': {
//             char c = va_arg(list, char);
//             vga_putc(c);
//             i++;
//             break;
//           }
//           case 's': {
//             char *input = va_arg(list, char *);
//             vga_puts(input);
//             i++;
//             break;
//           }
//           case 'd':
//           case 'i': {
//             int32_t integer = va_arg(list, int32_t);
//             char buf[32] = { 0 };
//             itoa(integer, buf, 10);
//             vga_puts(buf);
//             i++;
//             break;
//           }
//           case 'x':
//           case 'X': {
//             uint32_t integer = va_arg(list, uint32_t);
//             char buf[32] = { 0 };
//             itoa_u(integer, buf, 16);
//             vga_puts(buf);
//             i++;
//             break;
//           }
//           default: {
//             vga_putc(str[i]);
//             i++;
//             break;
//           }
//         }
//         break;
//       }
//       default: {
//         vga_putc(str[i]);
//         break;
//       }
//     }
//   }
//
//   va_end(list);
//   return 1;
// }

void vga_set_color(const uint8_t color) {
  screen_color = color;
}

void vga_goto_xy(const uint8_t x, const uint8_t y) {
  //check out of bounds
  if (x >= VGA_COLS || y >= VGA_ROWS) {
    return;
  }

  saved_x_pos = 0;
  x_pos = x;
  y_pos = y;
  vga_set_cursor(x_pos, y_pos);
}

uint8_t vga_get_x() {
  return x_pos;
}

uint8_t vga_get_y() {
  return y_pos;
}

void vga_set_cursor(const uint8_t x, const uint8_t y) {
  uint16_t location =
    (uint16_t)((uint32_t)VGA_COLS * (uint32_t)y + (uint32_t)x);

  // disable_interrupts();

  /** The Index Register is mapped to ports 0x3D5 or 0x3B5.
    * The Data Register is mapped to ports 0x3D4 or 0x3B4. 
    */

  /** write high byte */
  outportb(0x3D4, 14); // 14 = 0xE = CRT_Cursor_Location_High
  outportb(0x3D5, (uint8_t)(location >> 8));

  /** write low byte */
  outportb(0x3D4, 15); // 15 = oxF = CRT_Cursor_Location_Low
  outportb(0x3D5, (uint8_t)(location & 0x00FF));

  // enable_interrupts();
}

void vga_init(void) {
  vga_clr(0);
}
