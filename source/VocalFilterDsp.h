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
// include a VST3 header. Two reasons:
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

/** THE SIGN each formant is summed with. Alternating, which is what a
    parallel formant synthesiser has always done, and it is not cosmetic.
 *
 *  Three bandpasses summed in phase get the PEAKS right and the regions
 *  BETWEEN them wrong. A bandpass runs from +90 degrees below its centre
 *  to -90 above, so between F1 and F2 the F1 branch is near -90 and the F2
 *  branch near +90: summed with the SAME sign they are half a turn apart
 *  and CANCEL, putting a spurious deep null between the formants. An
 *  all-pole tract has no null there - it dips smoothly - and inverting F2
 *  is what removes it. Measured on the Aaa patch, the valley between F1
 *  and F2 is -20.4 dB summed in phase and -8.5 dB inverted; the second is
 *  the tract's shape. Fitting this bank against the
 *  all-pole cascade the same formants imply, over 100-4000 Hz with a 40 dB
 *  floor, each configuration given its own best levels:
 *
 *      vowel     all +      + - +
 *      Aaaa       5.56       4.16      RMS dB error
 *      Eeee       4.29       4.22
 *      Iiii       4.64       4.23
 *      Oooo       6.28       3.63
 *      Uuuu       5.44       3.19
 *
 *  Better for every vowel, and by over 2 dB on the back vowels. See
 *  ENGINEERING-NOTES DEVIATION 3. */
constexpr double kFormantPolarity[kFormantCount] = { 1.0, -1.0, 1.0 };

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

/** How long a formant takes to reach a new value, in milliseconds.
 *
 *  The default is 150 ms because that is roughly how long a real
 *  articulator takes: diphthong glides in speech run about 100-200 ms, so
 *  a vowel button at this setting moves the tract at about the rate a
 *  mouth does.
 *
 *  ZERO DOES NOT MEAN INSTANT. Every glide is floored at kGlideFloorMs,
 *  because F2 jumping from 870 Hz to 2290 Hz between two samples is a
 *  click, and a control that can produce one is a control that will. The
 *  floor is the 20 ms the line used to smooth everything with, so a glide
 *  of 0 is exactly the behaviour this plug-in had before the control
 *  existed. */
constexpr double kGlideMinMs     =    0.0;
constexpr double kGlideMaxMs     = 2000.0;
constexpr double kGlideDefaultMs =  150.0;
constexpr double kGlideFloorMs   =   20.0;

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
	{ 1090.0,  90.0,  -3.3 },   // F2
	{ 2440.0, 120.0, -26.8 },   // F3
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
 *  LEVELS ARE DERIVED, not chosen. A vocal tract is an all-pole filter,
 *  so its formant amplitudes are a CONSEQUENCE of the frequencies and
 *  bandwidths, not free parameters - which is exactly why a cascade
 *  synthesiser needs no amplitude controls and a parallel one, like this,
 *  cannot do without them (Klatt 1980, after Fant 1956).
 *
 *  Each pair below is fitted: build the all-pole cascade these formants
 *  imply, including higher poles at 3500/4500/5500 Hz for the ones a
 *  three-formant model leaves out, then find the A2 and A3 that make THIS
 *  bank - these bandpasses, this polarity - match it best over
 *  100-4000 Hz. A1 is pinned at 0 dB, which keeps the five vowels at
 *  roughly equal loudness; the tract alone would make Eeee 11 dB quieter
 *  than Aaaa, and a button that drops the mix 11 dB is not what anyone
 *  wants from an effect.
 *
 *  An earlier version of this table gave all five the same 0 / -7 / -12
 *  and called it a decision. It was a mistake, and ENGINEERING-NOTES
 *  DEVIATION 2 records how it was found. */
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
	  { {  270.0,  50.0,   0.0 }, { 2290.0, 100.0,  -8.0 }, { 3010.0, 140.0,  -2.3 } } },

	{ "Iiii", "/I/ as in bit",
	  { {  390.0,  60.0,   0.0 }, { 1990.0, 100.0,  -8.2 }, { 2550.0, 130.0,  -7.6 } } },

	// NOT Peterson & Barney: /ou/ is a diphthong and they measured only
	// monophthongs. This is the common /o/ set.
	//
	// F3 for the two back vowels fits at -34.6 and -37.3, but the fit is
	// FLAT below about -28 dB - F3 that far down barely affects the
	// spectrum, so moving it 6 dB changes the error by 0.05 dB. Both are
	// set to -30 instead: inside the flat region, and 10 dB clear of the
	// Level parameter's own silence floor at -40, where a small nudge
	// would switch F3 off altogether.
	{ "Oooo", "/o/ as in boat",
	  { {  450.0,  60.0,   0.0 }, {  900.0,  90.0,  -7.4 }, { 2400.0, 120.0, -30.0 } } },

	{ "Uuuu", "/u/ as in boot",
	  { {  300.0,  50.0,   0.0 }, {  870.0,  90.0, -10.2 }, { 2240.0, 120.0, -30.0 } } },
};

constexpr double kMixDefault = 100.0;   // fully wet: the model, not a colour

//------------------------------------------------------------------------
/** The vowel SELECTOR: 0 is Manual, 1..kVowelCount are the presets.
 *
 *  This is the only place the mapping lives, and it is here - in the layer
 *  that includes no SDK header - rather than in the processor, so the test
 *  suite can reach it. A host automating the Vowel parameter and a finger
 *  on a panel button both end up calling this.
 *
 *  Returns nullptr for Manual, meaning "use the nine formant parameters".
 */
