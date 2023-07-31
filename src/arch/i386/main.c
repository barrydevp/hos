#include <arch/i386/boot/gdt.h>
#include <kernel/kernel.h>

int kenter(void)
{
	gdt_init();

	/* give control to the real kernel main */
	return kmain();
}
