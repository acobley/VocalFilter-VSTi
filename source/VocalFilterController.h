//------------------------------------------------------------------------
// VocalFilter - edit controller
//
// The parameter list the host sees, and the state the editor would read.
// There is NO EDITOR yet: createView is not overridden, so the host draws
// its own generic parameter list. That is deliberate and is step 6 of the
// porting guide - get a silent plug-in validating first, with processor,
// controller and entry and nothing else, while there is little code to
// search. The editor is the LAST thing built.
//
// When you add one, the four things this class then needs are marked
// EDITOR HOOK below.
//------------------------------------------------------------------------

#pragma once

#include "VocalFilterParams.h"

#include "public.sdk/source/vst/vsteditcontroller.h"

namespace VocalFilter {

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

	//--------------------------------------------------------------------
	// EDITOR HOOK
	//
	// Add, in this order:
	//
	//   IPlugView* PLUGIN_API createView (FIDString name) override;
	//   tresult PLUGIN_API setParamNormalized (ParamID, ParamValue) override;
	//   void editorAttached (EditorView*) override;
	//   void editorRemoved (EditorView*) override;
	//   void editorDestroyed (EditorView*) override;
	//
	// and keep a std::vector<VocalFilterEditor*> of the open editors.
	//
	// THE TRAP in editorDestroyed: dynamic_cast returns NULL inside
	// ~EditorView(), which is one of its two callers. Compare UPCAST
	// pointers - static_cast<EditorView*> (e) == editor - or the list
	// keeps a dangling pointer that the next setParamNormalized follows.
	//--------------------------------------------------------------------

private:
	void addParameters ();
};

//------------------------------------------------------------------------
} // namespace VocalFilter
