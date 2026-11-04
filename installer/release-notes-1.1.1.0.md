# VocalFilter 1.1.1.0

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

## What changed since 1.1.0.0

- **Fully dry, or Trim at the bottom, no longer costs CPU.** Both settings
  are exactly zero, and the smoother heading for zero never got there: about
  15 seconds later it settled on a tiny "denormal" number for good, and on
  Intel Macs every sample then made two to four slow multiplies. Dry / Wet
  and Trim now land on exactly their target.
- **Flush-to-zero on the audio thread.** Each processing block now runs with
  the CPU's flush-to-zero mode on, and puts the host's setting back
  afterwards, so a signal decaying towards silence can never slow the
  plug-in down.

The sound, the vowel tables, parameters and saved projects are exactly as in
1.1.0.0.

## Installing

Download `VocalFilter-1.1.1.0.pkg` and open it. The installer offers the two
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
SHA-256: <paste from: shasum -a 256 installer/VocalFilter-1.1.1.0.pkg>
```

---

Copyright 2026 A. E. Cobley. CC BY-SA 4.0.
VST is a trademark of Steinberg Media Technologies GmbH.
