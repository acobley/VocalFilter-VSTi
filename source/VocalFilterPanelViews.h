//------------------------------------------------------------------------
// VocalFilter - the "vowel space" panel's views
//
// WRITTEN FOR THE DESIGN2 PANEL (16 Oct 2026), from the design in
// design/VocalFilter-Concept3b-VowelSpace.png.
//
// LIFTED from ~/DXi-DEv/R2Synth-VSTi/source/R2SynthPanelViews.*:
//
//   Palette, the faces, upper()     unchanged apart from the accents,
//                                   which are named by colour here
//   Knob                            unchanged
//   SegmentSwitch                   unchanged
//   Backdrop                        same drawing, this panel's tables
//
// NEW HERE, in the same idiom - no bitmaps, everything paths and text,
// colours from the palette:
//
//   VowelPad       the F1 x F2 vowel chart you can play
//   Spectrum       the response, with a handle on each formant
//   VowelButton    one vowel preset - IPA symbol and name
//   ManualButton   leave the presets, keeping the sound
//   GlideMeter     how far through the current glide the DSP is
//   BypassPill     the header's bypass toggle
//
// THE SAME TWO RULES AS R2SYNTH. No bitmaps, so the panel is
// resolution-independent and resource/ needs no artwork. And NO NUMBERS
// MADE HERE: every reading comes in through a formatter or a setter the
// editor drives, and every curve from bandpassResponse()/bankMagnitude(),
// the functions the DSP's own coefficients come from.
//
// REDRESSED AS THE BLACK PANEL (branch PolivoksPanel, 10 Nov 2026), from
// design/VocalFilter-Polivoks-1-BlackPanel.png - R2Synth's black panel
// (R2Synth-VSTi/source/R2SynthPanelViews.*, 22 Oct 2026) on this layout:
// matte black, white printing, each section boxed in a thin white line
// with its title in its own box, black knurled knobs with printed
// scales, black keys with a red lamp, the two screens as black glass
// behind a cut-out, and every word in the 70s sci-fi lettering
// (SciFiLettering.h), the IPA symbols included. The displays keep their
// content and the F1 amber / F2 blue / F3 violet code; the formant
// colours are the only colour on the panel besides the red lamps. Light
// wear (PanelWear.h): dust, a few hairlines, polished knob tops, dusty
// glass - the Backdrop draws the panel's, each Knob, VowelPad and
// Spectrum its own (setWear).
//
// VocalFilterControls.* (the SpyBand bar sliders) and
// VocalFilterDisplay.* (SpyResponseDisplay) are no longer on the panel.
// They are left in source/ untouched for now so the main branch's panel
// can be compared against this one.
//------------------------------------------------------------------------

#pragma once

#include "PanelWear.h"
#include "SciFiLettering.h"
#include "VocalFilterDsp.h"
#include "VocalFilterLayout.h"

#include "vstgui/vstgui.h"

#include <functional>
#include <string>
#include <vector>

