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

    The frequency ceiling, 0.45 x the sample rate (0.9 x Nyquist), keeps
    the resonator off Nyquist itself, where the bilinear transform's
    warping makes a bandpass stop behaving like one. It is a backstop
    only: kFormantFreqMax stops every centre at 4 kHz or below, far under
    it at any supported rate.

    The Q ceiling is the guard against unbounded resonance: nothing
    stops a host automating the bandwidth to its minimum while the centre
    frequency is at its maximum, which is a Q of 200 and a filter that
    rings for a second and a half. */
constexpr double kMinCentreHz = 20.0;
constexpr double kMaxCentreFraction = 0.45;
constexpr double kMinQ = 0.3;
constexpr double kMaxQ = 60.0;

struct BandpassCoeffs { double b0, b1, b2, a1, a2; };

BandpassCoeffs bandpassCoeffs (double centreHz, double bandwidthHz, double sampleRate)
{
	const double fs = (sampleRate > 0.0) ? sampleRate : 44100.0;

	double f0 = std::min (std::max (centreHz, kMinCentreHz), fs * kMaxCentreFraction);
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
double trimNormalized (double db)
{
	const double clamped = std::min (kTrimMaxDb, std::max (kTrimMinDb, db));
	return (clamped - kTrimMinDb) / (kTrimMaxDb - kTrimMinDb);
}

//------------------------------------------------------------------------
double trimNormalizedFromV1 (double v1Normalized)
{
	const double clamped = std::min (1.0, std::max (0.0, v1Normalized));
	return trimNormalized (kTrimMinDb + clamped * (kTrimV1MaxDb - kTrimMinDb));
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
// Level Comp
//------------------------------------------------------------------------
namespace {

/** One biquad's |H|^2 at `freqHz`, for a filter DESIGNED at `designRate`. */
double biquadPower (const double b[3], const double a[3], double freqHz, double designRate)
{
	const double w = 2.0 * M_PI * freqHz / designRate;
	const double c1 = std::cos (w), s1 = std::sin (w);
	const double c2 = std::cos (2.0 * w), s2 = std::sin (2.0 * w);
	const double nr = b[0] + b[1] * c1 + b[2] * c2, ni = -(b[1] * s1 + b[2] * s2);
	const double dr = a[0] + a[1] * c1 + a[2] * c2, di = -(a[1] * s1 + a[2] * s2);
	const double d = dr * dr + di * di;
	return (d > 1e-30) ? (nr * nr + ni * ni) / d : 0.0;
}

constexpr double kGridLowHz  = 20.0;
constexpr double kGridHighHz = 20000.0;

} // namespace

//------------------------------------------------------------------------
double kWeightingPower (double freqHz)
{
	// BS.1770-4's two stages, as published for 48 kHz. The WEIGHT is a
	// function of frequency, not of the plug-in's rate, so it is always
	// evaluated on the 48 kHz design - valid to 24 kHz, above the grid.
	static const double shelfB[3] = { 1.53512485958697, -2.69169618940638, 1.19839281085285 };
	static const double shelfA[3] = { 1.0,              -1.69065929318241, 0.73248077421585 };
	static const double hpB[3]    = { 1.0, -2.0, 1.0 };
	static const double hpA[3]    = { 1.0, -1.99004745483398, 0.99007225036621 };

	return biquadPower (shelfB, shelfA, freqHz, 48000.0)
	     * biquadPower (hpB, hpA, freqHz, 48000.0);
}

//------------------------------------------------------------------------
void LevelCompModel::setSampleRate (double sampleRate)
{
	const double fs = (sampleRate > 0.0) ? sampleRate : 44100.0;
	if (fs == mSampleRate)
		return;
	mSampleRate = fs;

	// Stop short of the bandpass's own centre ceiling at low rates, so the
	// grid never asks about a frequency the line cannot produce.
	const double high = std::min (kGridHighHz, fs * kMaxCentreFraction);
	const double ratio = std::log (high / kGridLowHz);

	mCount = kPoints;
	mWeightSum = 0.0;
	for (int i = 0; i < kPoints; ++i)
	{
		// Log-spaced, so equal weight per point IS pink noise; the
		// K-weighting is the only shaping on top.
		const double f = kGridLowHz * std::exp (ratio * i / (kPoints - 1));
		const double w = 2.0 * M_PI * f / fs;
		mCos1[i] = std::cos (w);
		mSin1[i] = std::sin (w);
		mCos2[i] = std::cos (2.0 * w);
		mSin2[i] = std::sin (2.0 * w);
		mWeight[i] = kWeightingPower (f);
		mWeightSum += mWeight[i];
	}
}

//------------------------------------------------------------------------
double LevelCompModel::weightedPowerGain (const double* freqHz, const double* bandwidthHz,
                                          const double* gainLinear, double mix) const
{
	if (mCount == 0 || mWeightSum <= 0.0)
		return 1.0;

	const double m = std::min (1.0, std::max (0.0, mix));

	// The SAME coefficients the running filter uses - same clamps, same
	// design - so the model is of what is heard.
	BandpassCoeffs c[kFormantCount];
	double g[kFormantCount];
	for (int k = 0; k < kFormantCount; ++k)
	{
		c[k] = bandpassCoeffs (freqHz[k], bandwidthHz[k], mSampleRate);
		g[k] = gainLinear[k] * kFormantPolarity[k] * m;
	}

	double sum = 0.0;
	for (int i = 0; i < mCount; ++i)
	{
		// The dry path, then each formant summed COMPLEX - as bankMagnitude
		// does and for the same reason.
		double re = 1.0 - m, im = 0.0;
		for (int k = 0; k < kFormantCount; ++k)
		{
			const double nr = c[k].b0 + c[k].b1 * mCos1[i] + c[k].b2 * mCos2[i];
			const double ni =         -(c[k].b1 * mSin1[i] + c[k].b2 * mSin2[i]);
			const double dr = 1.0     + c[k].a1 * mCos1[i] + c[k].a2 * mCos2[i];
			const double di =         -(c[k].a1 * mSin1[i] + c[k].a2 * mSin2[i]);
			const double d = dr * dr + di * di;
			if (d < 1e-30)
				continue;
			re += g[k] * (nr * dr + ni * di) / d;
			im += g[k] * (ni * dr - nr * di) / d;
		}
		sum += mWeight[i] * (re * re + im * im);
	}
	return sum / mWeightSum;
}

//------------------------------------------------------------------------
double LevelCompModel::compensationGain (const double* freqHz, const double* bandwidthHz,
                                         const double* gainLinear, double mix) const
{
	const double power = weightedPowerGain (freqHz, bandwidthHz, gainLinear, mix);
	const double maxGain = std::pow (10.0, kLevelCompMaxDb / 20.0);
	if (power <= 1e-30)
		return maxGain;
	const double gain = 1.0 / std::sqrt (power);
	return std::min (maxGain, std::max (1.0 / maxGain, gain));
}

//------------------------------------------------------------------------
double levelCompensationDb (const FormantSetting* formants, int count,
                            double mixPercent, double sampleRate)
{
	if (formants == nullptr || count != kFormantCount)
		return 0.0;

	double f[kFormantCount], b[kFormantCount], g[kFormantCount];
	for (int k = 0; k < kFormantCount; ++k)
	{
		f[k] = formants[k].freqHz;
		b[k] = formants[k].bandwidthHz;
		g[k] = dbToLinear (formants[k].levelDb, kLevelMinDb);
	}

	// About 19 kB, off the audio thread - this is for tests and displays.
	static thread_local LevelCompModel model;
	model.setSampleRate (sampleRate);
	return 20.0 * std::log10 (model.compensationGain (f, b, g, mixPercent / 100.0));
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
	mCompModel.setSampleRate (mSampleRate);
	mCompDirty = true;
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
		mCompDirty = true;
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
void Dsp::setLevelComp (bool on)
{
	if (on != mCompOn)
	{
		mCompOn = on;
		mCompDirty = true;
	}
}

//------------------------------------------------------------------------
double Dsp::computeCompGain () const
{
	if (! mCompOn)
		return 1.0;

	double f[kFormantCount], b[kFormantCount], g[kFormantCount];
	for (int k = 0; k < kFormantCount; ++k)
	{
		f[k] = mFormant[k].freq.value ();
		b[k] = mFormant[k].bw.value ();
		g[k] = mFormant[k].gain.value ();
	}
	return mCompModel.compensationGain (f, b, g, mMix);
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

	mCompTarget = mComp = computeCompGain ();
	mCompDirty = false;
	mCompWasMoving = false;
	mCompCountdown = 0;

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
		mMix  = smoothTowards (mMix,  mMixTarget,  mSmoothCoeff);
		mTrim = smoothTowards (mTrim, mTrimTarget, mSmoothCoeff);

		if (--mCoeffCountdown <= 0)
		{
			updateCoefficients ();
			mCoeffCountdown = kCoeffUpdateSamples;
		}

		// Level Comp follows the formants and the mix. Recomputed only
		// while one of them is moving, plus once after it stops so the
		// gain lands on the settled value, plus whenever it was told to.
		if (--mCompCountdown <= 0)
		{
			mCompCountdown = kCompUpdateSamples;
			const bool moving = mCompOn && (gliding () || mMix != mMixTarget);
			if (mCompDirty || moving || mCompWasMoving)
			{
				mCompTarget = computeCompGain ();
				mCompDirty = false;
			}
			mCompWasMoving = moving;
		}
		mComp = smoothTowards (mComp, mCompTarget, mSmoothCoeff);

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

		const double out = mTrim * mComp;
		outLeft[i]  = flush (static_cast<float> (left  * out));
		outRight[i] = flush (static_cast<float> (right * out));
	}
}

//------------------------------------------------------------------------
} // namespace VocalFilter
