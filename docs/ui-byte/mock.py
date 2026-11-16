# JIANT BYTE: maquetas de la UI demoscene / pixel (240 x 240, sin suavizado), escaladas x3
import math, sys, os
from PIL import Image, ImageDraw, ImageFont
D = os.path.dirname(os.path.abspath(__file__))
F5 = ImageFont.load(D + '/fonts/f5x8.pil')
F7 = ImageFont.load(D + '/fonts/f7x14.pil')
F10 = ImageFont.load(D + '/fonts/f10x20.pil')
W = H = 240
BG = (4, 4, 16)
INK = (220, 255, 220)
DIM = (60, 90, 70)
GRN = (80, 255, 120)
TRK = [(255, 85, 119), (85, 255, 255), (170, 255, 102), (255, 221, 68), (170, 119, 255), (255, 153, 51)]
BAY = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]

def new():
    im = Image.new('RGB', (W, H), BG)
    return im, ImageDraw.Draw(im)
def mix(a, b, k):
    return tuple(int(a[i] + (b[i] - a[i]) * k) for i in range(3))
def dither(im, x0, y0, x1, y1, c, lev):            # lev 0..1, Bayer 4x4
    px = im.load()
    for y in range(y0, y1):
        for x in range(x0, x1):
            if BAY[y & 3][x & 3] < lev * 16: px[x, y] = c
def t(d, x, y, s, f=F5, c=INK): d.text((x, y), s, font=f, fill=c)
def inv(d, x, y, s, f=F5, c=INK, bg=GRN):
    w = int(d.textlength(s, font=f))
    h = 8 if f is F5 else 14 if f is F7 else 20
    d.rectangle([x - 1, y, x + w, y + h - 1], fill=bg)
    d.text((x, y), s, font=f, fill=BG)
def header(im, d, trk, eng, extra='t=03A41F'):
    d.rectangle([0, 0, W - 1, 11], fill=(10, 14, 30))
    inv(d, 2, 2, str(trk + 1), F5, bg=TRK[trk])
    t(d, 10, 2, '>', F5, GRN)
    t(d, 17, 2, '124', F5, INK)
    d.rectangle([37, 4, 40, 7], fill=(255, 60, 60))
    t(d, 46, 2, eng, F5, INK)
    t(d, W - 6 * len(extra) - 14, 2, extra, F5, GRN)
    for i in range(6):                                 # battery as 3 blocks
        pass
    d.rectangle([W - 10, 3, W - 3, 8], outline=INK)
    d.rectangle([W - 9, 4, W - 5, 7], fill=GRN)
    for x in range(0, W, 2): im.putpixel((x, 12), DIM)  # dotted rule
def tabs(d, y, names, sel):
    x = 2
    for i, n in enumerate(names):
        if i == sel: inv(d, x, y, n)
        else: t(d, x, y, n, F5, DIM)
        x += 6 * len(n) + 8
def knob(im, d, x, y, w, lab, val, k, c, sel=False):   # a hard box, a segmented bar
    d.rectangle([x, y, x + w - 1, y + 27], outline=c if sel else DIM)
    t(d, x + 3, y + 2, lab, F5, c if sel else DIM)
    t(d, x + 3, y + 11, val, F7, INK)
    seg = (w - 6) // 3
    for i in range(seg):
        col = c if i < k * seg else (25, 30, 45)
        d.rectangle([x + 3 + i * 3, y + 24, x + 4 + i * 3, y + 25], fill=col)
def footer(d, name, page):
    t(d, 2, H - 9, name, F5, DIM)
    t(d, W - 6 * len(page) - 2, H - 9, page, F5, DIM)

def bb(tt): return (tt * (42 & tt >> 10)) & 255
def bb2(tt): return (tt * (tt >> 5 | tt >> 8)) & 255

