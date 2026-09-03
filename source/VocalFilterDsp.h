//------------------------------------------------------------------------
// VocalFilter - the audio line
//
// DELIBERATELY FREE OF SDK TYPES. Nothing in this file or its .cpp may
// include a VST3 header. Two reasons, both from the porting guide:
//
//   * it compiles and runs standalone with plain `c++ -std=c++17`, so the
//     numbers it produces can be tested for real before anything is built;
//   * VST3 splits the processor and the controller into separate
//     components, so anything the editor displays that the DSP computes
//     must come from ONE shared function both call. Those functions live
//     here.
//
// Right now the line is a pass-through with an output trim. Port the real
// DSP in line for line and resist improving it: odd gain staging and
// apparent bugs are the sound.
//------------------------------------------------------------------------

#pragma once

#include <cmath>

namespace VocalFilter {

//------------------------------------------------------------------------
// Shared mappings - call these from BOTH the DSP and the editor
//------------------------------------------------------------------------

/** The output trim in dB, from the parameter's normalised value. */
double trimDb (double normalized);

/** A linear gain from dB, with the bottom of the trim's travel treated as
    a true silence rather than -60 dB. */
double dbToLinear (double db);

/** The bottom and top of the trim's travel. Top of travel is unity: for a
    port, make it the ORIGINAL's gain staging - nothing lost, one turn
    away - and set the default 20 dB below it. */
constexpr double kTrimMinDb = -60.0;
constexpr double kTrimMaxDb =   0.0;
constexpr double kTrimDefaultDb = 0.0;

//------------------------------------------------------------------------
/** The audio line. Stereo in, stereo out; 32-bit float internally, which
    is what these plug-ins were written for, with a 64-bit host converted
    into it by the processor rather than refused. */
class Dsp
{
public:
	/** Everything rate-dependent is recomputed here, never inside
	    process(): a hard-coded coefficient puts its corner at a fixed
	    fraction of Nyquist and the same patch is an octave brighter at
	    96 k than at 44.1 k. */
	void setSampleRate (double sampleRate);
	double sampleRate () const { return mSampleRate; }

	/** Silence the state. Called from setActive, not from the audio
	    thread. */
	void reset ();

	/** The trim's target, normalised. Smoothed per SAMPLE inside
	    process - a block-rate step on a mixed output is itself a click. */
	void setTrimNormalized (double normalized);

	/** Snap the smoother to its target, for a state load or a reset where
	    a 10 ms ramp from the old value would be wrong. */
	void snapParameters ();

	/** in and out may alias. `frames` may be zero. */
	void process (const float* inLeft, const float* inRight,
	              float* outLeft, float* outRight, int frames);

	/** Latency in samples, for the processor's getLatencySamples. Zero
	    while the line is a pass-through. */
	int latencySamples () const { return 0; }

	/** How long the line keeps ringing after its input goes silent, in
	    samples. Zero while there is no tail. */
	int tailSamples () const { return 0; }

private:
	double mSampleRate = 44100.0;

	double mTrim = 1.0;         // current linear gain
	double mTrimTarget = 1.0;
	double mTrimCoeff = 0.0;    // one-pole, ~10 ms
};

//------------------------------------------------------------------------
} // namespace VocalFilter
