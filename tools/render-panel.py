#!/usr/bin/env python3
"""Render the editor's panel to a PNG, so it can be LOOKED AT before the
plug-in is built.

Nothing about the layout is duplicated here. tools/dump-layout.cpp is
compiled against source/VocalFilterLayout.h - the tables the editor builds
from - and prints them as JSON together with every reading and every curve,
computed by the DSP's own functions. This script only draws what it is
given, in the panel's palette, the way VocalFilterPanelViews.cpp draws it.

THE BLACK PANEL (branch PolivoksPanel, 10 Nov 2026): matte black, white
printing, boxed sections, knurled knobs, black keys with red lamps, the
two screens as black glass, every word in the 70s sci-fi lettering and
light wear. The lettering is stroked from the glyphs dump-layout prints
(parsed by SciFiLettering.h) and the wear is the marks it prints (made by
PanelWear.h), so this script keeps no copy of the face and no seed of its
own, and needs no fonts. The drawing helpers are R2Synth's
(R2Synth-VSTi/tools/render-panel.py), which does the same for its panel.

What it cannot show is VSTGUI's own rendering: this is a reconstruction
from the same numbers, not a screenshot.

    python3 tools/render-panel.py [out.png] [selected-vowel] [voice] [level-comp 0/1]

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

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..'))
SCALE = 2          # the PNG is drawn at 2x, like a Retina screen
SS = 4             # ...from a 4x drawing, for smooth curves


def dump(selector, voice, level_comp=0):
    with tempfile.TemporaryDirectory() as work:
        exe = os.path.join(work, 'dump-layout')
        cxx = os.environ.get('CXX', 'c++')
        subprocess.run([cxx, '-std=c++17', '-O1', '-I' + os.path.join(ROOT, 'source'),
                        '-I' + os.path.join(ROOT, 'external', 'vst3sdk'),
                        os.path.join(HERE, 'dump-layout.cpp'),
                        os.path.join(ROOT, 'source', 'VocalFilterParams.cpp'),
                        os.path.join(ROOT, 'source', 'VocalFilterDsp.cpp'), '-o', exe], check=True)
        out = subprocess.run([exe, str(selector), str(voice), str(level_comp)], check=True, capture_output=True,
                             text=True)
        return json.loads(out.stdout)


# ---------------------------------------------------------------------------
# The palette - VocalFilterPanelViews.h
# ---------------------------------------------------------------------------
GROUND = (18, 18, 18)
HEADER = (12, 12, 12)
HEADER_RULE = (233, 230, 220)
SECTION = (21, 21, 21)
SECTION_EDGE = (217, 214, 204)
INK = (244, 241, 232)
INK_SOFT = (185, 182, 174)
INK_DIM = (143, 140, 133)
LABEL = (217, 214, 204)
DIVIDER = (90, 88, 82)
RULE = (58, 58, 58)
TICK = (233, 230, 220)
RED = (226, 52, 43)
SCREW = (58, 58, 58)
KNOB_BODY = (10, 10, 10)
KNOB_RIM = (47, 47, 47)
KEY = (27, 27, 27)
KEY_EDGE = (74, 74, 74)
BEZEL = (0, 0, 0)
BEZEL_EDGE = (60, 60, 60)
SCREEN = (8, 9, 11)
GRID = (23, 27, 33)
SCREEN_TEXT = (141, 150, 163)
SCREEN_DIM = (89, 96, 107)
VOWEL_DISC = (18, 19, 22)
VOWEL_EDGE = (60, 64, 71)
LAMP_ON = (255, 59, 47)
LAMP_OFF = (58, 21, 18)
ACCENT = {'amber': (242, 165, 65), 'blue': (86, 168, 245), 'violet': (176, 140, 255), 'mint': (95, 208, 160)}
FORMANT = ['amber', 'blue', 'violet']


# ---------------------------------------------------------------------------
# Drawing helpers, in panel pixels - R2Synth's
# ---------------------------------------------------------------------------
def p(v):
    return v * SS


def pt(x, y):
    return (p(x), p(y))


def from_top(cx, cy, r, deg):
    """The layout's angles: degrees from twelve o'clock, clockwise."""
    t = math.radians(deg)
    return cx + r * math.sin(t), cy - r * math.cos(t)


def vstgui_angle(cx, cy, r, deg):
    """VSTGUI's angles: 0 at three o'clock, clockwise (y down)."""
    t = math.radians(deg)
    return cx + r * math.cos(t), cy + r * math.sin(t)


def stroke(d, pts, colour, width):
    """A polyline with round caps and joins."""
    if len(pts) == 1:
        pts = pts * 2
    d.line([pt(x, y) for x, y in pts], fill=colour, width=max(1, round(p(width))))
    rr = width / 2
    for x, y in pts:
        d.ellipse([p(x - rr), p(y - rr), p(x + rr), p(y + rr)], fill=colour)


def disc(d, cx, cy, radius, fill=None, outline=None, width=1.0):
    d.ellipse([p(cx - radius), p(cy - radius), p(cx + radius), p(cy + radius)], fill=fill, outline=outline,
              width=max(1, round(p(width))) if outline else 0)


def round_rect(d, r, radius, fill=None, outline=None, width=1.0):
    d.rounded_rectangle([p(r[0]), p(r[1]), p(r[2]), p(r[3])], radius=p(radius), fill=fill, outline=outline,
                        width=max(1, round(p(width))) if outline else 0)


def quad(p0, q, p2, steps=8):
    return [((1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * q[0] + t * t * p2[0],
             (1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * q[1] + t * t * p2[1])
            for t in (i / steps for i in range(1, steps + 1))]


def blend(c1, c2, t):
    return tuple(int(round(a + (b - a) * t)) for a, b in zip(c1, c2))



# ---------------------------------------------------------------------------
# The lettering - the glyphs as dump-layout printed them
# ---------------------------------------------------------------------------
L = None


def code_points(text, keep_case=False):
    out = []
    for ch in text:
        c = ord(ch)
        if ord('a') <= c <= ord('z'):
            if not (keep_case and str(c) in L['glyphs']):
                c -= 32
        elif 0xC0 <= c <= 0xFF and c not in (0xD7, 0xF7):
            c = ord('Y') if c == 0xFF else ord('AAAAAAACEEEEIIIIDNOOOOO*OUUUUYTS'[(c & ~0x20) - 0xC0])
        out.append(c)
    return out


def glyph(c):
    return L['glyphs'].get(str(c)) or L['glyphs']['missing']


def placed(text, style):
    x, out = 0.0, []
    for c in code_points(text, style.get('keepCase', False)):
        if c == 32:
            x += L['space']
            continue
        out.append((c, x))
        x += glyph(c)[0] + style['weight'] + style['track']
    return out, x


def text_width(text, style):
    _, end = placed(text, style)
    return (end - style['track']) * style['cap'] / 6 if end > 0 else 0.0


def lettering(d, text, style, colour, x, cy, align='left', stroke_units=None):
    s = style['cap'] / 6
    w = text_width(text, style)
    left = x - w if align == 'right' else (x - w / 2 if align == 'centre' else x)
    top = cy - (style['cap'] + style['weight'] * s) / 2
    half = style['weight'] / 2
    lw = (stroke_units or style['weight']) * s
    glyphs, _ = placed(text, style)
    for c, gx in glyphs:
        ox, oy = left + (gx + half) * s, top + half * s
        at = lambda u, v: (ox + u * s, oy + v * s)
        line, start = [], None
        for op, x1, y1, ex, ey in glyph(c)[1]:
            if op == 'M':
                if len(line) > 0:
                    stroke(d, line, colour, lw)
                line = [at(ex, ey)]
                start = line[0]
            elif op == 'L':
                line.append(at(ex, ey))
            elif op == 'Q':
                line += quad(line[-1], at(x1, y1), at(ex, ey))
            elif op == 'Z':
                line.append(start)
        if line:
            stroke(d, line, colour, lw)


# ---------------------------------------------------------------------------
# The wear - PanelWear.h's marks, composited with their alphas
# ---------------------------------------------------------------------------
def colour(name):
    return tuple(L['wear']['colours'][name])


def layer(img):
    """A clear layer the size of the panel, and something to draw on it."""
    o = Image.new('RGBA', img.size, (0, 0, 0, 0))
    return o, ImageDraw.Draw(o)


def radial(img, cx, cy, radius, rgb, stops):
    """A radial gradient over a disc: `stops` are (fraction of radius,
    alpha), piecewise linear, as CGradient draws them."""
    R = p(radius)
    size = int(math.ceil(R)) * 2 + 2
    mask = Image.new('L', (size, size), 0)
    md = ImageDraw.Draw(mask)
    c = size / 2
    steps = max(8, min(96, int(R / 2)))
    for i in range(steps, -1, -1):          # outside in: each ring overwrites the last
        t = i / steps
        a = stops[-1][1]
        for (t0, a0), (t1, a1) in zip(stops, stops[1:]):
            if t0 <= t <= t1:
                a = a0 + (a1 - a0) * ((t - t0) / (t1 - t0) if t1 > t0 else 0)
                break
        rr = R * t
        md.ellipse([c - rr, c - rr, c + rr, c + rr], fill=int(round(a)))
    patch = Image.new('RGBA', (size, size), rgb + (0,))
    patch.putalpha(mask)
    x, y = int(round(p(cx) - c)), int(round(p(cy) - c))
    # clip the patch to the panel before compositing
    sx, sy = max(0, -x), max(0, -y)
    ex, ey = min(size, img.width - x), min(size, img.height - y)
    if ex > sx and ey > sy:
        img.alpha_composite(patch.crop((sx, sy, ex, ey)), (x + sx, y + sy))


def soft_disc(img, cx, cy, radius, rgb, alpha):
    radial(img, cx, cy, radius, rgb, [(0, alpha), (0.5, alpha * 0.55), (1, 0)])


def wear_line(d, line, rgb):
    """Half strength in the first and last thirds, as wearLine() draws it."""
    pts = [(line['pts'][i], line['pts'][i + 1]) for i in range(0, len(line['pts']), 2)]
    n = len(pts)
    if n < 2:
        return
    a, b = max(1, n // 3), max(max(1, n // 3) + 1, n - n // 3)
    half = line['a'] // 2
    stroke(d, pts[0:a + 1], rgb + (half,), line['w'])
    stroke(d, pts[a:min(b + 1, n)], rgb + (line['a'],), line['w'])
    if b < n - 1:
        stroke(d, pts[b:n], rgb + (half,), line['w'])


def panel_wear(img):
    W = L['wear']
    dust = colour('dust')
    for x, y, r, a in W['haze']:
        soft_disc(img, x, y, r, dust, a)
    for r, fx, fy, a in W['drifts']:
        o, od = layer(img)
        if fx:
            n = r[2] - r[0]
            for i in range(int(p(n))):
                t = i / p(n)
                x = p(r[0]) + i if fx < 0 else p(r[2]) - 1 - i
                od.line([(x, p(r[1])), (x, p(r[3]) - 1)], fill=dust + (int(round(a * (1 - t))),))
        else:
            n = r[3] - r[1]
            for i in range(int(p(n))):
                t = i / p(n)
                y = p(r[1]) + i if fy < 0 else p(r[3]) - 1 - i
                od.line([(p(r[0]), y), (p(r[2]) - 1, y)], fill=dust + (int(round(a * (1 - t))),))
        img.alpha_composite(o)
    for cx, cy, inner, peak, outer, a in W['rings']:
        radial(img, cx, cy, outer, dust, [(0, 0), (inner / outer, 0), (peak / outer, a), (1, 0)])
    o, od = layer(img)
    for line in W['scratches']:
        wear_line(od, line, colour('scratch'))
    img.alpha_composite(o)
    o, od = layer(img)
    for x, y, r, a in W['specks']:
        disc(od, x, y, r, fill=colour('speck') + (a,))
    for line in W['fibres']:
        wear_line(od, line, colour('speck'))
    img.alpha_composite(o)


def glass_wear(img, index):
    G = L['wear']['glass'][index]
    o, od = layer(img)
    for ridges in G['prints']:
        for ridge in ridges:
            pts = [(ridge['pts'][i], ridge['pts'][i + 1]) for i in range(0, len(ridge['pts']), 2)]
            stroke(od, pts, colour('print') + (ridge['a'],), ridge['w'])
    img.alpha_composite(o)
    o, od = layer(img)
    for x, y, r, a in G['dust']:
        disc(od, x, y, r, fill=colour('glassDust') + (a,))
    for line in G['fibres']:
        wear_line(od, line, colour('glassDust'))
    img.alpha_composite(o)


def knob_wear(img, kw, cx, cy, body, a):
    """Relative to the pointer at `a` (VSTGUI degrees), as Knob::draw does."""
    cap_r = body * 0.78
    if kw['knurl']:
        o, od = layer(img)
        rr = (cap_r + body) / 2
        w = body - cap_r
        od.ellipse([p(cx - rr - w / 2), p(cy - rr - w / 2), p(cx + rr + w / 2), p(cy + rr + w / 2)],
                   outline=colour('knurlDust') + (kw['knurl'],), width=max(1, round(p(w))))
        img.alpha_composite(o)
    if kw['sheen']:
        soft_disc(img, cx - cap_r * 0.25, cy - cap_r * 0.3, cap_r * 0.55, colour('sheen'), kw['sheen'])
    o, od = layer(img)
    for radius, start, span, width, alpha in kw['scuffs']:
        rr = cap_r * radius
        st = a + start
        od.arc([p(cx - rr), p(cy - rr), p(cx + rr), p(cy + rr)], st, st + span,
               fill=colour('scuff') + (alpha,), width=max(1, round(p(width))))
    for radius, ang, r, alpha in kw['grit']:
        x, y = vstgui_angle(cx, cy, body * radius, a + ang)
        disc(od, x, y, r, fill=colour('knurlDust') + (alpha,))
    img.alpha_composite(o)


def cap(d, cx, cy, radius):
    """A black cap lit from the top left: concentric discs, the light
    drifting towards the highlight as they shrink."""
    steps = 24
    for i in range(steps):
        t = i / (steps - 1)
        rr = radius * (1 - t * 0.85)
        ox, oy = -radius * 0.25 * t, -radius * 0.35 * t
        col = blend((5, 5, 5), (22, 22, 22), min(1, t / 0.4)) if t < 0.4 else blend((22, 22, 22), (58, 58, 58), (t - 0.4) / 0.6)
        disc(d, cx + ox, cy + oy, rr, fill=col)


def knob(img, c, index):
    d = ImageDraw.Draw(img)
    S = L['styles']
    r, dia = c['r'], c['diameter']
    cx, cy = (r[0] + r[2]) / 2, r[1] + dia / 2
    outer = dia / 2
    body = outer - (6 if dia >= 44 else 5)
    a = 135 + 270 * max(0.0, min(1.0, c['value']))
    bipolar = c.get('bipolar', False)

    for i in range(11):
        t = 135 + 27 * i
        long_ = i in (0, 5, 10)
        stroke(d, [vstgui_angle(cx, cy, body + 1.5, t), vstgui_angle(cx, cy, outer - (0 if long_ else 1.8), t)],
               TICK, 1.3 if long_ else 0.9)
    if dia >= 52:
        lo = from_top(cx, cy, outer + 2, -138)
        hi = from_top(cx, cy, outer + 2, 138)
        lettering(d, '-' if bipolar else '0', S['scale'], TICK, lo[0] - 2, lo[1] + 4, 'right')
        lettering(d, '+' if bipolar else '10', S['scale'], TICK, hi[0] + 2, hi[1] + 4)

    teeth = [vstgui_angle(cx, cy, body if i % 2 == 0 else body - 1.4, 360 * i / 48) for i in range(48)]
    d.polygon([pt(x, y) for x, y in teeth], fill=KNOB_BODY, outline=KNOB_RIM)
    cap(d, cx, cy, body * 0.78)
    disc(d, cx, cy, body * 0.78, outline=(0, 0, 0), width=0.8)
    knob_wear(img, L['wear']['controls'][index], cx, cy, body, a)
    d = ImageDraw.Draw(img)
    stroke(d, [vstgui_angle(cx, cy, body * 0.15, a), vstgui_angle(cx, cy, body - 0.5, a)], INK,
           2.2 if dia >= 44 else 1.8)

    top = r[1] + dia + L['knobGap']
    lettering(d, c['reading'], S['value'], INK, cx, top + L['knobValueH'] / 2, 'centre')
    lettering(d, c['label'].upper(), S['label'], LABEL, cx, top + L['knobValueH'] + L['knobLabelH'] / 2, 'centre')




def dashed(d, p0, p1, colour, width, on, off):
    x0, y0 = p0
    x1, y1 = p1
    length = math.hypot(x1 - x0, y1 - y0)
    t = 0.0
    while length > 0 and t < length:
        t1 = min(length, t + on)
        d.line([pt(x0 + (x1 - x0) * t / length, y0 + (y1 - y0) * t / length),
                pt(x0 + (x1 - x0) * t1 / length, y0 + (y1 - y0) * t1 / length)],
               fill=colour, width=max(1, round(p(width))))
        t += on + off


def lamp(img, cx, cy, radius, on):
    if on:
        o, od = layer(img)
        disc(od, cx, cy, radius + 3, fill=LAMP_ON + (64,))
        img.alpha_composite(o)
    disc(ImageDraw.Draw(img), cx, cy, radius, fill=LAMP_ON if on else LAMP_OFF, outline=(0, 0, 0), width=0.6)


def key(img, r, down=False):
    o, od = layer(img)
    round_rect(od, [r[0], r[1] + 1.5, r[2], r[3] + 1.5], 4, fill=(0, 0, 0, 90))
    img.alpha_composite(o)
    d = ImageDraw.Draw(img)
    round_rect(d, [r[0] + 0.5, r[1] + 0.5, r[2] - 0.5, r[3] - 0.5], 4, fill=KEY, outline=KEY_EDGE, width=1)
    # lit faintly from above: a vertical gradient, light to dark
    o, od = layer(img)
    h = p(r[3] - r[1])
    for i in range(int(h)):
        t = i / max(1, h - 1)
        c = (255, 255, 255, int(18 * (1 - t))) if t < 0.5 else (0, 0, 0, int(46 * (t - 0.5) * 2))
        od.line([(p(r[0] + 1), p(r[1]) + i), (p(r[2] - 1), p(r[1]) + i)], fill=c)
    mask = Image.new('L', img.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle([p(r[0] + 0.5), p(r[1] + 0.5), p(r[2] - 0.5), p(r[3] - 0.5)],
                                           radius=p(4), fill=255)
    img.paste(Image.alpha_composite(img, o), (0, 0), mask)


def glass(d, r):
    round_rect(d, [r[0] + 0.5, r[1] + 0.5, r[2] - 0.5, r[3] - 0.5], 4, fill=BEZEL, outline=BEZEL_EDGE, width=1)
    round_rect(d, [r[0] + 2.5, r[1] + 2.5, r[2] - 2.5, r[3] - 2.5], 2.5, fill=SCREEN)


# ---------------------------------------------------------------------------
# The panel
# ---------------------------------------------------------------------------
def backdrop(d):
    W, H, HH = L['width'], L['height'], L['headerHeight']
    S = L['styles']
    d.rectangle([0, 0, p(W), p(H)], fill=GROUND)
    d.rectangle([0, 0, p(W), p(HH)], fill=HEADER)
    d.rectangle([0, p(HH - 2), p(W), p(HH)], fill=HEADER_RULE)

    for x, y in L['screws']:
        disc(d, x, y, 3.2, fill=SCREW, outline=(0, 0, 0), width=0.6)
        stroke(d, [(x - 2.2, y - 2.2), (x + 2.2, y + 2.2)], (34, 34, 34), 0.8)

    wm = L['wordmark']
    cy = HH / 2
    second = wm[0] + text_width('VOCAL', S['wordmark']) + L['wordmarkGap']
    for ink, units in ((INK, None), (HEADER, 0.32)):
        lettering(d, 'VOCAL', S['wordmark'], ink, wm[0], cy, stroke_units=units)
        lettering(d, 'FILTER', S['wordmark'], ink, second, cy, stroke_units=units)
    dv = L['headerDivider']
    d.rectangle([p(dv), p(16), p(dv + 1), p(40)], fill=DIVIDER)

    for sec in L['sections']:
        r = sec['r']
        round_rect(d, [r[0] + 0.5, r[1] + 0.5, r[2] - 0.5, r[3] - 0.5], L['sectionRadius'], fill=SECTION,
                   outline=SECTION_EDGE, width=1)
        b = sec['titleBox']
        round_rect(d, b, 1.5, fill=SECTION, outline=SECTION_EDGE, width=1)
        c = (b[1] + b[3]) / 2
        lettering(d, sec['title'], S['title'], INK, b[0] + 9, c)
        if sec['subtitle']:
            lettering(d, sec['subtitle'], S['subtitle'], INK_SOFT, sec['subtitleLeft'], c)

    for r in L['rules']:
        d.rectangle([p(r[0]), p(r[1]), p(r[2]) - 1, p(r[3]) - 1], fill=RULE)

    for c in L['captions']:
        r = c['r']
        cy = (r[1] + r[3]) / 2
        if c['style'] == 'block':
            lettering(d, c['text'], S['block'], ACCENT[c['accent']], r[0], cy)
        else:
            lettering(d, c['text'], S['note'], INK_SOFT, r[0], cy)


def pad(img):
    d = ImageDraw.Draw(img)
    S = L['styles']
    pr, pp = L['pad'], L['padPlot']
    glass(d, pr)
    for i in range(1, 8):
        x = round(pr[0] + i * (pr[2] - pr[0]) / 8)
        d.rectangle([p(x), p(pr[1] + 1), p(x + 1) - 1, p(pr[3] - 1)], fill=GRID)
    for j in range(1, 6):
        y = round(pr[1] + j * (pr[3] - pr[1]) / 6)
        d.rectangle([p(pr[0] + 1), p(y), p(pr[2] - 1), p(y + 1) - 1], fill=GRID)

    def at(u, v):
        return (pp[0] + u * (pp[2] - pp[0]), pp[1] + v * (pp[3] - pp[1]))

    lettering(d, 'F1 \u2193 close \u2026 open', S['screen'], ACCENT['amber'], pr[0] + 10, pr[1] + 15)
    lettering(d, '\u2190 F2 front \u2026 back', S['screen'], ACCENT['blue'], pr[2] - 10, pr[1] + 15, 'right')
    for hz, u in L['f2Ticks']:
        label = ('%gk' % (hz / 1000)) if hz >= 1000 else '%.0f' % hz
        lettering(d, label, S['scale'], SCREEN_DIM, at(u, 0)[0], pr[3] - 11, 'centre')
    for hz, v in L['f1Ticks']:
        lettering(d, '%.0f' % hz, S['scale'], SCREEN_DIM, pr[0] + 8, at(0, v)[1])

    mine, theirs = L['vowels'][L['voice']], L['vowels'][1 - L['voice']]
    order = L['trapezoid']
    o, od = layer(img)
    pts = [at(theirs[i]['u'], theirs[i]['v']) for i in order]
    for a, b in zip(pts, pts[1:]):
        dashed(od, a, b, INK + (42,), 1.2, 3, 4)
    stroke(od, [at(mine[i]['u'], mine[i]['v']) for i in order], ACCENT['amber'] + (90,), 1.2)
    for vw in theirs:
        x, y = at(vw['u'], vw['v'])
        disc(od, x, y, 5, outline=INK + (76,), width=1.2)
    img.alpha_composite(o)
    d = ImageDraw.Draw(img)
    lettering(d, '\u25cb ' + ('female' if L['voice'] == 0 else 'male') + ' means', S['scale'], SCREEN_DIM,
              pr[2] - 10, pr[3] - 27, 'right')
    rad = L['vowelRadius']
    for i, vw in enumerate(mine):
        x, y = at(vw['u'], vw['v'])
        lit = (i + 1 == L['selector'])
        disc(d, x, y, rad, fill=VOWEL_DISC, outline=ACCENT['amber'] if lit else VOWEL_EDGE, width=1.2)
        lettering(d, vw['ipa'], S['ipaChart'], ACCENT['amber'] if lit else INK, x, y - S['ipaChart']['cap'] / 6,
                  'centre')
        lettering(d, vw['name'], S['label'], SCREEN_TEXT, x, y + rad + 9, 'centre')

    dot = L['dot']
    x, y = at(dot['u'], dot['v'])
    o, od = layer(img)
    disc(od, x, y, 18, fill=INK + (20,))
    img.alpha_composite(o)
    d = ImageDraw.Draw(img)
    disc(d, x, y, 9, fill=INK, outline=(0, 0, 0), width=2)
    tw, th = 118, 36
    left, top = x + 24, y - th / 2
    if left + tw > pr[2] - 4:
        left = x - 24 - tw
    top = max(pr[1] + 26, min(top, pr[3] - th - 4))
    round_rect(d, [left, top, left + tw, top + th], 3, fill=SCREEN, outline=VOWEL_EDGE, width=1)

    def fhz(hz):
        return ('%.2fk' % (hz / 1000)) if hz >= 1000 else '%.0f Hz' % hz
    lettering(d, 'F1 ' + fhz(dot['f1']), S['screen'], ACCENT['amber'], left + 9, top + 10.5)
    lettering(d, 'F2 ' + fhz(dot['f2']), S['screen'], ACCENT['blue'], left + 9, top + 24.5)
    glass_wear(img, 0)


def spectrum(img):
    d = ImageDraw.Draw(img)
    S = L['styles']
    sr, ax = L['spectrum'], L['spectrumAxes']
    plot = [sr[0] + 6, sr[1] + 24, sr[2] - 6, sr[3] - 18]
    glass(d, sr)

    def sx(hz):
        return plot[0] + math.log(hz / ax['minHz']) / math.log(ax['maxHz'] / ax['minHz']) * (plot[2] - plot[0])

    def sy(db):
        return plot[1] + (ax['topDb'] - db) / (ax['topDb'] - ax['floorDb']) * (plot[3] - plot[1])
    for f in (100, 200, 300, 500, 1000, 2000, 3000, 5000):
        x = round(sx(f))
        d.rectangle([p(x), p(sr[1] + 1), p(x + 1) - 1, p(sr[3] - 1)], fill=GRID)
    db = 0.0
    while db >= ax['floorDb']:
        y = round(sy(db))
        d.rectangle([p(sr[0] + 1), p(y), p(sr[2] - 1), p(y + 1) - 1], fill=GRID)
        db -= 12
    for f in (100, 300, 1000, 3000):
        lettering(d, ('%gk' % (f / 1000)) if f >= 1000 else str(f), S['scale'], SCREEN_DIM, sx(f), sr[3] - 9.5,
                  'centre')
    lettering(d, 'live', S['screen'], SCREEN_TEXT, sr[0] + 10, sr[1] + 12)
    f3 = L['formants'][2][0]
    lettering(d, 'F3 ' + (('%.2fk' % (f3 / 1000)) if f3 >= 1000 else '%.0f Hz' % f3), S['screen'], SCREEN_TEXT,
              sr[2] - 10, sr[1] + 12, 'right')

    hz = L['curves']['hz']

    def curve(od, dbs, colour, width, fill=False):
        if fill:
            poly = [(plot[0], plot[3])] + [(sx(f), sy(max(ax['floorDb'], min(ax['topDb'], v))))
                                           for f, v in zip(hz, dbs)] + [(plot[2], plot[3])]
            od.polygon([pt(x, y) for x, y in poly], fill=colour[:3] + (16,))
        run = []
        for i, (f, v) in enumerate(zip(hz, dbs)):
            if v < ax['floorDb']:
                if run:
                    run.append((sx(f), plot[3]))
                    stroke(od, run, colour, width)
                run = []
                continue
            if not run and i > 0:
                run.append((sx(f), plot[3]))
            run.append((sx(f), sy(min(v, ax['topDb']))))
        if len(run) > 1:
            stroke(od, run, colour, width)
    o, od = layer(img)
    for k in range(3):
        curve(od, L['curves']['f%d' % (k + 1)], ACCENT[FORMANT[k]] + (150,), 1.2)
    img.alpha_composite(o)
    o, od = layer(img)
    curve(od, L['curves']['sum'], INK + (255,), 2.2, fill=True)
    img.alpha_composite(o)
    o, od = layer(img)
    for k, (f0, bw, lv) in enumerate(L['formants']):
        c = ACCENT[FORMANT[k]]
        hx, hy = sx(f0), sy(max(ax['floorDb'], min(ax['topDb'], lv)))
        dashed(od, (hx, hy), (hx, plot[3]), c + (80,), 1, 2, 3)
        lo, hi, yb = sx(max(ax['minHz'], f0 - bw / 2)), sx(min(ax['maxHz'], f0 + bw / 2)), hy + 9
        stroke(od, [(lo, yb), (hi, yb)], c + (255,), 2)
        stroke(od, [(lo, yb - 4), (lo, yb + 4)], c + (255,), 1.5)
        stroke(od, [(hi, yb - 4), (hi, yb + 4)], c + (255,), 1.5)
        disc(od, hx, hy, 6.5, fill=SCREEN + (255,), outline=c + (255,), width=2)
    img.alpha_composite(o)
    glass_wear(img, 1)


def buttons(img):
    S = L['styles']
    mine = L['vowels'][L['voice']]
    for i, r in enumerate(L['vowelButtons']):
        key(img, r)
        lamp(img, r[2] - 8, r[1] + 8, 2.6, i + 1 == L['selector'])
        d = ImageDraw.Draw(img)
        cx = (r[0] + r[2]) / 2
        lettering(d, mine[i]['ipa'], S['ipaKey'], INK, cx, r[1] + 16 - S['ipaKey']['cap'] / 6, 'centre')
        lettering(d, mine[i]['name'], S['label'], INK, cx, r[3] - 11.5, 'centre')
    m = L['manual']
    key(img, m)
    my = (m[1] + m[3]) / 2
    lamp(img, m[0] + 18, my, 4.5, L['selector'] == 0)
    d = ImageDraw.Draw(img)
    lettering(d, 'MANUAL', S['pill'], INK, m[0] + 32, my)
    lettering(d, L['manualHint'], S['hint'], INK_SOFT, m[2] - 12, my, 'right')

    g = L['glideMeter']
    slot = [g[0] + 0.5, g[1] + 2, g[2] - 0.5, g[1] + 10]
    round_rect(d, slot, 2, fill=BEZEL, outline=BEZEL_EDGE, width=1)
    for i in range(1, 10):
        x = round(slot[0] + i * (slot[2] - slot[0]) / 10)
        d.rectangle([p(x), p(slot[1] + 1.5), p(x + 1) - 1, p(slot[3] - 1.5)], fill=GRID)
    lettering(d, 'at rest', S['value'], LABEL, (g[0] + g[2]) / 2, g[1] + 23, 'centre')


def switch(d, c):
    """Panel::SegmentSwitch::draw. With a reading (Level Comp) the slot is
    the top of the rectangle, and the reading and label sit at its foot on
    the knobs' lines."""
    S = L['styles']
    whole = c['r']
    r = list(whole)
    if c['hasReading']:
        r[3] = r[1] + L['switchSlot']
        cx = (whole[0] + whole[2]) / 2
        top = whole[3] - L['switchReadingBand']
        lit = L['levelComp'] != 0
        lettering(d, c['reading'], S['value'], INK if lit else LABEL, cx, top + L['knobValueH'] / 2, 'centre')
        lettering(d, c['label'].upper(), S['label'], LABEL, cx, top + L['knobValueH'] + L['knobLabelH'] / 2,
                  'centre')
    round_rect(d, [r[0] + 0.6, r[1] + 0.6, r[2] - 0.6, r[3] - 0.6], 3, fill=HEADER, outline=SECTION_EDGE, width=1.2)
    names = c['names']
    segw = (r[2] - r[0] - 4) / len(names)
    cur = int(round(c['value']))
    for i, nm in enumerate(names):
        seg = [r[0] + 2 + i * segw, r[1] + 2, r[0] + 2 + (i + 1) * segw, r[3] - 2]
        if i == cur:
            round_rect(d, seg, 2, fill=INK)
        lettering(d, nm, S['pill'], GROUND if i == cur else INK_DIM, (seg[0] + seg[2]) / 2, (seg[1] + seg[3]) / 2,
                  'centre')


