#include <stdio.h>
#include <kernel/memory/pmm.h>

#define PRINTN(n) printf("0x%08x\n", n);

#define alloc(i)                         \
  int32_t f##i = pmm_first_free_frame(); \
  pmm_frame_set((uint32_t)f##i);         \
  printf("f" #i ": %d\n", f##i);

#define allocn(i, n)                          \
  int32_t fn_##i = pmm_first_nfree_frames(n); \
  for (uint32_t j = 0; j < n; j++) {          \
    pmm_frame_set(fn_##i + j);                \
  }                                           \
  printf("fn" #i "_" #n ": %d\n", fn_##i);

void printBinary(uint32_t num) {
  for (int i = 31; i >= 0; i--) {
    uint32_t mask = 1 << i;
    printf("%u", (num & mask) ? 1 : 0);

    if (i % 4 == 0) {
      printf(" ");
    }
  }
  printf("\n");
}

void printFrames(uint32_t *frames, uint32_t n) {
  printf("---FRAME_BIT_MAP---\n");
  for (uint32_t i = 0; i < n; i++) {
    // printf(" " BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(frames[i]));
    printBinary(frames[i]);
  }
  printf("-------------------\n");
}

void *_kernel_boot = 0x00100000;
void *_kernel_higher_half = 0xc0000000;
void *_kernel_start = 0xC0100000;
void *_kernel_text_start = 0xC0100000;
void *_kernel_text_end = 0xC0100000;
void *_kernel_data_start = 0xC0100000;
void *_kernel_data_end = 0xC0100000;
void *_kernel_end = 0xC0100000 + 0x00843244;


// uint32_t kernel_boot = 0x00100000;
// uint32_t kernel_higher_half = 0xc0000000;
// uint32_t kernel_start = 0xC0100000;
// uint32_t kernel_text_start = 0xC0100000;
// uint32_t kernel_text_end = 0xC0100000;
// uint32_t kernel_data_start = 0xC0100000;
// uint32_t kernel_data_end = 0xC0100000;
// uint32_t kernel_end = 0xC0100000 + 0x00843244;
//
// void *_kernel_boot = &kernel_boot;
// void *_kernel_higher_half = &kernel_higher_half;
// void *_kernel_start = &kernel_start;
// void *_kernel_text_start = &kernel_text_start;
// void *_kernel_text_end = &kernel_text_end;
// void *_kernel_data_start = &kernel_data_start;
// void *_kernel_data_end = &kernel_data_end;
// void *_kernel_end = &kernel_end;

int main() {
  // uint32_t frames[10] = { 0 };
  // frames[0] = 0xFFFFEFF;
  // frames[1] = 0xFFFFEFE;

  // pmm_init_test(frames, 10);
  // printFrames(frames, 10);

  // int32_t f1 = pmm_first_free_frame();
  // printf("f1: %d", f1);
  //
  // int32_t fn_10 = pmm_first_nfree_frames(10);
  // printf("fn_10: %d", fn_10);

  // alloc(0); // 0
  // alloc(1); // 1 -> 0
  // allocn(2, 5); // 2 -> 0

  // alloc(3); // 34 -> 1
  // alloc(4); // 35 -> 1
  // alloc(5); // 36 -> 1
  // alloc(6); // 37 -> 1
  //
  // allocn(7, 32); // 38 ->

  // printFrames(frames, 10);

  // for (int i = 0; i < 60; ++i) {
  //   alloc(i);
  // }

  // printf("0x%08x\n", frames[1]);

  uint32_t memsize = 0xFFFFFFFF;
  uint32_t max_frames = memsize >> FRAME_SHIFT;
  uint32_t frames_bitmap_size = FRAME_INDEX(max_frames) * 4;
  PRINTN(frames_bitmap_size);
  // we want frames_bitmap frame(page) aligned, bitmap fully fit into n frames(pages),
  // in another words, n frames(pages) contain only bitmap data
  frames_bitmap_size = FRAME_ALIGN(frames_bitmap_size);
  /* Allocate bitmap from KERNEL_END (KERNEL_HEAP_STRART?) */
  uint32_t *frames_bitmap = (uint32_t *)FRAME_ALIGN(KERNEL_END);

  PRINTN((uintptr_t)(KERNEL_START - KERNEL_HIGHER_HALF));
  PRINTN(KERNEL_END - KERNEL_START);
  // frames_bitmap region
  PRINTN((uintptr_t)(KERNEL_END - KERNEL_HIGHER_HALF));
  PRINTN((uint32_t)frames_bitmap - KERNEL_END + frames_bitmap_size);
  PRINTN(frames_bitmap_size);

  return 0;
}
