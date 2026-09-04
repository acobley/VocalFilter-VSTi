//------------------------------------------------------------------------
// VocalFilter - parameter definitions
//
// One table, read by BOTH the processor and the controller, so a range or
// a default cannot be typed twice and disagree.
//
// Every parameter carries THREE ranges, which is the shape the SpaceDub,
// ForTran and SpyBand ports all settled on:
//
//   * VST3 normalised, 0..1, which is what the host works in;
//   * "plain", the DXi *external* range, so the numbers on screen match
//     the numbers the original showed;
//   * "internal", what the DSP was actually handed, via toInternal(),
//     reproducing ParamInfo::MapToInternal.
//
// There is no DXi behind this plug-in yet, so for the one parameter below
// internal == plain. Say so when you add the real ones, and mark every
// departure from the original table as a DEVIATION in ENGINEERING-NOTES.md.
//
// APPEND NEW PARAMETERS, NEVER INSERT. An id that moves loads a saved
// project's value into the wrong control.
//------------------------------------------------------------------------

#pragma once

#include "VocalFilterDsp.h"

#include "pluginterfaces/vst/vsttypes.h"

namespace VocalFilter {

//------------------------------------------------------------------------
enum Param : Steinberg::Vst::ParamID
{
	kOutputTrim,     // dB, top of travel is unity

	//--------------------------------------------------------------------
	// The formant bank. APPENDED after kOutputTrim, which is why the trim
	// is still id 0 and heads the host's list: an id that MOVES loads a
	// saved project's value into the wrong control, and list order is a
	// far smaller price than that.
	//
	// Three per formant, in this order, so
	//
	//     id = kFormantBase + formant * 3 + field
	//
	// and formantParam() below is the only place that arithmetic lives.
	//--------------------------------------------------------------------
	kF1Freq, kF1Bandwidth, kF1Level,        // 1, 2, 3
	kF2Freq, kF2Bandwidth, kF2Level,        // 4, 5, 6
	kF3Freq, kF3Bandwidth, kF3Level,        // 7, 8, 9

	kMix,                                   // 10  dry .. wet, %
	kGlide,                                 // 11  vowel transition time, ms

	//--------------------------------------------------------------------
	// The vowel selector, appended last. Manual plus the five presets.
	//
	// This parameter is a MODE, and the processor acts on it: while it is
	// on a preset, the DSP takes that preset's nine values and IGNORES
	// parameters 1..9. That is what makes it work with the editor closed,
	// offline, and under automation - none of which a controller-side
	// implementation would survive.
	//
	// The price is that a host's own generic parameter list still shows
	// whatever 1..9 were last set to while a preset is selected. The
	// plug-in's own panel does not have that problem: it displays the
	// preset's values, and touching a slider captures them into 1..9 and
	// switches back to Manual so nothing jumps.
	//--------------------------------------------------------------------
	kVowel,                                 // 12  0 = Manual, 1..5 = presets

	//--------------------------------------------------------------------
	// WHERE THE DSP ACTUALLY IS, published by the processor for the
	// response display. Read-only and hidden, so no host shows them and
	// nothing outside the plug-in can write them.
	//
	// This is the route a per-block value takes from the processor to the
	// controller, and the reason it is not a
	// message is that a message sent from process() is silently discarded
	// by the host's connection proxy - it returns success and does
	// nothing. data.outputParameterChanges is the mechanism that works.
	//
	// Same layout as the nine above: base + formant * 3 + field, in the
	// same units, so paramDef() converts them back with no second table.
	//--------------------------------------------------------------------
	kLiveF1Freq, kLiveF1Bandwidth, kLiveF1Level,     // 13, 14, 15
	kLiveF2Freq, kLiveF2Bandwidth, kLiveF2Level,     // 16, 17, 18
	kLiveF3Freq, kLiveF3Bandwidth, kLiveF3Level,     // 19, 20, 21

