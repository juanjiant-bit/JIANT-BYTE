# JIANT BYTE: seres y animaciones de motores (maquetas). Usa los helpers de mock.py
import math, sys, os
sys.argv = [sys.argv[0], sys.argv[1]]
OUT = sys.argv[1]
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'mock.py')).read().split('shots = [')[0])

BYTEF = [lambda t: t * (42 & t >> 10), lambda t: t * (t >> 5 | t >> 8), lambda t: (t >> 4) * (13 & 0x8898A989 >> (t >> 11 & 30)),
         lambda t: t * (t >> 11 & t >> 8 & 123 & t >> 3), lambda t: (t * 9 & t >> 4) | (t * 5 & t >> 7) | (t * 3 & t >> 10),
         lambda t: t * ((t >> 9 | t >> 13) & 25 & t >> 6), lambda t: (t & t >> 8) * (t >> 4), lambda t: t ^ t >> 3 ^ t >> 7,
         lambda t: t * (t >> 3 & 7) | t >> 6, lambda t: (t >> 6 | t | t >> (t >> 16)) * 10 + ((t >> 11) & 7),
         lambda t: t * 5 & t >> 7 | t * 3 & t >> 10, lambda t: (t * (t >> 12 | t >> 8) & 63 & t >> 4)]
NAMES = ['GLOTO', 'PIXA', 'TUNA', 'BRAK', 'TRIPO', 'ZUMB', 'MOKO', 'XORI', 'RATA', 'OBO', 'KIRI', 'NIBL']

def sprite(f, frame, n=11):
    """el ser BYTE: una media grilla de bits de f, espejada; frame mueve t (camina)"""
    h = n
    half = (n + 1) // 2
    g = [[0] * n for _ in range(h)]
    for y in range(h):
        v = f(y * 1237 + 4096 + frame * 3) & 0xFFFFFFFF
        v ^= v >> 7
        for x in range(half):
            b = (v >> (x * 2 + (y & 1))) & 1
            # forma de cuerpo: mas lleno en el centro
            cx = half - 1 - x
            if (x == half - 1 and 2 <= y <= h - 3) or (b and abs(y - h / 2) + cx * 0.7 < h / 2 + 1):
                g[y][x] = g[y][n - 1 - x] = 1
    # ojos: dos huecos fijos, patas: la ultima fila alterna con el frame
    ey = h // 3
    for ex in (half - 3, n - half + 2):
        if 0 <= ex < n:
            g[ey][ex] = 2
    for x in range(n):
        if g[h - 2][x]:
            g[h - 1][x] = 1 if (x + frame) % 3 == 0 else 0
    return g

def draw_sprite(im, g, x0, y0, s, c, eye=(255, 255, 255)):
    d = ImageDraw.Draw(im)
    for y, row in enumerate(g):
        for x, v in enumerate(row):
            if v:
                col = c if v == 1 else eye
                d.rectangle([x0 + x * s, y0 + y * s, x0 + x * s + s - 1, y0 + y * s + s - 1], fill=col)
                if v == 1 and s > 2:
                    d.point([x0 + x * s, y0 + y * s], fill=mix(c, (255, 255, 255), 0.4))

def flt_being(im, cx, cy, r, k, ph, c, dots=260):
    """el ser FLOAT: el retrato de fase de una suma de senos (x = f(t), y = f(t + d)), mas tentaculos"""
    px = im.load()
    a, b = 1 + k % 3, 2 + k % 4
    for i in range(dots):
        u = i / dots * 2 * math.pi
        x = math.sin(a * u + ph) * (0.8 + 0.2 * math.sin(5 * u + k))
        y = math.sin(b * u) * (0.8 + 0.2 * math.cos(3 * u + ph))
        X, Y = int(cx + x * r), int(cy + y * r * 0.75)
        if 0 <= X < W and 0 <= Y < H:
            px[X, Y] = mix(c, (255, 255, 255), 0.5 * (1 + math.sin(u * 3 + ph)) / 2)
    for j in range(5):                                 # tentaculos: la onda colgando
        x0 = cx - r * 0.6 + j * r * 0.3
        for s in range(int(r * 0.9)):
            X = int(x0 + 3 * math.sin(s * 0.3 + ph * 2 + j))
            Y = int(cy + r * 0.6 + s)
            if 0 <= X < W and 0 <= Y < H and s % 2 == 0:
                px[X, Y] = mix(c, BG, s / (r * 0.9))
    for ex in (-1, 1):                                 # ojos pixel
        ImageDraw.Draw(im).rectangle([cx + ex * r * 0.3 - 1, cy - 2, cx + ex * r * 0.3 + 1, cy], fill=(255, 255, 255))

