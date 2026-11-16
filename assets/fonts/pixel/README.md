# Letras pixel de JIANT BYTE

`fixed_5x8.bdf`, `fixed_7x14.bdf`, `fixed_10x20.bdf`: las fuentes "misc-fixed" de X11, de dominio público
("Public domain font. Share and enjoy."). `tools/aa_raster.py raster_bdf` las convierte, sin suavizado, a las
tablas que dibuja `firmware/src/gfx.c` (preset `byte-pixel` de `tools/gen_aa_font.py`).
