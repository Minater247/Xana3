ENTRY(_start)

BOOTSTRAP_OFF = 0;
BOOTSTRAP_BASE = 0x00200000;

SECTIONS
{

	. = BOOTSTRAP_BASE + BOOTSTRAP_OFF;
 
	/* Multiboot comes first for GRUB to find, then stack & code*/
	.text ALIGN (0x1000) : AT(ADDR(.text) - BOOTSTRAP_OFF)
	{
		*(.multiboot*)
		*(.text*)
	}
 
	/* Read-only data. */
	.rodata ALIGN (0x1000) : AT(ADDR(.rodata) - BOOTSTRAP_OFF)
	{
		*(.rodata*)
	}
 
	/* Read-write data (initialized) */
	.data ALIGN (0x1000) : AT(ADDR(.data) - BOOTSTRAP_OFF)
	{
		*(.data*)
	}
 
	/* Read-write data (uninitialized) and stack */
	.bss ALIGN (0x1000) : AT(ADDR(.bss) - BOOTSTRAP_OFF)
	{
		*(COMMON*)
		*(.bss*)
	}

	BOOTSTRAP_END = .;
}