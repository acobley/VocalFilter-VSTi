#!/usr/bin/env python3
"""Three Polivoks-style VocalFilter panels in 70s sci-fi lettering, mildly
worn. R2Synth's Polivoks designs (R2Synth-VSTi/Designs/render-polivoks.py)
carried over to VocalFilter's vowel-space panel, with R2Synth's "Studio
Worn" weathering (render-weathered.py) at its lightest.

The geometry is the plug-in's own: vf-layout.json is tools/dump-layout's
output (Manual, male voice), so every box, knob, button and display is where
the editor puts it. Only the skin changes. The displays keep their content
and the F1/F2/F3 colour code; only the glass and grid are retinted.

    python3 render-polivoks.py vf-layout.json OUTDIR [1 2 3]

Needs Playwright (Chromium), numpy, scipy and Pillow.
"""
import json, math, os, re, sys, tempfile
from html import escape

import numpy as np
from PIL import Image, ImageDraw, ImageFilter
from scipy import ndimage as ndi

HERE = os.path.dirname(os.path.abspath(__file__))
L = json.load(open(sys.argv[1]))
OUT = sys.argv[2]
TMP = tempfile.mkdtemp()   # the HTML and the clean render, before the wear
WHICH = sys.argv[3:] or ['1', '2', '3']
W, H = L['size']
S = 2

SANS = "'DejaVu Sans Condensed', sans-serif"

# The face's advances, read from rbfont.js, to size tabs and place text.
RBJS = open(os.path.join(HERE, 'rbfont.js')).read() + open(os.path.join(HERE, 'vf-glyphs.js')).read()
ADV = {}
for m in re.finditer(r"(?:'(.)'|(\w)):\s*\[([\d.]+),", RBJS):
    ADV.setdefault(m.group(1) or m.group(2), float(m.group(3)))


def rbw(s, size, bold=True):
    """width in px of s drawn by rbConvert at this font-size"""
    w = 1.25 if bold else 0.95
    x = 0.0
    for ch in s.upper():
        if ch == ' ':
            x += 2.6
            continue
        x += ADV.get(ch, 0) + w + 1.05
    return max(0.0, x - 1.05) * size * 0.64 / 6


def pol(cx, cy, r, deg):
    t = math.radians(deg)
    return cx + r * math.sin(t), cy - r * math.cos(t)


class SVG:
    def __init__(s):
        s.parts = []

    def add(s, x):
        s.parts.append(x)

    def text(s, x, y, msg, size, fill, anchor='start', weight='700', keep=False):
        y = y + size * 0.32
        if keep:
            y -= size * 0.64 / 6
        k = ' data-keep="1"' if keep else ''
        s.add(f'<text x="{x:.1f}" y="{y:.1f}" font-family="{SANS}" font-size="{size}" font-weight="{weight}" '
              f'fill="{fill}" text-anchor="{anchor}"{k}>{escape(msg)}</text>')

    def rect(s, r, fill='none', stroke='none', sw=1, rx=0, extra=''):
        s.add(f'<rect x="{r[0]}" y="{r[1]}" width="{r[2]-r[0]}" height="{r[3]-r[1]}" rx="{rx}" '
              f'fill="{fill}" stroke="{stroke}" stroke-width="{sw}" {extra}/>')

    def line(s, x1, y1, x2, y2, stroke, sw=1, extra=''):
        s.add(f'<line x1="{x1:.2f}" y1="{y1:.2f}" x2="{x2:.2f}" y2="{y2:.2f}" stroke="{stroke}" '
              f'stroke-width="{sw}" stroke-linecap="round" {extra}/>')

    def poly(s, pts, stroke, sw, extra=''):
        d = ' '.join(f'{x:.2f},{y:.2f}' for x, y in pts)
        s.add(f'<polyline points="{d}" fill="none" stroke="{stroke}" stroke-width="{sw}" '
              f'stroke-linejoin="round" stroke-linecap="round" {extra}/>')

    def circle(s, cx, cy, r, fill='none', stroke='none', sw=1, extra=''):
        s.add(f'<circle cx="{cx:.2f}" cy="{cy:.2f}" r="{r:.2f}" fill="{fill}" stroke="{stroke}" '
              f'stroke-width="{sw}" {extra}/>')


# ---------------------------------------------------------------------------
# Themes. Display colours (F1 amber, F2 blue, F3 violet, sum white) are the
# editor's own in every theme; `ink` is the same code printed on the panel.
# ---------------------------------------------------------------------------
DISPLAY = {'amber': '#f2a541', 'blue': '#56a8f5', 'violet': '#b08cff', 'mint': '#5fd0a0'}

