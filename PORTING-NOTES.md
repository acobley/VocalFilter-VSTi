# VocalFilter — porting notes

The deliverable that outlives the session. Record the **symptom**, the
**measurement** and the **decision** — not just the change. When a
measurement here turns out to be wrong, correct it in place and say so.

---

## 0. What this is, as of the first commit

A new plug-in scaffolded from `~/DXi-DEv/vst3-port-template`, **not yet a
port of anything**. There is no DXi behind it: the audio line is a
pass-through with an output trim, and every ported decision below is
therefore still open.

| Decision | Settled as | Why |
|---|---|---|
| Kind | **Audio effect**, `PlugType::kFx` | stereo in / stereo out, no event input, nothing reads a MIDI queue |
| Buses | **Stereo in, stereo out, and nothing else** | asked for; `setBusArrangements` refuses every other layout and `resource/au-info.plist` lists only `2/2` to match — auval is strict about the two agreeing |
| Sample formats | 32- and 64-bit accepted | the line is float, as these plug-ins were; a 64-bit host is converted through `mScratchIn/Out` rather than refused |
| Editor | **None yet** | guide step 6: get a silent plug-in validating — processor, controller, entry — and fix everything there, while there is little code to search. The editor is built last |
| Custom controls | **None yet** | see "Building the editor" below for where the SpyBand set lives when you want it |
| Parameters | one, `kOutputTrim` | a placeholder that proves host → parameter → DSP end to end |

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
  can be tested for real long before the plug-in is built — `tests/` does
  exactly that;
* VST3 splits the processor and the controller into separate components, so
  anything the editor displays that the DSP computes must come from **one
  shared function both call**. `trimDb()` and `dbToLinear()` are the first
  two. A private copy in the editor will diverge at some sample rate you
  never test.

Everything rate-dependent is recomputed in `setSampleRate`, never inside
`process`: a hard-coded coefficient puts its corner at a fixed fraction of
Nyquist, so the same patch is over an octave brighter at 96 k than at 44.1 k.

### Output level, measured before anything was played

`tests/DspTests.cpp`, default patch, full-scale 440 Hz / 660 Hz stereo sine:

```
peak L 1.000000 (-0.00 dBFS)   peak R 0.999998 (-0.00 dBFS)
```

Unity, as a pass-through must be. **This measurement is the one to repeat
first when the real DSP goes in**, at 1, 2, 4, 8 and 16 voices if it turns
out to be polyphonic. These plug-ins predate loudness discipline and their
gain staging often cancels itself; a level that clips masks other faults and
sends you chasing the wrong bug — velocity appears not to work, and attacks
get reported as clicks.

The trim's top of travel is currently 0 dB. For a real port, **make the top
of travel the ORIGINAL's staging** — nothing lost, one turn away — and set
the default 20 dB below it. `kTrimMaxDb` and `kTrimDefaultDb` in
`VocalFilterDsp.h` are the two constants to move.

The trim is smoothed per **sample** (one-pole, 10 ms). A block-rate step on a
mixed output is itself a click; the suite asserts no single sample of a
full-travel move jumps by more than 1 %.

---

## 2. Parameters

`VocalFilterParams.h` carries the three-range shape the SpaceDub, ForTran and
SpyBand ports all settled on: VST3 normalised, the DXi **external** range so
displayed numbers match the original, and the DXi **internal** range via
`toInternal()` reproducing `MapToInternal` so the DSP is handed numerically
identical values. There is no DXi here yet, so for `kOutputTrim` internal ==
plain — say so explicitly for each real parameter as it lands, and mark every
departure from the original table as a **DEVIATION** in this file.

Two rules that cost real time elsewhere:

* **Append new parameters, never insert.** An id that moves loads a saved
  project's value into the wrong control.
* **`kBypass` is 1000**, far past the end of the table. Everything that
  indexes `kParams` range-checks first; `paramDef()` does it for you.

`setState` resets anything a **short** stream does not mention back to its
default. Without that, loading an old project after a new one inherits the
new one's settings for every parameter added since. The processor and the
controller read the identical layout — if one changes, both change.

---

