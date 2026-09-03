# VocalFilter — porting notes

The deliverable that outlives the session. Record the **symptom**, the
**measurement** and the **decision** — not just the change. When a
measurement here turns out to be wrong, correct it in place and say so.

---

## 0. What this is

A plug-in scaffolded from `~/DXi-DEv/vst3-port-template`, **not a port of
anything** — there is no DXi behind it. The audio line is a three-formant
vocal-tract model: on each channel, three bandpass resonators in parallel,
summed, mixed against the dry signal and trimmed.

| Decision | Settled as | Why |
|---|---|---|
| Kind | **Audio effect**, `PlugType::kFx` | stereo in / stereo out, no event input, nothing reads a MIDI queue |
| Buses | **Stereo in, stereo out, and nothing else** | `setBusArrangements` refuses every other layout and `resource/au-info.plist` lists only `2/2` to match — auval is strict about the two agreeing |
| Sample formats | 32- and 64-bit accepted | the line is float, as these plug-ins were; a 64-bit host is converted through `mScratchIn/Out` rather than refused |
| Filter topology | **parallel**, not cascaded | a parallel bank lets each formant carry its own amplitude, which is the whole point of setting a vowel by hand. A cascade derives the relative levels from the pole positions and gives you no say in them |
| Parameters | eleven: 3 × (freq, width, level), dry/wet, output trim | section 2 |
| Editor | **Yes** | the silent shell was validated first — guide step 6 — and the panel came afterwards |
| Custom controls | `SpySlider`, `SpyToggle`, `SpySelector`, lifted from SpyBand, plus `SpyPresetButton` | section 4 |
| Vowel presets | five buttons — A E I O U — each writing the nine formant parameters and nothing else | section 2 |

```
     in --+--> BP(F1, B1) * A1 --+
          |                      |
          +--> BP(F2, B2) * A2 --+--> wet --+
          |                      |          |
          +--> BP(F3, B3) * A3 --+          +--> mix --> trim --> out
          |                                 |
          +--------- dry -------------------+
```

### Identity — permanent from the first shipped build

| | |
|---|---|
| Class UIDs | `VocalFilterIDs.h`, generated fresh from `os.urandom` |
| VST3 bundle id | `audio.vocalfilter.vst3` |
| AU bundle id | `audio.vocalfilter.audiounit` |
| AU type / subtype / manufacturer | `aufx` / `VcFl` / `AECo` |

`AECo` is shared with SpaceDub, ForTran and SpyBand; only the subtype tells
them apart. Change any of these after a build has shipped and every existing
session silently loses the plug-in.

---

## 1. The audio line

`source/VocalFilterDsp.{h,cpp}` is **deliberately free of SDK types** and no
VST3 header may enter it. Two reasons:

* it compiles and runs standalone with plain `c++ -std=c++17`, so its numbers
  can be tested for real — `tests/` measures the actual magnitude response
  and checks where the peaks land;
* VST3 splits the processor and the controller into separate components, so
  anything the editor displays that the DSP computes must come from **one
  shared function both call**. `trimDb`, `dbToLinear`, `bandpassResponse`,
  `bandpassMagnitude` and `bankMagnitude` are those functions.

The processor hands the DSP **plain units** — hertz, hertz, decibels — never
normalised values, so nothing in `VocalFilterDsp` has to know what a `ParamID`
is or what range a host was shown. The parameter table is the only place the
two representations meet.

### The filter

RBJ cookbook bandpass, **constant 0 dB peak gain** form (`b0 = alpha`, *not*
`b0 = Q*alpha`). The other spelling makes the peak gain equal to Q, so
narrowing a formant would make it louder and the Width slider would be
arguing with the Level slider. `tests/DspTests.cpp` section 4 measures the
peak at three bandwidths and requires it to stay within 0.15 dB of unity,
which is the assertion that catches it.

Everything rate-dependent is recomputed in `setSampleRate`, never inside
`process`. Section 5 of the suite sweeps 44.1 k to 192 k and requires the
response at the three formant centres to stay within 0.25 dB.

Coefficients are recomputed **every 16 samples** rather than every sample — a
biquad update is a sin and a cos — while the parameters feeding them are
smoothed **per sample** over 20 ms. Slamming F2 end to end every 64 samples
moves the output by at most 0.04 between adjacent samples.

Two clamps, and both are load-bearing: the centre frequency is held below
0.45 × Nyquist, where the bilinear transform's warping stops a bandpass
behaving like one, and **Q is capped at 60**. Nothing stops a host automating
Width to its minimum while Freq is at its maximum, which is a Q of 200 and a
filter that rings for a second and a half.