THEMES = {
    '1': dict(
        file='VocalFilter-Polivoks-1-BlackPanel.png', dark=True,
        ground='#121212', header='#0c0c0c', header_rule='#e9e6dc',
        section_fill='#151515', section_stroke='#d9d6cc', section_rx=2, title_style='outline',
        title_ink='#f4f1e8', subtitle='#b9b6ae', legend='#f4f1e8', muted='#d9d6cc', value='#f4f1e8', dim='#8f8c85',
        ink=dict(DISPLAY), rule='#3a3a3a', tick='#e9e6dc',
        knob='knurled', pointer='#f4f1e8',
        key_fill='#1b1b1b', key_edge='#4a4a4a', key_ink='#f4f1e8',
        seg_on='#f4f1e8', seg_on_ink='#121212', seg_off='#8f8c85', seg_back='#0c0c0c', seg_edge='#d9d6cc',
        lamp_on='#ff3b2f', lamp_off='#3a1512',
        screen='#08090b', screen_edge='#3c3c3c', grid='#171b21', screen_text='#8d96a3', screen_dim='#59606b',
        word1='#f4f1e8', word2='#f4f1e8', wm_cut='#0c0c0c',
        pill_fill='#0c0c0c', pill_stroke='#e2342b', pill_ink='#f4f1e8',
        screw='#3a3a3a', trap='#f2a541'),
    '2': dict(
        file='VocalFilter-Polivoks-2-InstrumentGrey.png', dark=False,
        ground='#b9b8b1', header='#2a2a28', header_rule='#c4322a',
        section_fill='#c8c7c0', section_stroke='#8d8c86', section_rx=3, title_style='band',
        title_ink='#f5f2ea', title_bg='#c4322a', subtitle='#3d3d3a', legend='#1c1c1b', muted='#2c2c2a',
        value='#1c1c1b', dim='#5a5a56',
        ink={'amber': '#a85a00', 'blue': '#1d5a9e', 'violet': '#5d3aa8', 'mint': '#1f7a56'},
        rule='#8d8c86', tick='#1c1c1b',
        knob='skirted', pointer='#e0392f', skirt_edge='#7d7e7c',
        key_fill='#232322', key_edge='#000', key_ink='#f2efe6',
        seg_on='#c4322a', seg_on_ink='#f5f2ea', seg_off='#c9c8c1', seg_back='#232322', seg_edge='#000',
        lamp_on='#ff3b2f', lamp_off='#5a1e1a',
        screen='#0d0b08', screen_edge='#55534d', grid='#221d15', screen_text='#b6a283', screen_dim='#76694f',
        word1='#f2efe6', word2='#e0392f', wm_cut='#2a2a28',
        pill_fill='#1b1b1a', pill_stroke='#6f6e69', pill_ink='#f2efe6',
        screw='#777', trap='#f2a541'),
    '3': dict(
        file='VocalFilter-Polivoks-3-ColouredCaps.png', dark=True,
        ground='#1c1916', header='#141210', header_rule='#3a342c',
        section_fill='#221e1a', section_stroke='#4a4238', section_rx=6, title_style='underline',
        title_ink='#efe4cc', subtitle='#a39882', legend='#efe4cc', muted='#c9bea6', value='#efe4cc', dim='#8f8573',
        ink=dict(DISPLAY), rule='#3a342c', tick='#b3a88f',
        knob='capped', pointer='#f7f0e0',
        key_fill='#2b2622', key_edge='#000', key_ink='#efe4cc',
        seg_on='#f2a541', seg_on_ink='#1a1205', seg_off='#a39882', seg_back='#14110f', seg_edge='#4a4238',
        lamp_on='#ff4a35', lamp_off='#3d1a15',
        screen='#0e0d0c', screen_edge='#4a4238', grid='#201d19', screen_text='#a39882', screen_dim='#6b6355',
        word1='#efe4cc', word2='#f2a541', wm_cut='#141210',
        pill_fill='#2a2015', pill_stroke='#6b4e26', pill_ink='#f2c27a',
        screw='#3a342c', trap='#f2a541'),
}

FORMANT = ['amber', 'blue', 'violet']
SECTION_ACCENT = {s['title']: s['accent'] for s in L['sections']}

# Rewritten captions: the face is wider than mixed case, so the long notes
# are shortened (same meaning) to fit the boxes the layout gives them.
NOTE_REWRITE = {
    'Dry/Wet and Trim ease in over': 'MIX + TRIM EASE IN',
    'about 10 ms; the nine formant': 'OVER 10 MS; FORMANTS',
    'values follow the Glide.': 'FOLLOW THE GLIDE',
    'width, level, and F3, which the pad does not reach': 'WIDTH, LEVEL, AND F3 - OFF THE PAD',
}


def wordmark_width(size=22):
    s = size / 6
    def w(t):
        return (sum(ADV[c] + 1.7 + 1.2 for c in t) - 1.2) * s
    return w('VOCAL'), w('FILTER')


