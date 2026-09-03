//------------------------------------------------------------------------
// VocalFilter - audio processor
//
// Host plumbing only. The audio line itself is in VocalFilterDsp.h and
// knows nothing about VST3.
//------------------------------------------------------------------------

#pragma once

#include "VocalFilterDsp.h"
#include "VocalFilterParams.h"

#include "public.sdk/source/vst/vstaudioeffect.h"

#include <vector>

namespace VocalFilter {

//------------------------------------------------------------------------
class VocalFilterProcessor : public Steinberg::Vst::AudioEffect
{
public:
	VocalFilterProcessor ();
	~VocalFilterProcessor () SMTG_OVERRIDE = default;

	static Steinberg::FUnknown* createInstance (void*)
	{
		return (Steinberg::Vst::IAudioProcessor*)new VocalFilterProcessor;
	}

	Steinberg::tresult PLUGIN_API initialize (Steinberg::FUnknown* context) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API terminate () SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API setActive (Steinberg::TBool state) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API setupProcessing (Steinberg::Vst::ProcessSetup& setup) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API canProcessSampleSize (Steinberg::int32 symbolicSampleSize) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API setBusArrangements (Steinberg::Vst::SpeakerArrangement* inputs,
	                                                  Steinberg::int32 numIns,
	                                                  Steinberg::Vst::SpeakerArrangement* outputs,
	                                                  Steinberg::int32 numOuts) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API process (Steinberg::Vst::ProcessData& data) SMTG_OVERRIDE;

	Steinberg::tresult PLUGIN_API setState (Steinberg::IBStream* state) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API getState (Steinberg::IBStream* state) SMTG_OVERRIDE;

	Steinberg::uint32 PLUGIN_API getLatencySamples () SMTG_OVERRIDE;
	Steinberg::uint32 PLUGIN_API getTailSamples () SMTG_OVERRIDE;

private:
	/** Takes the LAST point of each parameter's change list, which is the
	    block-rate read the DXi did. Sample-accurate automation would walk
	    the queue here instead. */
	void applyParameterChanges (Steinberg::Vst::IParameterChanges* changes);

	/** Push mParams into the DSP. */
	void pushParameters ();

	Dsp mDsp;

	/** Normalised parameter values, indexed by Param. */
	std::vector<double> mParams;
	bool mBypass = false;

	double mSampleRate = 44100.0;

	/** 64-bit processing goes through these. The line is float, as the
	    original plug-ins were, but a 64-bit host is accepted rather than
	    refused. Sized in setupProcessing so the audio thread never
	    allocates. */
	std::vector<float> mScratchIn[2];
	std::vector<float> mScratchOut[2];
};

//------------------------------------------------------------------------
} // namespace VocalFilter
