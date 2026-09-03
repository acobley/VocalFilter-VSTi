//------------------------------------------------------------------------
// VocalFilter - DSP tests
//
// SDK-FREE, deliberately: VocalFilterDsp.{h,cpp} include no VST3 header,
// so this suite compiles and runs anywhere with
//
//     c++ -std=c++17 -O2 -I../source DspTests.cpp ../source/VocalFilterDsp.cpp \
//         -o /tmp/dsptests && /tmp/dsptests
//
// It is the only integration test the project has until the plug-in is
// built and put through the validator, so every assertion here should be
// one that would FAIL against a wrong implementation.
//
// The method throughout is to measure the SPECTRUM rather than compare
// samples. Four poles delay the signal even where their magnitude is
// flat, so two runs with identical spectra differ at every sample; a
// sample-difference test would fail on a correct change.
//------------------------------------------------------------------------

#include "VocalFilterDsp.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace VocalFilter;

namespace {

int gFailures = 0;

void check (bool condition, const char* what)
{
	std::printf ("  %-62s %s\n", what, condition ? "ok" : "FAILED");
	if (!condition)
		++gFailures;
}

void section (const char* title)
{
	std::printf ("\n%s\n", title);
}

double db (double linear)
{
	return 20.0 * std::log10 (std::max (linear, 1e-12));
}

//------------------------------------------------------------------------
/** The impulse response of the whole line, with the smoothers snapped so
    nothing is still ramping. */
std::vector<float> impulseResponse (Dsp& dsp, int frames, double sampleRate)
{
	dsp.setSampleRate (sampleRate);
	dsp.reset ();

	std::vector<float> in (frames, 0.0f), outL (frames, 0.0f), outR (frames, 0.0f);
	in[0] = 1.0f;
	dsp.process (in.data (), in.data (), outL.data (), outR.data (), frames);
	return outL;
}

/** |H(f)| by direct DTFT of the impulse response. Slower than an FFT and
    exact at whatever frequency is asked for, which is what matters here -
    the formant centres are not on any convenient bin boundary. */
double responseAt (const std::vector<float>& h, double freqHz, double sampleRate)
{
	const double w = 2.0 * M_PI * freqHz / sampleRate;
	double re = 0.0, im = 0.0;
	for (std::size_t n = 0; n < h.size (); ++n)
	{
		const double a = w * static_cast<double> (n);
		re += h[n] * std::cos (a);
		im -= h[n] * std::sin (a);
	}
	return std::sqrt (re * re + im * im);
}

/** What the editor would draw. Delegates to the DSP's own bankMagnitude
    rather than reimplementing the sum here - a private copy in the test
    would agree with a private copy in the editor and both could be wrong
    together, which is the failure mode this whole section exists to
    catch. */
double predictedResponse (const FormantSetting* formants, double freqHz, double sampleRate)
{
	return bankMagnitude (formants, kFormantCount, freqHz, sampleRate);
}

void applyPatch (Dsp& dsp, const FormantSetting* formants, double mixPercent = 100.0)
{
	for (int k = 0; k < kFormantCount; ++k)
		dsp.setFormant (k, formants[k].freqHz, formants[k].bandwidthHz, formants[k].levelDb);
	dsp.setMixPercent (mixPercent);
	dsp.setTrimNormalized (1.0);          // top of travel = unity
}

double peak (const std::vector<float>& x)
{
	double p = 0.0;
	for (float v : x)
		p = std::max (p, static_cast<double> (std::fabs (v)));
	return p;
}

void sine (std::vector<float>& out, double freqHz, double sampleRate, double amplitude = 1.0)
{
	for (std::size_t i = 0; i < out.size (); ++i)
		out[i] = static_cast<float> (amplitude *
			std::sin (2.0 * M_PI * freqHz * static_cast<double> (i) / sampleRate));
}

} // namespace