def page_byte_beings(frame=0):
    im, d = new()
    t(d, 4, 3, 'SERES BYTE  (bits de f(t), en espejo)', F5, INK)
    for i in range(12):
        x = 6 + (i % 4) * 58
        y = 18 + (i // 4) * 72
        g = sprite(BYTEF[i], frame)
        bob = 2 if (frame + i) % 2 else 0
        draw_sprite(im, g, x + 8, y + 6 + bob, 3, TRK[i % 6])
        t(d, x + 4, y + 50, NAMES[i], F5, TRK[i % 6])
        t(d, x + 4, y + 59, ['t*(42&', 't*(t>>5', '(t>>4)*', 't*(t>>1', 't*9&t>>', 't*((t>>', '(t&t>>8', 't^t>>3^', 't*(t>>3', '(t>>6|t', 't*5&t>>', 't*(t>>1'][i], F5, DIM)
    return im

def page_float_beings(ph=0.0):
    im, d = new()
    t(d, 4, 3, 'SERES FLOAT  (f(t) contra f(t+d))', F5, INK)
    nm = ['MEDU', 'ORBI', 'LISA', 'NUBE', 'ESPI', 'ALGA']
    for i in range(6):
        cx = 42 + (i % 3) * 78
        cy = 50 + (i // 3) * 104
        flt_being(im, cx, cy, 26, i, ph + i, TRK[(i + 1) % 6])
        t(d, cx - 12, cy + 50, nm[i], F5, TRK[(i + 1) % 6])
    return im

def page_life(frame=0):
    im, d = new()
    header(im, d, 0, 'HOME LIFE', 't=%06X' % (0x3A41F + frame * 977))
    tabs(d, 15, ['LIFE', 'MACRO', 'LIVE', 'TRK', 'TIME'], 0)
    px = im.load()
    import random
    random.seed(7)
    for i in range(60):                                # estrellas con paralaje
        x, y = (random.randrange(W) - frame * (1 + i % 3)) % W, random.randrange(26, 170)
        px[x, y] = (90, 90, 140) if i % 3 else (200, 200, 255)
    for x in range(W):                                 # el suelo: la mezcla como horizonte
        v = (bb((x + frame * 6) * 41) - 128) / 128
        y = 172 + int(v * 4)
        for yy in range(y, 196, 3):
            px[x, yy] = mix((40, 20, 90), BG, (yy - y) / 24)
        px[x, y] = (120, 60, 200)
    lv = [0.9, 0.55, 0.7, 0.3, 0.8, 0.45]
    for i in range(6):                                 # cada pista su ser, salta con sus notas
        x = 6 + i * 39
        jump = int(10 * abs(math.sin(frame * 0.9 + i * 1.3))) if lv[i] > 0.5 else 0
        if i in (0, 2, 5):
            g = sprite(BYTEF[[0, 0, 1, 0, 0, 6][i]], frame)
            draw_sprite(im, g, x, 140 - jump, 3, TRK[i])
        else:
            flt_being(im, x + 16, 140 - jump, 14, i, frame * 0.7 + i, TRK[i], dots=140)
        t(d, x + 8, 200, 'T%d' % (i + 1), F5, TRK[i])
        for k in range(int(lv[i] * 6)):
            d.rectangle([x + 4 + k * 5, 210, x + 7 + k * 5, 213], fill=TRK[i])
    msg = 'T3 sat(sin(w*2)*5)  T6 t*(t>>5|t>>8)'
    for i, ch in enumerate(msg):
        xx = 2 + i * 6 - (frame * 3) % 6
        if 0 <= xx <= W - 6:
            t(d, xx, 218 + int(2 * math.sin(i * 0.6 + frame)), ch, F5, mix(GRN, (85, 255, 255), (i % 12) / 12))
    footer(d, 'LIFE', '1/10')
    return im

def page_math_being(frame=0):
    im, d = new()
    header(im, d, 2, 'MATH BYTEBEAT')
    tabs(d, 15, ['SOUND', 'TONE', 'CAOS', 'KEY', 'AUTO'], 0)
    c = TRK[2]
    knob(im, d, 2, 26, 58, 'WAVE', 'BYTE', 0.3, c)
    knob(im, d, 62, 26, 58, 'ALGO', 'B07', 0.1, c, True)
    knob(im, d, 122, 26, 58, 'VAR', '30', 0.24, c)
    knob(im, d, 182, 26, 56, 'CAOS', '12', 0.1, c)
    d.rectangle([2, 58, W - 3, 84], outline=DIM)
    t(d, 5, 60, 'B07', F5, DIM)
    t(d, 5, 70, '(t*(0xCA98>>(t>>9&14)&15))|t>>8', F5, INK)
    px = im.load()
    f = lambda tt: (tt * (0xCA98 >> (tt >> 9 & 14) & 15)) | tt >> 8
    for y in range(88, 196):                           # el raster a la derecha
        for x in range(96, 236):
            v = f(((y - 88) * 140 + (x - 96)) * 6) & 255
            k = v / 255
            px[x, y] = mix((6, 10, 30), c, k) if k < 0.7 else mix(c, (255, 255, 255), (k - 0.7) / 0.3)
    d.line([96, 88 + (frame * 9) % 108, 235, 88 + (frame * 9) % 108], fill=(255, 255, 255))
    g = sprite(BYTEF[0], frame, 13)                    # el ser de la formula, a la izquierda
    draw_sprite(im, g, 8, 104 + (2 if frame % 2 else 0), 6, c)
    t(d, 16, 186, 'GLOTO', F5, c)
    d.rectangle([2, 198, W - 3, 226], outline=DIM)
    for x in range(4, 236):
        im.putpixel((x, 212 - int((f((x - 4) * 37 + 41000 + frame * 200) & 255) - 128) * 11 // 128), GRN)
    footer(d, 'SOUND', '1/9')
    return im

def page_drum(frame=0):
    im, d = new()
    header(im, d, 0, 'DRUM DRUM-X')
    tabs(d, 15, ['KIT', 'SOUND', 'LEVELS', 'TONE'], 0)
    names = ['BD', 'SD', 'CH', 'OH', 'CP', 'RS', 'TM', 'CB']
    hit = [0, 1, 2] if frame % 2 == 0 else [2, 3]
    for l in range(8):                                 # 8 bichos de 7x7, se aplastan al golpe
        x = 8 + (l % 4) * 58
        y = 34 + (l // 4) * 92
        g = sprite(BYTEF[(l * 5) % 12], frame, 9)
        s = 5
        if l in hit:
            g = [g[0]] * 0 + g[2:] + [g[-1]]           # aplastado
            d.rectangle([x - 2, y - 2, x + 47, y + 50], outline=(255, 255, 255))
        draw_sprite(im, g, x, y + (8 if l in hit else 0), s, TRK[l % 6])
        t(d, x, y + 56, names[l], F5, TRK[l % 6])
        t(d, x + 16, y + 56, ['KICK', 'F:BYTE', 'F:HASH', 'NOISE', 'CLAP', 'F:XOR', 'TOM', 'F:SQR'][l], F5, DIM)
        for k in range(8):
            on = (0x8888, 0x0808, 0xAAAA, 0x2222, 0x0080, 0x4104, 0x0011, 0x1240)[l] >> (15 - k * 2) & 1
            d.rectangle([x + k * 6, y + 68, x + k * 6 + 4, y + 71], fill=TRK[l % 6] if on else (30, 36, 60))
    footer(d, 'KIT', '1/15')
    return im

def page_fmf(frame=0):
    im, d = new()
    header(im, d, 3, 'FM-F BELL')
    tabs(d, 15, ['SOUND', 'MOD', 'ENV', 'AUTO'], 0)
    c = TRK[3]
    knob(im, d, 2, 26, 58, 'CAR', 'sin', 0.2, c)
    knob(im, d, 62, 26, 58, 'MOD', 't>>4', 0.5, c, True)
    knob(im, d, 122, 26, 58, 'RATIO', '3.5', 0.4, c)
    knob(im, d, 182, 26, 56, 'INDEX', '70', 0.55, c)
    px = im.load()
    cx, cy = 120, 132
    for ring in range(3):                              # el atomo: portadora al centro, moduladores en orbita
        r = 26 + ring * 22
        for i in range(180):
            u = i / 180 * 2 * math.pi
            X = int(cx + r * math.cos(u))
            Y = int(cy + r * 0.45 * math.sin(u) + r * 0.25 * math.cos(u) * (ring - 1))
            if i % 3 == 0: px[X, Y] = mix(BG, c, 0.4)
        u = frame * (0.6 - ring * 0.15) + ring * 2
        X = int(cx + r * math.cos(u))
        Y = int(cy + r * 0.45 * math.sin(u) + r * 0.25 * math.cos(u) * (ring - 1))
        g = sprite(BYTEF[3 + ring], frame, 7)
        draw_sprite(im, g, X - 7, Y - 7, 2, TRK[(3 + ring * 2) % 6])
    flt_being(im, cx, cy - 4, 14, 2, frame * 0.8, c, dots=160)
    t(d, 4, 196, 'f=sin(w*1+70*(t>>4)*sin(w*3.5))', F5, INK)
    for x in range(4, 236):
        v = math.sin(x * 0.08 + 0.7 * 2 * math.sin(x * 0.28 + frame))
        im.putpixel((x, 214 - int(v * 8)), GRN)
    footer(d, 'FM-F', '1/6')
    return im

def page_glitch(frame=0):
    im, d = new()
    header(im, d, 4, 'GLITCH GRAIN')
    tabs(d, 15, ['SOUND', 'GRAIN', 'JUMP', 'AUTO'], 0)
    c = TRK[4]
    knob(im, d, 2, 26, 58, 'SRC', 'T3', 0.4, c)
    knob(im, d, 62, 26, 58, 'GRAIN', '1/32', 0.3, c)
    knob(im, d, 122, 26, 58, 'JUMP', '64', 0.5, c, True)
    knob(im, d, 182, 26, 56, 'REV', '30', 0.24, c)
    tmp = Image.new('RGB', (W, H), BG)
    g = sprite(BYTEF[7], frame, 13)
    draw_sprite(tmp, g, 80, 70, 6, c)
    for y in range(60, 160):                           # el ser cortado en franjas, corridas por los saltos de t
        sh = ((bb(y * 131 + frame * 999) >> 3) - 16) if (y // 5 + frame) % 3 == 0 else 0
        row = tmp.crop((0, y, W, y + 1))
        im.paste(row, (sh, y))
    # la linea de t con sus saltos
    pts, tt = [], 0
    for x in range(4, 236):
        tt += 1
        if (x + frame * 7) % 37 == 0: tt -= 30
        pts.append((x, 186 - (tt * 7 % 30)))
    for p in pts: im.putpixel(p, (255, 255, 255))
    t(d, 4, 194, 't salta: GRAIN 1/32  JUMP 64', F5, INK)
    footer(d, 'GLITCH', '1/6')
    return im

def page_engines():
    im, d = new()
    header(im, d, 0, 'MOTORES', 'ELEGI')
    rows = [('DRUM', 'DRUM-X + lanes de formula', 0, 'BYTE'), ('MATH', 'bytebeat / floatbeat / USER', 2, 'BYTE'),
            ('WAVE', 'wavetable de formulas', 1, 'FLOAT'), ('FM-F', 'una formula modula a otra', 3, 'FLOAT'),
            ('GLITCH', 'granular sobre t', 4, 'BYTE')]
    for i, (n, s, k, fam) in enumerate(rows):
        y = 20 + i * 40
        sel = i == 1
        d.rectangle([2, y, W - 3, y + 36], outline=TRK[k] if sel else DIM)
        if i in (0, 1, 4):
            draw_sprite(im, sprite(BYTEF[(i * 3) % 12], 0, 9), 6, y + 4, 3, TRK[k])
        else:
            flt_being(im, 20, y + 14, 10, i, i, TRK[k], dots=110)
        t(d, 40, y + 4, n, F7, TRK[k])
        t(d, 40, y + 22, s, F5, INK if sel else DIM)
        t(d, W - 40, y + 4, fam, F5, DIM)
    footer(d, 'ENGINE', '2/5')
    return im

frames = 6
pages = [('seres_byte', page_byte_beings), ('seres_float', lambda f: page_float_beings(f * 0.6)), ('life', page_life),
         ('math_ser', page_math_being), ('drum_bichos', page_drum), ('fmf', page_fmf), ('glitch', page_glitch),
         ('motores', lambda f: page_engines())]
S = 2
sheet = Image.new('RGB', (4 * (W * S + 10), 2 * (H * S + 10)), (30, 30, 30))
for i, (n, fn) in enumerate(pages):
    ims = [fn(k) for k in range(frames)]
    big = [x.resize((W * 3, H * 3), Image.NEAREST) for x in ims]
    big[0].save('%s/ser_%s.gif' % (OUT, n), save_all=True, append_images=big[1:], duration=160, loop=0)
    sheet.paste(ims[1].resize((W * S, H * S), Image.NEAREST), ((i % 4) * (W * S + 10), (i // 4) * (H * S + 10)))
sheet.save('%s/seres_sheet.png' % OUT)
print('ok')