	kNumParams
};

constexpr Steinberg::Vst::ParamID kFormantBase = kF1Freq;

/** Which of a formant's three parameters. */
enum FormantField { kFieldFreq = 0, kFieldBandwidth = 1, kFieldLevel = 2 };

constexpr Steinberg::Vst::ParamID formantParam (int formant, FormantField field)
{
	return static_cast<Steinberg::Vst::ParamID> (kFormantBase + formant * 3 + field);
}

constexpr Steinberg::Vst::ParamID kLiveBase = kLiveF1Freq;

/** The published counterpart of one formant field. */
constexpr Steinberg::Vst::ParamID liveParam (int formant, FormantField field)
{
	return static_cast<Steinberg::Vst::ParamID> (kLiveBase + formant * 3 + field);
}

inline bool isLiveParam (Steinberg::Vst::ParamID id)
{
	return id >= kLiveBase && id < kNumParams;
}

/** Everything up to here is saved in the plug-in's state; the published
    values are not, because they are a view of the DSP rather than a
    setting. Both sides of setState/getState stop at this id. */
constexpr Steinberg::Vst::ParamID kNumStoredParams = kLiveBase;

/** The VST3 bypass, which hosts expect. 1000 is the convention, and it is
    far past the end of kParams - so RANGE-CHECK every id before indexing
    the table. paramDef() below does. */
constexpr Steinberg::Vst::ParamID kBypass = 1000;

//------------------------------------------------------------------------
enum class ParamType { Float, Bool, Enum, Int };

//------------------------------------------------------------------------
struct ParamDef
{
	Steinberg::Vst::ParamID id;
	const char* title;          // NEVER null - RangeParameter dereferences it
	const char* units;          // NEVER null; "" when the value string carries its own
	ParamType   type;
	double      plainMin;
	double      plainMax;
	double      plainDefault;
	double      internalMin;    // range the DSP expects
	double      internalMax;
	int         stepCount;      // 0 = continuous
	bool        smoothed;       // ramped per sample rather than per block

	//--------------------------------------------------------------------
	double toPlain (double normalized) const
	{
		return plainMin + normalized * (plainMax - plainMin);
	}

	double toNormalized (double plain) const
	{
		if (plainMax == plainMin)
			return 0.0;
		return (plain - plainMin) / (plainMax - plainMin);
	}

	/** Reproduces ParamInfo::MapToInternal from the DXi: booleans snap,
	    enums and integers round, floats scale. */
	double toInternal (double normalized) const
	{
		if (type == ParamType::Bool)
			return (normalized < 0.5) ? 0.0 : 1.0;

		const double v = internalMin + normalized * (internalMax - internalMin);
		if (type == ParamType::Enum || type == ParamType::Int)
			return static_cast<double> (static_cast<long> (v + 0.5));
		return v;
	}

	double defaultNormalized () const { return toNormalized (plainDefault); }
};

//------------------------------------------------------------------------
extern const ParamDef kParams[kNumParams];

/** The plain value of a parameter, given the whole normalised set. Used by
    the processor to feed the DSP and by anything that needs a number in
    the units the panel shows. */
inline double plainValue (const double* normalized, Steinberg::Vst::ParamID id)
{
	return kParams[id].toPlain (normalized[id]);
}

/** Look a definition up by id, range-checked. Returns kParams[kOutputTrim]
    for anything unknown - including kBypass, which is not in the table. */
const ParamDef& paramDef (Steinberg::Vst::ParamID id);

/** True for one of the nine formant parameters - the ones the vowel
    selector overrides while it is on a preset. */
inline bool isFormantParam (Steinberg::Vst::ParamID id)
{
	return id >= kFormantBase && id < kFormantBase + kFormantCount * 3;
}

/** True for an id the table actually describes. */
inline bool isTableParam (Steinberg::Vst::ParamID id) { return id < kNumParams; }

//------------------------------------------------------------------------
} // namespace VocalFilter