def splash():
    im, d = new()
    px = im.load()
    for y in range(H):                                 # copper bars
        for k, c in enumerate([(255, 60, 120), (60, 200, 255), (255, 200, 60)]):
            cy = 120 + 70 * math.sin(k * 2.1 + 0.6)
            a = max(0.0, 1 - abs(y - cy) / 9)
            if a > 0:
                for x in range(W):
                    px[x, y] = mix(px[x, y], c, a * a)
    import random
    random.seed(3)
    for i in range(70):
        x, y = random.randrange(W), random.randrange(H)
        px[x, y] = (200, 200, 255) if i % 3 else (90, 90, 140)
    # logo: JIANT / BYTE in 10x20 scaled x2, shadowed
    lg = Image.new('L', (120, 48), 0)
    ld = ImageDraw.Draw(lg)
    ld.text((10, 2), 'JIANT', font=F10, fill=255)
    ld.text((20, 24), 'BYTE', font=F10, fill=255)
    lg = lg.resize((240, 96), Image.NEAREST)
    for y in range(96):
        for x in range(240):
            if lg.getpixel((x, y)):
                if 0 <= x + 3 < W and 34 + y + 3 < H: px[x + 3, 34 + y + 3] = (40, 0, 60)
    for y in range(96):
        g = mix((255, 255, 120), (255, 60, 200), y / 95)
        for x in range(240):
            if lg.getpixel((x, y)): px[x, 34 + y] = g
    t(d, 52, 140, 'v0.1  FM-1  2026', F5, INK)
    t(d, 82, 166, 'BY JUAN JIANT', F5, INK)
    s = '  t*(42&t>>10)  sin(w*3)  t>>4|t>>8  CAOS 127 ... '
    for i, ch in enumerate(s):                         # sine scroller
        x = 4 + i * 7 - 0
        if x > W - 8: break
        y = 200 + int(8 * math.sin(i * 0.45))
        t(d, x, y, ch, F7, mix((80, 255, 120), (85, 255, 255), (i % 10) / 10))
    return im

def home():
    im, d = new()
    header(im, d, 0, 'DRUM ACOUSTIC')
    tabs(d, 15, ['LIFE', 'MACRO', 'LIVE', 'TRK', 'TIME'], 1)
    px = im.load()
    # plasma de baja resolucion (bloques de 4x4) a partir de la mezcla
    for by in range(26, 176, 4):
        for bx in range(0, W, 4):
            v = (math.sin(bx * 0.045) + math.sin(by * 0.06 + 1) + math.sin((bx + by) * 0.03) + bb(bx * 40 + by * 7) / 128) / 5
            k = (v + 1) / 2
            c = mix((8, 6, 30), (40, 20, 90), k) if k < 0.6 else mix((40, 20, 90), (120, 40, 140), (k - 0.6) / 0.4)
            d.rectangle([bx, by, bx + 3, by + 3], fill=c)
    # 6 barras de cobre, una por pista, altura = nivel
    lv = [0.9, 0.55, 0.7, 0.3, 0.8, 0.45]
    for i in range(6):
        y0 = 34 + i * 23
        for yy in range(9):
            a = 1 - abs(yy - 4) / 5
            c = mix(BG, TRK[i], a)
            ln = int(lv[i] * (W - 50))
            d.line([44, y0 + yy, 44 + ln, y0 + yy], fill=c)
        t(d, 4, y0 + 1, 'T%d' % (i + 1), F5, TRK[i])
        t(d, 18, y0 + 1, ['DRM', 'WAV', 'MTH', 'WAV', 'WAV', 'MTH'][i], F5, INK)
    # macros como bloques
    for k in range(4):
        x = 2 + k * 60
        knob(im, d, x, 176, 56, 'M%d' % (k + 1), ['TRI', '70', 'S&H', '50'][k], [0.3, 0.7, 0.2, 0.5][k], GRN, k == 0)
    # scroller con las formulas que suenan
    d.rectangle([0, 206, W - 1, 228], fill=(10, 14, 30))
    msg = 'T3 sat(sin(w*2)*5)  T6 t*(t>>5|t>>8)  T1 F:KICK'
    for i, ch in enumerate(msg):
        x = 2 + i * 6
        if x > W - 6: break
        t(d, x, 212 + int(3 * math.sin(i * 0.6)), ch, F5, mix(GRN, (85, 255, 255), (i % 12) / 12))
    footer(d, 'MACROS', '2/10')
    return im

