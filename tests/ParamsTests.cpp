//------------------------------------------------------------------------
// VocalFilter - parameter table tests
//
// A SECOND suite, and it exists because of a bug the first one could not
// possibly have caught.
//
// DspTests.cpp is deliberately free of SDK types, which is what lets it run
// anywhere - but it means it cannot see VocalFilterParams.h at all, and the
// parameter table is where ids, ranges and the predicates that classify them
// live. When kVoice was appended after the nine published values, the
// predicate `isLiveParam` still read `id >= kLiveBase && id < kNumParams`,
// so the voice was classified as a published value: the processor refused to
// record it and the editor refused to redraw for it. The switch wrote its
// parameter, the host saw the write, and nothing happened.
//
// This file needs only the SDK's HEADERS - vsttypes.h is typedefs - so it
// links against VocalFilterParams.cpp and nothing else:
//
//     c++ -std=c++17 -O2 -Isource -Iexternal/vst3sdk \
//         tests/ParamsTests.cpp source/VocalFilterParams.cpp \
//         -o /tmp/paramstests && /tmp/paramstests
//------------------------------------------------------------------------

#include "VocalFilterParams.h"

#include <cstdio>
#include <cstring>
#include <string>

using namespace VocalFilter;
using Steinberg::Vst::ParamID;

namespace {

int gFailures = 0;

void check (bool condition, const char* what)
{
	std::printf ("  %-64s %s\n", what, condition ? "ok" : "FAILED");
	if (!condition) ++gFailures;
}

void section (const char* title) { std::printf ("\n%s\n", title); }

/** The processor's own acceptance rule, copied here so the test asserts the
    thing the processor actually does rather than a paraphrase of it. If that
    line changes, this must change with it - and the point is that it should
    be hard to change one without noticing the other. */
bool processorWouldRecord (ParamID id)
{
	return id < kNumParams && !isLiveParam (id);
}

} // namespace

