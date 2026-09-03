//------------------------------------------------------------------------
// VocalFilter - edit controller
//
// The parameter list the host sees, and the state the editor reads.
//------------------------------------------------------------------------

#pragma once

#include "VocalFilterParams.h"

#include "public.sdk/source/vst/vsteditcontroller.h"

#include <vector>

namespace VocalFilter {

class VocalFilterEditor;

//------------------------------------------------------------------------
class VocalFilterController : public Steinberg::Vst::EditControllerEx1
{
public:
	VocalFilterController () = default;
	~VocalFilterController () SMTG_OVERRIDE = default;

	static Steinberg::FUnknown* createInstance (void*)
	{
		return (Steinberg::Vst::IEditController*)new VocalFilterController;
	}

	Steinberg::tresult PLUGIN_API initialize (Steinberg::FUnknown* context) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API terminate () SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API setComponentState (Steinberg::IBStream* state) SMTG_OVERRIDE;

	Steinberg::IPlugView* PLUGIN_API createView (Steinberg::FIDString name) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API setParamNormalized (
		Steinberg::Vst::ParamID tag, Steinberg::Vst::ParamValue value) SMTG_OVERRIDE;

	Steinberg::tresult PLUGIN_API notify (Steinberg::Vst::IMessage* message) SMTG_OVERRIDE;

	void editorAttached (Steinberg::Vst::EditorView* editor) SMTG_OVERRIDE;
	void editorRemoved (Steinberg::Vst::EditorView* editor) SMTG_OVERRIDE;
	void editorDestroyed (Steinberg::Vst::EditorView* editor) SMTG_OVERRIDE;

	//--------------------------------------------------------------------
	// What the response display needs and cannot work out for itself
	//--------------------------------------------------------------------

	/** The rate the DSP is actually running at, so a curve cannot disagree
	    with the filter it draws. 44100 until the processor says otherwise. */
	double dspSampleRate () const { return mSampleRate; }

	/** True once the processor's published values have actually arrived.
	    Until then - and for ever, in a host that does not forward
	    data.outputParameterChanges - the display draws the TARGETS
	    instead, which is the right filter arriving early rather than the
	    wrong one for ever. */
	bool hasLiveValues () const { return mHaveLiveValues; }

private:
	void addParameters ();

	/** Every open editor. A host may open more than one - two windows on
	    the same instance is legal - so this is a vector, not a pointer. */
	std::vector<VocalFilterEditor*> mEditors;

	double mSampleRate = 44100.0;
	bool   mHaveLiveValues = false;
};

//------------------------------------------------------------------------
} // namespace VocalFilter
