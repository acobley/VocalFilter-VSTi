//------------------------------------------------------------------------
// VocalFilter - the audio line
//
// A three-formant vocal-tract model: on EACH channel, three bandpass
// resonators IN PARALLEL, summed, mixed against the dry signal and trimmed.
//
// Parallel, not cascaded, because a parallel bank is what lets each
// formant carry its own amplitude - which is the whole point of setting a
// vowel by hand. A cascade derives the relative levels from the pole
// positions and gives you no say in them.
//
//     in --+--> BP(F1, B1) * A1 --+
//          |                      |
//          +--> BP(F2, B2) * A2 --+--> wet --+
//          |                      |          |
//          +--> BP(F3, B3) * A3 --+          +--> mix --> trim --> out
//          |                                 |
//          +--------- dry -------------------+
//
// DELIBERATELY FREE OF SDK TYPES. Nothing in this file or its .cpp may
// include a VST3 header. Two reasons, both from the porting guide:
//
//   * it compiles and runs standalone with plain `c++ -std=c++17`, so the
//     numbers it produces can be tested for real - tests/DspTests.cpp
//     measures the actual magnitude response and checks where the peaks
//     land;
//   * VST3 splits the processor and the controller into separate
//     components, so anything the editor displays that the DSP computes
//     must come from ONE shared function both call. bandpassMagnitude()
//     below is that function for the response curve.
//------------------------------------------------------------------------

#pragma once

#include <cmath>

