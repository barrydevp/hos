/* This will force us to create a kernel entry function instead of jumping to
 * kernel.c:0x00 */
void dummy_test_entrypoint() {
}

int kmain() {
  // char *video_memory = (char *)0xb8000;
  char *video_memory = (char *)0xC03FF000;
  *video_memory = 'H';

  for (;;) {
  }

  return 0;
}
