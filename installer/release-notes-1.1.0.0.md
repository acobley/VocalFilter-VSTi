# VocalFilter 1.1.0.0

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

## What changed since 1.0.1.0

- **A new panel, built around a vowel chart you can play.** The window is now
  1200 × 640 and has four sections:
  - **Vowel space**: F2 runs across and F1 down, the way phoneticians plot
    vowels. The five vowels of the current voice are labelled circles; the
    other voice's are faint dots. **Click a vowel** to select it, or **drag
    anywhere** to move F1 and F2 together (Shift for fine). A white dot shows
    where the sound is right now, and during a glide its path is drawn. The
    vowel buttons, **Manual**, **Glide** and a glide-progress meter sit
    underneath.
  - **Spectrum**: each formant in its own colour (F1 amber, F2 blue, F3
    violet), plus the summed response in white, on fixed axes (80 Hz – 8 kHz,
    +12 to −66 dB), so two vowels can be compared.
  - **Formants**: Freq, Width and Level knobs for each of the three formants.
  - **Output**: Dry / Wet and Trim. **Bypass** is the pill in the header.

  Knobs: drag vertically, Shift for fine, double-click for the default, the
  wheel steps.
- **Output Trim now goes up to +12 dB** (it was −60 … 0 dB), so a mix that
  the resonators have thinned out can be brought back up.

The sound itself, the vowel tables and the plug-in's identity are unchanged.
**Projects saved with earlier versions still open** with VocalFilter in place,
and a saved Trim setting comes back at the same level in dB.

**One thing to check after upgrading:** host **automation** of Output Trim
recorded with an earlier version can't be converted, because the host stores
it as a position on the old range. Those lanes now play **up to 12 dB
louder**: a lane that sat at 0 dB now reaches +12 dB. Redraw them, or pull
them down by the same amount. Trim settings saved in a project (not
automated) are converted and are not affected.

## Installing

Download `VocalFilter-1.1.0.0.pkg` and open it. The installer offers the two
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
SHA-256: <paste from: publish-release.sh writes this>
```

---

Copyright 2026 A. E. Cobley. CC BY-SA 4.0.
VST is a trademark of Steinberg Media Technologies GmbH.
