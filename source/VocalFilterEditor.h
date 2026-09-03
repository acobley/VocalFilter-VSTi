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
	// The grid. Every position on the panel is one of these times a row
	// or a column index, so the layout has no magic numbers in it.
	//--------------------------------------------------------------------
	static constexpr int kMargin      = 14;
	static constexpr int kSliderWidth = 96;
	static constexpr int kSliderHeight= 27;
	static constexpr int kColumnGap   = 16;
	static constexpr int kRowGap      = 6;
	static constexpr int kHeaderHeight= 26;
	static constexpr int kSectionGap  = 18;

	static constexpr int kColumns = kFormantCount;   // F1 F2 F3
	static constexpr int kRows    = 3;               // Freq, Width, Level

	static constexpr int kEditorWidth =
		kMargin * 2 + kColumns * kSliderWidth + (kColumns - 1) * kColumnGap;
	static constexpr int kEditorHeight =
		kMargin + kHeaderHeight
		+ kRows * kSliderHeight + (kRows - 1) * kRowGap
		+ kSectionGap + kSliderHeight + kMargin + 16;

private:
	VSTGUI::CRect cell (int column, int row) const;

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
