    section .bss

global boot_stack_top
boot_stack_bottom:
    resb 0x1000
boot_stack_top: