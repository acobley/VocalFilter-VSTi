//------------------------------------------------------------------------
// VocalFilter - the parameter table
//
// Every range and default here comes from a constant in VocalFilterDsp.h,
// so the DSP's idea of what a formant may be and the host's idea cannot
// drift apart. The factory patch is kAaaFormants - the vowel /a/ as in
// "father"; the numbers, their sources and why the levels are a choice
// rather than a consequence are in that header and in PORTING-NOTES
// section 2.
//
// The formants are numbered F1..F3 on the panel and in the host, the way
// phonetics numbers them, while the array index behind them is 0..2. The
// rows are written out rather than generated, so a title can be read
// straight against the id it belongs to.
//------------------------------------------------------------------------

#include "VocalFilterParams.h"

namespace VocalFilter {

//------------------------------------------------------------------------
// id                              title           units type              plainMin             plainMax             plainDefault                    internalMin          internalMax          steps smoothed
//------------------------------------------------------------------------
const ParamDef kParams[kNumParams] =
{
	{ kOutputTrim,                  "Output Trim",  "dB", ParamType::Float, kTrimMinDb,          kTrimMaxDb,          kTrimDefaultDb,                 kTrimMinDb,          kTrimMaxDb,          0,    true },

	{ kF1Freq,                      "F1  Freq",     "Hz", ParamType::Float, kFormantFreqMin[0],  kFormantFreqMax[0],  kAaaFormants[0].freqHz,         kFormantFreqMin[0],  kFormantFreqMax[0],  0,    true },
	{ kF1Bandwidth,                 "F1  Width",    "Hz", ParamType::Float, kBandwidthMin,       kBandwidthMax,       kAaaFormants[0].bandwidthHz,    kBandwidthMin,       kBandwidthMax,       0,    true },
	{ kF1Level,                     "F1  Level",    "dB", ParamType::Float, kLevelMinDb,         kLevelMaxDb,         kAaaFormants[0].levelDb,        kLevelMinDb,         kLevelMaxDb,         0,    true },

	{ kF2Freq,                      "F2  Freq",     "Hz", ParamType::Float, kFormantFreqMin[1],  kFormantFreqMax[1],  kAaaFormants[1].freqHz,         kFormantFreqMin[1],  kFormantFreqMax[1],  0,    true },
	{ kF2Bandwidth,                 "F2  Width",    "Hz", ParamType::Float, kBandwidthMin,       kBandwidthMax,       kAaaFormants[1].bandwidthHz,    kBandwidthMin,       kBandwidthMax,       0,    true },
	{ kF2Level,                     "F2  Level",    "dB", ParamType::Float, kLevelMinDb,         kLevelMaxDb,         kAaaFormants[1].levelDb,        kLevelMinDb,         kLevelMaxDb,         0,    true },

	{ kF3Freq,                      "F3  Freq",     "Hz", ParamType::Float, kFormantFreqMin[2],  kFormantFreqMax[2],  kAaaFormants[2].freqHz,         kFormantFreqMin[2],  kFormantFreqMax[2],  0,    true },
	{ kF3Bandwidth,                 "F3  Width",    "Hz", ParamType::Float, kBandwidthMin,       kBandwidthMax,       kAaaFormants[2].bandwidthHz,    kBandwidthMin,       kBandwidthMax,       0,    true },
	{ kF3Level,                     "F3  Level",    "dB", ParamType::Float, kLevelMinDb,         kLevelMaxDb,         kAaaFormants[2].levelDb,        kLevelMinDb,         kLevelMaxDb,         0,    true },

	{ kMix,                         "Dry / Wet",    "%",  ParamType::Float, kMixMin,             kMixMax,             kMixDefault,                    kMixMin,             kMixMax,             0,    true },
	{ kGlide,                       "Glide",        "ms", ParamType::Float, kGlideMinMs,         kGlideMaxMs,         kGlideDefaultMs,                kGlideMinMs,         kGlideMaxMs,         0,    false },

	// Defaults to MANUAL, not to Aaaa. The nine formant parameters already
	// default to the Aaa patch, so a fresh instance sounds exactly as it
	// did before this parameter existed and every slider is live. Landing
	// on a preset instead would make a new user's first slider drag do
	// nothing they could see a reason for.
	{ kVowel,                       "Vowel",        "",   ParamType::Enum,  0.0,                 kVowelCount,         kVowelManual,                   0.0,                 kVowelCount,         kVowelCount, false },
};

//------------------------------------------------------------------------
// The table is written by hand, so prove it is in id order and that
// formantParam() agrees with it - at COMPILE time, because a row in the
// wrong place is exactly the kind of mistake that presents as "the width
// slider moves the level".
//------------------------------------------------------------------------
static_assert (kNumParams == 13, "thirteen parameters: trim, 3 x 3 formant, mix, glide, vowel");
static_assert (formantParam (0, kFieldFreq)      == kF1Freq,      "F1 freq id");
static_assert (formantParam (0, kFieldBandwidth) == kF1Bandwidth, "F1 width id");
static_assert (formantParam (0, kFieldLevel)     == kF1Level,     "F1 level id");
static_assert (formantParam (1, kFieldFreq)      == kF2Freq,      "F2 freq id");
static_assert (formantParam (2, kFieldFreq)      == kF3Freq,      "F3 freq id");
static_assert (formantParam (2, kFieldLevel)     == kF3Level,     "F3 level id");

//------------------------------------------------------------------------
// And prove, also at compile time, that EVERY VOWEL BUTTON IS REACHABLE
// BY ITS SLIDERS.
//
// A preset that asks for a value outside a parameter's range does not
// fail loudly - toNormalized returns something outside 0..1, the host
// clamps it, and the button quietly recalls a different vowel from the
// one on its face. Eeee is the one that would go first: its F3 is
// 3010 Hz, over the top of the F2 range and most of the way up the F3
// range, so narrowing either range breaks it.
//------------------------------------------------------------------------
namespace {

constexpr bool within (double v, double lo, double hi) { return v >= lo && v <= hi; }

constexpr bool vowelIsReachable (int vowel)
{
	for (int k = 0; k < kFormantCount; ++k)
	{
		const FormantSetting& f = kVowels[vowel].formants[k];
		if (! within (f.freqHz,      kFormantFreqMin[k], kFormantFreqMax[k]) ||
		    ! within (f.bandwidthHz, kBandwidthMin,      kBandwidthMax)      ||
		    ! within (f.levelDb,     kLevelMinDb,        kLevelMaxDb))
			return false;
	}
	// And the formants must be in order, or it is not a vowel.
	return kVowels[vowel].formants[0].freqHz < kVowels[vowel].formants[1].freqHz
	    && kVowels[vowel].formants[1].freqHz < kVowels[vowel].formants[2].freqHz;
}

} // namespace

static_assert (kVowelCount == 5, "five buttons: A E I O U");
static_assert (vowelIsReachable (0), "Aaaa is outside its sliders' ranges");
static_assert (vowelIsReachable (1), "Eeee is outside its sliders' ranges");
static_assert (vowelIsReachable (2), "Iiii is outside its sliders' ranges");
static_assert (vowelIsReachable (3), "Oooo is outside its sliders' ranges");
static_assert (vowelIsReachable (4), "Uuuu is outside its sliders' ranges");

//------------------------------------------------------------------------
const ParamDef& paramDef (Steinberg::Vst::ParamID id)
{
	if (id < kNumParams)
		return kParams[id];
	return kParams[kOutputTrim];
}

//------------------------------------------------------------------------
} // namespace VocalFilter
