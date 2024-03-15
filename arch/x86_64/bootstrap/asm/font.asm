    [bits 32]

    section .data

global font_data_start

font_data_start:
    incbin "font/zap-vga09.psf"
font_data_end: