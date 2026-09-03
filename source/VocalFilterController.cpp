//------------------------------------------------------------------------
// VocalFilter - edit controller implementation
//------------------------------------------------------------------------

#include "VocalFilterController.h"
#include "VocalFilterEditor.h"
#include "VocalFilterIDs.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include "public.sdk/source/vst/vstparameters.h"

#include <algorithm>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace VocalFilter {

//------------------------------------------------------------------------
tresult PLUGIN_API VocalFilterController::initialize (FUnknown* context)
{
	const tresult result = EditControllerEx1::initialize (context);
	if (result != kResultOk)
		return result;

	addParameters ();
	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API VocalFilterController::terminate ()
{
	return EditControllerEx1::terminate ();
}

//------------------------------------------------------------------------
void VocalFilterController::addParameters ()
{
	for (ParamID id = 0; id < kNumParams; ++id)
	{
		const ParamDef& def = kParams[id];

		// NEVER pass a null title or units to RangeParameter: it
		// dereferences both without a null check, and the symptom is the
		// VALIDATOR segfaulting in the post-build step rather than
		// anything pointing at this line. A parameter whose value string
		// carries its own unit wants an EMPTY units field, not a null
		// one, or the host renders "200 ms %".
		String128 title;
		String128 units;
		UString (title, str16BufferSize (String128)).assign (def.title ? def.title : "Parameter");
		UString (units, str16BufferSize (String128)).assign (def.units ? def.units : "");

		// The published values are READ-ONLY and HIDDEN: no host lists
		// them, nothing outside the plug-in can write them, and they
		// exist only as the route data.outputParameterChanges travels on.
		// kIsHidden implies kIsReadOnly and the absence of kCanAutomate,
		// but saying both is clearer than relying on that.
		int32 flags = isLiveParam (id)
			? (ParameterInfo::kIsReadOnly | ParameterInfo::kIsHidden)
			: ParameterInfo::kCanAutomate;

		// An enumerated parameter is a StringListParameter, not a
		// RangeParameter with a step count: the host shows the names, and
		// getParamStringByValue / getParamValueByString round-trip through
		// them exactly rather than through a number that has to be
		// re-derived.
		if (def.type == ParamType::Enum)
		{
			auto* list = new StringListParameter (title, id, units, flags);
			for (int choice = 0; choice <= static_cast<int> (def.plainMax); ++choice)
			{
				String128 name;
				UString (name, str16BufferSize (String128))
					.assign (vowelSelectionName (choice));
				list->appendString (name);
			}
			parameters.addParameter (list);
			list->setNormalized (def.defaultNormalized ());
			continue;
		}

		RangeParameter* parameter = new RangeParameter (
			title, id, units,
			def.plainMin, def.plainMax, def.plainDefault,
			def.stepCount, flags);

		// The validator round-trips every parameter through
		// getParamStringByValue / getParamValueByString AT ITS CURRENT
		// VALUE and warns above 1e-4. A linear dB range round-trips
		// exactly; a NON-LINEAR display mapping needs toString AND
		// fromString overridden, and must be exact at the default even if
		// it rounds elsewhere - the base RangeParameter reads a leading
		// number as a plain value, so a typed "200 ms" becomes 200 and
		// clamps to full travel.
		parameters.addParameter (parameter);
	}

	// The host's own bypass. Its id is 1000, far past the end of the
	// table, which is why everything that indexes kParams range-checks
	// first.
	{
		String128 title;
		String128 units;
		UString (title, str16BufferSize (String128)).assign ("Bypass");
		UString (units, str16BufferSize (String128)).assign ("");

		parameters.addParameter (new StringListParameter (
			title, kBypass, units,
			ParameterInfo::kCanAutomate | ParameterInfo::kIsBypass));

		if (auto* bypass = static_cast<StringListParameter*> (parameters.getParameter (kBypass)))
		{
			bypass->appendString (STR16 ("Off"));
			bypass->appendString (STR16 ("On"));
			bypass->setNormalized (0.0);
		}
	}
}

//------------------------------------------------------------------------
tresult PLUGIN_API VocalFilterController::setComponentState (IBStream* state)
{
	// The SAME layout VocalFilterProcessor::getState writes. If one side
	// changes, both change - and a short stream resets everything it does
	// not mention back to its default, exactly as the processor does.
	if (state == nullptr)
		return kResultFalse;

	IBStreamer streamer (state, kLittleEndian);

	int32 version = 0;
	if (!streamer.readInt32 (version))
		return kResultFalse;

	// Only the SETTINGS are in the stream - the published values are a
	// view of the DSP, and the processor does not write them either.
	for (ParamID id = 0; id < kNumStoredParams; ++id)
		setParamNormalized (id, kParams[id].defaultNormalized ());
	setParamNormalized (kBypass, 0.0);

	double value = 0.0;
	for (ParamID id = 0; id < kNumStoredParams; ++id)
	{
		if (!streamer.readDouble (value))
			break;
		setParamNormalized (id, std::min (1.0, std::max (0.0, value)));
	}

	int32 bypass = 0;
	if (streamer.readInt32 (bypass))
		setParamNormalized (kBypass, bypass ? 1.0 : 0.0);

	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API VocalFilterController::notify (IMessage* message)
{
	if (message == nullptr)
		return kInvalidArgument;

	if (FIDStringsEqual (message->getMessageID (), kVocalFilterSampleRateMessage))
	{
		double rate = 0.0;
		if (message->getAttributes ()->getFloat (kVocalFilterSampleRateAttribute, rate)
		        == kResultOk && rate > 0.0)
		{
			mSampleRate = rate;
		}
		return kResultOk;
	}

	return EditControllerEx1::notify (message);
}

//------------------------------------------------------------------------
IPlugView* PLUGIN_API VocalFilterController::createView (FIDString name)
{
	if (name && FIDStringsEqual (name, ViewType::kEditor))
		return new VocalFilterEditor (this);
	return nullptr;
}

//------------------------------------------------------------------------
tresult PLUGIN_API VocalFilterController::setParamNormalized (ParamID tag, ParamValue value)
{
	const tresult result = EditControllerEx1::setParamNormalized (tag, value);
	if (result != kResultOk)
		return result;

	// A published value arriving is the proof that this host forwards
	// data.outputParameterChanges at all. Until one does, the display
	// draws the targets.
	if (isLiveParam (tag))
		mHaveLiveValues = true;

	// The host, an automation lane and the panel all arrive here, so this
	// is the one place a control's position is kept in step with the
	// parameter behind it.
	for (auto* editor : mEditors)
		editor->updateControl (tag, value);

	return result;
}

//------------------------------------------------------------------------
void VocalFilterController::editorAttached (EditorView* editor)
{
	if (auto* e = dynamic_cast<VocalFilterEditor*> (editor))
		if (std::find (mEditors.begin (), mEditors.end (), e) == mEditors.end ())
			mEditors.push_back (e);
}

//------------------------------------------------------------------------
void VocalFilterController::editorRemoved (EditorView* editor)
{
	editorDestroyed (editor);
}

//------------------------------------------------------------------------
void VocalFilterController::editorDestroyed (EditorView* editor)
{
	// Do NOT dynamic_cast here. EditorView::~EditorView() is one of the two
	// callers, and by then the VocalFilterEditor sub-object is gone, so the
	// cast yields null, the entry survives as a DANGLING POINTER, and the
	// next setParamNormalized above walks it. Comparing upcast pointers is
	// well defined at every point in the destruction sequence.
	mEditors.erase (std::remove_if (mEditors.begin (), mEditors.end (),
	                                [editor] (VocalFilterEditor* e) {
		                                return static_cast<EditorView*> (e) == editor;
	                                }),
	                mEditors.end ());
}

//------------------------------------------------------------------------
} // namespace VocalFilter