namespace VocalFilter {
namespace Panel {

//------------------------------------------------------------------------
// The palette - the black panel's tokens (R2Synth's)
//------------------------------------------------------------------------
namespace Palette {

const VSTGUI::CColor kGround     ( 18,  18,  18, 255);
const VSTGUI::CColor kHeader     ( 12,  12,  12, 255);
const VSTGUI::CColor kHeaderRule (233, 230, 220, 255);
const VSTGUI::CColor kSection    ( 21,  21,  21, 255);
const VSTGUI::CColor kSectionEdge(217, 214, 204, 255);   // box lines, the switch's frame
const VSTGUI::CColor kInk        (244, 241, 232, 255);   // the lettering: white on black
const VSTGUI::CColor kInkSoft    (185, 182, 174, 255);   // subtitles, notes
const VSTGUI::CColor kInkDim     (143, 140, 133, 255);   // a switch position not set
const VSTGUI::CColor kLabel      (217, 214, 204, 255);   // the names under the knobs
const VSTGUI::CColor kDivider    ( 90,  88,  82, 255);
const VSTGUI::CColor kRule       ( 58,  58,  58, 255);
const VSTGUI::CColor kTick       (233, 230, 220, 255);   // the printed scales
const VSTGUI::CColor kRed        (226,  52,  43, 255);   // the bypass window's frame
const VSTGUI::CColor kScrew      ( 58,  58,  58, 255);
const VSTGUI::CColor kKnobBody   ( 10,  10,  10, 255);
const VSTGUI::CColor kKnobRim    ( 47,  47,  47, 255);
const VSTGUI::CColor kKey        ( 27,  27,  27, 255);   // a vowel key, Manual
const VSTGUI::CColor kKeyDown    ( 40,  40,  40, 255);
const VSTGUI::CColor kKeyEdge    ( 74,  74,  74, 255);
const VSTGUI::CColor kBezel      (  0,   0,   0, 255);   // the cut-out round the glass
const VSTGUI::CColor kBezelEdge  ( 60,  60,  60, 255);
const VSTGUI::CColor kScreen     (  8,   9,  11, 255);
const VSTGUI::CColor kGrid       ( 23,  27,  33, 255);
const VSTGUI::CColor kScreenText (141, 150, 163, 255);   // captions on the glass
const VSTGUI::CColor kScreenDim  ( 89,  96, 107, 255);   // the axes' figures
const VSTGUI::CColor kVowelDisc  ( 18,  19,  22, 255);   // a vowel circle on the chart
const VSTGUI::CColor kVowelEdge  ( 60,  64,  71, 255);
const VSTGUI::CColor kLampOn     (255,  59,  47, 255);
const VSTGUI::CColor kLampOff    ( 58,  21,  18, 255);

/** The names the editor used before the black panel. */
const VSTGUI::CColor kText   = kInk;
const VSTGUI::CColor kSubtle = kInkSoft;

/** A role's colour: amber, blue, violet, mint. */
VSTGUI::CColor accent (Layout::Accent a);

/** The same at `alpha` (0..255). */
VSTGUI::CColor accentAlpha (Layout::Accent a, uint8_t alpha);

/** The text drawn ON an accent-coloured fill: a near-black of the same
    hue, so a lit segment reads at a glance. */
VSTGUI::CColor accentInk (Layout::Accent a);

/** Any colour at `alpha`. */
VSTGUI::CColor withAlpha (VSTGUI::CColor c, uint8_t alpha);

} // namespace Palette

//------------------------------------------------------------------------
// The lettering. The panel draws no font text: every word is stroked
// paths in the 70s sci-fi face.
//------------------------------------------------------------------------
enum class Align
{
	Left,
	Centre,
	Right
};

/** DRAW A RUN OF LETTERING: `text` in `style`, its ink box vertically
    centred on `centreY`, starting at, centred on or ending at `x`.
    `strokeUnits` > 0 strokes the same glyphs at another weight without
    moving them - the wordmark's inline cut. (R2Synth's, unchanged.) */
void drawLettering (VSTGUI::CDrawContext* context, const std::string& text, const Lettering::Style& style,
                    const VSTGUI::CColor& colour, VSTGUI::CCoord x, VSTGUI::CCoord centreY,
                    Align align = Align::Left, double strokeUnits = -1.0);

/** A string in capitals - labels are stored as words and drawn as small
    capitals. ASCII only, which is all the labels are. */
std::string upper (const std::string& s);

/** A frequency as the panel prints it: "730 Hz", "1.09k". */
std::string formatHz (double hz);

//------------------------------------------------------------------------
/** Everything that never changes, from the layout tables, in one view
    behind the others. Mouse-disabled. */
class Backdrop : public VSTGUI::CView
{
public:
	explicit Backdrop (const VSTGUI::CRect& size);
	void draw (VSTGUI::CDrawContext* context) override;
	CLASS_METHODS (Backdrop, VSTGUI::CView)

private:
	/** The wear on the panel - over the printing, under the controls.
	    Only the marks that touch the area being redrawn are drawn, so a
	    knob turning does not redraw a thousand specks. */
	void drawWear (VSTGUI::CDrawContext* context);

	Wear::PanelWear mWear;
};

//------------------------------------------------------------------------
/** A ROTARY KNOB - R2Synth's black-panel knob: black, knurled, a white
    line, a printed scale of eleven ticks over 270 degrees.

    THE WHOLE RECTANGLE IS THE TARGET, reading and label included.

      drag           vertically; up increases; 200 pixels is the whole range
      Shift          ten times finer, drag and wheel alike
      double-click   the parameter's default (setDefaultValue)
      wheel          one per cent a click */
class Knob : public VSTGUI::CControl
{
public:
	Knob (const VSTGUI::CRect& size, VSTGUI::IControlListener* listener, int32_t tag,
	      Layout::Accent accent, int diameter, bool bipolar);

