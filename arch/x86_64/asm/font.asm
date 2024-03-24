    [bits 64]

    section .data

global font_data_start

font_data_start:
    incbin "arch/x86_64/bootstrap/font/zap-vga09.psf"
font_data_end: