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
// VocalFilterControls.* (the SpyBand bar sliders) and
// VocalFilterDisplay.* (SpyResponseDisplay) are no longer on the panel.
// They are left in source/ untouched for now so the main branch's panel
// can be compared against this one.
//------------------------------------------------------------------------

#pragma once

#include "VocalFilterDsp.h"
#include "VocalFilterLayout.h"

#include "vstgui/vstgui.h"

#include <functional>
#include <string>
#include <vector>

namespace VocalFilter {
namespace Panel {

//------------------------------------------------------------------------
// The palette - R2Synth's tokens
//------------------------------------------------------------------------
namespace Palette {

const VSTGUI::CColor kGround    ( 20,  21,  24, 255);
const VSTGUI::CColor kHeader    ( 26,  28,  32, 255);
const VSTGUI::CColor kBorder    ( 42,  45,  51, 255);
const VSTGUI::CColor kSection   ( 28,  30,  34, 255);
const VSTGUI::CColor kRule      ( 36,  38,  43, 255);
const VSTGUI::CColor kTrack     ( 46,  49,  55, 255);
const VSTGUI::CColor kCap       ( 42,  44,  49, 255);
const VSTGUI::CColor kCapEdge   ( 60,  64,  71, 255);
const VSTGUI::CColor kText      (236, 234, 228, 255);
const VSTGUI::CColor kTitle     (201, 204, 210, 255);
const VSTGUI::CColor kMuted     (156, 161, 170, 255);
const VSTGUI::CColor kSubtle    (126, 131, 140, 255);
const VSTGUI::CColor kDim       ( 74,  78,  86, 255);
const VSTGUI::CColor kScreen    ( 15,  16,  18, 255);
const VSTGUI::CColor kScreenEdge( 38,  41,  46, 255);
const VSTGUI::CColor kGrid      ( 29,  32,  36, 255);
const VSTGUI::CColor kSegBack   ( 18,  19,  22, 255);
const VSTGUI::CColor kLampOn    (255, 138,  61, 255);
const VSTGUI::CColor kLampOff   ( 58,  42,  32, 255);
const VSTGUI::CColor kLampEdge  ( 90,  58,  40, 255);

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
// The faces. On macOS CoreText falls back to the system face if one is
// missing, so a name here can never leave text undrawn.
//------------------------------------------------------------------------
VSTGUI::CFontRef fontWordmark ();   // Avenir Next Condensed, bold 23
VSTGUI::CFontRef fontTitle ();      // Helvetica Neue, bold 13
VSTGUI::CFontRef fontSubtitle ();   // Helvetica Neue 12
VSTGUI::CFontRef fontLabel ();      // Helvetica Neue, bold 10 - capitals
VSTGUI::CFontRef fontValue ();      // Menlo 10 - every reading
VSTGUI::CFontRef fontNote ();       // Helvetica Neue 11
VSTGUI::CFontRef fontGlyph ();      // Helvetica Neue, bold 16 - the IPA on a vowel
VSTGUI::CFontRef fontGlyphSmall (); // Helvetica Neue, bold 15 - the IPA on the chart

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
};

//------------------------------------------------------------------------
/** A ROTARY KNOB - R2Synth's, unchanged. The arc runs 270 degrees; the
    value arc is drawn over a dark track in the role's colour.

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
	Layout::Accent mAccent;
	int  mDiameter;
	bool mBipolar;
	std::string mLabel;
	std::function<std::string (float)> mFormatter;

	bool mDragging = false;
	VSTGUI::CCoord mLastY = 0.;
};

//------------------------------------------------------------------------
/** A SEGMENTED SWITCH - R2Synth's, unchanged. One segment per named
    position, the current one lit; a click selects, the wheel steps. */
class SegmentSwitch : public VSTGUI::CControl
{
public:
	SegmentSwitch (const VSTGUI::CRect& size, VSTGUI::IControlListener* listener, int32_t tag,
	               Layout::Accent accent);

	void setNames (const std::vector<std::string>& names) { mNames = names; invalid (); }

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
};

//------------------------------------------------------------------------
/** ONE VOWEL PRESET: the IPA symbol large, the button's name under it.
    Lit in amber while that vowel is selected - by this panel or by host
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
/** MANUAL: a lamp, the word, and a hint. Lit while the Vowel selector is
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
/** BYPASS, as a pill in the header: a lamp and the word. A click toggles
    the host's bypass parameter. */
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
} // namespace Panel
} // namespace VocalFilter
