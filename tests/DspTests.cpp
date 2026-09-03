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