//------------------------------------------------------------------------
int main ()
{
	//--------------------------------------------------------------------
	section ("1. Every id is classified exactly once");
	//--------------------------------------------------------------------
	{
		bool ok = true;
		for (ParamID id = 0; id < kNumParams; ++id)
		{
			const int roles = (isFormantParam (id) ? 1 : 0) + (isLiveParam (id) ? 1 : 0);
			if (roles > 1) ok = false;                  // cannot be both
		}
		check (ok, "no id is both a formant parameter and a published value");

		check (isFormantParam (kF1Freq) && isFormantParam (kF3Level),
		       "the nine formant parameters are formant parameters");
		check (! isFormantParam (kOutputTrim) && ! isFormantParam (kMix) &&
		       ! isFormantParam (kGlide) && ! isFormantParam (kVowel) &&
		       ! isFormantParam (kVoice),
		       "trim, mix, glide, vowel and voice are not formant parameters");
	}

	//--------------------------------------------------------------------
	section ("2. THE REGRESSION - kVoice is a setting, not a published value");
	//--------------------------------------------------------------------
	{
		// The bug: isLiveParam was bounded by kNumParams rather than by the
		// end of the published block, so appending kVoice after that block
		// swept it up. Everything below failed, and the switch did nothing.
		check (! isLiveParam (kVoice), "isLiveParam (kVoice) is false");
		check (processorWouldRecord (kVoice),
		       "the processor records a Voice change from the host or the panel");
		check (kLiveEnd == kVoice, "the published block ends exactly where kVoice begins");

		bool liveExact = true;
		for (ParamID id = 0; id < kNumParams; ++id)
		{
			const bool wanted = (id >= kLiveF1Freq && id <= kLiveF3Level);
			if (isLiveParam (id) != wanted) liveExact = false;
		}
		check (liveExact, "isLiveParam is true for ids 13..21 and no others");

		// And every SETTING must be recordable, or some other control is
		// quietly dead in the same way.
		//
		// The settings are listed OUT, not derived from isLiveParam. The
		// first version of this asked "for every id where !isLiveParam, is
		// !isLiveParam true" - which is a tautology and passed happily
		// against the broken code. A guard that cannot fail is not a guard.
		static const ParamID kSettings[] = {
			kOutputTrim,
			kF1Freq, kF1Bandwidth, kF1Level,
			kF2Freq, kF2Bandwidth, kF2Level,
			kF3Freq, kF3Bandwidth, kF3Level,
			kMix, kGlide, kVowel, kVoice };

		bool allSettingsRecordable = true;
		for (ParamID id : kSettings)
			if (! processorWouldRecord (id))
				allSettingsRecordable = false;
		check (allSettingsRecordable,
		       "all 14 settings, listed by name, are recordable by the processor");

		// And that list is the whole of the non-published table.
		check (sizeof (kSettings) / sizeof (kSettings[0]) == kNumParams - 9,
		       "the settings list covers every id that is not a published value");

		// The published ones must NOT be, or a host could write over the
		// values the DSP is publishing.
		bool liveRejected = true;
		for (ParamID id = kLiveF1Freq; id <= kLiveF3Level; ++id)
			if (processorWouldRecord (id)) liveRejected = false;
		check (liveRejected, "the nine published values are refused from outside");
	}

	//--------------------------------------------------------------------
	section ("3. The table itself");
	//--------------------------------------------------------------------
	{
		bool inOrder = true, titled = true, defaultsInRange = true;
		for (ParamID id = 0; id < kNumParams; ++id)
		{
			const ParamDef& d = kParams[id];
			if (d.id != id) inOrder = false;
			if (d.title == nullptr || d.title[0] == '\0') titled = false;
			if (d.units == nullptr) titled = false;      // "" is fine, null is not
			if (d.plainDefault < d.plainMin || d.plainDefault > d.plainMax)
				defaultsInRange = false;
		}
		check (inOrder, "every row's id matches its index");
		check (titled, "every parameter has a non-null title and units");
		check (defaultsInRange, "every default lies inside its own range");

		// A null title or units reaching RangeParameter presents as the
		// VALIDATOR segfaulting, which is a long way from the cause.
		bool normalises = true;
		for (ParamID id = 0; id < kNumParams; ++id)
		{
			const ParamDef& d = kParams[id];
			const double n = d.defaultNormalized ();
			if (n < -1e-9 || n > 1.0 + 1e-9) normalises = false;
			if (std::fabs (d.toPlain (n) - d.plainDefault) > 1e-6) normalises = false;
		}
		check (normalises, "defaults round-trip through normalised and back");
	}

	//--------------------------------------------------------------------
	section ("4. Enumerated parameters name their own choices");
	//--------------------------------------------------------------------
	{
		check (std::string (enumChoiceName (kVoice, kVoiceMale)) == "Male" &&
		       std::string (enumChoiceName (kVoice, kVoiceFemale)) == "Female",
		       "kVoice names Male and Female");
		check (std::string (enumChoiceName (kVowel, kVowelManual)) == "Manual" &&
		       std::string (enumChoiceName (kVowel, 1)) == "Aaaa" &&
		       std::string (enumChoiceName (kVowel, 5)) == "Uuuu",
		       "kVowel names Manual and the five vowels");

		// The controller builds a StringListParameter with one entry per
		// choice from 0 to plainMax, so plainMax has to be the last index.
		check (static_cast<int> (kParams[kVoice].plainMax) == kVoiceCount - 1,
		       "kVoice's plainMax is the last choice index");
		check (static_cast<int> (kParams[kVowel].plainMax) == kVowelCount,
		       "kVowel's plainMax is the last choice index");

		bool enumsRound = true;
		for (int choice = 0; choice <= kVoiceCount - 1; ++choice)
		{
			const double n = kParams[kVoice].toNormalized (choice);
			const int back = static_cast<int> (kParams[kVoice].toInternal (n) + 0.5);
			if (back != choice) enumsRound = false;
		}
		for (int choice = 0; choice <= kVowelCount; ++choice)
		{
			const double n = kParams[kVowel].toNormalized (choice);
			const int back = static_cast<int> (kParams[kVowel].toInternal (n) + 0.5);
			if (back != choice) enumsRound = false;
		}
		check (enumsRound, "every enum choice survives the normalised round trip");
	}

	//--------------------------------------------------------------------
	section ("5. What gets saved");
	//--------------------------------------------------------------------
	{
		check (kNumStoredParams == kLiveBase,
		       "the contiguous stored block runs from 0 to the published values");

		// kVoice is past them, so it is saved by hand. If it ever moves
		// inside the block this test stops being necessary - and fails,
		// which is the prompt to delete it.
		check (kVoice >= kNumStoredParams,
		       "kVoice sits past the stored block and is saved explicitly");
	}

	std::printf ("\n%s\n", gFailures == 0 ? "all tests passed" : "TESTS FAILED");
	return gFailures == 0 ? 0 : 1;
}
