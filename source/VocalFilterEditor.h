//------------------------------------------------------------------------
// VocalFilter - editor
//
// THE "VOWEL SPACE" PANEL (branch Design2), from
// design/VocalFilter-Concept3b-VowelSpace.png: R2Synth's panel idiom with
// a vowel chart you can play. Every position comes from
// VocalFilterLayout.h; this class only builds the panel from those tables
// and keeps it in step with the parameters.
//
// WHAT DID NOT CHANGE from the SpyBand-slider panel, because it is about
// parameters rather than pixels:
//
//   * a vowel button writes ONE parameter, the Vowel selector, so the
//     panel and a host automating Vowel take the identical path;
//   * touching any formant control while a preset is live first copies
//     the preset into the nine parameters, then goes Manual - so the sound
//     does not jump (captureAndGoManual);
//   * while a preset is live the formant knobs SHOW THE PRESET, display
//     only, because that is what the DSP is using;
//   * the spectrum and the chart follow the processor's PUBLISHED values
//     on a timer, falling back to the targets if a host never forwards
//     them;
//   * showValue is the only place setValueNormalized is called
//     (tools/check-editor.py).
//
// NEW: the vowel chart writes F1 and F2 together as one gesture, and the
// editor tracks where each glide started so the chart can draw its path
// and the meter its progress. That tracking is display only - the DSP's
// ramp is the authority, and the editor only watches it arrive.
//------------------------------------------------------------------------

#pragma once

#include "VocalFilterLayout.h"
#include "VocalFilterPanelViews.h"
#include "VocalFilterParams.h"

#include "public.sdk/source/vst/vstguieditor.h"

#include <map>
#include <string>

namespace VocalFilter {

class VocalFilterController;

//------------------------------------------------------------------------
class VocalFilterEditor : public Steinberg::Vst::VSTGUIEditor,
                          public VSTGUI::IControlListener,
                          public Panel::IVowelPadListener
{
public:
	explicit VocalFilterEditor (VocalFilterController* controller);

	bool PLUGIN_API open (void* parent, const VSTGUI::PlatformType& platformType) SMTG_OVERRIDE;
	void PLUGIN_API close () SMTG_OVERRIDE;

	// IControlListener
	void valueChanged (VSTGUI::CControl* control) SMTG_OVERRIDE;
	void controlBeginEdit (VSTGUI::CControl* control) SMTG_OVERRIDE;
	void controlEndEdit (VSTGUI::CControl* control) SMTG_OVERRIDE;

	// IVowelPadListener
	void padDragBegin () override;
	void padDragMove (double f1Hz, double f2Hz) override;
	void padDragEnd () override;
	void padVowelClicked (int selector) override;

	/** The controller's setParamNormalized reaches the panel through
	    here. */
	void updateControl (Steinberg::Vst::ParamID tag, Steinberg::Vst::ParamValue normalized);

	static constexpr int kEditorWidth  = Layout::kEditorWidth;
	static constexpr int kEditorHeight = Layout::kEditorHeight;

	/** How often the displays ask the controller where the DSP is. 30 ms
	    is about 33 fps - a 150 ms glide is a movement, not three steps. */
	static constexpr int kTimerMs = 30;

private:
	/** Write one parameter as a complete edit gesture - begin, set,
	    perform, end - so the host records it and undo works. */
	void setParameter (Steinberg::Vst::ParamID tag, double plainValue);

	/** Point the Vowel parameter at one preset. ONE parameter write. */
	void selectVowel (int selector);

	/** Leave preset mode without the sound jumping: the live preset's nine
	    values into parameters 1..9 FIRST, then the selector to Manual. */
	void captureAndGoManual ();

	int currentVowel () const;
	int currentVoice () const;

	/** Light the right vowel button and Manual, and put the values the
	    DSP is actually using on the nine formant knobs. Display only. */
	void refreshVowelState ();

	/** The timer's work: where the formants are now, to the spectrum, the
	    chart, the glide meter and the engine line. */
	void refreshDisplay ();

	/** One parameter's value in its own plain unit. */
	double plainOf (Steinberg::Vst::ParamID tag) const;

	/** Set what a control SHOWS, and mark it for redraw.
	 *
	 *  THE ONLY PLACE THIS EDITOR CALLS setValueNormalized.
	 *  CControl::setValue assigns the value and nothing else - it does not
	 *  mark the view dirty - so a value set without the invalidate repaints
	 *  only when something else happens to dirty the same region. That is
	 *  how the Voice switch once presented: right under the mouse,
	 *  intermittent under automation. tools/check-editor.py fails if
	 *  another call site appears. */
	void showValue (VSTGUI::CControl* control, double normalized);

	/** Build one table entry: a knob or a switch, bound to its parameter. */
	void addControl (const Layout::Control& c);

	/** A line of text that changes - the engine line. */
	VSTGUI::CTextLabel* addTextLine (const Layout::Rect& r, VSTGUI::CFontRef font,
	                                 const VSTGUI::CColor& colour);

	VocalFilterController* mController = nullptr;

	std::map<Steinberg::Vst::ParamID, VSTGUI::CControl*> mControls;
	Panel::VowelButton* mVowelButtons[kVowelCount] = { nullptr };
	Panel::ManualButton* mManualButton = nullptr;
	Panel::VowelPad* mPad = nullptr;
	Panel::Spectrum* mSpectrum = nullptr;
	Panel::GlideMeter* mGlideMeter = nullptr;
	VSTGUI::CTextLabel* mEngineLine = nullptr;

	VSTGUI::SharedPointer<VSTGUI::CVSTGUITimer> mTimer;

	//--------------------------------------------------------------------
	// Glide tracking - display only
	//--------------------------------------------------------------------
	/** Where the DSP was told to go, as of the last tick. A change here is
	    the start of a glide. */
	FormantSetting mTarget[kFormantCount] = {};
	bool mHaveTarget = false;

	/** What the display showed on the last tick - which, when the target
	    moves, is where the new glide starts from. */
	FormantSetting mShown[kFormantCount] = {};

	FormantSetting mGlideFrom[kFormantCount] = {};
	bool mGliding = false;

	/** The chart's two edit gestures are open. */
	bool mPadEditing = false;
};

//------------------------------------------------------------------------
} // namespace VocalFilter