//------------------------------------------------------------------------
int main ()
{
	constexpr double kRate = 44100.0;
	constexpr int    kIR   = 16384;

	//--------------------------------------------------------------------
	section ("1. Parameter mappings - shared by the DSP and the editor");
	//--------------------------------------------------------------------
	check (std::fabs (trimDb (1.0) - kTrimMaxDb) < 1e-12, "trimDb(1) is the top of travel");
	check (std::fabs (trimDb (0.0) - kTrimMinDb) < 1e-12, "trimDb(0) is the bottom of travel");
	check (std::fabs (trimDb (0.5) - (kTrimMinDb + kTrimMaxDb) / 2.0) < 1e-12,
	       "trimDb is linear in dB");
	check (std::fabs (dbToLinear (0.0, kTrimMinDb) - 1.0) < 1e-12, "0 dB is unity");
	check (std::fabs (dbToLinear (-6.0206, kTrimMinDb) - 0.5) < 1e-4, "-6.02 dB is a half");
	check (dbToLinear (kLevelMinDb, kLevelMinDb) == 0.0, "a formant at the bottom of travel is silent");

	//--------------------------------------------------------------------
	section ("2. The Aaa patch puts its peaks where Peterson & Barney did");
	//--------------------------------------------------------------------
	{
		Dsp dsp;
		dsp.setSampleRate (kRate);
		applyPatch (dsp, kAaaFormants);
		const std::vector<float> h = impulseResponse (dsp, kIR, kRate);

		for (int k = 0; k < kFormantCount; ++k)
		{
			// Hunt the local maximum near the nominal centre, 1 Hz at a
			// time, and require it to land on the frequency the patch
			// asked for. A formant that is 5 % out is a different vowel.
			const double nominal = kAaaFormants[k].freqHz;
			double best = nominal, bestMag = 0.0;
			for (double f = nominal * 0.85; f <= nominal * 1.15; f += 1.0)
			{
				const double m = responseAt (h, f, kRate);
				if (m > bestMag) { bestMag = m; best = f; }
			}

			char label[96];
			std::snprintf (label, sizeof (label),
			               "F%d peak at %.0f Hz (asked %.0f, %+.2f %%)",
			               k + 1, best, nominal, 100.0 * (best - nominal) / nominal);
			check (std::fabs (best - nominal) / nominal < 0.02, label);
		}
	}

	//--------------------------------------------------------------------
	section ("2b. All five vowel buttons land where their table says");
	//--------------------------------------------------------------------
	{
		// The buttons write nine parameters each. If a preset's peaks do
		// not come out where the table claims, the button is labelled with
		// a vowel it does not produce - and nothing else in the plug-in
		// would ever say so.
		for (int v = 0; v < kVowelCount; ++v)
		{
			const VowelPreset& vowel = kVowels[v];

			Dsp dsp;
			dsp.setSampleRate (kRate);
			applyPatch (dsp, vowel.formants);
			const std::vector<float> h = impulseResponse (dsp, kIR, kRate);

			bool allGood = true;
			double found[kFormantCount] = { 0.0, 0.0, 0.0 };
			for (int k = 0; k < kFormantCount; ++k)
			{
				const double nominal = vowel.formants[k].freqHz;
				double best = nominal, bestMag = 0.0;
				for (double f = nominal * 0.88; f <= nominal * 1.12; f += 1.0)
				{
					const double m = responseAt (h, f, kRate);
					if (m > bestMag) { bestMag = m; best = f; }
				}
				found[k] = best;
				if (std::fabs (best - nominal) / nominal >= 0.03)
					allGood = false;
			}

			char label[128];
			std::snprintf (label, sizeof (label),
			               "%-5s %-18s peaks %4.0f %4.0f %4.0f Hz",
			               vowel.name, vowel.sound, found[0], found[1], found[2]);
			check (allGood, label);
		}
	}

	{
		// Two presets that are accidentally the same - a copy-paste that
		// was never finished - would give two buttons that sound alike and
		// nothing would complain.
		bool allDistinct = true;
		for (int a = 0; a < kVowelCount; ++a)
			for (int b = a + 1; b < kVowelCount; ++b)
			{
				bool same = true;
				for (int k = 0; k < kFormantCount; ++k)
					if (kVowels[a].formants[k].freqHz != kVowels[b].formants[k].freqHz)
						same = false;
				if (same) allDistinct = false;
			}
		check (allDistinct, "no two vowels have the same formant frequencies");

		// And every one is a vowel: F1 < F2 < F3. Also a static_assert in
		// VocalFilterParams.cpp, asserted here too because this file is
		// what someone reads to find out what is guaranteed.
		bool ordered = true;
		for (int v = 0; v < kVowelCount; ++v)
			if (! (kVowels[v].formants[0].freqHz < kVowels[v].formants[1].freqHz &&
			       kVowels[v].formants[1].freqHz < kVowels[v].formants[2].freqHz))
				ordered = false;
		check (ordered, "every vowel has F1 < F2 < F3");

		// THE LEVELS MUST DIFFER BETWEEN VOWELS. A vocal tract is an
		// all-pole filter, so amplitudes are a consequence of the
		// frequencies, not free parameters; giving all five the same
		// profile - which this table did until DEVIATION 2 - flattens a
		// range F3 genuinely spans. The assertion is the shape of that
		// range, not the individual numbers, so refitting the levels does
		// not break it but going back to a flat profile does.
		double loF3 = 1e9, hiF3 = -1e9;
		bool anySameProfile = false;
		for (int v = 0; v < kVowelCount; ++v)
		{
			loF3 = std::min (loF3, kVowels[v].formants[2].levelDb);
			hiF3 = std::max (hiF3, kVowels[v].formants[2].levelDb);
			for (int w = v + 1; w < kVowelCount; ++w)
			{
				bool same = true;
				for (int k = 1; k < kFormantCount; ++k)   // F1 is pinned at 0 dB
					if (kVowels[v].formants[k].levelDb != kVowels[w].formants[k].levelDb)
						same = false;
				if (same) anySameProfile = true;
			}
		}
		char label[128];
		std::snprintf (label, sizeof (label),
		               "F3 level spans %.1f dB across the five vowels", hiF3 - loF3);
		check (! anySameProfile && (hiF3 - loF3) > 20.0, label);

		// F1 is the reference and stays at 0 dB in every vowel, which is
		// what keeps the five at roughly equal loudness. The tract alone
		// would make Eeee 11 dB quieter than Aaaa; a button that drops the
		// mix 11 dB is not what anyone wants from an effect.
		bool f1Pinned = true;
		for (int v = 0; v < kVowelCount; ++v)
			if (kVowels[v].formants[0].levelDb != 0.0)
				f1Pinned = false;
		check (f1Pinned, "F1 is pinned at 0 dB in every vowel (equal-loudness by design)");

		// The back vowels must keep F3 clear of the Level parameter's own
		// silence floor, or a small nudge switches F3 off entirely.
		bool clearOfFloor = true;
		for (int v = 0; v < kVowelCount; ++v)
			for (int k = 0; k < kFormantCount; ++k)
				if (kVowels[v].formants[k].levelDb < kLevelMinDb + 8.0)
					clearOfFloor = false;
		check (clearOfFloor, "no vowel sits within 8 dB of the level floor");
	}

	{
		// POLARITY. F2 is summed inverted, and that is load-bearing: it is
		// what puts the region BETWEEN two formants where an all-pole
		// tract puts it. If it is ever dropped, the peaks stay exactly
		// where they are and only the valleys move, so nothing else in
		// this suite would notice - hence a test that looks at a valley.
		//
		// Summed IN PHASE the two branches are half a turn apart between
		// the formants and cancel, digging a spurious null a real tract
		// does not have; inverting F2 fills it in. The first version of
		// this test asserted the opposite - that inverting DEEPENED the
		// valley - and failed, which is how the direction got settled.
		Dsp dsp;
		dsp.setSampleRate (kRate);
		applyPatch (dsp, kAaaFormants);
		const std::vector<float> h = impulseResponse (dsp, kIR, kRate);

		// Compute BOTH candidate curves from the shared response function -
		// the bank as it is summed, and the same bank summed in phase -
		// and require the running filter to match the first and not the
		// second. No threshold picked by hand: the test discriminates
		// between two concrete alternatives, and prints how far apart they
		// are so a reader can see it had something to discriminate.
		auto valleyOf = [&] (bool invertF2)
		{
			double lowest = 1e9;
			for (double f = kAaaFormants[0].freqHz + 20.0;
			     f < kAaaFormants[1].freqHz - 20.0; f += 1.0)
			{
				double sumRe = 0.0, sumIm = 0.0;
				for (int k = 0; k < kFormantCount; ++k)
				{
					double re = 0.0, im = 0.0;
					bandpassResponse (kAaaFormants[k].freqHz, kAaaFormants[k].bandwidthHz,
					                  f, kRate, re, im);
					const double sign = (k == 1 && invertF2) ? -1.0 : 1.0;
					const double g = dbToLinear (kAaaFormants[k].levelDb, kLevelMinDb) * sign;
					sumRe += re * g;
					sumIm += im * g;
				}
				lowest = std::min (lowest, std::sqrt (sumRe * sumRe + sumIm * sumIm));
			}
			return lowest;
		};

		double measured = 1e9, at = 0.0;
		for (double f = kAaaFormants[0].freqHz + 20.0;
		     f < kAaaFormants[1].freqHz - 20.0; f += 1.0)
		{
			const double m = responseAt (h, f, kRate);
			if (m < measured) { measured = m; at = f; }
		}

		const double inverted = valleyOf (true);
		const double inPhase  = valleyOf (false);

		char label[128];
		std::snprintf (label, sizeof (label),
		               "F1-F2 valley at %.0f Hz: inverted %.1f dB, in phase %.1f dB, got %.1f",
		               at, db (inverted), db (inPhase), db (measured));
		// The two must actually differ, or this proves nothing...
		const bool discriminates = std::fabs (db (inverted) - db (inPhase)) > 3.0;
		// ...and the running filter must be the inverted one.
		const bool matches = std::fabs (db (measured) - db (inverted)) < 0.5;
		check (discriminates && matches, label);

		// And the constant that makes it happen is the one that says so.
		check (kFormantPolarity[0] > 0.0 && kFormantPolarity[1] < 0.0 &&
		       kFormantPolarity[2] > 0.0, "polarity is + - +");
	}

	//--------------------------------------------------------------------
	section ("2c. The vowel SELECTOR - what the host automates");
	//--------------------------------------------------------------------
	{
		// The selector's mapping lives in this SDK-free layer precisely so
		// it can be tested here rather than only inside a processor that
		// needs a host to run.
		check (vowelSelection (kVowelManual) == nullptr,
		       "selector 0 is Manual - the nine parameters, not a preset");

		bool mapped = true;
		for (int v = 1; v <= kVowelCount; ++v)
		{
			const FormantSetting* got = vowelSelection (v);
			if (got != kVowels[v - 1].formants)
				mapped = false;
		}
		check (mapped, "selectors 1..5 map to Aaaa..Uuuu in table order");

		// An out-of-range selector must be MANUAL, not a crash and not a
		// silently clamped preset. A host is free to send anything, and a
		// saved project from a future version with more vowels will.
		check (vowelSelection (-1) == nullptr &&
		       vowelSelection (kVowelCount + 1) == nullptr &&
		       vowelSelection (9999) == nullptr,
		       "an out-of-range selector falls back to Manual");

		// The names the host's parameter list shows must line up with the
		// presets they select, or the list is lying about what it does.
		bool named = (std::string (vowelSelectionName (kVowelManual)) == "Manual");
		for (int v = 1; v <= kVowelCount; ++v)
			if (std::string (vowelSelectionName (v)) != kVowels[v - 1].name)
				named = false;
		check (named, "every selector position is named after the vowel it selects");

		check (kVowelChoices == kVowelCount + 1, "six positions: Manual plus five vowels");
	}

	{
		// Switching vowels from the host must GLIDE, exactly as pressing a
		// button does - the processor feeds the selector's preset through
		// the same setFormant, so this is really a check that nothing
		// about the selector path snaps.
		Dsp dsp;
		dsp.setSampleRate (kRate);
		dsp.setGlideMs (150.0);
		applyPatch (dsp, vowelSelection (5));      // Uuuu
		dsp.reset ();

		const FormantSetting* target = vowelSelection (2);   // Eeee
		for (int k = 0; k < kFormantCount; ++k)
			dsp.setFormant (k, target[k].freqHz, target[k].bandwidthHz, target[k].levelDb);

		const int expected = static_cast<int> (150.0 * 0.001 * kRate + 0.5);
		float in = 0.0f, oL = 0.0f, oR = 0.0f;
		int n = 0;
		while (dsp.gliding () && n < expected * 4) { dsp.process (&in,&in,&oL,&oR,1); ++n; }

		char label[128];
		std::snprintf (label, sizeof (label),
		               "a selector change glides: settled after %d samples (want %d)",
		               n, expected);
		check (n == expected, label);

		bool arrived = true;
		for (int k = 0; k < kFormantCount; ++k)
			if (dsp.formantFreq (k) != target[k].freqHz ||
			    dsp.formantBandwidth (k) != target[k].bandwidthHz)
				arrived = false;
		check (arrived, "and lands exactly on the selected vowel");
	}

	//--------------------------------------------------------------------
	section ("3. The filter that RUNS is the filter the editor would DRAW");
	//--------------------------------------------------------------------
	{
		// The guide's rule that anything the editor displays and the DSP
		// computes must come from one shared function is only worth
		// anything if the shared function is actually right. This
		// compares the measured impulse response against
		// bandpassMagnitude() across the audible band.
		Dsp dsp;
		dsp.setSampleRate (kRate);
		applyPatch (dsp, kAaaFormants);
		const std::vector<float> h = impulseResponse (dsp, kIR, kRate);

		double worst = 0.0, worstAt = 0.0;
		for (int i = 0; i < 240; ++i)
		{
			const double f = 50.0 * std::pow (16000.0 / 50.0, i / 239.0);
			const double measured  = responseAt (h, f, kRate);
			const double predicted = predictedResponse (kAaaFormants, f, kRate);
			const double err = std::fabs (measured - predicted);
			if (err > worst) { worst = err; worstAt = f; }
		}
		char label[96];
		std::snprintf (label, sizeof (label),
		               "measured vs predicted: worst %.2e at %.0f Hz", worst, worstAt);
		check (worst < 5e-3, label);
	}

	//--------------------------------------------------------------------
	section ("4. Bandwidth means bandwidth, and the peak gain is constant");
	//--------------------------------------------------------------------
	{
		// One formant alone, the other two silenced, so nothing overlaps
		// what is being measured.
		for (double bw : { 40.0, 80.0, 160.0 })
		{
			const FormantSetting one[kFormantCount] =
				{ { 730.0, bw, 0.0 }, { 1090.0, 90.0, kLevelMinDb }, { 2440.0, 120.0, kLevelMinDb } };

			Dsp dsp;
			dsp.setSampleRate (kRate);
			applyPatch (dsp, one);
			const std::vector<float> h = impulseResponse (dsp, kIR, kRate);

			const double atCentre = responseAt (h, 730.0, kRate);

			// Walk out to the -3 dB points either side.
			const double target = atCentre / std::sqrt (2.0);
			double lo = 730.0, hi = 730.0;
			while (lo > 20.0   && responseAt (h, lo, kRate) > target) lo -= 0.5;
			while (hi < 4000.0 && responseAt (h, hi, kRate) > target) hi += 0.5;

			char label[96];
			std::snprintf (label, sizeof (label),
			               "BW %.0f Hz: peak %+.2f dB, measured -3 dB width %.1f Hz",
			               bw, db (atCentre), hi - lo);
			// Constant 0 dB peak gain: catches the b0 = Q*alpha spelling,
			// where narrowing a formant would make it louder.
			check (std::fabs (db (atCentre)) < 0.15 &&
			       std::fabs ((hi - lo) - bw) / bw < 0.06, label);
		}
	}

	//--------------------------------------------------------------------
	section ("5. Sample rate independence");
	//--------------------------------------------------------------------
	{
		// A hard-coded coefficient puts its corner at a fixed fraction of
		// Nyquist, so the same patch would be over an octave brighter at
		// 96 k. Assert the response at the formant centres does not move.
		double reference[kFormantCount] = { 0.0, 0.0, 0.0 };
		bool first = true, ok = true;

		for (double rate : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
		{
			Dsp dsp;
			dsp.setSampleRate (rate);
			applyPatch (dsp, kAaaFormants);
			const std::vector<float> h =
				impulseResponse (dsp, static_cast<int> (kIR * rate / kRate), rate);

			for (int k = 0; k < kFormantCount; ++k)
			{
				const double m = responseAt (h, kAaaFormants[k].freqHz, rate);
				if (first)
					reference[k] = m;
				else if (std::fabs (db (m) - db (reference[k])) > 0.25)
					ok = false;
			}
			first = false;
		}
		check (ok, "the Aaa response is within 0.25 dB from 44.1 k to 192 k");
	}

	//--------------------------------------------------------------------
	section ("6. Dry / Wet at zero is a true no-op");
	//--------------------------------------------------------------------
	{
		// The filters still RUN at mix 0 - this forces the active path and
		// requires bit-identical output, which compares the two branches
		// against each other rather than trusting they were typed the same
		// way.
		std::vector<float> in (4096);
		sine (in, 440.0, kRate);
		std::vector<float> outL (in.size ()), outR (in.size ());

		Dsp dsp;
		dsp.setSampleRate (kRate);
		applyPatch (dsp, kAaaFormants, 0.0);
		dsp.reset ();
		dsp.process (in.data (), in.data (), outL.data (), outR.data (),
		             static_cast<int> (in.size ()));

		bool identical = true;
		for (std::size_t i = 0; i < in.size (); ++i)
			if (outL[i] != in[i]) identical = false;
		check (identical, "mix 0% is bit-identical to the input");

		// NEGATIVE CONTROL. A guard that has never failed is a guess.
		Dsp wet;
		wet.setSampleRate (kRate);
		applyPatch (wet, kAaaFormants, 100.0);
		wet.reset ();
		wet.process (in.data (), in.data (), outL.data (), outR.data (),
		             static_cast<int> (in.size ()));
		identical = true;
		for (std::size_t i = 0; i < in.size (); ++i)
			if (outL[i] != in[i]) identical = false;
		check (!identical, "mix 100% is NOT identical (control for the test above)");
	}

	//--------------------------------------------------------------------
	section ("7. OUTPUT LEVEL of the factory patch, before anyone plays it");
	//--------------------------------------------------------------------
	{
		// Measured now, because a level that clips masks other faults and
		// sends you chasing the wrong bug.
		double worstPeak = 0.0;
		for (double f : { 110.0, 220.0, 730.0, 1090.0, 2440.0 })
		{
			std::vector<float> in (8192);
			sine (in, f, kRate);
			std::vector<float> outL (in.size ()), outR (in.size ());

			Dsp dsp;
			dsp.setSampleRate (kRate);
			applyPatch (dsp, kAaaFormants);
			dsp.reset ();
			dsp.process (in.data (), in.data (), outL.data (), outR.data (),
			             static_cast<int> (in.size ()));

			const double p = peak (outL);
			worstPeak = std::max (worstPeak, p);
			std::printf ("     full-scale sine at %6.0f Hz -> peak %.4f (%+.2f dBFS)\n",
			             f, p, db (p));
		}
		// And the input this plug-in is actually FOR: a harmonically rich
		// source, whose partials excite all three formants at once and can
		// sum where a single sine cannot.
		for (double f : { 82.4, 110.0, 146.8 })
		{
			std::vector<float> in (8192), outL (8192), outR (8192);
			for (std::size_t i = 0; i < in.size (); ++i)
			{
				const double phase = std::fmod (f * static_cast<double> (i) / kRate, 1.0);
				in[i] = static_cast<float> (2.0 * phase - 1.0);      // sawtooth
			}

			Dsp dsp;
			dsp.setSampleRate (kRate);
			applyPatch (dsp, kAaaFormants);
			dsp.reset ();
			dsp.process (in.data (), in.data (), outL.data (), outR.data (), 8192);

			const double p = peak (outL);
			worstPeak = std::max (worstPeak, p);
			std::printf ("     full-scale saw  at %6.1f Hz -> peak %.4f (%+.2f dBFS)\n",
			             f, p, db (p));
		}

		char label[96];
		std::snprintf (label, sizeof (label),
		               "worst full-scale peak %+.2f dBFS at the factory patch", db (worstPeak));
		check (worstPeak <= 1.05, label);
	}

	//--------------------------------------------------------------------
	section ("8. Stability and smoothing under abuse");
	//--------------------------------------------------------------------
	{
		// Every extreme of every range, at every realistic rate, driven by
		// full-scale noise: the output must stay finite and bounded. This
		// is the guard against unbounded resonance.
		bool bounded = true;
		for (double rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
		{
			for (double bw : { kBandwidthMin, kBandwidthMax })
			{
				const FormantSetting extreme[kFormantCount] =
				{
					{ kFormantFreqMin[0], bw, kLevelMaxDb },
					{ kFormantFreqMax[1], bw, kLevelMaxDb },
					{ kFormantFreqMax[2], bw, kLevelMaxDb },
				};

				std::vector<float> in (8192), outL (8192), outR (8192);
				unsigned seed = 12345u;
				for (float& v : in)
				{
					seed = seed * 1664525u + 1013904223u;
					v = static_cast<float> ((seed >> 8) / 8388608.0 - 1.0);
				}

				Dsp dsp;
				dsp.setSampleRate (rate);
				applyPatch (dsp, extreme);
				dsp.reset ();
				dsp.process (in.data (), in.data (), outL.data (), outR.data (), 8192);

				for (float v : outL)
					if (!std::isfinite (v) || std::fabs (v) > 64.0)
						bounded = false;
			}
		}
		check (bounded, "every extreme setting stays finite and under +36 dBFS");
	}

	{
		// Sweeping a formant must not step. The parameters are smoothed
		// per sample and the coefficients recomputed every sixteen, so
		// assert the output has no discontinuity a click would show as.
		std::vector<float> in (44100), outL (44100), outR (44100);
		sine (in, 220.0, kRate);

		Dsp dsp;
		dsp.setSampleRate (kRate);
		applyPatch (dsp, kAaaFormants);
		dsp.reset ();

		double biggestStep = 0.0;
		const int block = 64;
		for (int start = 0; start + block <= 44100; start += block)
		{
			// Slam F2 between the ends of its range every block - far
			// more violent than any real automation.
			const double f = ((start / block) % 2) ? kFormantFreqMin[1] : kFormantFreqMax[1];
			dsp.setFormant (1, f, kAaaFormants[1].bandwidthHz, kAaaFormants[1].levelDb);
			dsp.process (in.data () + start, in.data () + start,
			             outL.data () + start, outR.data () + start, block);
		}
		for (int i = 1; i < 44100; ++i)
			biggestStep = std::max (biggestStep,
			                        static_cast<double> (std::fabs (outL[i] - outL[i - 1])));

		char label[96];
		std::snprintf (label, sizeof (label),
		               "slamming F2 end to end: biggest sample-to-sample step %.4f", biggestStep);
		check (biggestStep < 0.25, label);
	}

	//--------------------------------------------------------------------
	section ("8b. GLIDE: nine parameters, one arrival");
	//--------------------------------------------------------------------
	{
		// The requirement in one test: change vowel, and every one of the
		// nine formant parameters must reach its target ON THE SAME
		// SAMPLE, however far it had to travel. Uuuu -> Eeee is the
		// cruellest pair in the table: F2 moves 1420 Hz while B2 moves
		// 10 Hz, a ratio of 142 to 1.
		for (double glideMs : { 0.0, 50.0, 150.0, 500.0, 2000.0 })
		{
			Dsp dsp;
			dsp.setSampleRate (kRate);
			dsp.setGlideMs (glideMs);
			applyPatch (dsp, kVowels[4].formants);      // Uuuu
			dsp.reset ();

			// Aim at Eeee, then walk one sample at a time and note when
			// each of the nine stops moving.
			applyPatch (dsp, kVowels[1].formants);      // Eeee

			const int expected = static_cast<int> (
				std::max (glideMs, kGlideFloorMs) * 0.001 * kRate + 0.5);

			int arrived[kFormantCount][3];
			for (auto& row : arrived) for (int& a : row) a = -1;

			float in = 0.0f, outL = 0.0f, outR = 0.0f;
			const int limit = expected + 64;
			for (int n = 1; n <= limit; ++n)
			{
				dsp.process (&in, &in, &outL, &outR, 1);
				for (int k = 0; k < kFormantCount; ++k)
				{
					const double want[3] = { kVowels[1].formants[k].freqHz,
					                         kVowels[1].formants[k].bandwidthHz,
					                         dbToLinear (kVowels[1].formants[k].levelDb,
					                                     kLevelMinDb) };
					const double have[3] = { dsp.formantFreq (k),
					                         dsp.formantBandwidth (k),
					                         dsp.formantGain (k) };
					for (int j = 0; j < 3; ++j)
						if (arrived[k][j] < 0 && have[j] == want[j])
							arrived[k][j] = n;
				}
			}

			// ONLY THE ONES THAT HAVE TO MOVE. Two things here do not:
			// the levels are one shared profile across all five vowels, and
			// - the reason this test failed when it was first written -
			// Uuuu and Eeee happen to share a 50 Hz B1. A parameter
			// already at its target is at its target on sample 1, which is
			// correct and is not an arrival.
			bool together = true;
			int first = -1, movers = 0;
			for (int k = 0; k < kFormantCount; ++k)
			{
				const double from[3] = { kVowels[4].formants[k].freqHz,
				                         kVowels[4].formants[k].bandwidthHz,
				                         kVowels[4].formants[k].levelDb };
				const double to[3]   = { kVowels[1].formants[k].freqHz,
				                         kVowels[1].formants[k].bandwidthHz,
				                         kVowels[1].formants[k].levelDb };
				for (int j = 0; j < 3; ++j)
				{
					if (from[j] == to[j])
						continue;                      // never moved
					++movers;
					if (first < 0) first = arrived[k][j];
					if (arrived[k][j] != first) together = false;
				}
			}

			char label[128];
			std::snprintf (label, sizeof (label),
			               "glide %6.0f ms: all %d movers arrive at sample %d (want %d)",
			               glideMs, movers, first, expected);
			check (together && first == expected && movers >= 4, label);
		}
	}

	{
		// And the distances really were different - otherwise the test
		// above proves nothing. Uuuu -> Eeee, in the units each ramp runs
		// in.
		double most = 0.0, least = 1e30;
		for (int k = 0; k < kFormantCount; ++k)
		{
			const double df = std::fabs (kVowels[1].formants[k].freqHz
			                           - kVowels[4].formants[k].freqHz);
			const double dw = std::fabs (kVowels[1].formants[k].bandwidthHz
			                           - kVowels[4].formants[k].bandwidthHz);
			for (double d : { df, dw })
				if (d > 0.0) { most = std::max (most, d); least = std::min (least, d); }
		}
		char label[128];
		std::snprintf (label, sizeof (label),
		               "the movers really do differ: furthest %.0f, shortest %.0f (%.0f:1)",
		               most, least, most / least);
		check (most / least > 20.0, label);
	}

	{
		// A glide of zero is FLOORED, not instant - a control that can
		// produce a click will produce one. Prove the floor is doing
		// something by requiring zero and the floor to behave identically
		// and both to take real time.
		Dsp a, b;
		for (Dsp* d : { &a, &b })
		{
			d->setSampleRate (kRate);
			applyPatch (*d, kVowels[4].formants);
			d->reset ();
		}
		a.setGlideMs (0.0);
		b.setGlideMs (kGlideFloorMs);
		applyPatch (a, kVowels[1].formants);
		applyPatch (b, kVowels[1].formants);

		float in = 0.0f, oL = 0.0f, oR = 0.0f;
		int stepsA = 0, stepsB = 0;
		while (a.gliding () && stepsA < 100000) { a.process (&in,&in,&oL,&oR,1); ++stepsA; }
		while (b.gliding () && stepsB < 100000) { b.process (&in,&in,&oL,&oR,1); ++stepsB; }

		char label[128];
		std::snprintf (label, sizeof (label),
		               "glide 0 ms is floored at %.0f ms (%d samples, not 1)",
		               kGlideFloorMs, stepsA);
		check (stepsA == stepsB && stepsA > 800, label);
	}

	{
		// CLICKS, detected spectrally, with a negative control.
		//
		// The input is a pure 220 Hz sine, so a linear filter can only put
		// energy at 220 Hz. Sweeping the filter spreads that into
		// sidebands NEAR 220 Hz - but a discontinuity is broadband and
		// puts energy at 8 kHz, where this patch has nothing of its own.
		//
		// The control is the same vowel change applied INSTANTLY by
		// snapping the ramps. If the fastest legal glide is not far
		// quieter up there than a snap, the coefficient update rate is too
		// coarse and the guard has caught it.
		auto artefactEnergy = [&] (bool snapInstead)
		{
			const int n = 8192, transition = 2048;
			std::vector<float> in (n), outL (n), outR (n);
			sine (in, 220.0, kRate);

			Dsp dsp;
			dsp.setSampleRate (kRate);
			dsp.setGlideMs (kGlideFloorMs);
			applyPatch (dsp, kVowels[4].formants);       // Uuuu
			dsp.reset ();

			// Settle, change vowel, then run through the transition.
			dsp.process (in.data (), in.data (), outL.data (), outR.data (), transition);
			applyPatch (dsp, kVowels[1].formants);       // Eeee
			if (snapInstead)
				dsp.snapParameters ();
			dsp.process (in.data () + transition, in.data () + transition,
			             outL.data () + transition, outR.data () + transition,
			             n - transition);

			// A HANN-WINDOWED SLIDING FRAME, and the maximum over frames.
			//
			// The first version of this measurement took one rectangular
			// DTFT over the whole tail and reported the two cases as
			// IDENTICAL - because a rectangular window on a 220 Hz sine
			// leaks at every frequency, the leakage is the same in both
			// runs, and it buried the thing being measured. A window kills
			// the leakage; a sliding frame is what makes a click, whose
			// energy is in one frame, stand out from a sweep, whose energy
			// is spread over hundreds.
			const int frame = 512, hop = 128;
			double worst = 0.0;
			std::vector<float> windowed (frame);
			for (int start = transition - frame; start + frame <= n; start += hop)
			{
				if (start < 0)
					continue;
				for (int i = 0; i < frame; ++i)
				{
					const double w = 0.5 - 0.5 * std::cos (2.0 * M_PI * i / (frame - 1));
					windowed[i] = static_cast<float> (outL[start + i] * w);
				}
				double e = 0.0;
				for (double f : { 6000.0, 8000.0, 10000.0, 12000.0 })
				{
					const double m = responseAt (windowed, f, kRate);
					e += m * m;
				}
				worst = std::max (worst, e);
			}
			return worst;
		};

		const double glided = artefactEnergy (false);
		const double snapped = artefactEnergy (true);

		char label[128];
		std::snprintf (label, sizeof (label),
		               "fastest glide is %.0f dB quieter at 6-12 kHz than a snap",
		               10.0 * std::log10 (snapped / std::max (glided, 1e-30)));
		// 30 dB. Measured at 37 with the coefficient update at 8 samples
		// and 24 at 16, so this threshold sits inside the knee and a
		// regression to the old update rate fails here rather than merely
		// sounding slightly worse. See kCoeffUpdateSamples.
		check (glided * 1000.0 < snapped, label);
	}

	//--------------------------------------------------------------------
	section ("9. Degenerate calls a host really does make");
	//--------------------------------------------------------------------
	{
		Dsp dsp;
		dsp.setSampleRate (kRate);
		applyPatch (dsp, kAaaFormants);
		dsp.reset ();

		std::vector<float> buf (64, 0.0f);
		dsp.process (buf.data (), buf.data (), buf.data (), buf.data (), 0);
		dsp.process (nullptr, nullptr, nullptr, nullptr, 64);
		dsp.setFormant (-1, 100.0, 100.0, 0.0);
		dsp.setFormant (kFormantCount, 100.0, 100.0, 0.0);
		check (true, "zero frames, null buffers and out-of-range formants do not crash");

		std::printf ("     tail: %d samples (%.0f ms at 44.1 k)\n",
		             dsp.tailSamples (), 1000.0 * dsp.tailSamples () / kRate);
	}

	std::printf ("\n%s\n", gFailures == 0 ? "all tests passed" : "TESTS FAILED");
	return gFailures == 0 ? 0 : 1;
}
