//------------------------------------------------------------------------
// VocalFilter - editor implementation
//------------------------------------------------------------------------

#include "VocalFilterEditor.h"
#include "VocalFilterController.h"

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
	const CCoord y = kMargin + kHeaderHeight + row * (kSliderHeight + kRowGap);
	return CRect (x, y, x + kSliderWidth, y + kSliderHeight);
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
	// The title, and the three column headings above the grid.
	//--------------------------------------------------------------------
	addHeading ("Vocal Tract  -  three parallel formants",
	            CRect (kMargin, 2, kEditorWidth - kMargin, 2 + 16));

	for (int column = 0; column < kColumns; ++column)
	{
		const CRect head = cell (column, 0);
		addHeading (kColumnNames[column],
		            CRect (head.left, kMargin + 6, head.right, kMargin + 6 + 14));
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
	// The bottom row: the two controls that are not part of a formant.
	// Dry / Wet sits under F1 because it is the one most likely to be
	// touched while listening; the trim sits under F3, at the end of the
	// signal path, which is where it is in the code.
	//--------------------------------------------------------------------
	const CCoord bottom = kMargin + kHeaderHeight
	                    + kRows * (kSliderHeight + kRowGap) - kRowGap
	                    + kSectionGap;

	CRect mixRect = cell (0, 0);
	mixRect.offset (0, bottom - mixRect.top);
	addSlider (kMix, "Dry / Wet", mixRect);

	CRect trimRect = cell (kColumns - 1, 0);
	trimRect.offset (0, bottom - trimRect.top);
	addSlider (kOutputTrim, "Output Trim", trimRect);

	frame->open (parent, platformType);
	return true;
}

//------------------------------------------------------------------------
void PLUGIN_API VocalFilterEditor::close ()
{
	mControls.clear ();

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

	auto it = mControls.find (tag);
	if (it == mControls.end () || it->second == nullptr)
		return;

	it->second->setValueNormalized (static_cast<float> (normalized));
	it->second->invalid ();
}

//------------------------------------------------------------------------
} // namespace VocalFilter
