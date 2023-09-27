ENTRY (main)

SECTIONS
{
	. = 1M;

	_app_start = 3M;

	. += _app_start;
	/* Add a symbol that indicates the start address of the kernel. */
	.text ALIGN (4K) : AT (ADDR (.text) - _app_start)
	{
		*(.text)
	}
	.rodata ALIGN (4K) : AT (ADDR (.rodata) - _app_start)
	{
		*(.rodata .rodata.*)
	}
	.data ALIGN (4K) : AT (ADDR (.data) - _app_start)
	{
		*(.data)
		*(.symbols)
	}
	.bss ALIGN (4K) : AT (ADDR (.bss) - _app_start)
	{
		*(.bss .bss.*)
	}
}
