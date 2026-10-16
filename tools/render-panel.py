#!/usr/bin/env python3
"""Render the editor's panel to a PNG, so it can be LOOKED AT before the
plug-in is built.

Nothing about the layout is duplicated here. tools/dump-layout.cpp is
compiled against source/VocalFilterLayout.h - the tables the editor builds
from - and prints them as JSON together with every reading and every curve,
computed by the DSP's own functions. This script only draws what it is
given, in the panel's palette, the way VocalFilterPanelViews.cpp draws it.

    python3 tools/render-panel.py [out.png] [selected-vowel] [voice]

`selected-vowel` is 0 for Manual (what a fresh instance loads with, and the
default) or 1..5; `voice` is 0 (male, the default) or 1 (female).

Needs a C++ compiler, Pillow, and the SDK's headers in external/vst3sdk
(for vsttypes.h, which the parameter table uses).
"""

import json
import math
import os
import subprocess
import sys
import tempfile

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..'))
S = 2   # drawn at twice the size, like a Retina screen

# ---- the palette: VocalFilterPanelViews.h ---------------------------------
GROUND, HEADER, BORDER, SECTION = (20, 21, 24), (26, 28, 32), (42, 45, 51), (28, 30, 34)
RULE, TRACK, CAP, CAPEDGE = (36, 38, 43), (46, 49, 55), (42, 44, 49), (60, 64, 71)
TEXT, TITLE, MUTED, SUBTLE, DIM = (236, 234, 228), (201, 204, 210), (156, 161, 170), (126, 131, 140), (74, 78, 86)
SCREEN, SCREENEDGE, GRID, SEGBACK = (15, 16, 18), (38, 41, 46), (29, 32, 36), (18, 19, 22)
ACCENT = {'amber': (242, 165, 65), 'blue': (86, 168, 245), 'violet': (176, 140, 255), 'mint': (95, 208, 160)}
INK = {'amber': (26, 18, 5), 'blue': (6, 20, 36), 'violet': (22, 15, 38), 'mint': (6, 30, 20)}
FORMANT = ['amber', 'blue', 'violet']


def font(size, bold=False, mono=False):
    names = []
    if mono:
        names += ['/System/Library/Fonts/Menlo.ttc', '/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf']
    elif bold:
        names += ['/System/Library/Fonts/HelveticaNeue.ttc', '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf']
    else:
        names += ['/System/Library/Fonts/HelveticaNeue.ttc', '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf']
    for path in names:
        if os.path.exists(path):
            try:
                index = 1 if (bold and path.endswith('HelveticaNeue.ttc')) else 0
                return ImageFont.truetype(path, int(size * S), index=index)
            except OSError:
                pass
    return ImageFont.load_default()


F_WORD = font(23, bold=True)
F_TITLE, F_SUB, F_LABEL = font(13, bold=True), font(12), font(10, bold=True)
F_VALUE, F_NOTE = font(10, mono=True), font(11)
F_GLYPH, F_GLYPH_S = font(16, bold=True), font(15, bold=True)
F_SEG = font(12, bold=True)


def dump(selector, voice):
    with tempfile.TemporaryDirectory() as work:
        exe = os.path.join(work, 'dump-layout')
        cxx = os.environ.get('CXX', 'c++')
        subprocess.run([cxx, '-std=c++17', '-O1', '-I' + os.path.join(ROOT, 'source'),
                        '-I' + os.path.join(ROOT, 'external', 'vst3sdk'),
                        os.path.join(HERE, 'dump-layout.cpp'),
                        os.path.join(ROOT, 'source', 'VocalFilterParams.cpp'),
                        os.path.join(ROOT, 'source', 'VocalFilterDsp.cpp'), '-o', exe], check=True)
        out = subprocess.run([exe, str(selector), str(voice)], check=True, capture_output=True, text=True)
        return json.loads(out.stdout)


def main():
    out_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'docs', 'panel.png')
    selector = int(sys.argv[2]) if len(sys.argv) > 2 else 0
    voice = int(sys.argv[3]) if len(sys.argv) > 3 else 0
    L = dump(selector, voice)

    W, H = L['size']
    img = Image.new('RGB', (W * S, H * S), GROUND)   # RGB, so RGBA fills BLEND
    d = ImageDraw.Draw(img, 'RGBA')

    def box(r):
        return [r[0] * S, r[1] * S, r[2] * S - 1, r[3] * S - 1]

    def rrect(r, radius, fill, outline=None, width=1):
        d.rounded_rectangle(box(r), radius * S, fill=fill, outline=outline, width=width * S if outline else 0)

    def text(x, y, s, f, colour, anchor='la'):
        d.text((x * S, y * S), s, font=f, fill=colour, anchor=anchor)

    def line(pts, colour, width=1.0):
        d.line([(x * S, y * S) for x, y in pts], fill=colour, width=max(1, int(round(width * S))), joint='curve')

    def dashed(p0, p1, colour, width, on, off):
        x0, y0 = p0
        x1, y1 = p1
        length = math.hypot(x1 - x0, y1 - y0)
        if length <= 0:
            return
        t = 0.0
        while t < length:
            t1 = min(length, t + on)
            a = (x0 + (x1 - x0) * t / length, y0 + (y1 - y0) * t / length)
            b = (x0 + (x1 - x0) * t1 / length, y0 + (y1 - y0) * t1 / length)
            line([a, b], colour, width)
            t += on + off

    def circle(cx, cy, r, fill=None, outline=None, width=1.0):
        d.ellipse([(cx - r) * S, (cy - r) * S, (cx + r) * S, (cy + r) * S], fill=fill, outline=outline,
                  width=max(1, int(round(width * S))) if outline else 0)

    def alpha(c, a):
        return tuple(c[:3]) + (a,)

    # ---- header -------------------------------------------------------------
    d.rectangle([0, 0, W * S, L['header'] * S], fill=HEADER)
    d.rectangle([0, (L['header'] - 1) * S, W * S, L['header'] * S], fill=BORDER)
    wm = L['wordmark']
    cy = (wm[1] + wm[3]) / 2
    text(wm[0], cy, 'VOCAL', F_WORD, TEXT, 'lm')
    text(wm[0] + d.textlength('VOCAL', font=F_WORD) / S + 1, cy, 'FILTER', F_WORD, ACCENT['amber'], 'lm')
    d.rectangle([(wm[2] + 4) * S, 16 * S, (wm[2] + 5) * S, 40 * S], fill=BORDER)
    el = L['engineLine']
    text(el[0], (el[1] + el[3]) / 2, L['engine'], F_SUB, SUBTLE, 'lm')
    bp = L['bypass']
    rrect(bp, (bp[3] - bp[1]) / 2, SEGBACK, BORDER)
    lamp = ACCENT['mint']
    circle(bp[0] + 20, (bp[1] + bp[3]) / 2, 4.5, alpha(lamp, 40), alpha(lamp, 115))
    text(bp[0] + 34, (bp[1] + bp[3]) / 2, 'BYPASS  off', F_LABEL, MUTED, 'lm')

    # ---- sections -------------------------------------------------------------
    for s in L['sections']:
        r = s['r']
        rrect(r, 10, SECTION, BORDER)
        row = r[1] + 12
        circle(r[0] + 18, row + 9, 4, ACCENT[s['accent']])
        text(r[0] + 30, row + 9, s['title'], F_TITLE, TITLE, 'lm')
        if s['subtitle']:
            tw = d.textlength(s['title'], font=F_TITLE) / S
            text(r[0] + 30 + tw + 12, row + 9, s['subtitle'], F_SUB, SUBTLE, 'lm')
    for r in L['rules']:
        d.rectangle(box(r), fill=RULE)
    for c in L['captions']:
        r = c['r']
        if c['style'] == 'block':
            text(r[0], (r[1] + r[3]) / 2, c['text'], F_TITLE, ACCENT[c['accent']], 'lm')
        else:
            text(r[0], (r[1] + r[3]) / 2, c['text'], F_NOTE, SUBTLE, 'lm')

    # ---- the vowel chart ---------------------------------------------------------
    pr, pp = L['pad'], L['padPlot']
    rrect(pr, 6, SCREEN)
    for i in range(1, 8):
        x = round(pr[0] + i * (pr[2] - pr[0]) / 8)
        d.rectangle([x * S, (pr[1] + 1) * S, (x + 1) * S - 1, (pr[3] - 1) * S], fill=GRID)
    for j in range(1, 6):
        y = round(pr[1] + j * (pr[3] - pr[1]) / 6)
        d.rectangle([(pr[0] + 1) * S, y * S, (pr[2] - 1) * S, (y + 1) * S - 1], fill=GRID)

    def pad_pt(u, v):
        return (pp[0] + u * (pp[2] - pp[0]), pp[1] + v * (pp[3] - pp[1]))

    text(pr[0] + 10, pr[1] + 15, 'F1 ↓  close … open', F_VALUE, ACCENT['amber'], 'lm')
    text(pr[2] - 10, pr[1] + 15, '← F2  front … back', F_VALUE, ACCENT['blue'], 'rm')
    for hz, u in L['f2Ticks']:
        label = f'{hz / 1000:g}k' if hz >= 1000 else f'{hz:.0f}'
        text(pad_pt(u, 0)[0], pr[3] - 11, label, F_VALUE, DIM, 'mm')
    for hz, v in L['f1Ticks']:
        text(pr[0] + 8, pad_pt(0, v)[1], f'{hz:.0f}', F_VALUE, DIM, 'lm')

    mine, theirs = L['vowels'][L['voice']], L['vowels'][1 - L['voice']]
    order = L['trapezoid']
    pts = [pad_pt(theirs[i]['u'], theirs[i]['v']) for i in order]
    for a, b in zip(pts, pts[1:]):
        dashed(a, b, alpha(TEXT, 42), 1.2, 3, 4)
    pts = [pad_pt(mine[i]['u'], mine[i]['v']) for i in order]
    line(pts, alpha(ACCENT['amber'], 90), 1.2)
    for vw in theirs:
        x, y = pad_pt(vw['u'], vw['v'])
        circle(x, y, 5, None, alpha(TEXT, 76), 1.2)
    text(pr[2] - 10, pr[3] - 27, '○ ' + ('female' if L['voice'] == 0 else 'male') + ' means', F_VALUE, DIM, 'rm')
    rad = L['vowelRadius']
    for i, vw in enumerate(mine):
        x, y = pad_pt(vw['u'], vw['v'])
        lit = (i + 1 == L['selector'])
        circle(x, y, rad, alpha(ACCENT['amber'], 56) if lit else SEGBACK, ACCENT['amber'] if lit else CAPEDGE, 1.2)
        text(x, y, vw['ipa'], F_GLYPH_S, ACCENT['amber'] if lit else TEXT, 'mm')
        text(x, y + rad + 9, vw['name'], F_LABEL, SUBTLE, 'mm')

    dot = L['dot']
    x, y = pad_pt(dot['u'], dot['v'])
    circle(x, y, 18, alpha(TEXT, 20))
    circle(x, y, 9, TEXT, GROUND, 2)
    tw, th = 112, 36
    left, top = x + 24, y - th / 2
    if left + tw > pr[2] - 4:
        left = x - 24 - tw
    top = max(pr[1] + 26, min(top, pr[3] - th - 4))
    rrect([left, top, left + tw, top + th], 6, alpha(SCREEN, 235), BORDER)

    def fhz(hz):
        return f'{hz / 1000:.2f}k' if hz >= 1000 else f'{hz:.0f} Hz'
    text(left + 8, top + 10.5, 'F1 ' + fhz(dot['f1']), F_VALUE, ACCENT['amber'], 'lm')
    text(left + 8, top + 24.5, 'F2 ' + fhz(dot['f2']), F_VALUE, ACCENT['blue'], 'lm')
    rrect(pr, 6, None, SCREENEDGE)

    # ---- the vowel buttons, Manual, the glide meter ---------------------------------
    for i, r in enumerate(L['vowelButtons']):
        lit = (i + 1 == L['selector'])
        rrect(r, 8, ACCENT['amber'] if lit else SEGBACK, ACCENT['amber'] if lit else BORDER)
        ink = INK['amber'] if lit else TEXT
        cx = (r[0] + r[2]) / 2
        text(cx, r[1] + 15.5, mine[i]['ipa'], F_GLYPH, ink, 'mm')
        text(cx, r[3] - 12.5, mine[i]['name'], F_LABEL, ink, 'mm')
    m = L['manual']
    manual = (L['selector'] == 0)
    rrect(m, 8, SEGBACK, alpha(ACCENT['amber'], 150) if manual else BORDER)
    my = (m[1] + m[3]) / 2
    if manual:
        circle(m[0] + 18, my, 7.5, alpha(ACCENT['amber'], 60))
        circle(m[0] + 18, my, 4.5, ACCENT['amber'])
    else:
        circle(m[0] + 18, my, 4.5, alpha(ACCENT['amber'], 40), alpha(ACCENT['amber'], 115))
    text(m[0] + 32, my, 'MANUAL', F_LABEL, ACCENT['amber'] if manual else MUTED, 'lm')
    text(m[2] - 14, my, 'drag on the chart to leave the presets', F_NOTE, SUBTLE, 'rm')
    g = L['glideMeter']
    rrect([g[0], g[1] + 2, g[2], g[1] + 10], 4, TRACK)
    text((g[0] + g[2]) / 2, g[1] + 23, 'at rest', F_VALUE, SUBTLE, 'mm')

    # ---- the spectrum ----------------------------------------------------------------
    sr = L['spectrum']
    ax = L['spectrumAxes']
    plot = [sr[0] + 6, sr[1] + 24, sr[2] - 6, sr[3] - 18]
    rrect(sr, 6, SCREEN)

    def sx(hz):
        return plot[0] + math.log(hz / ax['minHz']) / math.log(ax['maxHz'] / ax['minHz']) * (plot[2] - plot[0])

    def sy(db):
        return plot[1] + (ax['topDb'] - db) / (ax['topDb'] - ax['floorDb']) * (plot[3] - plot[1])
    for f in (100, 200, 300, 500, 1000, 2000, 3000, 5000):
        x = round(sx(f))
        d.rectangle([x * S, (sr[1] + 1) * S, (x + 1) * S - 1, (sr[3] - 1) * S], fill=GRID)
    db = 0.0
    while db >= ax['floorDb']:
        y = round(sy(db))
        d.rectangle([(sr[0] + 1) * S, y * S, (sr[2] - 1) * S, (y + 1) * S - 1], fill=GRID)
        db -= 12
    for f in (100, 300, 1000, 3000):
        text(sx(f), sr[3] - 9.5, f'{f / 1000:g}k' if f >= 1000 else str(f), F_VALUE, DIM, 'mm')
    text(sr[0] + 10, sr[1] + 12, 'live', F_VALUE, SUBTLE, 'lm')
    text(sr[2] - 10, sr[1] + 12, 'F3 ' + fhz(L['formants'][2][0]), F_VALUE, SUBTLE, 'rm')

    hz = L['curves']['hz']

    def curve(dbs, colour, width, fill=False):
        if fill:
            poly = [(plot[0], plot[3])]
            for f, v in zip(hz, dbs):
                poly.append((sx(f), sy(max(ax['floorDb'], min(ax['topDb'], v)))))
            poly.append((plot[2], plot[3]))
            d.polygon([(px * S, py * S) for px, py in poly], fill=alpha(colour, 16))
        # Down to the bottom edge where it leaves the scale, back up from it.
        run = []
        for i, (f, v) in enumerate(zip(hz, dbs)):
            if v < ax['floorDb']:
                if run:
                    run.append((sx(f), plot[3]))
                    line(run, colour, width)
                run = []
                continue
            if not run and i > 0:
                run.append((sx(f), plot[3]))
            run.append((sx(f), sy(min(v, ax['topDb']))))
        if len(run) > 1:
            line(run, colour, width)
    for k in range(3):
        curve(L['curves'][f'f{k + 1}'], alpha(ACCENT[FORMANT[k]], 150), 1.2)
    curve(L['curves']['sum'], TEXT, 2.2, fill=True)
    for k, (f0, bw, lv) in enumerate(L['formants']):
        c = ACCENT[FORMANT[k]]
        hx, hy = sx(f0), sy(max(ax['floorDb'], min(ax['topDb'], lv)))
        dashed((hx, hy), (hx, plot[3]), alpha(c, 80), 1, 2, 3)
        lo, hi, yb = sx(max(ax['minHz'], f0 - bw / 2)), sx(min(ax['maxHz'], f0 + bw / 2)), hy + 9
        line([(lo, yb), (hi, yb)], c, 2)
        line([(lo, yb - 4), (lo, yb + 4)], c, 1.5)
        line([(hi, yb - 4), (hi, yb + 4)], c, 1.5)
        circle(hx, hy, 6.5, SCREEN, c, 2)
    rrect(sr, 6, None, SCREENEDGE)

    # ---- knobs and the switch ------------------------------------------------------------
    for c in L['controls']:
        r = c['r']
        col = ACCENT[c['accent']]
        if c['kind'] == 'switch':
            rrect(r, 7, SEGBACK, BORDER)
            names = ['Male', 'Female']
            n = len(names)
            segw = (r[2] - r[0] - 4) / n
            cur = int(round(c['value']))
            for i, nm in enumerate(names):
                seg = [r[0] + 2 + i * segw, r[1] + 2, r[0] + 2 + (i + 1) * segw, r[3] - 2]
                if i == cur:
                    rrect(seg, 5, col)
                text((seg[0] + seg[2]) / 2, (seg[1] + seg[3]) / 2, nm, F_SEG, INK[c['accent']] if i == cur else MUTED, 'mm')
            continue
        dia = c['diameter']
        stroke = 5 if dia >= 60 else 4
        cx, cyk = (r[0] + r[2]) / 2, r[1] + dia / 2
        rad = dia / 2 - stroke / 2 - 1
        bb = [(cx - rad) * S, (cyk - rad) * S, (cx + rad) * S, (cyk + rad) * S]
        d.arc(bb, 135, 405, fill=TRACK, width=stroke * S)
        a = 135 + 270 * c['value']
        if a - 135 > 0.5:
            d.arc(bb, 135, a, fill=col, width=stroke * S)
        cap = rad - stroke / 2 - 4
        circle(cx, cyk, cap, CAP, CAPEDGE, 1)
        t = math.radians(a)
        line([(cx + cap * 0.2 * math.cos(t), cyk + cap * 0.2 * math.sin(t)),
              (cx + (cap - 2) * math.cos(t), cyk + (cap - 2) * math.sin(t))], TEXT, 2)
        top = r[1] + dia + 2
        text(cx, top + 6.5, c['reading'], F_VALUE, TEXT, 'mm')
        text(cx, top + 13 + 6, c['label'].upper(), F_LABEL, MUTED, 'mm')

    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    img.save(out_path)
    print('wrote', out_path)


if __name__ == '__main__':
    main()
