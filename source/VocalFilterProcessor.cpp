//------------------------------------------------------------------------
// VocalFilter - audio processor implementation
//------------------------------------------------------------------------

#include "VocalFilterProcessor.h"
#include "VocalFilterIDs.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"

#include <algorithm>
#include <cstring>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace VocalFilter {

namespace {

/** Bumped only if the state layout below changes incompatibly. */
constexpr int32 kStateVersion = 1;

} // namespace

//------------------------------------------------------------------------
VocalFilterProcessor::VocalFilterProcessor ()
{
	setControllerClass (kVocalFilterControllerUID);

	// NO processContextRequirements - deliberately.
	//
	// Since VST3 3.7 the ProcessContext is opt-in and the default is NO
	// FLAGS, so a plug-in that reads data.processContext->tempo without
	// asking gets 120 in every host and no warning anywhere; the validator
	// prints "ProcessContextRequirements: - None" rather than complaining.
	// This line is a pass-through and syncs to nothing. THE MOMENT
	// ANYTHING HERE READS THE TEMPO, THE TRANSPORT OR THE MUSICAL
	// POSITION, add:
	//
	//     processContextRequirements.needTempo ();
	//
	// here, in the constructor.

	mParams.assign (kNumParams, 0.0);
	for (ParamID id = 0; id < kNumParams; ++id)
		mParams[id] = kParams[id].defaultNormalized ();
}

//------------------------------------------------------------------------
tresult PLUGIN_API VocalFilterProcessor::initialize (FUnknown* context)
{
	const tresult result = AudioEffect::initialize (context);
	if (result != kResultOk)
		return result;

	addAudioInput (STR16 ("Stereo In"), SpeakerArr::kStereo);
	addAudioOutput (STR16 ("Stereo Out"), SpeakerArr::kStereo);

	// No event input. This is an EFFECT: nothing here reads a MIDI queue.
	// If you later port a DXi that registered as CSoftSynth, check first
	// whether its loop reads its INPUT BUFFER - if it does it is an effect
	// that was using the synth interface to reach the tempo map, and
	// porting the MIDI layer wastes days.

	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API VocalFilterProcessor::terminate ()
{
	return AudioEffect::terminate ();
}

//------------------------------------------------------------------------
tresult PLUGIN_API VocalFilterProcessor::setActive (TBool state)
{
	if (state)
	{
		pushParameters ();
		mDsp.reset ();
		mWasGliding = true;              // publish once on the first block
		sendSampleRateToController ();
	}
	return AudioEffect::setActive (state);
}

//------------------------------------------------------------------------
tresult PLUGIN_API VocalFilterProcessor::setupProcessing (ProcessSetup& setup)
{
	mSampleRate = setup.sampleRate;
	mDsp.setSampleRate (mSampleRate);

	const std::size_t maxFrames =
		static_cast<std::size_t> (std::max<int32> (setup.maxSamplesPerBlock, 1));
	for (int c = 0; c < 2; ++c)
	{
		mScratchIn[c].assign (maxFrames, 0.0f);
		mScratchOut[c].assign (maxFrames, 0.0f);
	}

	return AudioEffect::setupProcessing (setup);
}

//------------------------------------------------------------------------
tresult PLUGIN_API VocalFilterProcessor::canProcessSampleSize (int32 symbolicSampleSize)
{
	if (symbolicSampleSize == kSample32 || symbolicSampleSize == kSample64)
		return kResultTrue;
	return kResultFalse;
}

//------------------------------------------------------------------------
tresult PLUGIN_API VocalFilterProcessor::setBusArrangements (SpeakerArrangement* inputs, int32 numIns,
                                                             SpeakerArrangement* outputs, int32 numOuts)
{
	// Stereo in, stereo out, and nothing else. Whatever is accepted here
	// MUST match the SupportedNumChannels list in resource/au-info.plist
	// or hosts will not offer the layout and auval will say so.
	if (numIns == 1 && numOuts == 1 &&
	    inputs[0] == SpeakerArr::kStereo && outputs[0] == SpeakerArr::kStereo)
	{
		return AudioEffect::setBusArrangements (inputs, numIns, outputs, numOuts);
	}
	return kResultFalse;
}

//------------------------------------------------------------------------
void VocalFilterProcessor::applyParameterChanges (IParameterChanges* changes)
{
	if (changes == nullptr)
		return;

	const int32 count = changes->getParameterCount ();
	for (int32 i = 0; i < count; ++i)
	{
		IParamValueQueue* queue = changes->getParameterData (i);
		if (queue == nullptr)
			continue;

		const int32 points = queue->getPointCount ();
		if (points <= 0)
			continue;

		int32 sampleOffset = 0;
		ParamValue value = 0.0;
		if (queue->getPoint (points - 1, sampleOffset, value) != kResultTrue)
			continue;

		const ParamID id = queue->getParameterId ();

		// RANGE-CHECK before indexing: kBypass is 1000 and is not in the
		// table.
		if (id == kBypass)
			mBypass = (value >= 0.5);
		else if (id < kNumParams && !isLiveParam (id))
			mParams[id] = value;      // the published ones are ours to write
	}
}

//------------------------------------------------------------------------
void VocalFilterProcessor::pushParameters ()
{
	// The DSP is handed PLAIN units - hertz, hertz, decibels - not
	// normalised values, so nothing in VocalFilterDsp has to know what a
	// ParamID is or what range a host chose to present. The table is the
	// only place the two representations meet.
	// GLIDE FIRST. setFormant reads the glide length when it starts a
	// ramp, so a block that changes both the glide time and a vowel must
	// have the new length in hand before the vowel is pushed - otherwise
	// the first transition after moving the Glide slider uses the old
	// time and the control looks like it lags by one change.
	mDsp.setGlideMs (plainValue (mParams.data (), kGlide));

	// THE VOWEL SELECTOR IS A MODE. On a preset the DSP takes that vowel's
	// nine values and parameters 1..9 are ignored; on Manual it takes them.
	// Either way these are only TARGETS - setFormant starts a glide, so
	// switching vowels from the host slides exactly as pressing a button
	// does, and switching back to Manual slides to wherever the sliders
	// were left.
	const int selector = static_cast<int> (
		kParams[kVowel].toInternal (mParams[kVowel]) + 0.5);
	const int voice = static_cast<int> (
		kParams[kVoice].toInternal (mParams[kVoice]) + 0.5);
	const FormantSetting* preset = vowelSelection (selector, voice);

	for (int formant = 0; formant < kFormantCount; ++formant)
	{
		if (preset != nullptr)
		{
			mDsp.setFormant (formant, preset[formant].freqHz,
			                 preset[formant].bandwidthHz, preset[formant].levelDb);
		}
		else
		{
			mDsp.setFormant (formant,
			                 plainValue (mParams.data (), formantParam (formant, kFieldFreq)),
			                 plainValue (mParams.data (), formantParam (formant, kFieldBandwidth)),
			                 plainValue (mParams.data (), formantParam (formant, kFieldLevel)));
		}
	}

	mDsp.setMixPercent (plainValue (mParams.data (), kMix));
	mDsp.setTrimNormalized (mParams[kOutputTrim]);
}

//------------------------------------------------------------------------
void VocalFilterProcessor::sendSampleRateToController ()
{
	if (IPtr<IMessage> message = owned (allocateMessage ()))
	{
		message->setMessageID (kVocalFilterSampleRateMessage);
		message->getAttributes ()->setFloat (kVocalFilterSampleRateAttribute, mSampleRate);
		sendMessage (message);
	}
}

//------------------------------------------------------------------------
void VocalFilterProcessor::publishLiveValues (IParameterChanges* out)
{
	if (out == nullptr)
		return;

	for (int formant = 0; formant < kFormantCount; ++formant)
	{
		const double plain[3] = { mDsp.formantFreq (formant),
		                          mDsp.formantBandwidth (formant),
		                          // back to dB, and the floor is a real
		                          // silence rather than log(0).
		                          mDsp.formantGain (formant) > 0.0
		                              ? 20.0 * std::log10 (mDsp.formantGain (formant))
		                              : kLevelMinDb };
		const FormantField fields[3] = { kFieldFreq, kFieldBandwidth, kFieldLevel };

		for (int j = 0; j < 3; ++j)
		{
			const ParamID id = liveParam (formant, fields[j]);
			const double n = std::min (1.0, std::max (0.0,
				kParams[id].toNormalized (plain[j])));

			int32 index = 0;
			if (IParamValueQueue* queue = out->addParameterData (id, index))
			{
				int32 point = 0;
				queue->addPoint (0, n, point);
			}
		}
	}
}

//------------------------------------------------------------------------
tresult PLUGIN_API VocalFilterProcessor::process (ProcessData& data)
{
	applyParameterChanges (data.inputParameterChanges);
	pushParameters ();

	// WHERE THE FORMANTS ARE, for the response display. Published while a
	// glide is running and once more on the block it finishes, so the
	// curve follows the sound and then lands exactly - rather than every
	// block for ever, which would be a parameter change per formant per
	// block for a value that is not moving.
	const bool gliding = mDsp.gliding ();
	if (gliding || mWasGliding)
		publishLiveValues (data.outputParameterChanges);
	mWasGliding = gliding;

	// A parameter-only block. Legal, and common when a host is drawing
	// automation while the transport is stopped.
	if (data.numSamples <= 0 || data.numInputs <= 0 || data.numOutputs <= 0)
		return kResultOk;

	AudioBusBuffers& in  = data.inputs[0];
	AudioBusBuffers& out = data.outputs[0];
	const int32 frames = data.numSamples;

	if (in.numChannels < 2 || out.numChannels < 2)
		return kResultOk;

	// The host may hand us a bus it knows to be silent. Say the output is
	// silent only when it really is - the trim's smoother can still be
	// ramping down through a tail of samples, so clear the flag whenever
	// there is anything to process.
	out.silenceFlags = 0;

	if (data.symbolicSampleSize == kSample32)
	{
		float* inL  = in.channelBuffers32[0];
		float* inR  = in.channelBuffers32[1];
		float* outL = out.channelBuffers32[0];
		float* outR = out.channelBuffers32[1];

		if (mBypass)
		{
			if (outL != inL)
				std::memcpy (outL, inL, static_cast<std::size_t> (frames) * sizeof (float));
			if (outR != inR)
				std::memcpy (outR, inR, static_cast<std::size_t> (frames) * sizeof (float));
			return kResultOk;
		}

		mDsp.process (inL, inR, outL, outR, frames);
		return kResultOk;
	}

	if (data.symbolicSampleSize == kSample64)
	{
		double* inL  = in.channelBuffers64[0];
		double* inR  = in.channelBuffers64[1];
		double* outL = out.channelBuffers64[0];
		double* outR = out.channelBuffers64[1];

		if (mBypass)
		{
			if (outL != inL)
				std::memcpy (outL, inL, static_cast<std::size_t> (frames) * sizeof (double));
			if (outR != inR)
				std::memcpy (outR, inR, static_cast<std::size_t> (frames) * sizeof (double));
			return kResultOk;
		}

		// setupProcessing sized these to maxSamplesPerBlock; a host that
		// exceeds its own declared maximum gets a pass-through rather
		// than an allocation on the audio thread.
		const std::size_t need = static_cast<std::size_t> (frames);
		if (mScratchIn[0].size () < need || mScratchOut[0].size () < need)
			return kResultOk;

		for (int32 i = 0; i < frames; ++i)
		{
			mScratchIn[0][i] = static_cast<float> (inL[i]);
			mScratchIn[1][i] = static_cast<float> (inR[i]);
		}

		mDsp.process (mScratchIn[0].data (), mScratchIn[1].data (),
		              mScratchOut[0].data (), mScratchOut[1].data (), frames);

		for (int32 i = 0; i < frames; ++i)
		{
			outL[i] = mScratchOut[0][i];
			outR[i] = mScratchOut[1][i];
		}
		return kResultOk;
	}

	return kResultOk;
}

//------------------------------------------------------------------------
uint32 PLUGIN_API VocalFilterProcessor::getLatencySamples ()
{
	return static_cast<uint32> (mDsp.latencySamples ());
}

//------------------------------------------------------------------------
uint32 PLUGIN_API VocalFilterProcessor::getTailSamples ()
{
	const int tail = mDsp.tailSamples ();
	return (tail <= 0) ? kNoTail : static_cast<uint32> (tail);
}

//------------------------------------------------------------------------
tresult PLUGIN_API VocalFilterProcessor::setState (IBStream* state)
{
	if (state == nullptr)
		return kResultFalse;

	IBStreamer streamer (state, kLittleEndian);

	int32 version = 0;
	if (!streamer.readInt32 (version))
		return kResultFalse;
	if (version > kStateVersion)
		return kResultFalse;

	// Anything a SHORT stream does not mention goes back to its DEFAULT,
	// not to whatever the last project left in this instance. Without
	// this, loading an old project after a new one inherits the new one's
	// settings for every parameter added since.
	// Only the SETTINGS are stored - the published values are a view of
	// the DSP, not something to save and restore.
	for (ParamID id = 0; id < kNumStoredParams; ++id)
		mParams[id] = kParams[id].defaultNormalized ();
	mBypass = false;

	double value = 0.0;
	for (ParamID id = 0; id < kNumStoredParams; ++id)
	{
		if (!streamer.readDouble (value))
			break;
		mParams[id] = std::min (1.0, std::max (0.0, value));
	}

	int32 bypass = 0;
	if (streamer.readInt32 (bypass))
		mBypass = (bypass != 0);

	// kVoice is id 22, past the published values, so the loop above does not
	// reach it. Written last and read last, and ABSENT IN AN OLDER STREAM -
	// a project saved before the switch existed simply keeps the male voice
	// it was made with, which is the right answer and needs no version bump.
	mParams[kVoice] = kParams[kVoice].defaultNormalized ();
	int32 voice = 0;
	if (streamer.readInt32 (voice))
	{
		mParams[kVoice] = kParams[kVoice].toNormalized (
			std::min (static_cast<double> (kVoiceCount - 1),
			          std::max (0.0, static_cast<double> (voice))));
	}

	pushParameters ();
	mDsp.snapParameters ();   // a load is not a 10 ms ramp from the old value

	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API VocalFilterProcessor::getState (IBStream* state)
{
	if (state == nullptr)
		return kResultFalse;

	IBStreamer streamer (state, kLittleEndian);

	if (!streamer.writeInt32 (kStateVersion))
		return kResultFalse;

	for (ParamID id = 0; id < kNumStoredParams; ++id)
	{
		if (!streamer.writeDouble (mParams[id]))
			return kResultFalse;
	}

	if (!streamer.writeInt32 (mBypass ? 1 : 0))
		return kResultFalse;

	// Appended after the bypass, so a reader that stops early - one built
	// before the switch existed - still gets a complete, valid state.
	if (!streamer.writeInt32 (static_cast<int32> (
			kParams[kVoice].toInternal (mParams[kVoice]) + 0.5)))
		return kResultFalse;

	return kResultOk;
}

//------------------------------------------------------------------------
} // namespace VocalFilter
