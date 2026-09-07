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

It is worth having because three things about this plug-in are easier to see
than to read:

* **F2 is summed inverted.** Between F1 and F2 the two branches are half a
  turn apart, so the same sign makes them cancel and digs a null an all-pole
  tract does not have. Dropping the inversion leaves every peak exactly where
  it is and moves only the valleys, which is why the test for it looks at a
  valley.
* **Vowel is a mode, not a recall.** On a preset the DSP takes that vowel's
  nine values and the nine parameters are ignored. The processor reads it, not
  the controller, which is what makes it work with the editor closed.
* **The display reads published values, not parameters.** The nine read-only
  hidden parameters exist because a message sent from `process()` is silently
  discarded by the host's connection proxy.

## panel.png / panel-vowel.png

The editor's layout, drawn by `tools/render-panel.py`. Not screenshots and not
mock-ups: the script **parses the layout constants out of
`source/VocalFilterEditor.h`** and the vowel names out of
`source/VocalFilterDsp.h` and evaluates them in declaration order, so a
control in the wrong place here is in the wrong place in the plug-in. If a
constant ever becomes an expression the script cannot evaluate, it fails
loudly rather than drawing a layout that is not the one that will ship.

```sh
python3 tools/render-panel.py                           # Manual, male: no button lit
python3 tools/render-panel.py docs/panel-vowel.png 1 0  # Aaaa, male voice
python3 tools/render-panel.py docs/panel-female.png 2 1 # Eeee, female voice
```

The third argument is the voice, so the female formants can be **looked at**
rather than reasoned about. `panel-female.png` is female Eeee, whose F2 and F3
sit only 520 Hz apart — the closest pair either table produces, and the case
that decides whether the display's axis needs changing. It does not: they
resolve about 11 px apart on the 288 px plot, and narrowing the axis from
8 kHz to 5 kHz would buy one pixel.

`panel.png` is the state the plug-in loads in; `panel-vowel.png` shows a
vowel button lit, which is what the Vowel parameter looks like on a preset
whether it got there from a click or from automation.

Looking at these caught two faults that reading the code had not: F3's skirts
drew a flat line along the bottom of the response display instead of ending,
and the legend ran off the plate when "Sum" was added to it.

The response curves in `render-panel.py` are a **re-implementation** of
`bandpassMagnitude` for the mock — the real one is in `VocalFilterDsp.cpp` and
is what the plug-in draws. A small difference between the picture and the
plug-in is expected; what these renders check is that the panel *fits*.