constexpr int kVowelManual  = 0;
constexpr int kVowelChoices = kVowelCount + 1;

inline const FormantSetting* vowelSelection (int selector)
{
	if (selector <= kVowelManual || selector > kVowelCount)
		return nullptr;
	return kVowels[selector - 1].formants;
}

/** What the host's parameter list calls each position. */
inline const char* vowelSelectionName (int selector)
{
	if (selector <= kVowelManual || selector > kVowelCount)
		return "Manual";
	return kVowels[selector - 1].name;
}

//------------------------------------------------------------------------
/** A LINEAR ramp of fixed duration - the thing a glide is made of.
 *
 *  Not a one-pole. A one-pole is the right smoother for a control that
 *  should just stop tearing, but it is the wrong thing here for two
 *  reasons: it never actually ARRIVES, only approaches; and its duration
 *  does not depend on how far it has to go, so "all parameters arrive at
 *  the same time" cannot even be stated in it. A ramp of N samples arrives
 *  exactly, at sample N, whatever the distance - so nine of them started
 *  together and given the same N finish together, which is the whole
 *  requirement.
 *
 *  The last step ASSIGNS the target rather than adding the increment
 *  again: over 2000 ms at 192 k that is 384000 additions of a number
 *  around 1e-5, and the accumulated error is what would leave a formant
 *  a hertz or two short of where the panel says it is. */
class Ramp
{
public:
	/** Aim somewhere new over `samples`. A target that has not changed is
	    IGNORED - the processor pushes every parameter every block, and
	    restarting the ramp each time would mean it never arrived. */
	void setTarget (double target, int samples)
	{
		if (target == mTarget)
			return;

		mTarget = target;
		mRemaining = (samples > 1) ? samples : 1;
		mIncrement = (mTarget - mValue) / static_cast<double> (mRemaining);
	}

	/** Set both ends at once, for construction and for a state load. */
	void snapTo (double value)
	{
		mValue = mTarget = value;
		mIncrement = 0.0;
		mRemaining = 0;
	}

	/** Finish immediately wherever we are aimed. */
	void snap () { snapTo (mTarget); }

	inline double tick ()
	{
		if (mRemaining > 0)
		{
			if (--mRemaining == 0)
				mValue = mTarget;          // exact arrival, not 384000 additions
			else
				mValue += mIncrement;
		}
		return mValue;
	}

	double value () const { return mValue; }
	double target () const { return mTarget; }
	bool moving () const { return mRemaining > 0; }

private:
	double mValue = 0.0;
	double mTarget = 0.0;
	double mIncrement = 0.0;
	int    mRemaining = 0;
};

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

	/** How long a formant takes to reach a new value. Applies to the NEXT
	    move, not to one already under way. */
	void setGlideMs (double milliseconds);

	/** True while any formant is still on its way somewhere - the test
	    suite uses it to measure that they all stop on the same sample. */
	bool gliding () const;

	//--------------------------------------------------------------------
	// Where the formants are RIGHT NOW, part way through a glide, as
	// opposed to where the parameters say they are going. The test suite
	// reads these to prove the nine arrive together; a response curve on
	// the panel would want them too, so that what is drawn during a glide
	// is what is being heard.
	//--------------------------------------------------------------------
	double formantFreq (int index) const;
	double formantBandwidth (int index) const;
	double formantGain (int index) const;      // LINEAR

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

	/** Samples in a glide, from the current glide time and sample rate,
	    floored at kGlideFloorMs. */
	int glideSamples () const;

	struct Formant
	{
		// Frequency ramps LINEARLY IN HERTZ, not in semitones. A formant
		// is a resonance of a tube whose geometry is changing, and tract
		// geometry maps to formant frequency far closer to linearly than
		// logarithmically - so this is the shape an articulator actually
		// makes. A log glide is one line away if it ever sounds better.
		Ramp freq;
		Ramp bw;
		Ramp gain;                        // LINEAR gain, not dB
		Biquad filter[2];                 // shared coefficients, per-channel state
	};

	Formant mFormant[kFormantCount];

	// Dry/Wet and Output Trim are NOT part of a vowel, so they keep the
	// old 20 ms one-pole: they should stop tearing, and nothing needs them
	// to arrive in step with anything.
	double mMixTarget = 1.0, mMix = 1.0;      // 0 dry .. 1 wet
	double mTrimTarget = 1.0, mTrim = 1.0;    // linear

	double mGlideMs = kGlideDefaultMs;
	bool   mSeeded = false;                   // first setFormant snaps rather than glides

	/** Coefficients are recomputed every this many samples rather than
	    every sample, because a biquad update is a sin and a cos.
	 *
	 *  EIGHT IS MEASURED, NOT GUESSED. Sweeping this while measuring the
	 *  artefact energy a fastest-legal glide puts at 6-12 kHz, against the
	 *  same vowel change applied instantly:
	 *
	 *      1, 2, 4, 8 samples  ->  37 dB below a snap
	 *            16 samples    ->  24 dB
	 *            32 samples    ->  24 dB
	 *            64 samples    ->  15 dB
	 *
	 *  There is a knee between 8 and 16: everything finer than 8 buys
	 *  nothing, and 16 - which is what this was originally - costs 13 dB.
	 *  The test in section 8b asserts the 30 dB side of that knee, so a
	 *  change back to 16 fails it. */
	static constexpr int kCoeffUpdateSamples = 8;
	int mCoeffCountdown = 0;
};

//------------------------------------------------------------------------
} // namespace VocalFilter
