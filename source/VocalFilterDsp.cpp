//------------------------------------------------------------------------
// VocalFilter - the audio line
//------------------------------------------------------------------------

#include "VocalFilterDsp.h"

#include <algorithm>

namespace VocalFilter {

namespace {

/** The trim smoother's time constant. 10 ms is short enough that a knob
    feels immediate and long enough that a jump of 60 dB does not click. */
constexpr double kTrimSmoothSeconds = 0.010;

/** Below this the trim is silence, not a very small number. Denormals in
    a feedback path cost hundreds of cycles a sample on some hardware; a
    line with no feedback cannot generate them, but the moment you add one
    this is where the flush belongs. */
constexpr float kDenormalFloor = 1.0e-25f;

inline float flush (float x)
{
	return (x > -kDenormalFloor && x < kDenormalFloor) ? 0.0f : x;
}

} // namespace

//------------------------------------------------------------------------
double trimDb (double normalized)
{
	const double clamped = std::min (1.0, std::max (0.0, normalized));
	return kTrimMinDb + clamped * (kTrimMaxDb - kTrimMinDb);
}

//------------------------------------------------------------------------
double dbToLinear (double db)
{
	if (db <= kTrimMinDb)
		return 0.0;
	return std::pow (10.0, db / 20.0);
}

//------------------------------------------------------------------------
void Dsp::setSampleRate (double sampleRate)
{
	mSampleRate = (sampleRate > 0.0) ? sampleRate : 44100.0;
	mTrimCoeff = 1.0 - std::exp (-1.0 / (kTrimSmoothSeconds * mSampleRate));
}

//------------------------------------------------------------------------
void Dsp::reset ()
{
	snapParameters ();
}

//------------------------------------------------------------------------
void Dsp::setTrimNormalized (double normalized)
{
	mTrimTarget = dbToLinear (trimDb (normalized));
}

//------------------------------------------------------------------------
void Dsp::snapParameters ()
{
	mTrim = mTrimTarget;
}

//------------------------------------------------------------------------
void Dsp::process (const float* inLeft, const float* inRight,
                   float* outLeft, float* outRight, int frames)
{
	if (frames <= 0 || inLeft == nullptr || inRight == nullptr ||
	    outLeft == nullptr || outRight == nullptr)
		return;

	// Recomputed here rather than assumed, because setSampleRate may not
	// have been called yet in a standalone test.
	if (mTrimCoeff <= 0.0)
		setSampleRate (mSampleRate);

	for (int i = 0; i < frames; ++i)
	{
		//----------------------------------------------------------------
		// THE LINE. Pass-through for now; the ported DSP goes here.
		//----------------------------------------------------------------
		double left  = inLeft[i];
		double right = inRight[i];

		// Output trim, smoothed per sample.
		mTrim += (mTrimTarget - mTrim) * mTrimCoeff;

		outLeft[i]  = flush (static_cast<float> (left  * mTrim));
		outRight[i] = flush (static_cast<float> (right * mTrim));
	}
}

//------------------------------------------------------------------------
} // namespace VocalFilter
