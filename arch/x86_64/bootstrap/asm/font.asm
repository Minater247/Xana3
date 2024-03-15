    [bits 32]

    section .data

    global _binary_font_psf_start
    global _binary_font_psf_end

    ; Include binary file ../font/zap-vga09.psf
_binary_font_psf_start:
    incbin "../font/zap-vga09.psf"
_binary_font_psf_end: