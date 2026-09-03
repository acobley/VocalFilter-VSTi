//------------------------------------------------------------------------
// VocalFilter - editor implementation
//------------------------------------------------------------------------

#include "VocalFilterEditor.h"
#include "VocalFilterController.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace VSTGUI;

namespace VocalFilter {

namespace {

/** The panel behind the controls. SpyBand's dialog background was a
    bitmap; this one is a colour, because there is no artwork to recover
    and inventing some would only be something else to keep in step with
    the layout. The value is SpyBand's own fallback - the colour its
    editor paints under the bitmap so a missing file reads as a dark panel
    rather than as whatever the host left in the window. */
const CColor kPanel (64, 64, 64, 255);

/** The three formant columns are labelled by what they are, not by their
    index: someone reading the panel wants "F2 - 1090 Hz" to mean the
    second formant of a vowel. */
const char* const kColumnNames[kFormantCount] = { "F1", "F2", "F3" };

} // namespace

//------------------------------------------------------------------------
VocalFilterEditor::VocalFilterEditor (VocalFilterController* controller)
: VSTGUIEditor (controller)
, mController (controller)
{
	ViewRect rect (0, 0, kEditorWidth, kEditorHeight);
	setRect (rect);
}

//------------------------------------------------------------------------
CRect VocalFilterEditor::cell (int column, int row) const
{
	const CCoord x = kMargin + column * (kSliderWidth + kColumnGap);
	const CCoord y = kGridTop + row * (kSliderHeight + kRowGap);
	return CRect (x, y, x + kSliderWidth, y + kSliderHeight);
}

//------------------------------------------------------------------------
CRect VocalFilterEditor::vowelCell (int index) const
{
	// Five buttons across exactly the width three sliders occupy, so the
	// row lines up with the grid at both ends whatever the slider width
	// is. Kept in floating point and rounded only at the edges, or the
	// accumulated truncation leaves the last button short.
	const double width = (kContentWidth - (kVowelCount - 1) * kVowelGap)
	                   / static_cast<double> (kVowelCount);
	const double left  = kMargin + index * (width + kVowelGap);
	return CRect (std::round (left), kVowelTop,
	              std::round (left + width), kVowelTop + kVowelHeight);
}

//------------------------------------------------------------------------
void VocalFilterEditor::setParameter (ParamID tag, double plain)
{
	if (mController == nullptr)
		return;

	const ParamDef& def = paramDef (tag);
	const double normalized = std::min (1.0, std::max (0.0, def.toNormalized (plain)));

	// A complete gesture. beginEdit / endEdit around it is what makes a
	// host treat nine writes as something it can record and undo, rather
	// than as nine unexplained jumps; setParamNormalized is what moves
	// this panel's own slider, because it comes back through
	// updateControl.
	mController->beginEdit (tag);
	mController->setParamNormalized (tag, normalized);
	mController->performEdit (tag, normalized);
	mController->endEdit (tag);
}

//------------------------------------------------------------------------
int VocalFilterEditor::currentVowel () const
{
	if (mController == nullptr)
		return kVowelManual;
	return static_cast<int> (
		kParams[kVowel].toInternal (mController->getParamNormalized (kVowel)) + 0.5);
}

//------------------------------------------------------------------------
void VocalFilterEditor::selectVowel (int selector)
{
	setParameter (kVowel, static_cast<double> (selector));
}

//------------------------------------------------------------------------
void VocalFilterEditor::captureAndGoManual ()
{
	const FormantSetting* preset = vowelSelection (currentVowel ());
	if (preset == nullptr)
		return;                        // already Manual

	// The nine FIRST, the mode second. Not for the DSP's sake - each
	// parameter has its own queue, so within a block the order does not
	// reach it - but so that a host recording this gesture records the
	// values arriving before the mode that makes them matter.
	for (int k = 0; k < kFormantCount; ++k)
	{
		setParameter (formantParam (k, kFieldFreq),      preset[k].freqHz);
		setParameter (formantParam (k, kFieldBandwidth), preset[k].bandwidthHz);
		setParameter (formantParam (k, kFieldLevel),     preset[k].levelDb);
	}

	setParameter (kVowel, static_cast<double> (kVowelManual));
}

//------------------------------------------------------------------------
void VocalFilterEditor::refreshVowelState ()
{
	if (frame == nullptr || mController == nullptr)
		return;

	const int selector = currentVowel ();
	const FormantSetting* preset = vowelSelection (selector);

	for (int v = 0; v < kVowelCount; ++v)
		if (mVowelButtons[v])
			mVowelButtons[v]->setSelected (v + 1 == selector);

	// The nine sliders show WHAT THE DSP IS USING, which on a preset is
	// the preset and not the parameters. Display only - setValueNormalized
	// without performEdit - because the parameters genuinely still hold
	// the manual values, and pretending otherwise is what captureAndGoManual
	// is for.
	for (int k = 0; k < kFormantCount; ++k)
	{
		const double plain[3] = {
			preset ? preset[k].freqHz      : 0.0,
			preset ? preset[k].bandwidthHz : 0.0,
			preset ? preset[k].levelDb     : 0.0 };
		const FormantField fields[3] = { kFieldFreq, kFieldBandwidth, kFieldLevel };

		for (int j = 0; j < 3; ++j)
		{
			const ParamID tag = formantParam (k, fields[j]);
			auto it = mControls.find (tag);
			if (it == mControls.end () || it->second == nullptr)
				continue;

			const double shown = preset ? paramDef (tag).toNormalized (plain[j])
			                            : mController->getParamNormalized (tag);
			it->second->setValueNormalized (static_cast<float> (shown));
			it->second->invalid ();
		}
	}
}

//------------------------------------------------------------------------
SpySlider* VocalFilterEditor::addSlider (ParamID tag, const char* label, const CRect& rect)
{
	auto* control = new SpySlider (rect, this, static_cast<int32_t> (tag));
	control->setLabel (label);

	// The number across the middle is the parameter's own PLAIN value,
	// read out of the same table the host formats from - so the panel and
	// the host cannot disagree about what a control says. Hertz get no
	// decimal place; decibels and per cent get one, because a dB is worth
	// resolving and 100 % should not read as 99 %.
	const ParamDef& def = paramDef (tag);
	const bool integral = (def.units != nullptr && def.units[0] == 'H');
	control->setFormatter ([def, integral] (float normalized)
	{
		char buffer[32];
		std::snprintf (buffer, sizeof (buffer), integral ? "%.0f %s" : "%.1f %s",
		               def.toPlain (normalized), def.units ? def.units : "");
		return std::string (buffer);
	});

	mControls[tag] = control;
	if (mController)
		control->setValueNormalized (
			static_cast<float> (mController->getParamNormalized (tag)));

	// Z-order is the order views are added, and every control here is a
	// direct child of the frame - so a control's getViewSize() is already
	// in frame coordinates and nothing needs a parent chain walked.
	frame->addView (control);
	return control;
}

//------------------------------------------------------------------------
CTextLabel* VocalFilterEditor::addHeading (const char* text, const CRect& rect)
{
	auto* label = new CTextLabel (rect);
	label->setFont (panelFont ());
	label->setFontColor (Colours::kLabel);
	label->setBackColor (kTransparentCColor);
	label->setFrameColor (kTransparentCColor);
	label->setStyle (CParamDisplay::kNoFrame);
	label->setText (text);
	frame->addView (label);
	return label;
}

//------------------------------------------------------------------------
bool PLUGIN_API VocalFilterEditor::open (void* parent, const PlatformType& platformType)
{
	if (frame != nullptr)
		return false;

	const CRect frameSize (0, 0, kEditorWidth, kEditorHeight);
	frame = new CFrame (frameSize, this);
	frame->setBackgroundColor (kPanel);

	//--------------------------------------------------------------------
	// The title.
	//--------------------------------------------------------------------
	addHeading ("Vocal Tract  -  three parallel formants",
	            CRect (kMargin, kTitleTop, kEditorWidth - kMargin,
	                   kTitleTop + kTitleHeight));

	//--------------------------------------------------------------------
	// The five vowel buttons. Each one writes the VOWEL SELECTOR - one
	// parameter, not nine - so a button press and a host automating Vowel
	// travel the identical path through the processor.
	//
	// They are also indicators: the selector can move with nobody
	// touching the panel, so refreshVowelState lights whichever is live.
	//
	// The handler captures `this` and an INDEX, not a pointer into
	// kVowels: the button outlives nothing here, but an index cannot be
	// left dangling by a later refactor that makes the table dynamic.
	//--------------------------------------------------------------------
	for (int v = 0; v < kVowelCount; ++v)
	{
		auto* button = new SpyPresetButton (vowelCell (v), kVowels[v].name);
		// v + 1, because 0 on the selector is Manual.
		button->setHandler ([this, v] { selectVowel (v + 1); });
		mVowelButtons[v] = button;
		frame->addView (button);
	}

	//--------------------------------------------------------------------
	// The three column headings above the grid.
	//--------------------------------------------------------------------
	for (int column = 0; column < kColumns; ++column)
	{
		const CRect head = cell (column, 0);
		addHeading (kColumnNames[column],
		            CRect (head.left, kHeadingTop, head.right,
		                   kHeadingTop + kHeadingHeight));
	}

	//--------------------------------------------------------------------
	// The grid: a column per formant, a row per field. The label under
	// each slider names the field rather than repeating the formant,
	// because the heading above the column has already said which one it
	// is - and on a 96-pixel control the room saved is the difference
	// between the readout fitting and not.
	//--------------------------------------------------------------------
	static const FormantField kFields[kRows] =
		{ kFieldFreq, kFieldBandwidth, kFieldLevel };
	static const char* const kFieldLabels[kRows] =
		{ "Freq", "Width", "Level" };

	for (int column = 0; column < kColumns; ++column)
	{
		for (int row = 0; row < kRows; ++row)
		{
			addSlider (formantParam (column, kFields[row]),
			           kFieldLabels[row],
			           cell (column, row));
		}
	}

	//--------------------------------------------------------------------
	// The bottom row: the three controls that are not part of a formant.
	// Dry / Wet sits under F1 because it is the one most likely to be
	// touched while listening; Glide sits in the middle, under the column
	// whose F2 moves furthest between vowels and so shows the glide most;
	// the trim sits under F3, at the end of the signal path, which is
	// where it is in the code.
	//--------------------------------------------------------------------
	for (int column = 0; column < kColumns; ++column)
	{
		static const ParamID kBottom[kColumns] = { kMix, kGlide, kOutputTrim };
		static const char* const kBottomLabels[kColumns] =
			{ "Dry / Wet", "Glide", "Output Trim" };

		CRect r = cell (column, 0);
		r.offset (0, kBottomRowTop - r.top);
		addSlider (kBottom[column], kBottomLabels[column], r);
	}

	refreshVowelState ();

	frame->open (parent, platformType);
	return true;
}

//------------------------------------------------------------------------
void PLUGIN_API VocalFilterEditor::close ()
{
	mControls.clear ();
	for (auto*& button : mVowelButtons)
		button = nullptr;

	if (frame)
	{
		frame->forget ();
		frame = nullptr;
	}
}

//------------------------------------------------------------------------
void VocalFilterEditor::valueChanged (CControl* control)
{
	if (mController == nullptr || control == nullptr)
		return;

	const ParamID tag = static_cast<ParamID> (control->getTag ());
	const ParamValue value = control->getValueNormalized ();

	// TOUCHING A FORMANT SLIDER LEAVES PRESET MODE. The alternative is a
	// slider that visibly moves and changes nothing, because the processor
	// is reading the preset and ignoring parameters 1..9 - which is the
	// worst thing a control can do. Capturing first means the sound does
	// not jump: the eight values you did not touch are already the ones
	// you could hear.
	if (tag < kNumParams && isFormantParam (tag) && currentVowel () != kVowelManual)
		captureAndGoManual ();

	mController->setParamNormalized (tag, value);
	mController->performEdit (tag, value);
}

//------------------------------------------------------------------------
void VocalFilterEditor::controlBeginEdit (CControl* control)
{
	if (mController && control)
		mController->beginEdit (static_cast<ParamID> (control->getTag ()));
}

//------------------------------------------------------------------------
void VocalFilterEditor::controlEndEdit (CControl* control)
{
	if (mController && control)
		mController->endEdit (static_cast<ParamID> (control->getTag ()));
}

//------------------------------------------------------------------------
void VocalFilterEditor::updateControl (ParamID tag, ParamValue normalized)
{
	if (frame == nullptr)
		return;

	// The selector moving changes what every formant slider should be
	// showing, so it is not a control update - it is a whole-panel one.
	if (tag == kVowel)
	{
		refreshVowelState ();
		return;
	}

	// While a preset is live the nine sliders are showing the PRESET, so a
	// change to the parameters behind them is not something to display -
	// it would overwrite the preset's values with the manual ones the
	// panel is deliberately not showing.
	if (isFormantParam (tag) && currentVowel () != kVowelManual)
		return;

	auto it = mControls.find (tag);
	if (it == mControls.end () || it->second == nullptr)
		return;

	it->second->setValueNormalized (static_cast<float> (normalized));
	it->second->invalid ();
}

//------------------------------------------------------------------------
} // namespace VocalFilter
