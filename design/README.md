# VocalFilter interface concepts

Three editor ideas built on R2Synth's design: the same palette, section cards, knobs, segment switches and screens. `source/common.js` is the shared kit, copied unchanged from ForTran's Designs folder. `source/vf.js` adds what VocalFilter needs.

The response curves are **computed, not drawn**. They are RBJ constant-0 dB-peak bandpasses at 48 kHz, summed as complex values + − + the way `Dsp::process` does it. They use the male and female vowel tables from `VocalFilterDsp.h`.

| | |
|---|---|
| `VocalFilter-Concept1-ThreeStrips.png` | R2Synth section for section: Vowel in the oscillators' place, Formants (display + 9 knobs) in the filter's, Output in the amp's, and a Glide strip showing the last ramp. |
| `VocalFilter-Concept2-DisplayFirst.png` | The response display is the editor. Each formant is a handle you drag for frequency and level (wheel for width). Three slider cards underneath give the same nine values as text, with the other voice ghosted for comparison. |
| `VocalFilter-Concept3-VowelSpace.png` | An F1 × F2 vowel chart you can play. The five presets sit where a phonetician would plot them, and the puck sets F1 and F2 together. Pressing a vowel slides the puck along the glide. F3, the widths and the levels are on knobs. |

Each concept is 1200 × 640 and is rendered at 2×. To re-render:

    python3 render.py Concept1-ThreeStrips.html=VocalFilter-Concept1-ThreeStrips.png   # etc.