def header(img):
    d = ImageDraw.Draw(img)
    S = L['styles']
    er = L['engineLine']
    lettering(d, L['engine'], S['engine'], INK_SOFT, er[0], (er[1] + er[3]) / 2)
    bp = L['bypass']
    round_rect(d, [bp[0] + 0.6, bp[1] + 0.6, bp[2] - 0.6, bp[3] - 0.6], 3, fill=HEADER, outline=RED, width=1.2)
    cy = (bp[1] + bp[3]) / 2
    lamp(img, bp[0] + 18, cy, 4.5, False)
    d = ImageDraw.Draw(img)
    lettering(d, 'BYPASS  OFF', S['pill'], INK, bp[0] + 32, cy)


def main():
    global L
    out_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'docs', 'panel.png')
    selector = int(sys.argv[2]) if len(sys.argv) > 2 else 0
    voice = int(sys.argv[3]) if len(sys.argv) > 3 else 0
    level_comp = int(sys.argv[4]) if len(sys.argv) > 4 else 0
    L = dump(selector, voice, level_comp)
    L['width'], L['height'] = L['size']
    L['headerHeight'] = L['header']

    img = Image.new('RGBA', (p(L['width']), p(L['height'])), GROUND + (255,))
    backdrop(ImageDraw.Draw(img))
    panel_wear(img)
    header(img)
    pad(img)
    spectrum(img)
    buttons(img)
    for i, c in enumerate(L['controls']):
        if c['kind'] == 'knob':
            knob(img, c, i)
        else:
            switch(ImageDraw.Draw(img), c)
    img = img.convert('RGB').resize((L['width'] * SCALE, L['height'] * SCALE), Image.LANCZOS)
    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    img.save(out_path)
    print('wrote %s (%dx%d)' % (out_path, img.width, img.height))


if __name__ == '__main__':
    main()