	void setLabel (const std::string& label) { mLabel = upper (label); }

	/** The reading, given the NORMALISED value - the editor hands over
	    the parameter table's own formatting. */
	void setFormatter (std::function<std::string (float)> formatter) { mFormatter = std::move (formatter); }

	/** Its wear (Wear::knobWear) - drawn relative to the pointer, so it
	    turns with the knob. None until set. */
	void setWear (const Wear::KnobWear& wear) { mWear = wear; }

	void draw (VSTGUI::CDrawContext* context) override;

	void onMouseDownEvent (VSTGUI::MouseDownEvent& event) override;
	void onMouseMoveEvent (VSTGUI::MouseMoveEvent& event) override;
	void onMouseUpEvent (VSTGUI::MouseUpEvent& event) override;
	void onMouseCancelEvent (VSTGUI::MouseCancelEvent& event) override;
	void onMouseWheelEvent (VSTGUI::MouseWheelEvent& event) override;

	CLASS_METHODS (Knob, VSTGUI::CControl)

	/** VSTGUI's degrees (0 = three o'clock, clockwise): 135 at 0, 270 (the
	    top) at 0.5, 405 at 1. */
	static double angleOf (double normalized) { return 135.0 + 270.0 * normalized; }

private:
	/** Its formant. Every knob on the black panel is black and white;
	    kept so a knob still knows. */
	Layout::Accent mAccent;
	int  mDiameter;
	bool mBipolar;
	std::string mLabel;
	std::function<std::string (float)> mFormatter;
	Wear::KnobWear mWear;

	bool mDragging = false;
	VSTGUI::CCoord mLastY = 0.;
};

//------------------------------------------------------------------------
/** A SEGMENTED SWITCH - the Voice switch: a white-framed black slot, one
    segment per named position, the current one printed white with black
    lettering; a click selects, the wheel steps.

    Given a LABEL, it is the Level Comp switch instead: the slot takes the
    top Layout::kSwitchSlotHeight of the rectangle and a reading and the
    label are printed under it, on the lines a knob's would be. The whole
    rectangle is still the target. */
class SegmentSwitch : public VSTGUI::CControl
{
public:
	SegmentSwitch (const VSTGUI::CRect& size, VSTGUI::IControlListener* listener, int32_t tag,
	               Layout::Accent accent);

	void setNames (const std::vector<std::string>& names) { mNames = names; invalid (); }

	/** Under the slot: the label, in capitals. Empty (the default) is a
	    bare slot filling the rectangle. */
	void setLabel (const std::string& label) { mLabel = upper (label); invalid (); }

	/** The reading over the label - white when `lit`, the label's grey
	    when not. Repaints only if something changed. */
	void setReading (const std::string& reading, bool lit);

	void draw (VSTGUI::CDrawContext* context) override;
	void onMouseDownEvent (VSTGUI::MouseDownEvent& event) override;
	void onMouseUpEvent (VSTGUI::MouseUpEvent& event) override;
	void onMouseWheelEvent (VSTGUI::MouseWheelEvent& event) override;

	CLASS_METHODS (SegmentSwitch, VSTGUI::CControl)

private:
	int count () const { return static_cast<int> (mNames.size ()); }
	int currentIndex () const;
	void select (int index);

	Layout::Accent mAccent;
	std::vector<std::string> mNames;
	std::string mLabel;
	std::string mReading;
	bool mReadingLit = false;
};

//------------------------------------------------------------------------
/** What the vowel chart reports. The editor implements it: the pad knows
    nothing about parameters, edits or presets, only about hertz. */
class IVowelPadListener
{
public:
	virtual ~IVowelPadListener () = default;

	/** A drag has started. The editor leaves preset mode (keeping the
	    sound) and opens the two edit gestures. */
	virtual void padDragBegin () = 0;

	/** The pointer is at (F1, F2), already clamped to the parameter
	    ranges. */
	virtual void padDragMove (double f1Hz, double f2Hz) = 0;

	virtual void padDragEnd () = 0;

	/** A vowel circle was clicked, not dragged. 1..5, as the Vowel
	    selector counts. */
	virtual void padVowelClicked (int selector) = 0;
};

//------------------------------------------------------------------------
/** THE VOWEL CHART. F2 across (falling to the right), F1 down (falling
    downward), both log-scaled over exactly their parameter ranges - see
    Layout::padU / padV.

    Drawn: the five vowels of the CURRENT voice as labelled circles joined
    round the chart, the other voice's five as faint hollow dots joined by
    a dotted line, the dot at the F1/F2 the DSP is using right now, and -
    while a glide is under way - its path: dashed from where it started to
    where it is going, solid up to where it is.

      click a vowel circle      select that vowel (the Vowel parameter)
      drag anywhere             move F1 and F2 together; Manual
      Shift while dragging      ten times finer, relative to the dot

    A press on a vowel circle that then moves more than a few pixels
    becomes a drag, so a vowel is a place to start from as well as a
    button. */
class VowelPad : public VSTGUI::CView
{
public:
	VowelPad (const VSTGUI::CRect& size, IVowelPadListener* listener);

	struct State
	{
		int    voice = kVoiceMale;
		int    selector = kVowelManual;  // the lit vowel, 0 for none
		double f1 = 730.0, f2 = 1090.0;  // where the dot is
		bool   gliding = false;
		double fromF1 = 0.0, fromF2 = 0.0;
		double toF1 = 0.0, toF2 = 0.0;
	};

	/** Returns true if anything that is drawn changed. */
	bool setState (const State& state);

	/** The dust and the fingerprint on its glass (Wear::glassWear (0)),
	    drawn over everything. */
	void setWear (const Wear::GlassWear& wear) { mWear = wear; }

	void draw (VSTGUI::CDrawContext* context) override;

	void onMouseDownEvent (VSTGUI::MouseDownEvent& event) override;
	void onMouseMoveEvent (VSTGUI::MouseMoveEvent& event) override;
	void onMouseUpEvent (VSTGUI::MouseUpEvent& event) override;
	void onMouseCancelEvent (VSTGUI::MouseCancelEvent& event) override;

	CLASS_METHODS (VowelPad, VSTGUI::CView)

private:
	VSTGUI::CRect plot () const;
	VSTGUI::CPoint pointOf (double f1Hz, double f2Hz) const;
	void hzOf (const VSTGUI::CPoint& p, double& f1Hz, double& f2Hz) const;
	int vowelAt (const VSTGUI::CPoint& p) const;   // index 0..4, or -1
	void beginDrag (const VSTGUI::CPoint& p, bool fine);
	void dragTo (const VSTGUI::CPoint& p, bool fine);

	IVowelPadListener* mListener;
	State mState;
	Wear::GlassWear mWear;

	enum class Press { None, OnVowel, Dragging };
	Press mPress = Press::None;
	int   mPressedVowel = -1;
	VSTGUI::CPoint mDownPoint;
	VSTGUI::CPoint mLastPoint;
	double mDragU = 0.0, mDragV = 0.0;   // where the drag has put the dot, 0..1
};

//------------------------------------------------------------------------
/** THE SPECTRUM: the three branches and their + - + sum on a log
    frequency axis, with a handle on each formant's peak and a bracket
    showing its width.

    The curves come from bandpassResponse()/bandpassMagnitude() and
    bankMagnitude() - the DSP's own functions - at the rate the DSP is
    running. A line that leaves the scale is drawn down to the bottom
    edge and broken there, never run along the floor. Mouse-disabled: it is a readout. */
class Spectrum : public VSTGUI::CView
{
public:
	explicit Spectrum (const VSTGUI::CRect& size);

	/** Returns true if anything moved. */
	bool setFormants (const FormantSetting* formants, double sampleRate);

	void setCaption (const std::string& caption);
	void setAnnotation (const std::string& annotation);

	/** The dust and the fingerprint on its glass (Wear::glassWear (1)). */
	void setWear (const Wear::GlassWear& wear) { mWear = wear; }

	void draw (VSTGUI::CDrawContext* context) override;

	CLASS_METHODS (Spectrum, VSTGUI::CView)

private:
	VSTGUI::CRect plot () const;
	VSTGUI::CCoord xOf (double hz, const VSTGUI::CRect& p) const;
	VSTGUI::CCoord yOf (double db, const VSTGUI::CRect& p) const;
	void strokeCurve (VSTGUI::CDrawContext* context, const VSTGUI::CRect& p,
	                  const std::function<double (double)>& dbAt,
	                  const VSTGUI::CColor& colour, VSTGUI::CCoord width, bool fill);

