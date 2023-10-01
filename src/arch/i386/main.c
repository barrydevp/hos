#include <arch/i386/gdt.h>
#include <arch/i386/tss.h>
#include <arch/i386/irq.h>
#include <arch/i386/pic.h>
#include <arch/i386/pit.h>
#include <arch/i386/rtc.h>
#include <arch/i386/timer.h>
#include <arch/i386/cpu.h>
#include <arch/i386/serial.h>

#include <kernel/kernel.h>
#include <kernel/boot.h>
#include <kernel/multiboot.h>
#include <kernel/drivers/video.h>
#include <kernel/drivers/keyboard.h>
#include <kernel/memory/pmm.h>
#include <kernel/memory/vmm.h>
#include <kernel/memory/mmu.h>

#include <kernel/printf.h>

/// @brief Boot info provided to the kmain function.
static struct boot_info_t boot_info;

static void early_log_init(void) {
  serial_enable(SERIAL_PORT_A);
}

static inline void boot_init(uint32_t magic, uint32_t addr) {
  // Init multiboot structure
  multiboot_init(magic, addr);

  boot_info.magic            = magic;
  boot_info.multiboot_header = &mboot;

  // Get the physical addresses of where the kernel starts and ends.
  boot_info.bootloader_phy_start = BOOTLOADER_START;
  boot_info.bootloader_phy_end   = BOOTLOADER_END;

  // Get the addresses of kernel
  boot_info.kernel_phy_start = KERNEL_START - KERNEL_HIGHER_HALF;
  boot_info.kernel_phy_end   = KERNEL_END - KERNEL_HIGHER_HALF;
  boot_info.kernel_start     = KERNEL_START;
  boot_info.kernel_end       = KERNEL_END;
  boot_info.kernel_size      = KERNEL_END - KERNEL_START;

  // Get the maximum memory
  uint32_t highest_address  = 0;
  uint32_t addressable_size = 0;
  load_memory_info(&highest_address, &addressable_size);
  boot_info.highest_address  = highest_address;
  boot_info.addressable_size = addressable_size;

  // 128 KB reserve for frame bit map for 32 bit address space
  uint32_t reserved_size = 128 * KB;
  addressable_size -= reserved_size;

  // framebuffer region
  // TODO: move to multiboot.c function
  struct multiboot_tag_framebuffer *fb = mboot.multiboot_framebuffer;
  if (fb &&
      fb->common.framebuffer_type != MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT) {
    // framebuffer support
    boot_info.video_type                         = VIDEO_TYPE_FRAMEBUFFER;
    struct multiboot_tag_framebuffer_common *fbc = &fb->common;
    boot_info.video_phy_start                    = fbc->framebuffer_addr;
    boot_info.video_phy_end =
      fbc->framebuffer_addr +
      fbc->framebuffer_width * fbc->framebuffer_bpp / 8 +
      fbc->framebuffer_height * fbc->framebuffer_pitch;
  } else {
    // EGA text mode
    boot_info.video_type      = VIDEO_TYPE_EGA_TEXT;
    boot_info.video_phy_start = 0xB8000;
    boot_info.video_phy_end   = 0xB8000 + 80 * 25 * 2;
  }
  uint32_t fb_size = boot_info.video_phy_end - boot_info.video_phy_start;
  // framebuffer virtual region
  boot_info.video_start = FRAMEBUFFER_START;
  boot_info.video_end   = boot_info.video_start + fb_size;

  // Reserve space for the kernel stack at the end of lowmem.
  boot_info.stack_base = KERNEL_STACK_TOP;

  // Reserve space for kernel heap
  boot_info.heap_start = KERNEL_HEAP_START;
  boot_info.heap_end   = KERNEL_HEAP_END;
  uint32_t heap_size   = boot_info.heap_end - boot_info.heap_start;
  // if (heap_size > addressable_size) {
  //   dprintf("Low kernel heap memory:\n"
  //           " available: %uKB\n"
  //           " desired: %uKB\n",
  //           __ALIGN_DOWN(addressable_size, PAGE_SIZE) / KB, heap_size / KB);
  //   boot_info.heap_end =
  //     boot_info.heap_start + __ALIGN_DOWN(addressable_size, PAGE_SIZE);
  //   heap_size = boot_info.heap_end - boot_info.heap_start;
  // }

  dprintf("Memory summary: \n"
          " total: %uGB + %uMB + %uKB\n"
          " kernel: phy=0x%p->0x%p virt=0x%p->0x%p (%uKB)\n"
          " heap: virt=0x%p (%uMB)\n"
          " video: phy=0x%p virt=0x%p type=%u (%uKB)\n",
          highest_address / GB, (highest_address % GB) / MB,
          ((highest_address % GB) % MB) / KB, boot_info.kernel_phy_start,
          boot_info.kernel_phy_end, boot_info.kernel_start,
          boot_info.kernel_end, boot_info.kernel_size / KB,
          boot_info.heap_start, heap_size / MB, boot_info.video_phy_start,
          boot_info.video_start, (unsigned)boot_info.video_type, fb_size / KB);
}

int kenter(uint32_t magic, uint32_t addr) {
  // Early init
  early_log_init();
  video_early_init();
  // Dhisable the keyboard, otherwise the PS/2 initialization does not
  // work properly.
  keyboard_disable();

  // Setup boot_info
  boot_init(magic, addr);

  // Setup prerequisite hardware
  gdt_init();
  tss_init(5, 0x10);
  idt_init();
  pic_init();
  pit_init();

  // Memory management
  pmm_init(&boot_info);
  vmm_init(&boot_info);
  mmu_init(&boot_info);

  // Timing
  rtc_init();
  timer_init();

  // System call
  syscall_init();

  // Graphic
  video_init(&boot_info);

  // Kernel
  if (kinit(&boot_info)) {
    dprintf("Kernel init failed!\n");
    return 1;
  }

  /* give control to the real kernel main */
  return kmain();
}
