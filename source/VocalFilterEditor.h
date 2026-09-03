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

	static constexpr int kEditorWidth  = kMargin * 2 + kContentWidth;
	static constexpr int kEditorHeight = kBottomRowTop + kSliderHeight + kMargin;

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

	/** Recall one vowel: the nine formant parameters, and nothing else.
	    Dry/Wet and Output Trim are the user's, not the vowel's. */
	void applyVowel (int index);

	/** A slider bound to a parameter, labelled, and formatting its own
	    readout from the SAME table the host reads. */
	SpySlider* addSlider (Steinberg::Vst::ParamID tag, const char* label,
	                      const VSTGUI::CRect& rect);

	VSTGUI::CTextLabel* addHeading (const char* text, const VSTGUI::CRect& rect);

	VocalFilterController* mController = nullptr;

	std::map<Steinberg::Vst::ParamID, VSTGUI::CControl*> mControls;
};

//------------------------------------------------------------------------
} // namespace VocalFilter
