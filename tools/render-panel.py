#!/usr/bin/env python3
"""Render the editor's layout to a PNG, so it can be LOOKED AT before the
plug-in is built.

The geometry is not duplicated here: the constants are PARSED out of
VocalFilterEditor.h and evaluated in the order they are declared, and the
vowel names and parameter ranges out of VocalFilterDsp.h. If a constant
becomes an expression this cannot evaluate, the script fails loudly rather
than quietly drawing a layout that is not the one that will ship.

    python3 tools/render-panel.py [out.png] [selected-vowel] [voice]

`selected-vowel` is 0 for Manual (the default the plug-in loads with, and
what this draws if the argument is left off) or 1..5 to show one of the
vowel buttons lit, as it is when the Vowel parameter is on a preset.

`voice` is 0 for male (the default) or 1 for female. It sets the switch's
label and the formant values and curves the panel is drawn with, so the
female set can be LOOKED AT rather than reasoned about.
"""

import cmath
import math
import os
import re
import sys

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, '..', 'source')


def constants(path, extra):
    """Every `constexpr int k... = <expr>;`, evaluated in declaration order."""
    text = open(path, encoding='utf-8').read()
    env = dict(extra)
    for name, expr in re.findall(
            r'(?:static\s+)?constexpr\s+int\s+(k\w+)\s*=\s*([^;]+);', text):
        expr = re.sub(r'\s+', ' ', expr).strip()
        try:
            env[name] = int(eval(expr, {'__builtins__': {}}, env))
        except Exception as exc:                       # noqa: BLE001
            raise SystemExit(f'cannot evaluate {name} = {expr}: {exc}')
    return env


def vowel_names(path):
    text = open(path, encoding='utf-8').read()
    block = text[text.index('constexpr VowelPreset kVowelsMale'):]
    block = block[:block.index('};')]
    return re.findall(r'\{\s*"([^"]+)"\s*,\s*"([^"]+)"', block)


def vowel_table(path, which):
    """The five (freq, bandwidth, level) triples of one voice's table."""
    text = open(path, encoding='utf-8').read()
    block = text[text.index('constexpr VowelPreset kVowels' + which):]
    block = block[:block.index('};')]
    rows = re.findall(
        r'\{\s*([-\d.]+),\s*([-\d.]+),\s*([-\d.]+)\s*\}', block)
    out, cur = [], []
    for a, b, c in rows:
        cur.append((float(a), float(b), float(c)))
        if len(cur) == 3:
            out.append(cur); cur = []
    return out


DSP = os.path.join(SRC, 'VocalFilterDsp.h')
ED = os.path.join(SRC, 'VocalFilterEditor.h')

env = constants(DSP, {})
env = constants(ED, env)
VOWELS = vowel_names(DSP)

W, H = env['kEditorWidth'], env['kEditorHeight']
SCALE = 2                                              # so the text is legible

PANEL = (64, 64, 64)
LABEL = (50, 255, 50)
VALUE = (232, 232, 232)
BAR_LIGHT = (200, 200, 200)
BAR_HIGH = (255, 255, 255)
BAR_FILL = (100, 100, 100)

img = Image.new('RGB', (W * SCALE, H * SCALE), PANEL)
d = ImageDraw.Draw(img)


def font(size):
    for path in ('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',
                 '/System/Library/Fonts/Supplemental/Arial.ttf'):
        if os.path.exists(path):
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()


F8, F7 = font(8 * SCALE), font(7 * SCALE)


def box(x0, y0, x1, y1, fill=None):
    r = [x0 * SCALE, y0 * SCALE, x1 * SCALE - 1, y1 * SCALE - 1]
    if fill:
        d.rectangle(r, fill=fill)
    d.line([r[0], r[1], r[2], r[1]], fill=BAR_LIGHT)
    d.line([r[0], r[1], r[0], r[3]], fill=BAR_LIGHT)
    d.line([r[0], r[3], r[2], r[3]], fill=BAR_HIGH)
    d.line([r[2], r[1], r[2], r[3]], fill=BAR_HIGH)


def centred(text, x0, y0, x1, y1, colour, f=F8):
    w = d.textlength(text, font=f)
    d.text(((x0 + x1) * SCALE / 2 - w / 2, y0 * SCALE), text, fill=colour, font=f)


def cell(column, row):
    x = env['kMargin'] + column * (env['kSliderWidth'] + env['kColumnGap'])
    y = env['kGridTop'] + row * (env['kSliderHeight'] + env['kRowGap'])
    return x, y, x + env['kSliderWidth'], y + env['kSliderHeight']