### DEVIATION 1 — `bankMagnitude` sums COMPLEX responses, and had to be told to

The first version of the response function summed the three formants'
**magnitudes**. The test that compares the running filter's measured impulse
response against the curve the editor would draw failed at **0.255 absolute,
at 997 Hz** — a quarter of full scale, right in the valley between F1 and F2.

The cause is that a bandpass runs from +90° below its centre to −90° above,
so at 997 Hz the F1 branch is most of a half-turn away from the F2 branch and
the two partly cancel. **The dip between two formants is where it is because
of the phase between them.** Summing magnitudes puts it in the wrong place
and makes it far too shallow.

With the complex sum the same test reads **4.29e-09**. This is the porting
guide's warning about a suite that compared only magnitudes while the
transcription quietly computed phases nobody looked at — the same trap, found
by writing the test first.

---

## 2. Parameters, and the Aaa patch

Eleven, in id order. `kOutputTrim` is still **id 0** and so heads the host's
list, because the formant parameters were **appended** to it rather than
inserted before it: an id that moves loads a saved project's value into the
wrong control, and list order is a far smaller price than that.

```
 0  Output Trim   dB   -60 .. 0        default   0
 1  F1 Freq       Hz   200 .. 1200     default 730
 2  F1 Width      Hz    20 .. 400      default  80
 3  F1 Level      dB   -40 .. +12      default   0
 4  F2 Freq       Hz   500 .. 3000     default 1090
 5  F2 Width      Hz    20 .. 400      default  90
 6  F2 Level      dB   -40 .. +12      default  -7
 7  F3 Freq       Hz  1500 .. 4000     default 2440
 8  F3 Width      Hz    20 .. 400      default 120
 9  F3 Level      dB   -40 .. +12      default -12
10  Dry / Wet      %     0 .. 100      default 100
```

`kBypass` is 1000, far past the end of the table; everything that indexes
`kParams` range-checks first. `formantParam(formant, field)` is the only
place the `base + n*3 + field` arithmetic lives, and `VocalFilterParams.cpp`
holds `static_assert`s that prove it agrees with the hand-written table — a
row in the wrong place is exactly the kind of mistake that presents as *"the
Width slider moves the Level"*.

Every range is **linear in its plain unit**, so `RangeParameter` round-trips
`getParamStringByValue` / `getParamValueByString` exactly and no `toString` /
`fromString` override is needed. If a non-linear mapping is ever wanted,
override **both**, and make it exact at the default: the validator round-trips
each parameter at its current value and warns above 1e-4.

### Each formant gets its own frequency range

Not one wide range shared by all three. A slider spanning 100 Hz to 4 kHz
wastes most of its travel on settings that are not a vowel, and per-formant
ranges keep F1 < F2 < F3 without a constraint to enforce. They are generous
enough for every English vowel and then some.

### The factory patch: /ɑ/ as in "father"

| | F1 | F2 | F3 |
|---|---|---|---|
| Frequency | 730 Hz | 1090 Hz | 2440 Hz |
| Bandwidth | 80 Hz | 90 Hz | 120 Hz |
| Level | 0 dB | −7 dB | −12 dB |

**Frequencies** are the classic adult-male means from Peterson & Barney
(1952). Women and children run higher — roughly 850 / 1220 / 2810 for
women — so this patch is a **male** /ɑ/ specifically, and F2 in particular is
what makes it /ɑ/ rather than /ɔ/ or /æ/.

**Bandwidths** sit mid-range of the measured adult values. The published
spread is wide and method-dependent: about 50–140 Hz for B1, 62–149 for B2
and 67–223 for B3 across studies, so 80 / 90 / 120 is a defensible middle
rather than any one paper's number.

**Levels are the part a parallel bank makes you choose.** They do not follow
from the frequencies the way they would in a cascade — they are set. F1
dominates in a low back vowel, so the higher formants are stepped down from
it. These are a starting point for the ear, not a measurement.

Measured peaks, from the running filter's impulse response: **728, 1100 and
2452 Hz** — within 0.9 % of what was asked for. The error is the bilinear
transform's frequency warping plus the 1 Hz search grid, and it is well under
the ~5 % that would start to read as a different vowel.

### Output level, measured before anything was played

Full-scale input, factory patch:

```
  sine  110 Hz  ->  -29.13 dBFS        saw   82.4 Hz  ->  -12.44 dBFS
  sine  220 Hz  ->  -23.80 dBFS        saw  110.0 Hz  ->  -12.87 dBFS
  sine  730 Hz  ->   +0.05 dBFS        saw  146.8 Hz  ->  -11.60 dBFS
  sine 1090 Hz  ->   -6.37 dBFS
  sine 2440 Hz  ->  -11.72 dBFS
```

