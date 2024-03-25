ENTRY(__init)

KERNEL_OFF = 0xffffff8000000000;
KERNEL_BASE = 0x00800000;

SECTIONS
{

	/* Begin putting sections at 1 MiB, a conventional place for kernels to be
	   loaded at by the bootloader, and add C0000000 (3GB) for paging. */
	. = KERNEL_BASE + KERNEL_OFF;
 
	/* Multiboot comes first for GRUB to find, then stack & code*/
	.text ALIGN (0x1000) : AT(ADDR(.text) - KERNEL_OFF)
	{
		*(.multiboot*)
		*(.text*)
	}
 
	/* Read-only data. */
	.rodata ALIGN (0x1000) : AT(ADDR(.rodata) - KERNEL_OFF)
	{
		*(.rodata*)
	}
 
	/* Read-write data (initialized) */
	.data ALIGN (0x1000) : AT(ADDR(.data) - KERNEL_OFF)
	{
		*(.data*)
	}
 
	/* Read-write data (uninitialized) and stack */
	.bss ALIGN (0x1000) : AT(ADDR(.bss) - KERNEL_OFF)
	{
		*(COMMON*)
		*(.bss*)
	}

	KERNEL_END = .;
}