	FormantSetting mFormants[kFormantCount] = {};
	double mSampleRate = 44100.0;
	std::string mCaption;
	std::string mAnnotation;
	Wear::GlassWear mWear;
};

//------------------------------------------------------------------------
/** ONE VOWEL PRESET: a black key, the IPA symbol large, the button's
    name under it, a lamp in its corner. Lamp lit while that vowel is
    selected - by this panel or by host
    automation, so it is an indicator as well as a button. Fires on mouse
    UP, and only if the pointer is still inside. Carries no parameter: the
    handler writes the Vowel selector. */
class VowelButton : public VSTGUI::CView
{
public:
	VowelButton (const VSTGUI::CRect& size, const std::string& glyph, const std::string& name);

	void setHandler (std::function<void ()> handler) { mHandler = std::move (handler); }
	void setSelected (bool selected);

	void draw (VSTGUI::CDrawContext* context) override;
	void onMouseDownEvent (VSTGUI::MouseDownEvent& event) override;
	void onMouseMoveEvent (VSTGUI::MouseMoveEvent& event) override;
	void onMouseUpEvent (VSTGUI::MouseUpEvent& event) override;
	void onMouseCancelEvent (VSTGUI::MouseCancelEvent& event) override;

	CLASS_METHODS (VowelButton, VSTGUI::CView)

private:
	std::string mGlyph;
	std::string mName;
	std::function<void ()> mHandler;
	bool mSelected = false;
	bool mPressed = false;
	bool mInside = false;
};

//------------------------------------------------------------------------
/** MANUAL: a black key with a lamp, the word, and a hint. Lit while the Vowel selector is
    on Manual. A click leaves the presets WITHOUT the sound jumping - the
    editor copies the live vowel into the nine parameters first. */
class ManualButton : public VSTGUI::CView
{
public:
	ManualButton (const VSTGUI::CRect& size, const std::string& hint);

	void setHandler (std::function<void ()> handler) { mHandler = std::move (handler); }
	void setLit (bool lit);

	void draw (VSTGUI::CDrawContext* context) override;
	void onMouseDownEvent (VSTGUI::MouseDownEvent& event) override;
	void onMouseMoveEvent (VSTGUI::MouseMoveEvent& event) override;
	void onMouseUpEvent (VSTGUI::MouseUpEvent& event) override;
	void onMouseCancelEvent (VSTGUI::MouseCancelEvent& event) override;

	CLASS_METHODS (ManualButton, VSTGUI::CView)

private:
	std::string mHint;
	std::function<void ()> mHandler;
	bool mLit = false;
	bool mPressed = false;
	bool mInside = false;
};

//------------------------------------------------------------------------
/** How far through the current glide the DSP is: a bar and the elapsed
    time under it. Mouse-disabled. */
class GlideMeter : public VSTGUI::CView
{
public:
	explicit GlideMeter (const VSTGUI::CRect& size);

	/** `progress` 0..1; `text` under the bar. Negative progress: no glide
	    under way. */
	void setProgress (double progress, const std::string& text);

	void draw (VSTGUI::CDrawContext* context) override;

	CLASS_METHODS (GlideMeter, VSTGUI::CView)

private:
	double mProgress = -1.0;
	std::string mText;
};

//------------------------------------------------------------------------
/** BYPASS, as a red-framed window in the header: a lamp and the words.
    A click toggles the host's bypass parameter. */
class BypassPill : public VSTGUI::CControl
{
public:
	BypassPill (const VSTGUI::CRect& size, VSTGUI::IControlListener* listener, int32_t tag);

	void draw (VSTGUI::CDrawContext* context) override;
	void onMouseDownEvent (VSTGUI::MouseDownEvent& event) override;
	void onMouseUpEvent (VSTGUI::MouseUpEvent& event) override;

	CLASS_METHODS (BypassPill, VSTGUI::CControl)
};

//------------------------------------------------------------------------
/** ONE LINE OF LETTERING whose words change - the engine line in the
    header. Left-aligned, vertically centred in the view.
    Mouse-disabled. */
class TextLine : public VSTGUI::CView
{
public:
	TextLine (const VSTGUI::CRect& size, const Lettering::Style& style, const VSTGUI::CColor& colour);
	void setText (const std::string& text);
	const std::string& getText () const { return mText; }
	void draw (VSTGUI::CDrawContext* context) override;
	CLASS_METHODS (TextLine, VSTGUI::CView)

private:
	Lettering::Style mStyle;
	VSTGUI::CColor mColour;
	std::string mText;
};

//------------------------------------------------------------------------
} // namespace Panel
} // namespace VocalFilter
