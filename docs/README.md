# docs

Everything here is **generated from the source**, not drawn or captured by
hand, so a picture that disagrees with the code is a bug in a script rather
than a stale file nobody noticed.

## signal-path.html / signal-path.png

The routing, transcribed from `Dsp::process` and
`VocalFilterProcessor::pushParameters`. The HTML is the source and the PNG is
rendered from it with headless Chromium, so the picture stays regenerable
rather than replaceable:

```sh
python3 - <<'PY'
from playwright.sync_api import sync_playwright
import os
with sync_playwright() as p:
    b = p.chromium.launch()
    pg = b.new_page(viewport={"width": 1240, "height": 1000}, device_scale_factor=2)
    pg.goto("file://" + os.path.abspath("docs/signal-path.html"))
    pg.screenshot(path="docs/signal-path.png", full_page=True)
    b.close()
PY
```

Blue is audio, amber is control — what the filters are *told* to be — and
grey dashed is observation. **Nothing drawn dashed is in the audio path.**

Regenerate it after any change to the routing: the HTML is hand-authored and
the PNG is rendered from it, so the two only stay in step deliberately.

It is worth having because four things about this plug-in are easier to see
than to read:

* **F2 is summed inverted.** Between F1 and F2 the two branches are half a
  turn apart, so the same sign makes them cancel and digs a null an all-pole
  tract does not have. Dropping the inversion leaves every peak exactly where
  it is and moves only the valleys, which is why the test for it looks at a
  valley.
* **Vowel is a mode, not a recall.** On a preset the DSP takes that vowel's
  nine values and the nine parameters are ignored. The processor reads it, not
  the controller, which is what makes it work with the editor closed. **Voice**
  picks which of the two tables that preset comes from and is read the same
  way — which is why it appears in the control plane rather than beside the
  filters.
* **The display reads published values, not parameters.** The nine read-only
  hidden parameters exist because a message sent from `process()` is silently
  discarded by the host's connection proxy. The Level Comp gain is a tenth,
  for the reading under its selector.
* **Level Comp is computed, not measured.** Its gain comes from a model of
  the whole line in the control plane: the formants as they glide, plus
  Dry / Wet. It never listens to the audio, so it sits in the audio path as a
  plain gain stage between Dry / Wet and Trim.

## Guards

`python3 tools/check-editor.py` — `setValueNormalized` may appear in the
editor only inside `showValue`, which pairs it with `invalid()`. Without that
pairing a control holds the right value and repaints only when something else
dirties its region, which is how the Voice switch came to follow the mouse but
not host automation.

## panel.png / panel-vowel.png / panel-female.png

The panel, drawn by `tools/render-panel.py`. Not screenshots and not
mock-ups. `tools/dump-layout.cpp` is compiled against
**`source/VocalFilterLayout.h`** — the tables `VocalFilterEditor::open` builds
the panel from — and prints them as JSON together with every knob's reading
and the four response curves, computed by the DSP's own `bandpassMagnitude`
and `bankMagnitude`. The script only draws what it is given, in the panel's
palette. A control in the wrong place here is in the wrong place in the
plug-in, and the curves are the plug-in's, not a re-implementation.

```sh
python3 tools/render-panel.py                           # Manual, male: what a fresh instance loads
python3 tools/render-panel.py docs/panel-vowel.png 1 0  # Aaaa, male voice
python3 tools/render-panel.py docs/panel-female.png 4 1 # Oooo, female voice
python3 tools/render-panel.py /tmp/comp.png 1 0 1       # Aaaa, male, Level Comp on (+16.0 dB)
```

The fourth argument turns Level Comp on. The committed images leave it off,
as a fresh instance does.

It needs a C++ compiler, Pillow, and the SDK's headers in `external/vst3sdk`.
What it cannot show: the glide (it draws the panel at rest), hover and press
states, and VSTGUI's own anti-aliasing. Since the black panel (10 Nov 2026)
it needs no fonts: the lettering is stroked from the glyphs `dump-layout`
prints from `source/SciFiLettering.h`, and the wear is the marks it prints
from `source/PanelWear.h`, so the picture and the plug-in draw the same
strokes and the same dust.

The layout is also held to its rules by `tests/LayoutTests.cpp` (run by
`tools/run-tests.sh`): every parameter with a control appears once, nothing
overlaps or leaves its section, the widest reading of every knob fits it, the
vowel chart's axes are exactly the F1 and F2 parameter ranges, and no two
vowel circles are close enough for a click to be ambiguous.

Two lessons from the previous panel's renders carry over to the spectrum: the
floor is −66 dB, below every preset's deepest notch (the worst is female Aaaa
at −61.5), and a curve that does leave the scale is drawn down to the bottom
edge rather than stopping at the last sample that happened to be on it, which
left an arc floating mid-plot that read as the curve turning back up.
