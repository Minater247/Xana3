section .multiboot

align 0x8
Multiboot2Header:
    dd 0xe85250d6
    dd 0
    dd Multiboot2HeaderEnd - Multiboot2Header
    dd -(0xe85250d6 + 0 + (Multiboot2HeaderEnd - Multiboot2Header))

    ; Framebuffer tag
    align 0x8
framebuffer_tag_start:
    dw 5 ; type
    dw 1 ; flags
    dd framebuffer_tag_end - framebuffer_tag_start
    dd 1920 ; width
    dd 1080 ; height
    dd 32 ; depth
framebuffer_tag_end:

    ; End tag
    align 0x8
    dw 0
    dw 0
    dd 0
Multiboot2HeaderEnd: