ENTRY (_start)

SECTIONS
{
	. = 1M;
	/* The kernel will live at 3GB + 1MB in the virtual address space, */
	/* which will be mapped to 1MB in the physical address space. */
	/* Note that we page-align the sections. */

	_bootloader_start = .;

	.multiboot.data : {
		*(.multiboot.data)
	}

	.multiboot.text : {
		*(.multiboot.text)
	}


	/* Add a symbol that indicates the end address of the bootloader. */
	_bootloader_end = .;

	_kernel_higher_half = 0xC0000000;

	. += _kernel_higher_half;
	_kernel_start = .;

	/* Add a symbol that indicates the start address of the kernel. */
	.text ALIGN (4K) : AT (ADDR (.text) - _kernel_higher_half)
	{
		_kernel_text_start = .;
		*(.text)
		_kernel_text_end = .;
	}
	.rodata ALIGN (4K) : AT (ADDR (.rodata) - _kernel_higher_half)
	{
		*(.rodata .rodata.*)
	}
	.data ALIGN (4K) : AT (ADDR (.data) - _kernel_higher_half)
	{
		_kernel_data_start = .;
		*(.data)
		/* *(.symbols) */
		/* PROVIDE(_kernel_symbols_start = .); */
		/* PROVIDE(_kernel_symbols_end = .); */
		_kernel_data_end = .;
		PROVIDE(bss_start = .);
	}
	.bss ALIGN (4K) : AT (ADDR (.bss) - _kernel_higher_half)
	{
		*(COMMON)
		*(.bss .bss.*)
		*(.kernel_stack)
	}
	/* Add a symbol that indicates the end address of the kernel. */
	_kernel_end = .;
}
