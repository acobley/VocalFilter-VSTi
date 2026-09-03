//------------------------------------------------------------------------
// VocalFilter - editor
//
// There is no .rc behind this panel and no artwork to recover, so unlike
// the SpyBand and ForTran editors nothing here is converted from dialog
// units: the layout is in pixels, computed from one grid, and the grid is
// the only thing to change if the panel is resized.
//
// The panel is a column per formant and a row per field, which is how the
// numbers are read - F1 F2 F3 across, Freq / Width / Level down - plus a
// bottom row for the two global controls. A vowel is a shape across that
// grid, so putting the three formants side by side is what makes one
// legible at a glance.
//------------------------------------------------------------------------

#pragma once

#include "VocalFilterControls.h"
#include "VocalFilterDisplay.h"
#include "VocalFilterParams.h"

#include "public.sdk/source/vst/vstguieditor.h"

#include <map>

namespace VocalFilter {

class VocalFilterController;

//------------------------------------------------------------------------
class VocalFilterEditor : public Steinberg::Vst::VSTGUIEditor,
                          public VSTGUI::IControlListener
{
public:
	explicit VocalFilterEditor (VocalFilterController* controller);

	bool PLUGIN_API open (void* parent, const VSTGUI::PlatformType& platformType) SMTG_OVERRIDE;
	void PLUGIN_API close () SMTG_OVERRIDE;

	// IControlListener
	void valueChanged (VSTGUI::CControl* control) SMTG_OVERRIDE;
	void controlBeginEdit (VSTGUI::CControl* control) SMTG_OVERRIDE;
	void controlEndEdit (VSTGUI::CControl* control) SMTG_OVERRIDE;

	/** The controller's setParamNormalized reaches the panel through
	    here. */
	void updateControl (Steinberg::Vst::ParamID tag, Steinberg::Vst::ParamValue normalized);

	//--------------------------------------------------------------------
	// The layout. Every position on the panel is derived from these, so
	// there are no magic numbers below and resizing means editing here.
	//
	// Reading order down the panel is: what the panel is, then PICK A
	// VOWEL, then adjust it, then the two controls that are not part of a
	// vowel. The buttons go above the grid because that is the order the
	// panel is used in.
	//--------------------------------------------------------------------
	static constexpr int kMargin       = 14;
	static constexpr int kSliderWidth  = 96;
	static constexpr int kSliderHeight = 27;
	static constexpr int kColumnGap    = 16;
	static constexpr int kRowGap       = 6;

	static constexpr int kColumns = kFormantCount;   // F1 F2 F3
	static constexpr int kRows    = 3;               // Freq, Width, Level

	static constexpr int kContentWidth =
		kColumns * kSliderWidth + (kColumns - 1) * kColumnGap;

	static constexpr int kTitleTop    = 4;
	static constexpr int kTitleHeight = 15;

	static constexpr int kVowelTop    = kTitleTop + kTitleHeight + 7;
	static constexpr int kVowelHeight = 23;
	static constexpr int kVowelGap    = 8;

	static constexpr int kHeadingTop    = kVowelTop + kVowelHeight + 11;
	static constexpr int kHeadingHeight = 14;

	static constexpr int kGridTop    = kHeadingTop + kHeadingHeight + 3;
	static constexpr int kSectionGap = 18;

	static constexpr int kBottomRowTop =
		kGridTop + kRows * (kSliderHeight + kRowGap) - kRowGap + kSectionGap;

	/** The response display, to the RIGHT of the controls. The panel grew
	    rightwards to make room rather than the controls being squeezed:
	    every slider position below is unchanged by its arrival. */
	static constexpr int kDisplayGap   = 16;
	static constexpr int kDisplayWidth = 300;
	static constexpr int kDisplayLeft  = kMargin + kContentWidth + kDisplayGap;
	static constexpr int kDisplayTop    = kVowelTop;
	static constexpr int kDisplayBottom = kBottomRowTop + kSliderHeight;

	static constexpr int kEditorWidth  =
		kDisplayLeft + kDisplayWidth + kMargin;
	static constexpr int kEditorHeight = kBottomRowTop + kSliderHeight + kMargin;

	/** How often the display asks the controller where the DSP is. 30 ms
	    is about 33 fps - fast enough that a 150 ms glide is a movement
	    rather than three steps, and slow enough to cost nothing. */
	static constexpr int kTimerMs = 30;

private:
	VSTGUI::CRect cell (int column, int row) const;

	/** The five vowel buttons share the grid's width, so a row of five
	    lines up with a row of three without either being told about the
	    other. */
	VSTGUI::CRect vowelCell (int index) const;

	/** Write one parameter as a complete edit gesture - begin, set,
	    perform, end - so the host records it, undo works, and every open
	    editor's slider follows. */
	void setParameter (Steinberg::Vst::ParamID tag, double plainValue);

	/** Point the Vowel parameter at one preset. ONE parameter write, not
	    nine: the processor is what reads the selector and feeds the DSP,
	    so the panel and a host automation lane do the identical thing. */
	void selectVowel (int selector);

	/** Leave preset mode without the sound jumping: copy the live preset's
	    nine values into parameters 1..9 FIRST, then switch the selector to
	    Manual. Called when a formant slider is touched while a preset is
	    selected. */
	void captureAndGoManual ();

	/** The selector's current position, 0 = Manual. */
	int currentVowel () const;

	/** Light the right button, and put the values the DSP is actually
	    using on the nine sliders - which on a preset are the preset's, not
	    the parameters'. Display only; nothing is written. */
	void refreshVowelState ();

	/** The timer's work: hand the display where the formants are now. */
	void refreshDisplay ();

	/** One parameter's value in its own plain unit. */
	double plainOf (Steinberg::Vst::ParamID tag) const;

	/** A slider bound to a parameter, labelled, and formatting its own
	    readout from the SAME table the host reads. */
	SpySlider* addSlider (Steinberg::Vst::ParamID tag, const char* label,
	                      const VSTGUI::CRect& rect);

	VSTGUI::CTextLabel* addHeading (const char* text, const VSTGUI::CRect& rect);

	VocalFilterController* mController = nullptr;

	std::map<Steinberg::Vst::ParamID, VSTGUI::CControl*> mControls;
	SpyPresetButton* mVowelButtons[kVowelCount] = { nullptr };
	SpyResponseDisplay* mDisplay = nullptr;

	VSTGUI::SharedPointer<VSTGUI::CVSTGUITimer> mTimer;
};

//------------------------------------------------------------------------
} // namespace VocalFilter