namespace VocalFilter {

//------------------------------------------------------------------------
// Shared mappings - call these from BOTH the DSP and the editor
//------------------------------------------------------------------------

//------------------------------------------------------------------------
/** One formant's settings, in the units phoneticians use. */
struct FormantSetting
{
	double freqHz;
	double bandwidthHz;
	double levelDb;
};


/** The output trim in dB, from the parameter's normalised value. */
double trimDb (double normalized);

/** A linear gain from dB, with `floorDb` and below treated as a true
    silence rather than a very small number. */
double dbToLinear (double db, double floorDb);

/** The COMPLEX response of ONE formant's bandpass at `freqHz`, excluding
    its level. Exactly the filter the DSP runs - same clamps, same
    coefficients - so a curve drawn from this cannot disagree with what is
    heard. */
void bandpassResponse (double centreHz, double bandwidthHz,
                       double freqHz, double sampleRate,
                       double& outReal, double& outImag);

/** |bandpassResponse|, for one formant on its own. */
double bandpassMagnitude (double centreHz, double bandwidthHz,
                          double freqHz, double sampleRate);

/** The magnitude of the WHOLE parallel bank, levels included - what the
    ear hears and what a response curve on the panel must draw.
 *
 *  It sums the formants as COMPLEX responses, and that is the whole point
 *  of the function existing. Adding the three magnitudes instead is wrong
 *  by up to a quarter of full scale between F1 and F2, because the dip
 *  between two formants is where it is BECAUSE OF THE PHASE BETWEEN THEM:
 *  a bandpass runs from +90 degrees below its centre to -90 above, so by
 *  1 kHz the F1 branch is most of a half-turn away from the F2 branch and
 *  the two partly cancel. tests/DspTests.cpp section 3 measures the
 *  difference against the running filter, and it was that test, written
 *  against a magnitude-summing version of this function, that caught it. */
double bankMagnitude (const FormantSetting* formants, int count,
                      double freqHz, double sampleRate);

//------------------------------------------------------------------------
// Ranges
//------------------------------------------------------------------------

constexpr int kFormantCount = 3;

/** The output trim. Top of travel is unity; for a port, make it the
    ORIGINAL's gain staging and set the default 20 dB below it. */
constexpr double kTrimMinDb     = -60.0;
constexpr double kTrimMaxDb     =   0.0;
constexpr double kTrimDefaultDb =   0.0;

/** Each formant gets its OWN frequency range rather than one wide range
    shared by all three. Two reasons: a slider spanning 100 Hz to 4 kHz
    wastes most of its travel on settings that are not a vowel, and the
    ranges keep F1 < F2 < F3 without a constraint to enforce. They are
    generous enough for every English vowel and then some. */
constexpr double kFormantFreqMin[kFormantCount] = {  200.0,  500.0, 1500.0 };
constexpr double kFormantFreqMax[kFormantCount] = { 1200.0, 3000.0, 4000.0 };

/** Bandwidth in Hz, the phonetics convention (Q is derived as f0/B).
    Measured adult bandwidths run about 50-140 Hz for B1, 62-149 for B2
    and 67-223 for B3, so this range covers the literature with room for
    deliberately unnatural settings at both ends. */
constexpr double kBandwidthMin = 20.0;
constexpr double kBandwidthMax = 400.0;

/** Per-formant level. The bottom of travel is silence, not -40 dB. */
constexpr double kLevelMinDb = -40.0;
constexpr double kLevelMaxDb =  12.0;

/** Dry / wet, as a percentage. */
constexpr double kMixMin = 0.0;
constexpr double kMixMax = 100.0;

//------------------------------------------------------------------------
/** The factory patch: the vowel /a/ as in "father" - "Aaa".
 *
 *  Frequencies are the classic adult-male means from Peterson & Barney
 *  (1952): F1 730, F2 1090, F3 2440 Hz. Women and children run higher -
 *  roughly 850 / 1220 / 2810 for women - so this patch is a male /a/
 *  specifically, and F2 in particular is what makes it /a/ rather than
 *  /o/ or /ae/.
 *
 *  Bandwidths sit mid-range of the measured adult values (B1 50-140,
 *  B2 62-149, B3 67-223 Hz across studies).
 *
 *  Levels are the part a parallel bank makes you choose: they do not come
 *  from the frequencies, they are set. F1 dominates in a low back vowel,
 *  so the higher formants are stepped down from it. */
constexpr FormantSetting kAaaFormants[kFormantCount] =
{
	{  730.0,  80.0,   0.0 },   // F1
	{ 1090.0,  90.0,  -7.0 },   // F2
	{ 2440.0, 120.0, -12.0 },   // F3
};

//------------------------------------------------------------------------
/** The five vowel buttons: A, E, I, O, U, as the letters are said.
 *
 *  FREQUENCIES are the classic adult-male means from Peterson & Barney
 *  (1952) for the ten monophthongs they measured - so all five sit in one
 *  consistent voice rather than being collected from wherever. The one
 *  exception is O: the letter names a diphthong, /ou/, which P&B did not
 *  measure, so it carries the widely used /o/ set instead. That is the
 *  only row not from the same table and it is marked below.
 *
 *  BANDWIDTHS are chosen within the measured adult spread (B1 50-140,
 *  B2 62-149, B3 67-223 Hz across studies), narrower at B1 for the close
 *  vowels - /i/ and /u/ - because bandwidth rises with formant frequency
 *  and those two have the lowest F1 of the set.
 *
 *  LEVELS are the same profile for all five, and that is a DECISION, not
 *  an oversight. See PORTING-NOTES section 2: deriving them from a
 *  three-pole cascade was tried and rejected - it puts F3 between -31 and
 *  -41 dB, which is inaudible, because a bare cascade of unity-DC
 *  resonators has neither the source's spectral tilt nor a higher-pole
 *  correction. There is no published parallel-bank amplitude table that
 *  covers these five, so rather than invent one dressed up as a
 *  measurement, every button recalls the same balance and the Level
 *  sliders are where you shape it. */
struct VowelPreset
{
	const char* name;       // what the button says
	const char* sound;      // what it actually is
	FormantSetting formants[kFormantCount];
};

constexpr int kVowelCount = 5;

constexpr VowelPreset kVowels[kVowelCount] =
{
	{ "Aaaa", "/a/ as in father",
	  { kAaaFormants[0], kAaaFormants[1], kAaaFormants[2] } },

	{ "Eeee", "/i/ as in beet",
	  { {  270.0,  50.0,   0.0 }, { 2290.0, 100.0,  -7.0 }, { 3010.0, 140.0, -12.0 } } },

	{ "Iiii", "/I/ as in bit",
	  { {  390.0,  60.0,   0.0 }, { 1990.0, 100.0,  -7.0 }, { 2550.0, 130.0, -12.0 } } },

	// NOT Peterson & Barney: /ou/ is a diphthong and they measured only
	// monophthongs. This is the common /o/ set.
	{ "Oooo", "/o/ as in boat",
	  { {  450.0,  60.0,   0.0 }, {  900.0,  90.0,  -7.0 }, { 2400.0, 120.0, -12.0 } } },

	{ "Uuuu", "/u/ as in boot",
	  { {  300.0,  50.0,   0.0 }, {  870.0,  90.0,  -7.0 }, { 2240.0, 120.0, -12.0 } } },
};

constexpr double kMixDefault = 100.0;   // fully wet: the model, not a colour

//------------------------------------------------------------------------
/** A bandpass biquad, RBJ cookbook, CONSTANT 0 dB PEAK GAIN form - so a
    formant's level is its level and not something the bandwidth also has
    a say in. Transposed direct form II. */
class Biquad
{
public:
	/** Coefficients only; the state is untouched, so this is safe to call
	    while running. */
	void setBandpass (double centreHz, double bandwidthHz, double sampleRate);

	void reset ();

	inline double process (double x)
	{
		const double y = mB0 * x + mS1;
		mS1 = mB1 * x - mA1 * y + mS2;
		mS2 = mB2 * x - mA2 * y;

		// A high-Q resonator decaying into silence is exactly where
		// denormals appear, and they cost hundreds of cycles a sample on
		// some hardware.
		if (mS1 > -1e-25 && mS1 < 1e-25) mS1 = 0.0;
		if (mS2 > -1e-25 && mS2 < 1e-25) mS2 = 0.0;

		return y;
	}

private:
	double mB0 = 0.0, mB1 = 0.0, mB2 = 0.0, mA1 = 0.0, mA2 = 0.0;
	double mS1 = 0.0, mS2 = 0.0;
};

//------------------------------------------------------------------------
/** The audio line. Stereo in, stereo out; 32-bit float internally, which
    is what these plug-ins were written for, with a 64-bit host converted
    into it by the processor rather than refused. */
class Dsp
{
public:
	/** Everything rate-dependent is recomputed here, never inside
	    process(): a hard-coded coefficient puts its corner at a fixed
	    fraction of Nyquist, so the same patch is over an octave brighter
	    at 96 k than at 44.1 k. */
	void setSampleRate (double sampleRate);
	double sampleRate () const { return mSampleRate; }

	/** Silence the state and snap the smoothers. Called from setActive,
	    not from the audio thread. */
	void reset ();

	//--------------------------------------------------------------------
	// Targets. All in plain units - the processor converts from
	// normalised through the parameter table, so the DSP never has to
	// know what a ParamID is. Every one of these is SMOOTHED; nothing
	// here takes effect as a step.
	//--------------------------------------------------------------------
	void setFormant (int index, double freqHz, double bandwidthHz, double levelDb);
	void setMixPercent (double percent);
	void setTrimNormalized (double normalized);

	/** Snap every smoother to its target, for a state load or an activate
	    where a 20 ms ramp from the old value would be wrong. */
	void snapParameters ();

	/** in and out may alias. `frames` may be zero. */
	void process (const float* inLeft, const float* inRight,
	              float* outLeft, float* outRight, int frames);

	/** Latency in samples. Zero - the bank is minimum-phase and adds no
	    delay line. */
	int latencySamples () const { return 0; }

	/** How long the narrowest resonator keeps ringing after its input
	    goes silent, in samples. A host that cuts processing at the end of
	    a note otherwise chops the tail off. */
	int tailSamples () const;

private:
	void updateCoefficients ();

	double mSampleRate = 44100.0;
	double mSmoothCoeff = 0.0;

	struct Formant
	{
		double freqTarget = 1000.0, bwTarget = 100.0, gainTarget = 1.0;
		double freq       = 1000.0, bw       = 100.0, gain       = 1.0;
		Biquad filter[2];                 // shared coefficients, per-channel state
	};

	Formant mFormant[kFormantCount];

	double mMixTarget = 1.0, mMix = 1.0;      // 0 dry .. 1 wet
	double mTrimTarget = 1.0, mTrim = 1.0;    // linear

	/** Coefficients are recomputed every this many samples rather than
	    every sample: a biquad update is a sin and a cos, and with the
	    parameters themselves smoothed over 20 ms the difference across
	    sixteen samples is far below anything audible. */
	static constexpr int kCoeffUpdateSamples = 16;
	int mCoeffCountdown = 0;
};

//------------------------------------------------------------------------
} // namespace VocalFilter