# ---------------------------------------------------------------------------
def build(T):
    g = SVG()
    g.add('<defs>'
          '<radialGradient id="capgrad" cx="38%" cy="32%" r="75%"><stop offset="0" stop-color="#3a3a3a"/>'
          '<stop offset="0.55" stop-color="#1a1a1a"/><stop offset="1" stop-color="#0b0b0b"/></radialGradient>'
          '<radialGradient id="skirtgrad" cx="40%" cy="30%" r="75%"><stop offset="0" stop-color="#f4f5f3"/>'
          '<stop offset="0.6" stop-color="#c9cac7"/><stop offset="1" stop-color="#8f908d"/></radialGradient>'
          '<radialGradient id="shine" cx="35%" cy="28%" r="70%"><stop offset="0" stop-color="#fff" stop-opacity="0.35"/>'
          '<stop offset="0.6" stop-color="#fff" stop-opacity="0"/></radialGradient>'
          '<linearGradient id="keygrad" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#fff" stop-opacity="0.07"/>'
          '<stop offset="1" stop-color="#000" stop-opacity="0.18"/></linearGradient>'
          '</defs>')
    hh = L['header']
    g.rect([0, 0, W, H], fill=T['ground'])
    if not T['dark']:
        g.add('<filter id="ham"><feTurbulence type="fractalNoise" baseFrequency="0.9" numOctaves="2" seed="4"/>'
              '<feColorMatrix values="0 0 0 0 0  0 0 0 0 0  0 0 0 0 0  0 0 0 0.10 0"/></filter>')
        g.rect([0, 0, W, H], fill='#000', extra='filter="url(#ham)"')
    g.rect([0, 0, W, hh], fill=T['header'])
    g.line(0, hh - 1, W, hh - 1, T['header_rule'], 2)
    for x, y in [(8, 8), (W - 8, 8), (8, H - 8), (W - 8, H - 8)]:
        g.circle(x, y, 3.2, fill=T['screw'], stroke='#000', sw=0.6)
        g.line(x - 2.2, y - 2.2, x + 2.2, y + 2.2, '#222', 0.8)

    # header: wordmark, engine line, bypass window
    wm = L['wordmark']
    w1, w2 = wordmark_width()
    g.add(f'<g id="wm" transform="translate({wm[0]},{hh / 2 - 12:.1f})"></g>')
    dvx = wm[0] + w1 + 12 + w2 + 16
    g.line(dvx, 16, dvx, 40, '#5a5852', 1)
    g.text(dvx + 14, hh / 2, L['engine'].replace('  ', ' ').upper(), 10.5,
           '#b9b6ae' if T['dark'] else '#c9c8c1', weight='400')
    bp = L['bypass']
    g.rect(bp, fill=T['pill_fill'], stroke=T['pill_stroke'], sw=1.2, rx=3)
    cy = (bp[1] + bp[3]) / 2
    g.circle(bp[0] + 18, cy, 4.5, fill=T['lamp_off'], stroke='#000', sw=0.6)
    g.text(bp[0] + 32, cy + 0.5, 'BYPASS  OFF', 10.5, T['pill_ink'])

    # sections
    for s in L['sections']:
        r = s['r']
        g.rect(r, fill=T['section_fill'], stroke=T['section_stroke'], sw=1.2, rx=T['section_rx'])
        ty = r[1] + 21
        tw = rbw(s['title'], 13)
        tab = [r[0] + 14, ty - 11, r[0] + 14 + tw + 18, ty + 11]
        if T['title_style'] == 'outline':
            g.rect(tab, fill=T['section_fill'], stroke=T['title_ink'], sw=1.4, rx=1.5)
        elif T['title_style'] == 'band':
            g.rect(tab, fill=T['title_bg'], rx=1.5)
        else:
            g.line(tab[0] + 2, tab[3] + 1, tab[2] - 6, tab[3] + 1, T['ink'][s['accent']], 2.4)
        g.text(tab[0] + 9, ty, s['title'], 13, T['title_ink'] if T['title_style'] != 'underline' else T['legend'])
        sub = NOTE_REWRITE.get(s['subtitle'], s['subtitle'])
        if sub:
            g.text(tab[2] + 12, ty, sub.upper(), 10, T['subtitle'], weight='400')
    for r in L['rules']:
        g.rect(r, fill=T['rule'])
    for c in L['captions']:
        r = c['r']
        y = (r[1] + r[3]) / 2
        if c['style'] == 'block':
            g.text(r[0], y, c['text'], 13, T['ink'][c['accent']])
        else:
            g.text(r[0] + (6 if c['text'].startswith('on') or c['text'].startswith('knob') else 0), y,
                   NOTE_REWRITE.get(c['text'], c['text']).upper(), 10, T['subtitle'], weight='400')

    draw_pad(g, T)
    draw_spectrum(g, T)
    draw_buttons(g, T)
    for c in L['controls']:
        if c['kind'] == 'switch':
            draw_switch(g, T, c)
        else:
            draw_knob(g, T, c)
    return '\n'.join(g.parts)


def glass(g, T, r):
    # the bezel sits inside the layout's rect, so nothing moves or collides
    g.rect(r, fill='#000', stroke=T['screen_edge'], sw=1, rx=4)
    g.rect([r[0] + 2.5, r[1] + 2.5, r[2] - 2.5, r[3] - 2.5], fill=T['screen'], rx=2.5)


def draw_pad(g, T):
    pr, pp = L['pad'], L['padPlot']
    glass(g, T, pr)
    for i in range(1, 8):
        x = round(pr[0] + i * (pr[2] - pr[0]) / 8)
        g.line(x, pr[1] + 1, x, pr[3] - 1, T['grid'], 1)
    for j in range(1, 6):
        y = round(pr[1] + j * (pr[3] - pr[1]) / 6)
        g.line(pr[0] + 1, y, pr[2] - 1, y, T['grid'], 1)

    def pt(u, v):
        return (pp[0] + u * (pp[2] - pp[0]), pp[1] + v * (pp[3] - pp[1]))

    g.text(pr[0] + 10, pr[1] + 15, 'F1 ↓ CLOSE … OPEN', 9.5, DISPLAY['amber'], weight='400')
    g.text(pr[2] - 10, pr[1] + 15, '← F2 FRONT … BACK', 9.5, DISPLAY['blue'], anchor='end', weight='400')
    for hz, u in L['f2Ticks']:
        g.text(pt(u, 0)[0], pr[3] - 11, f'{hz / 1000:g}K' if hz >= 1000 else f'{hz:.0f}', 9, T['screen_dim'],
               anchor='middle', weight='400')
    for hz, v in L['f1Ticks']:
        g.text(pr[0] + 8, pt(0, v)[1], f'{hz:.0f}', 9, T['screen_dim'], weight='400')

    mine, theirs = L['vowels'][L['voice']], L['vowels'][1 - L['voice']]
    order = L['trapezoid']
    g.poly([pt(theirs[i]['u'], theirs[i]['v']) for i in order], '#ecEAE4', 1.2,
           'stroke-opacity="0.17" stroke-dasharray="3 4" stroke-linecap="butt"')
    g.poly([pt(mine[i]['u'], mine[i]['v']) for i in order], T['trap'], 1.2, 'stroke-opacity="0.35"')
    for vw in theirs:
        x, y = pt(vw['u'], vw['v'])
        g.circle(x, y, 5, stroke='#ecEAE4', sw=1.2, extra='stroke-opacity="0.3"')
    g.text(pr[2] - 10, pr[3] - 27, '○ ' + ('FEMALE' if L['voice'] == 0 else 'MALE') + ' MEANS', 9,
           T['screen_dim'], anchor='end', weight='400')
    rad = L['vowelRadius']
    for i, vw in enumerate(mine):
        x, y = pt(vw['u'], vw['v'])
        lit = (i + 1 == L['selector'])
        g.circle(x, y, rad, fill='#121316', stroke=DISPLAY['amber'] if lit else '#3c4047', sw=1.2)
        g.text(x, y, vw['ipa'], 15, DISPLAY['amber'] if lit else '#ecEAE4', anchor='middle', keep=True)
        g.text(x, y + rad + 9, vw['name'], 9.5, T['screen_text'], anchor='middle')

    dot = L['dot']
    x, y = pt(dot['u'], dot['v'])
    g.circle(x, y, 18, fill='#ecEAE4', extra='fill-opacity="0.08"')
    g.circle(x, y, 9, fill='#ecEAE4', stroke='#000', sw=2)
    tw, th = 118, 36
    left, top = x + 24, y - th / 2
    if left + tw > pr[2] - 4:
        left = x - 24 - tw
    top = max(pr[1] + 26, min(top, pr[3] - th - 4))
    g.rect([left, top, left + tw, top + th], fill=T['screen'], stroke='#3c4047', sw=1, rx=3,
           extra='fill-opacity="0.92"')

    def fhz(hz):
        return f'{hz / 1000:.2f}K' if hz >= 1000 else f'{hz:.0f} HZ'
    g.text(left + 9, top + 10.5, 'F1 ' + fhz(dot['f1']), 9.5, DISPLAY['amber'], weight='400')
    g.text(left + 9, top + 24.5, 'F2 ' + fhz(dot['f2']), 9.5, DISPLAY['blue'], weight='400')


def draw_spectrum(g, T):
    sr, ax = L['spectrum'], L['spectrumAxes']
    plot = [sr[0] + 6, sr[1] + 24, sr[2] - 6, sr[3] - 18]
    glass(g, T, sr)

    def sx(hz):
        return plot[0] + math.log(hz / ax['minHz']) / math.log(ax['maxHz'] / ax['minHz']) * (plot[2] - plot[0])

    def sy(db):
        return plot[1] + (ax['topDb'] - db) / (ax['topDb'] - ax['floorDb']) * (plot[3] - plot[1])
    for f in (100, 200, 300, 500, 1000, 2000, 3000, 5000):
        g.line(round(sx(f)), sr[1] + 1, round(sx(f)), sr[3] - 1, T['grid'], 1)
    db = 0.0
    while db >= ax['floorDb']:
        g.line(sr[0] + 1, round(sy(db)), sr[2] - 1, round(sy(db)), T['grid'], 1)
        db -= 12
    for f in (100, 300, 1000, 3000):
        g.text(sx(f), sr[3] - 9.5, f'{f / 1000:g}K' if f >= 1000 else str(f), 9, T['screen_dim'],
               anchor='middle', weight='400')
    g.text(sr[0] + 10, sr[1] + 12, 'LIVE', 9.5, T['screen_text'], weight='400')
    f3 = L['formants'][2][0]
    g.text(sr[2] - 10, sr[1] + 12, 'F3 ' + (f'{f3 / 1000:.2f}K' if f3 >= 1000 else f'{f3:.0f} HZ'), 9.5,
           T['screen_text'], anchor='end', weight='400')

    hz = L['curves']['hz']

    def curve(dbs, colour, width, extra='', fill=False):
        if fill:
            pts = [(plot[0], plot[3])] + [(sx(f), sy(max(ax['floorDb'], min(ax['topDb'], v)))) for f, v in zip(hz, dbs)] \
                + [(plot[2], plot[3])]
            d = ' '.join(f'{x:.2f},{y:.2f}' for x, y in pts)
            g.add(f'<polygon points="{d}" fill="{colour}" fill-opacity="0.07"/>')
        run = []
        for i, (f, v) in enumerate(zip(hz, dbs)):
            if v < ax['floorDb']:
                if run:
                    run.append((sx(f), plot[3]))
                    g.poly(run, colour, width, extra)
                run = []
                continue
            if not run and i > 0:
                run.append((sx(f), plot[3]))
            run.append((sx(f), sy(min(v, ax['topDb']))))
        if len(run) > 1:
            g.poly(run, colour, width, extra)
    for k in range(3):
        curve(L['curves'][f'f{k + 1}'], DISPLAY[FORMANT[k]], 1.2, 'stroke-opacity="0.6"')
    curve(L['curves']['sum'], '#ecEAE4', 2.2, fill=True)
    for k, (f0, bw, lv) in enumerate(L['formants']):
        c = DISPLAY[FORMANT[k]]
        hx, hy = sx(f0), sy(max(ax['floorDb'], min(ax['topDb'], lv)))
        g.line(hx, hy, hx, plot[3], c, 1, 'stroke-opacity="0.32" stroke-dasharray="2 3" stroke-linecap="butt"')
        lo, hi, yb = sx(max(ax['minHz'], f0 - bw / 2)), sx(min(ax['maxHz'], f0 + bw / 2)), hy + 9
        g.line(lo, yb, hi, yb, c, 2)
        g.line(lo, yb - 4, lo, yb + 4, c, 1.5)
        g.line(hi, yb - 4, hi, yb + 4, c, 1.5)
        g.circle(hx, hy, 6.5, fill=T['screen'], stroke=c, sw=2)


def draw_buttons(g, T):
    mine = L['vowels'][L['voice']]
    for i, r in enumerate(L['vowelButtons']):
        lit = (i + 1 == L['selector'])
        g.rect([r[0], r[1] + 1.5, r[2], r[3] + 1.5], fill='#000', rx=4, extra='fill-opacity="0.35"')
        g.rect(r, fill=T['key_fill'], stroke=T['key_edge'], sw=1, rx=4)
        g.rect(r, fill='url(#keygrad)', rx=4)
        cx = (r[0] + r[2]) / 2
        g.circle(r[2] - 8, r[1] + 8, 2.6, fill=T['lamp_on'] if lit else T['lamp_off'], stroke='#000', sw=0.5)
        g.text(cx, r[1] + 16, mine[i]['ipa'], 16, T['key_ink'], anchor='middle', keep=True)
        g.text(cx, r[3] - 11.5, mine[i]['name'], 9.5, T['key_ink'], anchor='middle')
    m = L['manual']
    manual = (L['selector'] == 0)
    g.rect([m[0], m[1] + 1.5, m[2], m[3] + 1.5], fill='#000', rx=4, extra='fill-opacity="0.35"')
    g.rect(m, fill=T['key_fill'], stroke=T['key_edge'], sw=1, rx=4)
    g.rect(m, fill='url(#keygrad)', rx=4)
    my = (m[1] + m[3]) / 2
    if manual:
        g.circle(m[0] + 18, my, 7.5, fill=T['lamp_on'], extra='fill-opacity="0.25"')
    g.circle(m[0] + 18, my, 4.5, fill=T['lamp_on'] if manual else T['lamp_off'], stroke='#000', sw=0.6)
    g.text(m[0] + 32, my, 'MANUAL', 10.5, T['key_ink'])
    g.text(m[2] - 12, my, 'DRAG THE CHART TO LEAVE PRESETS', 9, '#a7a49d' if T['dark'] or True else T['dim'],
           anchor='end', weight='400')
    gm = L['glideMeter']
    g.rect([gm[0], gm[1] + 2, gm[2], gm[1] + 10], fill='#000', stroke=T['screen_edge'], sw=1, rx=2)
    for i in range(1, 10):
        x = gm[0] + i * (gm[2] - gm[0]) / 10
        g.line(x, gm[1] + 3.5, x, gm[1] + 8.5, T['grid'], 1)
    g.text((gm[0] + gm[2]) / 2, gm[1] + 23, 'AT REST', 9.5, T['muted'], anchor='middle', weight='400')


def draw_switch(g, T, c):
    r = c['r']
    g.rect(r, fill=T['seg_back'], stroke=T['seg_edge'], sw=1.2, rx=3)
    names = ['MALE', 'FEMALE']
    segw = (r[2] - r[0] - 4) / 2
    cur = int(round(c['value']))
    for i, nm in enumerate(names):
        seg = [r[0] + 2 + i * segw, r[1] + 2, r[0] + 2 + (i + 1) * segw, r[3] - 2]
        if i == cur:
            g.rect(seg, fill=T['seg_on'], rx=2)
        g.text((seg[0] + seg[2]) / 2, (seg[1] + seg[3]) / 2, nm, 10.5, T['seg_on_ink'] if i == cur else T['seg_off'],
               anchor='middle')


def draw_knob(g, T, c):
    r, dia = c['r'], c['diameter']
    cx, cy = (r[0] + r[2]) / 2, r[1] + dia / 2
    outer = dia / 2
    body = outer - (6 if dia >= 44 else 5)
    ang = -135 + 270 * max(0.0, min(1.0, c['value']))
    for i in range(11):
        t = -135 + 27 * i
        long_ = i in (0, 5, 10)
        x0, y0 = pol(cx, cy, body + 1.5, t)
        x1, y1 = pol(cx, cy, outer - (0 if long_ else 1.8), t)
        g.line(x0, y0, x1, y1, T['tick'], 1.3 if long_ else 0.9)
    style = T['knob']
    if style == 'knurled':
        teeth = 24
        pts = ' '.join('%.2f,%.2f' % pol(cx, cy, body if i % 2 == 0 else body - 1.2, 360 * i / (teeth * 2))
                       for i in range(teeth * 2))
        g.add(f'<polygon points="{pts}" fill="#0a0a0a" stroke="#2f2f2f" stroke-width="0.6"/>')
        g.circle(cx, cy, body * 0.78, fill='url(#capgrad)', stroke='#000', sw=0.8)
        x0, y0 = pol(cx, cy, body * 0.15, ang)
        x1, y1 = pol(cx, cy, body - 0.5, ang)
        g.line(x0, y0, x1, y1, T['pointer'], 2.0 if dia >= 44 else 1.7)
    elif style == 'skirted':
        g.circle(cx, cy, body, fill='url(#skirtgrad)', stroke=T['skirt_edge'], sw=0.8)
        x0, y0 = pol(cx, cy, body * 0.66, ang)
        x1, y1 = pol(cx, cy, body - 0.6, ang)
        g.line(x0, y0, x1, y1, '#1c1c1b', 1.5)
        g.circle(cx, cy, body * 0.64, fill='url(#capgrad)', stroke='#000', sw=0.8)
        x0, y0 = pol(cx, cy, body * 0.12, ang)
        x1, y1 = pol(cx, cy, body * 0.6, ang)
        g.line(x0, y0, x1, y1, T['pointer'], 2.0 if dia >= 44 else 1.7)
    else:
        a = DISPLAY[c['accent']]
        g.circle(cx, cy, body, fill='#14110f', stroke='#000', sw=0.8)
        x0, y0 = pol(cx, cy, body * 0.7, ang)
        x1, y1 = pol(cx, cy, body - 0.6, ang)
        g.line(x0, y0, x1, y1, T['pointer'], 1.5)
        g.circle(cx, cy, body * 0.68, fill=a, stroke='#000', sw=0.8)
        g.circle(cx, cy, body * 0.68, fill='url(#shine)')
        x0, y0 = pol(cx, cy, body * 0.1, ang)
        x1, y1 = pol(cx, cy, body * 0.62, ang)
        g.line(x0, y0, x1, y1, '#14110f', 2.0 if dia >= 44 else 1.7)
    top = r[1] + dia + 2
    g.text(cx, top + 7, c['reading'].upper(), 9.5, T['value'], anchor='middle', weight='400')
    g.text(cx, top + 13 + 6.5, c['label'].upper(), 9.5, T['muted'], anchor='middle')


# ---------------------------------------------------------------------------
def render(key):
    T = THEMES[key]
    body = build(T)
    w1, _ = wordmark_width()
    html = f"""<!doctype html><html><head><meta charset="utf-8"><style>html,body{{margin:0;background:{T['ground']}}}svg{{display:block}}</style></head>
<body><svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">{body}</svg>
<script>{RBJS}
const wm = document.getElementById('wm');
const big = (t, col, x) => {{ const a = rbPaths(t, 22, 1.7, 1.2); return {{ a, svg: `<g transform="translate(${{x}},0)"><g fill="none" stroke="${{col}}" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round">${{a.paths}}</g><g fill="none" stroke="{T['wm_cut']}" stroke-width=".32" stroke-linecap="round" stroke-linejoin="round">${{a.paths}}</g></g>` }}; }};
const p1 = big('VOCAL', '{T['word1']}', 0), p2 = big('FILTER', '{T['word2']}', p1.a.width + 12);
wm.innerHTML = p1.svg + p2.svg;
rbConvertKeep();
rbConvert();
</script></body></html>"""
    hp = os.path.join(TMP, f'polivoks-{key}.html')
    open(hp, 'w').write(html)
    from playwright.sync_api import sync_playwright
    with sync_playwright() as p:
        b = p.chromium.launch()
        pg = b.new_page(viewport={'width': W, 'height': H}, device_scale_factor=S)
        pg.goto('file://' + os.path.abspath(hp))
        pg.wait_for_timeout(300)
        print(key, 'page', pg.evaluate('[document.body.scrollWidth, document.body.scrollHeight]'))
        raw = os.path.join(TMP, f'polivoks-{key}-clean.png')
        pg.screenshot(path=raw, clip={'x': 0, 'y': 0, 'width': W, 'height': H})
        b.close()
    return raw