def slider(x0, y0, x1, y1, label, value, fraction=0.5):
    # The SlideSpin geometry: a bar along the bottom edge, a green label
    # under it, red value text across the middle.
    bar_bottom, bar_h, label_top, label_bot = 3, 12, 17, 6
    bx1 = x0 + (x1 - x0) * fraction
    box(x0, y1 - bar_bottom - bar_h, bx1, y1 - bar_bottom, fill=BAR_FILL)
    box(x0, y1 - bar_bottom - bar_h, x1, y1 - bar_bottom)
    centred(value, x0, y0 + 1, x1, y0 + 10, VALUE, F8)
    centred(label, x0, y1 - label_top, x1, y1 - label_bot, LABEL, F7)


# title
centred('Vocal Tract  -  three parallel formants',
        env['kMargin'], env['kTitleTop'], W - env['kMargin'],
        env['kTitleTop'] + env['kTitleHeight'], LABEL)

# vowel buttons. The selected one is filled and lettered in green - it is a
# push button and a state indicator, because the Vowel parameter can move
# under automation with nobody touching the panel.
SELECTED = int(sys.argv[2]) if len(sys.argv) > 2 else 0
VOICE = int(sys.argv[3]) if len(sys.argv) > 3 else 0

# the voice switch, a two-state SlideSpin one grid column wide
vt = env['kVoiceTop']
box(env['kMargin'], vt, env['kMargin'] + env['kSliderWidth'], vt + env['kVoiceHeight'] - 3,
    fill=BAR_FILL if VOICE else None)
centred('Female' if VOICE else 'Male', env['kMargin'], vt + 4,
        env['kMargin'] + env['kSliderWidth'], vt + 17, LABEL)
d.text(((env['kMargin'] + env['kSliderWidth'] + 14) * SCALE, (vt + 4) * SCALE),
       'voice - moves every vowel\'s formants', fill=LABEL, font=F7)

n = env['kVowelCount']
bw = (env['kContentWidth'] - (n - 1) * env['kVowelGap']) / n
for i, (name, sound) in enumerate(VOWELS):
    left = env['kMargin'] + i * (bw + env['kVowelGap'])
    lit = (i + 1 == SELECTED)
    box(round(left), env['kVowelTop'], round(left + bw),
        env['kVowelTop'] + env['kVowelHeight'] - 3,
        fill=BAR_FILL if lit else None)
    centred(name, round(left), env['kVowelTop'] + 5, round(left + bw),
            env['kVowelTop'] + 18, LABEL if lit else VALUE)

# column headings
for c, head in enumerate(('F1', 'F2', 'F3')):
    x0, _, x1, _ = cell(c, 0)
    centred(head, x0, env['kHeadingTop'], x1,
            env['kHeadingTop'] + env['kHeadingHeight'], LABEL)

# the grid, showing the Aaa patch
TABLE = vowel_table(DSP, 'Female' if VOICE else 'Male')
SHOWN = TABLE[max(SELECTED - 1, 0)]
AAA = [(f'{f:.0f} Hz', f'{b:.0f} Hz', f'{l:.1f} dB') for f, b, l in SHOWN]
for c in range(3):
    for r, lab in enumerate(('Freq', 'Width', 'Level')):
        x0, y0, x1, y1 = cell(c, r)
        slider(x0, y0, x1, y1, lab, AAA[c][r], (0.55, 0.2, 0.7)[r])

# ---------------------------------------------------------------------------
# The response display.
#
# The curve maths below is a RE-IMPLEMENTATION of bandpassMagnitude for the
# mock - the real one is in VocalFilterDsp.cpp and is what the plug-in draws.
# Do not read a small difference between this picture and the plug-in as a
# bug: what this script is for is checking that the panel FITS, that nothing
# collides and that the legend is readable.
# ---------------------------------------------------------------------------
TRACE = ((255, 214, 64), (72, 226, 86), (96, 160, 255))
MIN_HZ, MAX_HZ, MAX_DB, MIN_DB = 80.0, 8000.0, 12.0, -48.0
FS = 44100.0


def bandpass_complex(f0, bw, hz):
    """H(e^jw) for one formant's bandpass, complex."""
    q = min(max(f0 / max(bw, 1.0), 0.3), 60.0)
    w0 = 2 * math.pi * f0 / FS
    alpha = math.sin(w0) / (2 * q)
    a0 = 1 + alpha
    b0, b2 = alpha / a0, -alpha / a0
    a1, a2 = (-2 * math.cos(w0)) / a0, (1 - alpha) / a0
    z = cmath.exp(-2j * math.pi * hz / FS)
    return (b0 + b2 * z * z) / (1 + a1 * z + a2 * z * z)