## 3. Deliberate omissions, and where the trap is when you undo them

**No `processContextRequirements`.** Since VST3 3.7 the `ProcessContext` is
opt-in and the default is *no flags*, so a plug-in that reads
`data.processContext->tempo` without asking gets 120 in every host, with no
error anywhere — the validator prints `ProcessContextRequirements: - None`
rather than complaining. Nothing here syncs to anything. The moment something
does, add `processContextRequirements.needTempo ();` to the processor's
constructor.

**No processor → controller messages.** When you add one, remember every
message travels on the **UI thread**: `sendMessage` from `process()` returns
success and is then silently discarded by the host's connection proxy.
Per-block values from the DSP go out through `data.outputParameterChanges`
with a `kIsReadOnly` parameter instead. Messages are fine from `setActive`,
`setState` and `notify`.

**No editor.** See below.

---

## 4. Building the editor, when you get there

The controller has an `EDITOR HOOK` block listing the five overrides to add
and the order to add them in.

The trap worth reading twice is in `editorDestroyed`: **`dynamic_cast`
returns null inside `~EditorView()`**, which is one of its two callers.
Compare upcast pointers — `static_cast<EditorView*> (e) == editor` — or the
editor list keeps a dangling pointer that the next `setParamNormalized`
follows.

**The SpyBand control set is the nearest model** and is designed to be
lifted: `~/DXi-DEv/SpyBand-VSTi/source/SpyBandControls.{h,cpp}` holds

* `SpySlider` — horizontal drag slider, progress bar, green label, red value
* `SpyToggle` — two-state
* `SpySelector` — multi-state, left click steps down, right click steps up
* `SpyFileButton`, `SpyPatchBoard`, `SpyLedColumn`, `SpyBandMeter`

none of which is a bitmap — every one draws itself with shapes and text, so
there is no artwork to recover. Copy the file, rename the namespace, and keep
`SpyBandEditor.{h,cpp}` open beside it as the worked example of wiring them
to parameters. `~/DXi-DEv/ForTran-VSTi/` is the larger example: 145
parameters and a bitmap UI rebuilt from the `.rc`.

Two VSTGUI traps that will bite immediately:

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
SDK=~/DXi-DEv/SpyBand-VSTi/external/vst3sdk
for f in source/*.cpp; do
  g++ -c -std=c++17 -DLINUX=1 -DRELEASE=1 -I$SDK -I$SDK/vstgui4 -Isource \
      -o /tmp/$(basename $f .cpp).o $f || echo "FAILED $f"
done
nm -C /tmp/*.o | grep " U " | grep VocalFilter::   # must all be defined elsewhere
```

`-DRELEASE=1` is required or `fdebug.h` refuses to compile. Compile to an
**object file** and `nm -C` it rather than using `-fsyntax-only`: a scripted
edit that matched nothing leaves a header declaring a function no one
defined, and syntax-only will not catch it.

The DSP suite:

```sh
c++ -std=c++17 -O2 -Isource tests/DspTests.cpp source/VocalFilterDsp.cpp \
    -o /tmp/dsptests && /tmp/dsptests
```

All fourteen assertions pass as of the first commit. Test 4 is a **negative
control** for test 3 — a guard that has never failed is a guess.

**Before shipping**, run the SDK validator and
`auval -v aufx VcFl AECo`. Debug a validator segfault with
`lldb -- build/bin/Debug/validator <bundle>`; the usual cause is a null title
or units reaching `RangeParameter`, which dereferences both without a check.

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

The build was run and both bundles are in place:

```
build/VST3/Release/VocalFilter.vst3
build/VST3/Release/VocalFilter.component
build/VST3/Debug/VocalFilter.vst3
```

Sources are globbed with `CONFIGURE_DEPENDS`, so there is no file list to
maintain — at the cost of one wrinkle under the Xcode generator: the first
build after you **add** a source file compiles the old file list anyway, and
the SDK post-build check reports *"Bundle does not export the required
'GetPluginFactory' function"*. A PRE_BUILD check catches it and names what
changed; re-run `./setup-xcode.sh --no-open` and build again.
