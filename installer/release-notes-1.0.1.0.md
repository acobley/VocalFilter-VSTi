# VocalFilter 1.0.1.0

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
so however far each has to travel they all arrive on the same sample. The
display draws all three formants and their sum, following the DSP as it moves.

## What changed since 1.0.0.1

- **The maker is now "AE Cobley"**, in both formats — it was "A. E. Cobley".
  REAPER files an Audio Unit's presets under its name cut off at the first
  full stop, so every A. E. Cobley Audio Unit shared one REAPER preset list
  and showed the others' presets, which did nothing when chosen. The
  plug-in's identity codes are unchanged, so **projects saved with earlier
  versions still open** with VocalFilter in place.

Nothing in the sound changed.

**After upgrading**, some things stay where earlier versions left them:

- Presets **you** saved in a host that files them by maker are in
  `~/Library/Audio/Presets/A. E. Cobley/VocalFilter`. Move them to
  `~/Library/Audio/Presets/AE Cobley/VocalFilter` to see them again.
- In REAPER, AU presets you saved earlier stay in REAPER's old shared list
  and will not appear under the new name. The VST3 is unaffected.
- Some hosts list the plug-in under its maker; look under **AE Cobley**.

## Installing

Download `VocalFilter-1.0.1.0.pkg` and open it. The installer offers the two
formats separately:

    /Library/Audio/Plug-Ins/VST3/VocalFilter.vst3
    /Library/Audio/Plug-Ins/Components/VocalFilter.component

Quit your DAW first — a running host holds the old copy open. Signed with a
Developer ID and notarised by Apple.

Requires macOS 10.13 or later.

## Two deliberate behaviours

Both of these are on purpose, and both would otherwise arrive as bug reports:

* **While a vowel button is lit, the nine formant sliders do nothing.** Vowel
  is a *mode*, not a recall — the preset's own values are what you hear, and
  the processor reads the mode itself so it works with the editor closed and
  under host automation. Touch any slider and it takes the preset's current
  values over and switches back to *Manual*, so nothing jumps at the handover.
* **A Glide of zero still takes 20 ms.** An instant jump of F2 from 870 Hz to
  2290 Hz is an audible click, so the shortest glide is floored just above it.

One more that is easy to misread: changing **Voice** glides exactly as changing
vowel does. It just moves F2 by at most 3.8 semitones against up to 16.8, so
the same Glide time sounds far less dramatic.

## Known gaps

* **No preset browser and no factory presets** beyond the five vowel buttons.
  The host's own preset handling works; there is nothing shipped to browse.
* **Formant frequency ranges are bounded per formant** (F1 200–1200, F2
  500–3000, F3 1500–4000 Hz) rather than free, so a child or a very deep voice
  cannot be dialled in at the extremes.
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

Taken from the finished, notarised and **stapled** package — stapling changes
the bytes, so a checksum taken before it does not match what you downloaded.

```
SHA-256: <paste from: shasum -a 256 installer/VocalFilter-1.0.1.0.pkg>
```

---

Copyright 2026 A. E. Cobley. CC BY-SA 4.0.
VST is a trademark of Steinberg Media Technologies GmbH.
