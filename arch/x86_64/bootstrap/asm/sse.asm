[bits 32]

global enable_sse
enable_sse:
    mov eax, cr0
    and ax, 0xFFFB
    or ax, 2
    mov cr0, eax
    mov eax, cr4
    or ax, 0x600
    mov cr4, eax
    ret