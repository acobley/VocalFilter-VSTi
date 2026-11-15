# VocalFilter 1.3.0.0

A **three-formant vocal-tract model** for macOS, as a **VST3** and an **Audio
Unit**. Stereo in, stereo out. Feed it anything with harmonics and it puts a
vowel on it.

Three bandpass resonators per channel, in **parallel** rather than cascaded, so
each formant carries its own amplitude — which is the point of dialling a vowel
by hand. F2 is summed **inverted**: three bandpasses added in phase cancel
between the peaks and dig a null a real tract does not have.

Five vowels — Aaaa, Eeee, Iiii, Oooo, Uuuu — in an **adult male and an adult
female voice**, the frequencies taken from Peterson & Barney's (1952) measured
means. The formant *levels* are derived rather than chosen: a vocal tract is an
all-pole filter, so the amplitudes follow from the frequencies, and each pair
is fitted against the all-pole cascade those frequencies imply.

A **Glide** control slides every parameter to its new value on one timed ramp,
so however far each has to travel they all arrive on the same sample.

## What changed since 1.2.0.0

- **Level Comp: optional make-up gain.** A formant bank throws most of the
  spectrum away, so fully wet the output is roughly 14–16 LU quieter than the
  input. The amount changes with every vowel, so one fixed trim can't put it
  back. Level Comp works the gain out from the filter itself (vowel, voice,
  formant settings and Dry / Wet), so pink noise comes out within half an LU
  of where it went in. Other material lands near that. It follows a glide and
  adds no latency. It can't pump or lift noise in pauses, because it never
  listens to the audio.
- **A rotary selector for it on the panel,** in the Output section: OFF / ON,
  in the same chicken-head style as R2Synth's black panel. The make-up being
  applied is read out under it, live ("+16.0 dB").
- **Level Comp is off by default.** New instances, and every project saved
  with an earlier version, sound exactly as before. It can be automated like
  any other parameter.
- The note that sat right of Trim now sits under the OUTPUT title.

The vowel tables, the filter and every other parameter are unchanged.
Projects saved with 1.2.0.0 open as they were.

## Installing

Download `VocalFilter-1.3.0.0.pkg` and open it. The installer offers the two
formats separately:

    /Library/Audio/Plug-Ins/VST3/VocalFilter.vst3
    /Library/Audio/Plug-Ins/Components/VocalFilter.component

Quit your DAW first, because a running host holds the old copy open. Signed
with a Developer ID and notarised by Apple.

Requires macOS 10.13 or later.

## Two deliberate behaviours

Both of these are on purpose, and both would otherwise arrive as bug reports:

* **While a vowel is selected, the formant knobs show the preset and do not
  change the sound.** Vowel is a *mode*, not a recall: the preset's own values
  are what you hear, and the processor reads the mode itself so it works with
  the editor closed and under host automation. Turn a formant knob, drag on
  the vowel chart or press **Manual**, and it takes the preset's current
  values over and switches to *Manual*, so nothing jumps at the handover.
* **A Glide of zero still takes 20 ms.** An instant jump of F2 from 870 Hz to
  2290 Hz is an audible click, so the shortest glide is floored just above it.

One more that is easy to misread: changing **Voice** glides exactly as changing
vowel does. It just moves F2 by at most 3.8 semitones against up to 16.8, so
the same Glide time sounds far less dramatic.

## Known gaps

* **No output level meter.** The panel has no meter yet; use the host's.
* **Level Comp is exact for pink noise only.** Material with a different
  spectrum lands near its input loudness, not exactly on it. A test on real
  material measured a 16.9 LU loss where the model makes up 14–16 dB.
* **The lettering's O and 0 are the same shape**, so the button names
  Oooo and Iiii read as OOOO and IIII in the face's capitals; the IPA symbol
  above each name tells them apart.
* **No preset browser and no factory presets** beyond the five vowels.
  The host's own preset handling works; there is nothing shipped to browse.
* **Formant frequency ranges are bounded per formant** (F1 200–1200, F2
  500–3000, F3 1500–4000 Hz) rather than free, so a child or a very deep voice
  cannot be dialled in at the extremes. The vowel chart covers exactly these
  ranges.
* **Both voices share one set of formant levels.** Refitting them for the
  female tract puts one formant outside its parameter's range and leaves /i/
  with no F2 peak at all; the measurements are in `ENGINEERING-NOTES.md` §2d.

## Uninstalling

A `.pkg` never will, so:

```sh
sudo rm -rf /Library/Audio/Plug-Ins/VST3/VocalFilter.vst3
sudo rm -rf /Library/Audio/Plug-Ins/Components/VocalFilter.component
```

## Checksum

Taken from the finished, notarised and **stapled** package. Stapling changes
the bytes, so a checksum taken before it does not match what you downloaded.

```
SHA-256: <paste from: shasum -a 256 installer/VocalFilter-1.3.0.0.pkg>
```

---

Copyright 2026 A. E. Cobley. CC BY-SA 4.0.
VST is a trademark of Steinberg Media Technologies GmbH.
