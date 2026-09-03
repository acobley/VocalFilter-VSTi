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
// departure from the original table as a DEVIATION in PORTING-NOTES.md.
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

	kNumParams
};

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

/** Look a definition up by id, range-checked. Returns kParams[kOutputTrim]
    for anything unknown - including kBypass, which is not in the table. */
const ParamDef& paramDef (Steinberg::Vst::ParamID id);

/** True for an id the table actually describes. */
inline bool isTableParam (Steinberg::Vst::ParamID id) { return id < kNumParams; }

//------------------------------------------------------------------------
} // namespace VocalFilter
