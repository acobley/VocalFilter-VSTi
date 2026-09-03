#!/usr/bin/env python3
"""Render the editor's layout to a PNG, so it can be LOOKED AT before the
plug-in is built.

The geometry is not duplicated here: the constants are PARSED out of
VocalFilterEditor.h and evaluated in the order they are declared, and the
vowel names and parameter ranges out of VocalFilterDsp.h. If a constant
becomes an expression this cannot evaluate, the script fails loudly rather
than quietly drawing a layout that is not the one that will ship.

    python3 tools/render-panel.py [out.png]
"""

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
    block = text[text.index('constexpr VowelPreset kVowels'):]
    block = block[:block.index('};')]
    return re.findall(r'\{\s*"([^"]+)"\s*,\s*"([^"]+)"', block)


DSP = os.path.join(SRC, 'VocalFilterDsp.h')
ED = os.path.join(SRC, 'VocalFilterEditor.h')

env = constants(DSP, {})
env = constants(ED, env)
VOWELS = vowel_names(DSP)

W, H = env['kEditorWidth'], env['kEditorHeight']
SCALE = 2                                              # so the text is legible

PANEL = (64, 64, 64)
LABEL = (50, 255, 50)
VALUE = (192, 50, 50)
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

# vowel buttons
n = env['kVowelCount']
bw = (env['kContentWidth'] - (n - 1) * env['kVowelGap']) / n
for i, (name, sound) in enumerate(VOWELS):
    left = env['kMargin'] + i * (bw + env['kVowelGap'])
    box(round(left), env['kVowelTop'], round(left + bw),
        env['kVowelTop'] + env['kVowelHeight'] - 3)
    centred(name, round(left), env['kVowelTop'] + 5, round(left + bw),
            env['kVowelTop'] + 18, VALUE)

# column headings
for c, head in enumerate(('F1', 'F2', 'F3')):
    x0, _, x1, _ = cell(c, 0)
    centred(head, x0, env['kHeadingTop'], x1,
            env['kHeadingTop'] + env['kHeadingHeight'], LABEL)

# the grid, showing the Aaa patch
AAA = [('730 Hz', '80 Hz', '0.0 dB'),
       ('1090 Hz', '90 Hz', '-7.0 dB'),
       ('2440 Hz', '120 Hz', '-12.0 dB')]
for c in range(3):
    for r, lab in enumerate(('Freq', 'Width', 'Level')):
        x0, y0, x1, y1 = cell(c, r)
        slider(x0, y0, x1, y1, lab, AAA[c][r], (0.55, 0.2, 0.7)[r])

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