def math_page():
    im, d = new()
    header(im, d, 2, 'MATH USER')
    tabs(d, 15, ['SOUND', 'TONE', 'CAOS', 'KEY', 'AUTO'], 0)
    c = TRK[2]
    knob(im, d, 2, 26, 58, 'WAVE', 'USER', 1, c)
    knob(im, d, 62, 26, 58, 'SEED', '7', 0.04, c, True)
    knob(im, d, 122, 26, 58, 'MUTA', '30', 0.24, c)
    knob(im, d, 182, 26, 56, 'CAOS', '12', 0.1, c)
    # la formula, el token que toca la perilla invertido
    d.rectangle([2, 58, W - 3, 84], outline=DIM)
    t(d, 5, 60, 'f(t)=', F5, DIM)
    parts = [('t*(', INK), ('42', None), ('&t>>', INK), ('10', None), (')', INK)]
    x = 5
    for s, col in parts:
        if col is None:
            inv(d, x, 70, s, F7, bg=c); x += 7 * len(s)
        else:
            t(d, x, 70, s, F7, col); x += 7 * len(s)
    t(d, 160, 60, 'INT   7 ins', F5, DIM)
    # el raster bytebeat: x = t mod 232, y = t / 232, color por valor
    px = im.load()
    for y in range(88, 196):
        for x in range(4, 236):
            tt = (y - 88) * 232 + (x - 4)
            v = bb(tt * 4)
            k = v / 255
            px[x, y] = mix((6, 10, 30), c, k) if k < 0.7 else mix(c, (255, 255, 255), (k - 0.7) / 0.3)
    yy = 150
    d.line([4, yy, 235, yy], fill=(255, 255, 255))   # t, el cabezal
    t(d, 206, yy - 9, 't>', F5, (255, 255, 255))
    # osciloscopio pixel
    d.rectangle([2, 198, W - 3, 226], outline=DIM)
    pts = []
    for x in range(4, 236):
        v = (bb((x - 4) * 37 + 41000) - 128) / 128
        pts.append((x, 212 - int(v * 11)))
    for p in pts: im.putpixel(p, GRN)
    footer(d, 'SOUND', '1/9')
    return im

def wave_page():
    im, d = new()
    header(im, d, 1, 'WAVE FM KEYS')
    tabs(d, 15, ['SOUND', 'WARP', 'VOICES', 'AUTO'], 0)
    c = TRK[1]
    knob(im, d, 2, 26, 58, 'A', 'SINE', 0.1, c)
    knob(im, d, 62, 26, 58, 'POS', '42', 0.33, c, True)
    knob(im, d, 122, 26, 58, 'WARP', 'FM', 0.8, c)
    knob(im, d, 182, 26, 56, 'AMT', '60', 0.47, c)
    # wavetable como terreno de alambre (filas = POS), la actual resaltada
    rows = 22
    for r in range(rows, -1, -1):
        pos = r / rows
        y0 = 60 + r * 5
        xo = 20 + r * 2
        pts = []
        for i in range(0, 161, 2):
            ph = i / 160 * 2 * math.pi
            v = math.sin(ph + pos * 3 * math.sin(ph * 2)) * (0.6 + 0.4 * pos)
            pts.append((xo + i, y0 + 40 - int(v * 26)))
        sel = r == 9
        col = (255, 255, 255) if sel else mix((20, 40, 70), c, 0.25 + 0.6 * (1 - r / rows))
        d.line(pts, fill=col)
        if sel:
            d.line([(p[0], p[1] + 1) for p in pts], fill=c)
    d.rectangle([0, 192, W - 1, 230], fill=BG)
    t(d, 4, 196, 'f=sin(w+A*sin(w*2))', F5, INK)
    t(d, 4, 207, 'POS 42  3 VOICES  SUB -1', F5, DIM)
    footer(d, 'WAVE', '1/8')
    return im

