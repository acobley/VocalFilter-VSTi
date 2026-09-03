//------------------------------------------------------------------------
// VocalFilter - the audio line
//------------------------------------------------------------------------

#include "VocalFilterDsp.h"

#include <algorithm>

namespace VocalFilter {

namespace {

/** The parameter smoothers' time constant. 20 ms is short enough that a
    slider feels immediate and long enough that sweeping a formant across
    its whole range does not tear. */
constexpr double kSmoothSeconds = 0.020;

constexpr float kDenormalFloor = 1.0e-25f;

inline float flush (float x)
{
	return (x > -kDenormalFloor && x < kDenormalFloor) ? 0.0f : x;
}

/** Clamps shared by the running filter and by bandpassMagnitude, so a
    curve drawn from the one cannot disagree with the other.

    The frequency ceiling keeps the resonator well clear of Nyquist, where
    the bilinear transform's warping makes a bandpass stop behaving like
    one. The Q ceiling is the guard against unbounded resonance: nothing
    stops a host automating the bandwidth to its minimum while the centre
    frequency is at its maximum, which is a Q of 200 and a filter that
    rings for a second and a half. */
constexpr double kMinCentreHz = 20.0;
constexpr double kNyquistFraction = 0.45;
constexpr double kMinQ = 0.3;
constexpr double kMaxQ = 60.0;

struct BandpassCoeffs { double b0, b1, b2, a1, a2; };

BandpassCoeffs bandpassCoeffs (double centreHz, double bandwidthHz, double sampleRate)
{
	const double fs = (sampleRate > 0.0) ? sampleRate : 44100.0;

	double f0 = std::min (std::max (centreHz, kMinCentreHz), fs * kNyquistFraction);
	double bw = std::max (bandwidthHz, 1.0);
	double q  = std::min (std::max (f0 / bw, kMinQ), kMaxQ);

	const double w0    = 2.0 * M_PI * f0 / fs;
	const double cosw0 = std::cos (w0);
	const double alpha = std::sin (w0) / (2.0 * q);

	// RBJ bandpass, CONSTANT 0 dB PEAK GAIN: b0 = alpha, not b0 = q*alpha.
	// The other spelling makes the peak gain equal to Q, so narrowing a
	// formant would make it louder and the level slider would be arguing
	// with the bandwidth slider.
	const double a0 = 1.0 + alpha;

	BandpassCoeffs c;
	c.b0 =  alpha / a0;
	c.b1 =  0.0;
	c.b2 = -alpha / a0;
	c.a1 = (-2.0 * cosw0) / a0;
	c.a2 = ( 1.0 - alpha) / a0;
	return c;
}

} // namespace

//------------------------------------------------------------------------
double trimDb (double normalized)
{
	const double clamped = std::min (1.0, std::max (0.0, normalized));
	return kTrimMinDb + clamped * (kTrimMaxDb - kTrimMinDb);
}

//------------------------------------------------------------------------
double dbToLinear (double db, double floorDb)
{
	if (db <= floorDb)
		return 0.0;
	return std::pow (10.0, db / 20.0);
}

//------------------------------------------------------------------------
void bandpassResponse (double centreHz, double bandwidthHz,
                       double freqHz, double sampleRate,
                       double& outReal, double& outImag)
{
	const double fs = (sampleRate > 0.0) ? sampleRate : 44100.0;
	const BandpassCoeffs c = bandpassCoeffs (centreHz, bandwidthHz, fs);

	// H(e^jw) evaluated directly from the coefficients, so this is the
	// response of the filter that actually runs rather than of the
	// analogue prototype it came from.
	const double w = 2.0 * M_PI * freqHz / fs;
	const double cw = std::cos (w), sw = std::sin (w);
	const double c2w = std::cos (2.0 * w), s2w = std::sin (2.0 * w);

	const double numRe = c.b0 + c.b1 * cw + c.b2 * c2w;
	const double numIm =      -(c.b1 * sw + c.b2 * s2w);
	const double denRe = 1.0  + c.a1 * cw + c.a2 * c2w;
	const double denIm =      -(c.a1 * sw + c.a2 * s2w);

	const double den = denRe * denRe + denIm * denIm;
	if (den < 1e-30)
	{
		outReal = 0.0;
		outImag = 0.0;
		return;
	}

	// (num / den) with a complex denominator, written out rather than
	// pulled in from <complex> - this header is included by the tests and
	// by anything that draws a curve, and it stays free of everything it
	// does not need.
	outReal = (numRe * denRe + numIm * denIm) / den;
	outImag = (numIm * denRe - numRe * denIm) / den;
}

//------------------------------------------------------------------------
double bandpassMagnitude (double centreHz, double bandwidthHz,
                          double freqHz, double sampleRate)
{
	double re = 0.0, im = 0.0;
	bandpassResponse (centreHz, bandwidthHz, freqHz, sampleRate, re, im);
	return std::sqrt (re * re + im * im);
}

//------------------------------------------------------------------------
double bankMagnitude (const FormantSetting* formants, int count,
                      double freqHz, double sampleRate)
{
	if (formants == nullptr || count <= 0)
		return 0.0;

	double sumRe = 0.0, sumIm = 0.0;
	for (int k = 0; k < count; ++k)
	{
		double re = 0.0, im = 0.0;
		bandpassResponse (formants[k].freqHz, formants[k].bandwidthHz,
		                  freqHz, sampleRate, re, im);

		// COMPLEX sum, weighted by the formant's own level AND ITS SIGN -
		// see the notes in the header. Summing |H| here instead is the bug
		// the test suite was written to catch; dropping the sign would be
		// the same mistake in a quieter form, since the polarity is what
		// puts the region between two formants where a tract puts it.
		const double gain = dbToLinear (formants[k].levelDb, kLevelMinDb)
		                  * kFormantPolarity[k];
		sumRe += re * gain;
		sumIm += im * gain;
	}
	return std::sqrt (sumRe * sumRe + sumIm * sumIm);
}

//------------------------------------------------------------------------
void Biquad::setBandpass (double centreHz, double bandwidthHz, double sampleRate)
{
	const BandpassCoeffs c = bandpassCoeffs (centreHz, bandwidthHz, sampleRate);
	mB0 = c.b0; mB1 = c.b1; mB2 = c.b2; mA1 = c.a1; mA2 = c.a2;
}

//------------------------------------------------------------------------
void Biquad::reset ()
{
	mS1 = 0.0;
	mS2 = 0.0;
}

//------------------------------------------------------------------------
void Dsp::setSampleRate (double sampleRate)
{
	mSampleRate = (sampleRate > 0.0) ? sampleRate : 44100.0;
	mSmoothCoeff = 1.0 - std::exp (-1.0 / (kSmoothSeconds * mSampleRate));
	updateCoefficients ();
}

//------------------------------------------------------------------------
void Dsp::reset ()
{
	snapParameters ();
	for (Formant& f : mFormant)
		for (Biquad& b : f.filter)
			b.reset ();
}

//------------------------------------------------------------------------
int Dsp::glideSamples () const
{
	const double ms = std::max (mGlideMs, kGlideFloorMs);
	const int n = static_cast<int> (ms * 0.001 * mSampleRate + 0.5);
	return (n > 1) ? n : 1;
}

//------------------------------------------------------------------------
void Dsp::setGlideMs (double milliseconds)
{
	mGlideMs = std::min (std::max (milliseconds, kGlideMinMs), kGlideMaxMs);
}

//------------------------------------------------------------------------
void Dsp::setFormant (int index, double freqHz, double bandwidthHz, double levelDb)
{
	if (index < 0 || index >= kFormantCount)
		return;

	Formant& f = mFormant[index];
	const double gain = dbToLinear (levelDb, kLevelMinDb);

	// THE FIRST SETTINGS ARE NOT A GLIDE. Without this the plug-in would
	// spend its first 150 ms sliding up from silence at 0 Hz to the
	// factory patch, every time it was instantiated.
	if (! mSeeded)
	{
		f.freq.snapTo (freqHz);
		f.bw.snapTo (bandwidthHz);
		f.gain.snapTo (gain);

		// Seeded once the LAST formant has been given its first values,
		// so all three are snapped before any of them can glide.
		if (index == kFormantCount - 1)
			mSeeded = true;
		return;
	}

	// One length for all nine, taken once, so a vowel's three frequencies,
	// three widths and three levels are given the same number of samples
	// and therefore arrive on the same sample - however far each has to
	// travel. That is the whole of "they arrive together"; the rest is
	// making sure nothing restarts a ramp that is already running (see
	// Ramp::setTarget) and nothing accumulates error on the way (see
	// Ramp::tick).
	const int samples = glideSamples ();

	f.freq.setTarget (freqHz, samples);
	f.bw.setTarget (bandwidthHz, samples);
	f.gain.setTarget (gain, samples);
}

//------------------------------------------------------------------------
bool Dsp::gliding () const
{
	for (const Formant& f : mFormant)
		if (f.freq.moving () || f.bw.moving () || f.gain.moving ())
			return true;
	return false;
}

//------------------------------------------------------------------------
double Dsp::formantFreq (int index) const
{
	return (index >= 0 && index < kFormantCount) ? mFormant[index].freq.value () : 0.0;
}

double Dsp::formantBandwidth (int index) const
{
	return (index >= 0 && index < kFormantCount) ? mFormant[index].bw.value () : 0.0;
}

double Dsp::formantGain (int index) const
{
	return (index >= 0 && index < kFormantCount) ? mFormant[index].gain.value () : 0.0;
}

//------------------------------------------------------------------------
void Dsp::setMixPercent (double percent)
{
	mMixTarget = std::min (1.0, std::max (0.0, percent / 100.0));
}

//------------------------------------------------------------------------
void Dsp::setTrimNormalized (double normalized)
{
	mTrimTarget = dbToLinear (trimDb (normalized), kTrimMinDb);
}

//------------------------------------------------------------------------
void Dsp::snapParameters ()
{
	for (Formant& f : mFormant)
	{
		f.freq.snap ();
		f.bw.snap ();
		f.gain.snap ();
	}
	mMix  = mMixTarget;
	mTrim = mTrimTarget;

	updateCoefficients ();
	mCoeffCountdown = 0;
}

//------------------------------------------------------------------------
void Dsp::updateCoefficients ()
{
	for (Formant& f : mFormant)
	{
		f.filter[0].setBandpass (f.freq.value (), f.bw.value (), mSampleRate);
		f.filter[1].setBandpass (f.freq.value (), f.bw.value (), mSampleRate);
	}
}

//------------------------------------------------------------------------
int Dsp::tailSamples () const
{
	// A resonator's envelope decays as exp(-pi * B * t), so 60 dB takes
	// about 7 / (pi * B) seconds. Report the NARROWEST formant's tail,
	// since that is the one still ringing last.
	double narrowest = kBandwidthMax;
	for (const Formant& f : mFormant)
		narrowest = std::min (narrowest, std::max (f.bw.value (), 1.0));

	const double seconds = 7.0 / (M_PI * narrowest);
	return static_cast<int> (seconds * mSampleRate + 0.5);
}

//------------------------------------------------------------------------
void Dsp::process (const float* inLeft, const float* inRight,
                   float* outLeft, float* outRight, int frames)
{
	if (frames <= 0 || inLeft == nullptr || inRight == nullptr ||
	    outLeft == nullptr || outRight == nullptr)
		return;

	if (mSmoothCoeff <= 0.0)
		setSampleRate (mSampleRate);

	for (int i = 0; i < frames; ++i)
	{
		//----------------------------------------------------------------
		// Advance every glide one sample, and one-pole the two controls
		// that are not part of a vowel.
		//----------------------------------------------------------------
		for (Formant& f : mFormant)
		{
			f.freq.tick ();
			f.bw.tick ();
			f.gain.tick ();
		}
		mMix  += (mMixTarget  - mMix)  * mSmoothCoeff;
		mTrim += (mTrimTarget - mTrim) * mSmoothCoeff;

		if (--mCoeffCountdown <= 0)
		{
			updateCoefficients ();
			mCoeffCountdown = kCoeffUpdateSamples;
		}

		//----------------------------------------------------------------
		// The bank. Parallel, summed, then mixed against the dry signal.
		//----------------------------------------------------------------
		const double dryL = inLeft[i];
		const double dryR = inRight[i];

		double wetL = 0.0, wetR = 0.0;
		for (int k = 0; k < kFormantCount; ++k)
		{
			Formant& f = mFormant[k];
			const double gain = f.gain.value () * kFormantPolarity[k];
			wetL += f.filter[0].process (dryL) * gain;
			wetR += f.filter[1].process (dryR) * gain;
		}

		const double left  = dryL + (wetL - dryL) * mMix;
		const double right = dryR + (wetR - dryR) * mMix;

		outLeft[i]  = flush (static_cast<float> (left  * mTrim));
		outRight[i] = flush (static_cast<float> (right * mTrim));
	}
}

//------------------------------------------------------------------------
} // namespace VocalFilter