The only case that reaches unity is a sine sitting **exactly** on F1, which is
what a 0 dB constant-peak bandpass is supposed to do. A harmonically rich
source — the input this is actually for — comes out around −12 dBFS, because
a formant bank throws most of the spectrum away. So the trim's top of travel
stays at unity and its default stays there with it; there is no gain staging
to be 20 dB below, because there is no original.

Nothing clips inside a float plug-in, but this is the measurement to repeat
first after any change to the levels or the topology. A level that clips masks
other faults and sends you chasing the wrong bug.

### The five vowel buttons — A, E, I, O, U

`kVowels` in `VocalFilterDsp.h`. The letters are as they are *said*, so E is
/i/ ("ee") and I is /ɪ/, not the diphthong /aɪ/ the letter names.

| Button | Sound | F1 | F2 | F3 | B1 | B2 | B3 |
|---|---|---|---|---|---|---|---|
| Aaaa | /ɑ/ father | 730 | 1090 | 2440 | 80 | 90 | 120 |
| Eeee | /i/ beet | 270 | 2290 | 3010 | 50 | 100 | 140 |
| Iiii | /ɪ/ bit | 390 | 1990 | 2550 | 60 | 100 | 130 |
| Oooo | /o/ boat | 450 | 900 | 2400 | 60 | 90 | 120 |
| Uuuu | /u/ boot | 300 | 870 | 2240 | 50 | 90 | 120 |

**Frequencies** are Peterson & Barney adult-male means, so all five sit in one
consistent voice rather than being collected from wherever — with **one
exception, marked in the source**: the letter O names a diphthong, /oʊ/, and
P&B measured only monophthongs, so that row carries the widely used /o/ set
instead.

**Bandwidths** are chosen within the measured adult spread, narrower at B1 for
the close vowels /i/ and /u/, because bandwidth rises with formant frequency
and those two have the lowest F1 of the set.

Measured peaks, from each preset's own impulse response: **728/1100/2452,
270/2290/3023, 390/1991/2565, 449/907/2410, 300/876/2249 Hz** — every one
within 0.6 % of its table value.

`VocalFilterParams.cpp` carries a `static_assert` per vowel proving **every
preset is reachable by its sliders** and that F1 < F2 < F3. A preset outside a
parameter's range does not fail loudly: `toNormalized` returns something
outside 0..1, the host clamps it, and the button quietly recalls a different
vowel from the one on its face. Eeee is the one that would go first — its F3
is 3010 Hz, most of the way up the F3 range.

### DEVIATION 2 — the levels are one shared profile, and that is a decision

All five presets carry the same 0 / −7 / −12 dB balance. Per-vowel levels were
**tried and rejected**, and the rejection is the useful part.

The attempt was to derive them the way a cascade synthesiser implies: build the
all-pole cascade of the three resonances, evaluate its magnitude at each
formant centre, and set the parallel bank's level so the peak heights match.
That is principled, and it produces this:

| | A1 | A2 | A3 |
|---|---|---|---|
| Aaaa | 0.0 | −3.3 | **−30.9** |
| Eeee | 0.0 | −16.9 | **−26.9** |
| Iiii | 0.0 | −10.5 | **−19.3** |
| Oooo | 0.0 | −8.5 | **−38.5** |
| Uuuu | 0.0 | −13.0 | **−40.9** |

F3 between −27 and −41 dB is inaudible, and the reason is that a bare cascade
of three unity-DC resonators has **neither the source's spectral tilt nor a
higher-pole correction** — a real vocal tract has poles above F3 whose skirts
hold the upper spectrum up, and a real glottal source is not flat. Modelling
either properly is a much larger job than this plug-in is.

There is no published parallel-bank amplitude table covering these five, so
rather than invent one and dress it up as a measurement, every button recalls
the same balance and the Level sliders are where you shape it. The test suite
asserts the profile is shared, so if per-vowel levels are ever derived
properly this note is what fails and points at what was rejected.

---

## 3. Deliberate omissions, and where the trap is when you undo them

**No `processContextRequirements`.** Since VST3 3.7 the `ProcessContext` is
opt-in and the default is *no flags*, so a plug-in that reads
`data.processContext->tempo` without asking gets 120 in every host, with no
error anywhere — the validator prints `ProcessContextRequirements: - None`
rather than complaining. Nothing here syncs to anything. The moment something
does (an LFO on F1, say), add `processContextRequirements.needTempo ();` to
the processor's constructor.

**No processor → controller messages.** When you add one — a response curve
or a level meter would want one — remember every message travels on the **UI
thread**: `sendMessage` from `process()` returns success and is then silently
discarded by the host's connection proxy. Per-block values from the DSP go out
through `data.outputParameterChanges` with a `kIsReadOnly` parameter instead.
Messages are fine from `setActive`, `setState` and `notify`.

**`getTailSamples` is real now.** A resonator decays as exp(−π·B·t), so 60 dB
takes about 7/(π·B) seconds; the narrowest formant's tail is reported, which is
28 ms at the factory patch. A host that cuts processing at the end of a region
would otherwise chop the ring off.

---

## 4. The editor

`source/VocalFilterControls.{h,cpp}` is **lifted verbatim** from
`~/DXi-DEv/SpyBand-VSTi/source/SpyBandControls.*`, with three changes and no
others: the namespace is `VocalFilter`; the vocoder-specific views are gone
(`SpyFileButton`, `SpyPatchBoard`, `SpyLedColumn`, `SpyBandMeter`,
`IPatchBoardListener`); and the banner says so.

**The class names are deliberately unchanged.** `SpySlider` is still
`SpySlider`, so `diff` against SpyBand's copy shows only what genuinely
differs and a fix made in either can be carried to the other by hand.
Renaming them would buy tidiness and cost that.

They port at all because the DXi property page these came from **used no
bitmaps**: every control drew itself with GDI rectangles and text over the
panel. The colours in `namespace Colours` are the originals, taken from
`SlideSpin::PaintBk` rather than matched by eye.

`VocalFilterEditor` has no `.rc` behind it and no artwork to recover, so
unlike the SpyBand and ForTran editors nothing is converted from dialog
units: the layout is in pixels, computed from one grid, and the grid
constants in the header are the only thing to change to resize the panel. A
column per formant and a row per field — F1 F2 F3 across, Freq / Width /
Level down — because **a vowel is a shape across that grid** and putting the
three side by side is what makes one legible at a glance. Dry/Wet and Output
Trim take a bottom row of their own.

Each slider's readout is formatted from the **same table the host reads**, so
the panel and the host cannot disagree about what a control says.

The background is a colour, not a bitmap: `CColor(64,64,64)`, which is
SpyBand's own fallback — the colour its editor paints *under* the artwork so
a missing file reads as a dark panel rather than as whatever the host left in
the window.

### The vowel buttons

`SpyPresetButton` is new — not in SpyBand. The nearest thing there was
`SpyFileButton`, which is a SlideSpin with its indicator turned on and a click
handler on the part that is not the lamp; this is that idea with the lamp
taken off and the parameter taken away.

It is a `CControl` only to inherit `SpySlider`'s text fitting. It **carries no
tag and never calls `valueChanged`, `beginEdit` or `endEdit`**, so a host sees
nothing when it is clicked except the nine parameters the handler then writes.
Every mouse handler is overridden for that reason — `SpySlider`'s would drag a
value that is not there. The click fires on mouse **up**, and only if the
pointer is still inside: pressing a vowel and sliding off it is how you change
your mind.

`applyVowel` writes each parameter as a **complete gesture** —
`beginEdit` / `setParamNormalized` / `performEdit` / `endEdit` — so the host
records it as something it can automate and undo rather than as nine
unexplained jumps, and every open editor's slider follows because
`setParamNormalized` comes back through `updateControl`.

**Dry/Wet and Output Trim are deliberately not touched.** They are how the
plug-in is set up in a mix; the vowel is what it is saying.

### Verifying the layout without building

`tools/render-panel.py` draws the panel to `docs/panel.png` — the SlideSpin
geometry, the colours, the readouts — so it can be **looked at**. The
constants are not duplicated in it: they are parsed out of
`VocalFilterEditor.h` and evaluated in declaration order, and the vowel names
out of `VocalFilterDsp.h`, so the picture cannot drift from the code. If a
constant becomes an expression the script cannot evaluate, it fails loudly
rather than drawing a layout that is not the one that will ship.

The panel is **348 × 229**. Run it after any layout change and look at the
result; arithmetic that says two controls do not overlap has been wrong before.

### The trap in `editorDestroyed`

**`dynamic_cast` returns null inside `~EditorView()`**, which is one of its two
callers. By then the `VocalFilterEditor` sub-object is gone, the cast yields
null, the entry survives as a dangling pointer, and the next
`setParamNormalized` walks it. `VocalFilterController::editorDestroyed`
compares **upcast** pointers instead, which is well defined at every point in
the destruction sequence. This one cost real time on SpaceDub.