def bandpass_db(f0, bw, hz, gain_db):
    q = min(max(f0 / max(bw, 1.0), 0.3), 60.0)
    w0 = 2 * math.pi * f0 / FS
    alpha = math.sin(w0) / (2 * q)
    a0 = 1 + alpha
    b0, b2 = alpha / a0, -alpha / a0
    a1, a2 = (-2 * math.cos(w0)) / a0, (1 - alpha) / a0
    w = 2 * math.pi * hz / FS
    cw, sw, c2, s2 = math.cos(w), math.sin(w), math.cos(2 * w), math.sin(2 * w)
    nre, nim = b0 + b2 * c2, -(b2 * s2)
    dre, dim = 1 + a1 * cw + a2 * c2, -(a1 * sw + a2 * s2)
    den = math.hypot(dre, dim)
    mag = (math.hypot(nre, nim) / den) if den > 1e-30 else 0.0
    return 20 * math.log10(max(mag * (10 ** (gain_db / 20.0)), 1e-6))


dl, dt = env['kDisplayLeft'], env['kDisplayTop']
dr, dbm = dl + env['kDisplayWidth'], env['kDisplayBottom']
d.rectangle([dl * SCALE, dt * SCALE, dr * SCALE - 1, dbm * SCALE - 1],
            fill=(20, 20, 20), outline=(150, 150, 150))

px0, py0, px1, py1 = dl + 6, dt + 5 + 12, dr - 6, dbm - 5


def x_of(hz):
    t = (math.log10(hz) - math.log10(MIN_HZ)) / (math.log10(MAX_HZ) - math.log10(MIN_HZ))
    return px0 + (px1 - px0) * min(1.0, max(0.0, t))


def y_of(v):
    t = (MAX_DB - v) / (MAX_DB - MIN_DB)
    return py0 + (py1 - py0) * min(1.0, max(0.0, t))


for hz in (100, 200, 500, 1000, 2000, 5000):
    g = 90 if hz in (100, 1000) else 60
    d.line([x_of(hz) * SCALE, py0 * SCALE, x_of(hz) * SCALE, py1 * SCALE], fill=(g, g, g))
for v in (0, -12, -24, -36):
    g = 90 if v == 0 else 60
    d.line([px0 * SCALE, y_of(v) * SCALE, px1 * SCALE, y_of(v) * SCALE], fill=(g, g, g))

SUM_TRACE = (255, 255, 255)
cap = (dl + 5, dt + 4)
d.text((cap[0] * SCALE, cap[1] * SCALE), "Filter response", fill=(200, 200, 200), font=F7)
LEGEND = (("F1", TRACE[0], 22), ("F2", TRACE[1], 22),
          ("F3", TRACE[2], 22), ("Sum", SUM_TRACE, 28))
lx = dr - 5 - sum(w for _, _, w in LEGEND)
for name, colour, w in LEGEND:
    d.text((lx * SCALE, cap[1] * SCALE), name, fill=colour, font=F7)
    lx += w

FORMANTS = tuple(SHOWN)
POLARITY = (1.0, -1.0, 1.0)


def broken_line(sampler, colour, width):
    run = []
    for i in range(240):
        t = i / 239.0
        hz = MIN_HZ * (MAX_HZ / MIN_HZ) ** t
        v = sampler(hz)
        if v < MIN_DB:
            if len(run) > 1:
                d.line(run, fill=colour, width=width)
            run = []
            continue
        run.append((x_of(hz) * SCALE, y_of(v) * SCALE))
    if len(run) > 1:
        d.line(run, fill=colour, width=width)


for k, (f0, bw, lvl) in enumerate(FORMANTS):
    # Broken where it falls off the bottom, as the plug-in draws it: clamping
    # to the floor draws a flat line across the whole width instead.
    broken_line(lambda hz, f0=f0, bw=bw, lvl=lvl: bandpass_db(f0, bw, hz, lvl),
                TRACE[k], SCALE)


# The sum, last and heaviest. COMPLEX sum with the polarity, exactly as
# bankMagnitude does it - adding the magnitudes would put the valley between
# F1 and F2 in the wrong place and make it far too shallow.
def sum_db(hz):
    total = 0j
    for (f0, bw, lvl), sign in zip(FORMANTS, POLARITY):
        total += sign * (10 ** (lvl / 20.0)) * bandpass_complex(f0, bw, hz)
    return 20 * math.log10(max(abs(total), 1e-9))


# Solid here; the plug-in draws it at alpha 205 so a component underneath
# tints it. PIL would need manual blending for that and the mock exists to
# check the LAYOUT, not the blend.
broken_line(sum_db, SUM_TRACE, 2 * SCALE)

# bottom row
BOTTOM = (('Dry / Wet', '100.0 %', 1.0),
          ('Glide', '150.0 ms', 0.075),
          ('Output Trim', '0.0 dB', 1.0))
for c, (lab, val, frac) in enumerate(BOTTOM):
    x0, _, x1, _ = cell(c, 0)
    slider(x0, env['kBottomRowTop'], x1,
           env['kBottomRowTop'] + env['kSliderHeight'], lab, val, frac)

out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'docs', 'panel.png')
os.makedirs(os.path.dirname(out), exist_ok=True)
img.save(out)
print(f'{W} x {H} logical, written to {out}')
