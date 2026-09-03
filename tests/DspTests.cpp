//------------------------------------------------------------------------
// VocalFilter - DSP tests
//
// SDK-FREE, deliberately: VocalFilterDsp.{h,cpp} include no VST3 header,
// so this suite compiles and runs anywhere with
//
//     c++ -std=c++17 -I../source DspTests.cpp ../source/VocalFilterDsp.cpp \
//         -o /tmp/dsptests && /tmp/dsptests
//
// It is the only integration test the project has until the plug-in is
// built and put through the validator. Every assertion here should be one
// that would FAIL against a wrong implementation - see the porting guide's
// "Verifying a DSP change".
//------------------------------------------------------------------------

#include "VocalFilterDsp.h"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace VocalFilter;

namespace {

int gFailures = 0;

void check (bool condition, const char* what)
{
	std::printf ("%-58s %s\n", what, condition ? "ok" : "FAILED");
	if (!condition)
		++gFailures;
}

/** A short stereo sine, full scale, so a gain error shows as a peak
    error. */
void makeInput (std::vector<float>& left, std::vector<float>& right,
                int frames, double sampleRate)
{
	left.assign (frames, 0.0f);
	right.assign (frames, 0.0f);
	for (int i = 0; i < frames; ++i)
	{
		const double t = i / sampleRate;
		left[i]  = static_cast<float> (std::sin (2.0 * M_PI * 440.0 * t));
		right[i] = static_cast<float> (std::sin (2.0 * M_PI * 660.0 * t));
	}
}

double peak (const std::vector<float>& x)
{
	double p = 0.0;
	for (float v : x)
		p = std::max (p, static_cast<double> (std::fabs (v)));
	return p;
}

} // namespace

//------------------------------------------------------------------------
int main ()
{
	constexpr int    kFrames = 4096;
	constexpr double kRate   = 44100.0;

	std::vector<float> inL, inR;
	makeInput (inL, inR, kFrames, kRate);

	std::vector<float> outL (kFrames, 0.0f), outR (kFrames, 0.0f);

	//--------------------------------------------------------------------
	// 1. The trim's dB mapping, which the editor will call too. Both ends
	//    and the middle, so a swapped min and max cannot pass.
	//--------------------------------------------------------------------
	check (std::fabs (trimDb (1.0) - kTrimMaxDb) < 1e-12, "trimDb(1) is the top of travel");
	check (std::fabs (trimDb (0.0) - kTrimMinDb) < 1e-12, "trimDb(0) is the bottom of travel");
	check (std::fabs (trimDb (0.5) - (kTrimMinDb + kTrimMaxDb) / 2.0) < 1e-12,
	       "trimDb is linear in dB");
	check (std::fabs (dbToLinear (0.0) - 1.0) < 1e-12, "0 dB is unity");
	check (std::fabs (dbToLinear (-6.0206) - 0.5) < 1e-4, "-6.02 dB is a half");
	check (dbToLinear (kTrimMinDb) == 0.0, "the bottom of travel is true silence");

	//--------------------------------------------------------------------
	// 2. OUTPUT LEVEL AT THE DEFAULT PATCH. Measured before anything is
	//    played, because a level that clips masks other faults: velocity
	//    stops working and attacks are reported as clicks.
	//--------------------------------------------------------------------
	{
		Dsp dsp;
		dsp.setSampleRate (kRate);
		dsp.setTrimNormalized ((kTrimDefaultDb - kTrimMinDb) / (kTrimMaxDb - kTrimMinDb));
		dsp.reset ();
		dsp.process (inL.data (), inR.data (), outL.data (), outR.data (), kFrames);

		const double pL = peak (outL), pR = peak (outR);
		std::printf ("   default patch peak: L %.6f (%.2f dBFS)  R %.6f (%.2f dBFS)\n",
		             pL, 20.0 * std::log10 (pL), pR, 20.0 * std::log10 (pR));
		check (pL <= 1.0 && pR <= 1.0, "default patch does not exceed 0 dBFS");
	}

	//--------------------------------------------------------------------
	// 3. The line is a PASS-THROUGH at the top of travel: bit-identical,
	//    not merely close. This is the assertion the real DSP will break
	//    the moment it does anything, which is the point of writing it now.
	//--------------------------------------------------------------------
	{
		Dsp dsp;
		dsp.setSampleRate (kRate);
		dsp.setTrimNormalized (1.0);
		dsp.reset ();                 // snaps the smoother - no 10 ms ramp
		dsp.process (inL.data (), inR.data (), outL.data (), outR.data (), kFrames);

		bool identical = true;
		for (int i = 0; i < kFrames; ++i)
			if (outL[i] != inL[i] || outR[i] != inR[i])
				identical = false;
		check (identical, "unity trim is bit-identical to the input");
	}

	//--------------------------------------------------------------------
	// 4. NEGATIVE CONTROL. A guard that has never failed is a guess: the
	//    same comparison at half gain must NOT be identical, or test 3 is
	//    passing for the wrong reason.
	//--------------------------------------------------------------------
	{
		Dsp dsp;
		dsp.setSampleRate (kRate);
		dsp.setTrimNormalized ((-6.0206 - kTrimMinDb) / (kTrimMaxDb - kTrimMinDb));
		dsp.reset ();
		dsp.process (inL.data (), inR.data (), outL.data (), outR.data (), kFrames);

		bool identical = true;
		for (int i = 0; i < kFrames; ++i)
			if (outL[i] != inL[i])
				identical = false;
		check (!identical, "half gain is NOT identical (control for the test above)");
		check (std::fabs (peak (outL) - 0.5) < 1e-3, "half gain peaks at 0.5");
	}

	//--------------------------------------------------------------------
	// 5. The trim is smoothed PER SAMPLE. A block-rate step on a mixed
	//    output is itself a click, so assert that no single sample of a
	//    full-travel move jumps by more than a small fraction, and that
	//    the move has finished within a few time constants.
	//--------------------------------------------------------------------
	{
		Dsp dsp;
		dsp.setSampleRate (kRate);
		dsp.setTrimNormalized (0.0);      // silence
		dsp.reset ();

		std::vector<float> ones (kFrames, 1.0f);
		dsp.setTrimNormalized (1.0);      // jump to unity
		dsp.process (ones.data (), ones.data (), outL.data (), outR.data (), kFrames);

		double biggestStep = 0.0;
		for (int i = 1; i < kFrames; ++i)
			biggestStep = std::max (biggestStep,
			                        static_cast<double> (std::fabs (outL[i] - outL[i - 1])));

		// 10 ms at 44.1 k is 441 samples; one sample of a one-pole is at
		// most 1/441 of the remaining distance, so ~0.23 %.
		check (biggestStep < 0.01, "a full-travel trim move steps by under 1% per sample");
		check (outL[kFrames - 1] > 0.999, "the trim reaches its target within 4096 samples");
	}

	//--------------------------------------------------------------------
	// 6. Degenerate calls a host really does make.
	//--------------------------------------------------------------------
	{
		Dsp dsp;
		dsp.setSampleRate (kRate);
		dsp.reset ();
		dsp.process (inL.data (), inR.data (), outL.data (), outR.data (), 0);
		dsp.process (nullptr, nullptr, nullptr, nullptr, 64);
		check (true, "zero frames and null buffers do not crash");
	}

	std::printf ("\n%s\n", gFailures == 0 ? "all tests passed" : "TESTS FAILED");
	return gFailures == 0 ? 0 : 1;
}