Two more that will bite the moment this panel grows a container:

* **`CViewContainer::drawRect` never calls `draw()`.** A `draw()` override on
  a container is dead code that compiles and produces nothing. Override
  `drawBackgroundRect`, and note the context is already translated by the
  container's origin — draw in local coordinates from `(0,0)`.
* **A view's size is in its PARENT's coordinates.** Anything positioning
  itself from `control->getViewSize()` lands hundreds of pixels away for a
  control inside a container. Walk the parent chain adding offsets.

---

## 5. Verification, and what cannot be done from here

The cloud session reaches this Mac through a **Linux** VM, so **cmake, Xcode,
the SDK validator and `auval` cannot be run from a Claude session — you
build.** What *was* run, and what to re-run after any scripted edit:

```sh
# every translation unit, semantically checked against the vendored SDK
SDK=external/vst3sdk
for f in source/*.cpp; do
  g++ -c -std=c++17 -Wall -Wextra -Wno-multichar -Wno-unused-parameter \
      -DLINUX=1 -DRELEASE=1 -I$SDK -I$SDK/vstgui4 -Isource \
      -o /tmp/$(basename $f .cpp).o $f 2>/tmp/$(basename $f).log
done
nm -C /tmp/*.o | grep " U " | grep VocalFilter::   # must all be defined elsewhere
```

Seven translation units, **zero errors and zero warnings in our own sources**,
every undefined `VocalFilter::` symbol defined in another object.

`-DRELEASE=1` is required or `fdebug.h` refuses to compile. Compile to an
**object file** and `nm -C` it rather than using `-fsyntax-only`: a scripted
edit that matched nothing leaves a header declaring a function no one
defined, and syntax-only will not catch it. Do not pipe the compiler into
`head` — the closed pipe kills it with SIGPIPE and you get a missing object
file and no error message.

The DSP suite:

```sh
c++ -std=c++17 -O2 -Isource tests/DspTests.cpp source/VocalFilterDsp.cpp \
    -o /tmp/dsptests && /tmp/dsptests
```

Twenty-eight assertions, all passing. The ones worth knowing about:

* **§3 compares the RUNNING filter against the curve the editor would DRAW**,
  across 240 log-spaced bins from 50 Hz to 16 kHz. This is the one that caught
  DEVIATION 1.
* **§4 proves the peak gain is constant** across bandwidths, and that the
  measured −3 dB width is the width that was asked for, within 6 %.
* **§6 is a no-op proof with a negative control**: at Dry/Wet 0 % the filters
  still run and the output must be **bit-identical** to the input; at 100 % it
  must not be. A guard that has never failed is a guess.
* **§8 drives every extreme of every range at four sample rates** with
  full-scale noise and requires the output to stay finite and bounded.
* **§2b measures all five vowel presets** and checks no two are the same, that
  every one has F1 < F2 < F3, and that the level profile really is shared.

Compare spectra, not samples, when a filter changes: four poles delay the
signal even where their magnitude is flat, so two runs with identical spectra
differ at every sample and a sample-difference test would fail on a correct
change.

**Before shipping**, run the SDK validator and `auval -v aufx VcFl AECo`.
Debug a validator segfault with `lldb -- build/bin/Debug/validator <bundle>`;
the usual cause is a null title or units reaching `RangeParameter`, which
dereferences both without a check.

---

## 6. Building

```sh
cd ~/DXi-DEv/VocalFilter-VSTi
./setup-xcode.sh
```

**DECISION: this project keeps its OWN copy of the SDK** in `external/`
(247 MB, `vst3sdk` + `AudioUnitSDK`), cloned by the first configure, rather
than pointing at the copy SpyBand already has via `VST3_SDK_ROOT`. The reason
is shipping: VocalFilter may go out separately from the other plug-ins, and a
project whose SDK lives inside a sibling's folder is not self-contained — the
tree is either buildable on its own or it is not. `external/` is
`.gitignore`d, so this costs disk and a few minutes on the first configure,
nothing in the repo.

Sources are globbed with `CONFIGURE_DEPENDS`, so there is no file list to
maintain — at the cost of one wrinkle under the Xcode generator: the first
build after you **add** a source file compiles the old file list anyway, and
the SDK post-build check reports *"Bundle does not export the required
'GetPluginFactory' function"*. A PRE_BUILD check catches it and names what
changed; re-run `./setup-xcode.sh --no-open` and build again. **This applies
now** — `VocalFilterControls.cpp` and `VocalFilterEditor.cpp` are new since
the last successful configure.