def drum_page():
    im, d = new()
    header(im, d, 0, 'DRUM DRUM-X')
    tabs(d, 15, ['KIT', 'SOUND', 'LEVELS', 'TONE'], 0)
    names = ['BD', 'SD', 'CH', 'OH', 'CP', 'RS', 'TM', 'CB']
    forms = ['KICK', 'F:BYTE', 'F:HASH', 'NOISE', 'CLAP', 'F:XOR', 'TOM', 'F:SQR']
    pat = [0x8888, 0x0808, 0xAAAA, 0x2222, 0x0080, 0x4104, 0x0011, 0x1240]
    for l in range(8):
        y = 28 + l * 22
        c = TRK[l % 6]
        t(d, 2, y + 2, names[l], F5, c)
        t(d, 2, y + 11, forms[l], F5, DIM)
        for s in range(16):
            x = 46 + s * 12
            on = pat[l] >> (15 - s) & 1
            if on:
                d.rectangle([x, y + 2, x + 9, y + 15], fill=c)
                if s == 4: d.rectangle([x, y + 2, x + 9, y + 15], fill=(255, 255, 255))
            else:
                dither(im, x, y + 2, x + 10, y + 16, (30, 36, 60), 0.5 if s % 4 == 0 else 0.25)
    d.line([46 + 4 * 12 - 2, 26, 46 + 4 * 12 - 2, 204], fill=(255, 255, 255))
    t(d, 2, 210, 'SD = (t*9&t>>4)^RND', F5, INK)
    footer(d, 'KIT', '1/15')
    return im

def punch_page():
    im, d = new()
    header(im, d, 0, '[FX] CAOS MAX', 't=FFFFFF')
    px = im.load()
    for y in range(14, 228):
        sh = (bb(y * 97) >> 4) - 8 if (y // 6) % 3 == 0 else 0
        for x in range(W):
            v = bb2(((x + sh) & 255) * 60 + y * 31)
            px[x, y] = mix(BG, TRK[(y // 12) % 6], (v / 255) ** 2 * 0.8)
    d.rectangle([20, 90, 220, 140], fill=BG, outline=(255, 255, 255))
    t(d, 34, 96, 'CAOS MAX', F10, (255, 255, 120))
    t(d, 40, 122, 'HOLD F#4 > t ROTO', F5, INK)
    footer(d, 'PUNCH', 'F#4')
    return im

def palettes():
    pal = {
        'BYTE': [(4, 4, 16), (80, 255, 120), (220, 255, 220), (255, 85, 119)],
        'C64': [(64, 49, 141), (120, 105, 196), (255, 255, 255), (184, 199, 111)],
        'AMIGA': [(0, 0, 34), (255, 136, 0), (255, 255, 255), (0, 170, 255)],
        'CGA': [(0, 0, 0), (85, 255, 255), (255, 85, 255), (255, 255, 255)],
        'GAMEBOY': [(15, 56, 15), (48, 98, 48), (139, 172, 15), (155, 188, 15)],
        'ATARI': [(0, 0, 0), (232, 92, 16), (252, 252, 84), (104, 168, 252)],
    }
    im = Image.new('RGB', (W, H), BG)
    d = ImageDraw.Draw(im)
    t(d, 4, 4, 'PALETAS (MENU > COLOR)', F5, INK)
    for i, (n, cs) in enumerate(pal.items()):
        y = 20 + i * 36
        d.rectangle([4, y, 235, y + 31], fill=cs[0])
        t(d, 8, y + 3, n, F7, cs[2])
        for k, c in enumerate(cs[1:]):
            d.rectangle([110 + k * 40, y + 6, 140 + k * 40, y + 25], fill=c)
        dither(im, 64, y + 6, 104, y + 26, cs[1], 0.5)
    return im

shots = [('splash', splash()), ('home', home()), ('math', math_page()), ('wave', wave_page()), ('drum', drum_page()),
         ('punch', punch_page()), ('paletas', palettes())]
out = sys.argv[1]
S = 3
for n, im in shots:
    im.resize((W * S, H * S), Image.NEAREST).save('%s/mock_%s.png' % (out, n))
cols = 4
sheet = Image.new('RGB', (cols * (W * 2 + 10), 2 * (H * 2 + 10)), (30, 30, 30))
for i, (n, im) in enumerate(shots):
    sheet.paste(im.resize((W * 2, H * 2), Image.NEAREST), ((i % cols) * (W * 2 + 10), (i // cols) * (H * 2 + 10)))
sheet.save('%s/mock_sheet.png' % out)
print('ok')