# ---------------------------------------------------------------------------
# Studio Worn, from R2Synth's render-weathered.py, turned down: fine haze,
# a few hairlines, polished knob tops, dust on the glass.
# ---------------------------------------------------------------------------
def weather(raw, T, out_name, seed):
    BASE = np.asarray(Image.open(raw).convert('RGB')).astype(np.float32) / 255.0
    Hp, Wp = BASE.shape[:2]
    YY, XX = np.mgrid[0:Hp, 0:Wp].astype(np.float32)
    rng = np.random.default_rng(seed)

    def disc(cx, cy, r, soft=1.0):
        return np.clip((r - np.hypot(XX - cx, YY - cy)) / soft + 0.5, 0, 1)

    def rectmask(r, pad=0):
        m = np.zeros((Hp, Wp), np.float32)
        m[int(r[1] - pad):int(r[3] + pad), int(r[0] - pad):int(r[2] + pad)] = 1
        return m

    knobs = []
    for c in L['controls']:
        if c['kind'] == 'knob':
            r, d = c['r'], c['diameter']
            body = d / 2 - (6 if d >= 44 else 5)
            knobs.append(dict(cx=(r[0] + r[2]) / 2 * S, cy=(r[1] + d / 2) * S, body=body * S,
                              cap=body * (0.78 if T['knob'] == 'knurled' else 0.66) * S))
    screens = [[v * S for v in L['pad']], [v * S for v in L['spectrum']]]
    keys = [[v * S for v in r] for r in L['vowelButtons'] + [L['manual']]]
    M_KNOB = np.zeros((Hp, Wp), np.float32)
    for k in knobs:
        M_KNOB = np.maximum(M_KNOB, disc(k['cx'], k['cy'], k['body'] + 1))
    M_SCREEN = np.zeros((Hp, Wp), np.float32)
    M_BEZEL = np.zeros((Hp, Wp), np.float32)
    for r in screens:
        M_SCREEN = np.maximum(M_SCREEN, rectmask(r))
        M_BEZEL = np.maximum(M_BEZEL, rectmask(r, 8))
    M_KEY = np.zeros((Hp, Wp), np.float32)
    for r in keys:
        M_KEY = np.maximum(M_KEY, rectmask(r))
    M_PANEL = np.clip(1 - np.maximum(np.maximum(M_KNOB, M_KEY), M_BEZEL), 0, 1)

    def noise(sigma, octaves=1):
        acc, amp, tot = np.zeros((Hp, Wp), np.float32), 1.0, 0.0
        for o in range(octaves):
            n = ndi.gaussian_filter(rng.standard_normal((Hp, Wp)).astype(np.float32), sigma / (2 ** o))
            acc += amp * n / (n.std() + 1e-6)
            tot += amp
            amp *= 0.5
        return acc / tot

    def norm01(a):
        lo, hi = np.percentile(a, 1), np.percentile(a, 99)
        return np.clip((a - lo) / (hi - lo + 1e-6), 0, 1)

    def over(img, colour, alpha):
        a = np.clip(alpha, 0, 1)[..., None]
        return img * (1 - a) + np.asarray(colour, np.float32)[None, None, :] / 255.0 * a

    def stroke_layer(paths):
        ss = 2
        im = Image.new('L', (Wp * ss, Hp * ss), 0)
        d = ImageDraw.Draw(im)
        for pts, width, alpha in paths:
            p = [(x * ss, y * ss) for x, y in pts]
            n = len(p)
            for i in range(n - 1):
                t = i / max(1, n - 2)
                taper = math.sin(math.pi * min(1, max(0, t))) ** 0.5
                d.line([p[i], p[i + 1]], fill=int(255 * alpha * (0.35 + 0.65 * taper)),
                       width=max(1, int(round(width * ss * (0.4 + 0.6 * taper)))))
        return np.asarray(im.resize((Wp, Hp), Image.LANCZOS)).astype(np.float32) / 255.0

    def scratch_path(x, y, ang, length, bend=0.0, jitter=0.15, step=6):
        pts, a = [(x, y)], ang
        for _ in range(int(length / step)):
            a += bend + rng.normal(0, jitter) * 0.05
            x += math.cos(a) * step
            y += math.sin(a) * step
            pts.append((x, y))
        return pts

    def specks(n, region, rmin, rmax, alpha_rng, fibres=0):
        im = Image.new('L', (Wp, Hp), 0)
        d = ImageDraw.Draw(im)
        ys, xs = np.nonzero(region > 0.5)
        for i in rng.integers(0, len(xs), n):
            x, y, r = xs[i], ys[i], rng.uniform(rmin, rmax)
            d.ellipse([x - r, y - r, x + r, y + r], fill=int(255 * rng.uniform(*alpha_rng)))
        for _ in range(fibres):
            i = rng.integers(0, len(xs))
            d.line(scratch_path(xs[i], ys[i], rng.uniform(0, 6.28), rng.uniform(20, 70), bend=rng.normal(0, 0.12), step=3),
                   fill=int(255 * rng.uniform(0.2, 0.4)), width=1)
        return np.asarray(im.filter(ImageFilter.GaussianBlur(0.6))).astype(np.float32) / 255.0

    def fingerprint(cx, cy, rx, ry, ang):
        ca, sa = math.cos(ang), math.sin(ang)
        X, Y = XX - cx, YY - cy
        u, v = (X * ca + Y * sa) / rx, (-X * sa + Y * ca) / ry
        rr = np.sqrt(u * u + v * v)
        return (0.5 + 0.5 * np.sin((rr + noise(30) * 0.05) * 2 * math.pi * 14)) * np.exp(-(rr ** 2) * 2.2)

    lum = BASE[..., 0] * 0.299 + BASE[..., 1] * 0.587 + BASE[..., 2] * 0.114
    lines = ((lum > 0.55) if T['dark'] else (lum < 0.28)) & (M_PANEL > 0.5)
    edge = np.exp(-ndi.distance_transform_edt(~lines) / 10.0) * M_PANEL
    ring = np.zeros((Hp, Wp), np.float32)
    for k in knobs:
        ring = np.maximum(ring, np.exp(-((np.hypot(XX - k['cx'], YY - k['cy']) - k['body'] - 3) / 5) ** 2))

    dust_col = (150, 146, 138) if T['dark'] else (96, 92, 84)
    hair_col = (120, 120, 116) if T['dark'] else (228, 228, 222)
    img = BASE.copy()
    haze = norm01(noise(90, 3)) * 0.55 + norm01(noise(6, 2)) * 0.45
    reach = np.clip(0.5 + (XX / Wp) * 0.3 - (YY / Hp) * 0.25, 0, 1)
    img = over(img, dust_col, (haze * 0.055 * reach + edge * 0.045 + ring * 0.08) * M_PANEL)

    paths = []
    for _ in range(6):
        cx, cy, ang = rng.uniform(0, Wp), rng.uniform(0, Hp), rng.normal(0.15, 0.4)
        for _ in range(int(rng.integers(2, 7))):
            paths.append((scratch_path(cx + rng.normal(0, 60), cy + rng.normal(0, 36), ang + rng.normal(0, 0.08),
                                       rng.uniform(40, 200), bend=rng.normal(0, 0.002)),
                          rng.uniform(0.5, 0.85), rng.uniform(0.12, 0.32)))
    for _ in range(26):
        paths.append((scratch_path(rng.uniform(0, Wp), rng.uniform(0, Hp), rng.uniform(0, 6.28), rng.uniform(15, 80),
                                   bend=rng.normal(0, 0.01)), 0.6, rng.uniform(0.08, 0.24)))
    img = over(img, hair_col, stroke_layer(paths) * (1 - M_SCREEN) * 0.8)

    kp = []
    for k in knobs:
        for _ in range(int(rng.integers(5, 12))):
            rr, a0, span = k['cap'] * rng.uniform(0.25, 0.95), rng.uniform(0, 2 * math.pi), rng.uniform(0.3, 1.5)
            kp.append(([(k['cx'] + rr * math.cos(a0 + span * t / 12), k['cy'] + rr * math.sin(a0 + span * t / 12))
                        for t in range(13)], rng.uniform(0.6, 1.2), rng.uniform(0.1, 0.28)))
    img = over(img, (150, 150, 148), stroke_layer(kp) * M_KNOB)
    sheen = np.zeros((Hp, Wp), np.float32)
    for k in knobs:
        sheen = np.maximum(sheen, disc(k['cx'] - k['cap'] * 0.25, k['cy'] - k['cap'] * 0.3, k['cap'] * 0.55, 8) * 0.05)
    img = img + sheen[..., None]

    # the keys: tops polished where thumbs land, a little dust round the edges
    kd = np.zeros((Hp, Wp), np.float32)
    for r in keys:
        cxk, cyk = (r[0] + r[2]) / 2, (r[1] + r[3]) / 2
        kd = np.maximum(kd, disc(cxk, cyk, min(r[2] - r[0], r[3] - r[1]) * 0.32, 10) * 0.04)
    img = img + (kd * M_KEY)[..., None]

    sd = specks(220, M_SCREEN, 0.4, 1.3, (0.12, 0.45), fibres=3)
    img = over(img, (170, 170, 168), sd * M_SCREEN)
    pad, spec = screens
    fp = fingerprint(pad[0] + (pad[2] - pad[0]) * 0.62, pad[1] + (pad[3] - pad[1]) * 0.7, 26, 34, 0.4) * 0.05 \
        + fingerprint(spec[0] + (spec[2] - spec[0]) * 0.78, spec[1] + (spec[3] - spec[1]) * 0.55, 24, 30, -0.3) * 0.04
    img = over(img, (200, 200, 205), fp * M_SCREEN)
    pd = specks(1000, np.clip(M_PANEL + M_KNOB, 0, 1), 0.3, 1.2, (0.12, 0.4), fibres=7)
    img = over(img, (175, 170, 160) if T['dark'] else (80, 76, 70), pd)

    path = os.path.join(OUT, out_name)
    Image.fromarray((np.clip(img, 0, 1) * 255 + 0.5).astype(np.uint8)).save(path, optimize=True)
    print('wrote', path)


if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    for key in WHICH:
        raw = render(key)
        weather(raw, THEMES[key], THEMES[key]['file'], seed=10 + int(key))
