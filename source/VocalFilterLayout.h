//------------------------------------------------------------------------
// VocalFilter - the panel's layout, as data
//
// THE "VOWEL SPACE" PANEL (branch Design2, 16 Oct 2026), from the design
// design/VocalFilter-Concept3b-VowelSpace.png. It replaces the 348 x 229
// grid of SpyBand bar sliders with R2Synth's panel idiom - sections,
// knobs, segment switches, dark screens - and one new control: a vowel
// chart you can play.
//
// THE LAYOUT IS DATA, not positions typed into the middle of open(), the
// way R2Synth's UpdatedUI branch did it. This header includes NO VSTGUI,
// so three things read the same numbers:
//
//   * VocalFilterEditor::open(), which builds the panel from the tables;
//   * tests/LayoutTests.cpp, which checks nothing overlaps and everything
//     fits - the only way to test a layout from a session that cannot
//     run a host;
//   * tools/dump-layout.cpp + tools/render-panel.py, which draw
//     docs/panel.png from these tables.
//
//   +--------------------------------------------------------------+
//   | VOCALFILTER | engine line                        [BYPASS]    |  header
//   +-----------------------------+--------------------------------+
//   | VOWEL SPACE      [Male|Fem] | SPECTRUM                        |
//   |  F1 x F2 chart, the five    |  response, three handles        |
//   |  vowels, the dot, the glide +--------------------------------+
//   |                             | FORMANTS  F1 | F2 | F3 knobs    |
//   | [a][i][I][o][u]   (Glide)   +--------------------------------+
//   | [ MANUAL       ]   progress | OUTPUT  Dry/Wet Trim  Lvl Comp |
//   +-----------------------------+--------------------------------+
//
// THE PAD IS THE PARAMETER SPACE. Its axes run over exactly the F1 and F2
// parameter ranges (kFormantFreqMin/Max), log-scaled and laid out the way
// a phonetician plots vowels - F2 FALLING to the right, F1 FALLING
// downward - so every point on it is a reachable setting and every
// setting is a point on it. padX/padY and their inverses are here, not in
// the view, so the tests can hold them to that.
//
// The panel is 1200 x 640, R2Synth's size.
//
// THE BLACK PANEL (branch PolivoksPanel, 10 Nov 2026), from
// design/VocalFilter-Polivoks-1-BlackPanel.png: R2Synth's black panel
// redress (matte black, white printing, boxed sections, knurled knobs,
// every word in the 70s sci-fi lettering, light wear) on THIS layout. The
// only moves: the wordmark is wider, so the header divider and the engine
// line shift right; four notes are shortened to fit the wider face. The
// lettering's sizes (kText*) and the boxes the title and subtitle land
// in are here, so the tests measure the same runs the views draw.
//------------------------------------------------------------------------

#pragma once

#include "SciFiLettering.h"
#include "VocalFilterDsp.h"
#include "VocalFilterParams.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace VocalFilter {
namespace Layout {

//------------------------------------------------------------------------
/** A rectangle, left/top/right/bottom, in panel pixels. */
struct Rect
{
	int left, top, right, bottom;

	constexpr int width () const { return right - left; }
	constexpr int height () const { return bottom - top; }
};

/** The same in fractional pixels - where a run of lettering lands. */
struct Box
{
	double left, top, right, bottom;
};

struct Point { int x, y; };

//------------------------------------------------------------------------
// The panel
//------------------------------------------------------------------------
constexpr int kEditorWidth  = 1200;
constexpr int kEditorHeight = 640;
constexpr int kMargin       = 16;

constexpr int kHeaderHeight = 56;

/** Inside a section: the title row, and the padding round it. */
constexpr int kSectionPadX     = 14;
constexpr int kSectionPadTop   = 12;
constexpr int kSectionTitleH   = 18;
constexpr int kSectionRadius   = 3;

/** Under every knob: two pixels, the reading, the label. */
constexpr int kKnobGap       = 2;
constexpr int kKnobValueH    = 13;
constexpr int kKnobLabelH    = 12;
constexpr int kKnobTextBand  = kKnobGap + kKnobValueH + kKnobLabelH;

/** A segmented switch that sits in a section's title row. */
constexpr int kTitleSwitchHeight = 24;

/** A ROTARY SELECTOR's rectangle (R2Synth's chicken head - the Polivoks
    panel's switch), top to bottom: a band over the knob, the knob, and at
    the foot a reading and its name on the lines a knob's would be.
 *
 *  The band is 6 px, not R2Synth's 26, because R2Synth's carries the
 *  MIDDLE name of a three-position selector over the knob and the only
 *  selector here has two, whose names hang beside it at -45 and +45 - the
 *  tests hold those inside the rectangle. A three-position selector needs
 *  R2Synth's band back. */
constexpr int kSelectorTopBand     = 6;
constexpr int kSelectorReadingBand = kKnobValueH + kKnobLabelH;

/** The Level Comp reading: the make-up being applied, to a tenth of a dB,
    signed - "+16.0 dB" - and a plain "0.0 dB" when there is none. Here, so
    the editor and the tests format the same string. */
inline std::string levelCompReading (double db)
{
	char buffer[24];
	if (std::fabs (db) < 0.05)
		std::snprintf (buffer, sizeof (buffer), "0.0 dB");
	else
		std::snprintf (buffer, sizeof (buffer), "%+.1f dB", db);
	return buffer;
}

//------------------------------------------------------------------------
// The lettering - SciFiLettering.h's face at the panel's sizes. Cap
// heights are 0.64 of the font sizes the design was drawn with, so the
// tests measure exactly what is drawn (Lettering::width).
//------------------------------------------------------------------------
constexpr Lettering::Style kTextWordmark { 22.0, 1.7, 1.2 };
constexpr Lettering::Style kTextTitle    { 8.3, Lettering::kBold };     // a section's name
constexpr Lettering::Style kTextSubtitle { 6.1, Lettering::kRegular };
constexpr Lettering::Style kTextBlock    { 8.3, Lettering::kBold };     // F1 / F2 / F3
constexpr Lettering::Style kTextNote     { 6.4, Lettering::kRegular };
constexpr Lettering::Style kTextLabel    { 6.1, Lettering::kBold };     // knob names, vowel names
constexpr Lettering::Style kTextValue    { 6.1, Lettering::kRegular };  // every reading
constexpr Lettering::Style kTextEngine   { 6.7, Lettering::kRegular };
constexpr Lettering::Style kTextPill     { 6.7, Lettering::kBold };     // BYPASS, MANUAL, Male / Female
constexpr Lettering::Style kTextHint     { 5.8, Lettering::kRegular };  // the Manual button's hint
constexpr Lettering::Style kTextScreen   { 6.1, Lettering::kRegular };  // captions on the glass
constexpr Lettering::Style kTextScale    { 5.8, Lettering::kRegular };  // the axes' figures
constexpr Lettering::Style kTextIpaKey   { 10.2, Lettering::kBold, 1.05, true };   // a vowel button's symbol
constexpr Lettering::Style kTextIpaChart { 9.6, Lettering::kBold, 1.05, true };    // a vowel circle's symbol

//------------------------------------------------------------------------
// Colours, by role. One per formant, carried through the knobs, the
// handles on the spectrum and the column headings, so F2 is blue
// everywhere it appears.
//------------------------------------------------------------------------
enum class Accent
{
	Amber,    // F1, and the vowel space
	Blue,     // F2, and the spectrum
	Violet,   // F3, the formants section, the glide
	Mint      // output
};

constexpr Accent formantAccent (int formant)
{
	return formant == 0 ? Accent::Amber : formant == 1 ? Accent::Blue : Accent::Violet;
}

//------------------------------------------------------------------------
// Sections
//------------------------------------------------------------------------
struct Section
{
	const char* title;
	const char* subtitle;   // may be empty
	Accent accent;
	Rect r;
};

constexpr Rect kVowelRect    { 16,  72,  576,  624 };
constexpr Rect kSpectrumRect { 592, 72,  1184, 334 };
constexpr Rect kFormantRect  { 592, 350, 1184, 512 };
constexpr Rect kOutputRect   { 592, 528, 1184, 624 };

constexpr Section kSections[] = {
	{ "VOWEL SPACE", "drag to blend between vowels",                       Accent::Amber,  kVowelRect    },
	{ "SPECTRUM",    "the filter as it sounds now",                        Accent::Blue,   kSpectrumRect },
	{ "FORMANTS",    "width, level, and F3 - off the pad",                 Accent::Violet, kFormantRect  },
	{ "OUTPUT",      "",                                                   Accent::Mint,   kOutputRect   },
};
constexpr int kNumSections = static_cast<int> (sizeof (kSections) / sizeof (kSections[0]));

constexpr double titleRowCentre (const Rect& section)
{
	return section.top + kSectionPadTop + kSectionTitleH / 2.0 - 0.5;
}

/** THE TITLE BOX: the section's name in a white-outlined box at the left
    of its title row, 9 px of black each side of the lettering. */
inline Box titleBox (const Section& s)
{
	const double left = s.r.left + kSectionPadX;
	const double centre = titleRowCentre (s.r);
	return { left, centre - 10.5, left + 18.0 + Lettering::width (s.title, kTextTitle), centre + 10.5 };
}

/** Where the subtitle starts: 12 px after the title box. */
inline double subtitleLeft (const Section& s)
{
	return titleBox (s).right + 12.0;
}

//------------------------------------------------------------------------
// Controls - every parameter that has a knob or a switch
//------------------------------------------------------------------------
enum class Kind
{
	Knob,     // Panel::Knob - vertical drag
	Switch,   // Panel::SegmentSwitch - one segment per named position (Voice, in a title row)
	Selector  // Panel::RotarySwitch - a chicken head, its positions printed round it (Level Comp)
};

struct Control
{
	Steinberg::Vst::ParamID tag;
	Kind   kind;
	Accent accent;
	int    x, y;        // top left of the control's rectangle
	int    width;
	int    diameter;    // a knob's; a selector's chicken-head circle; 0 for a switch
	const char* label;
	int    height = 0;  // switches and selectors
};

constexpr Rect controlRect (const Control& c)
{
	return { c.x, c.y, c.x + c.width,
	         c.y + ((c.kind == Kind::Knob) ? c.diameter + kKnobTextBand : c.height) };
}

/** The formant columns: 192 pixels each, starting inside the section. */
constexpr int kFormantColLeft  = 606;
constexpr int kFormantColPitch = 192;
constexpr int formantCol (int k) { return kFormantColLeft + k * kFormantColPitch; }

constexpr int kFormantKnobTop  = 422;
constexpr int kFormantKnobDia  = 36;
constexpr int kFormantKnobW    = 56;

/** EVERY PARAMETER WITH A CONTROL, once each - the tests hold it to that.
    The Vowel selector is the one stored parameter with no entry: its
    controls are the five vowel buttons, the Manual button and the vowel
    circles on the pad, all of which write it through the editor. */
constexpr Control kControls[] = {
	// ---- the voice, in the vowel section's title row: it changes what
	// every vowel on the pad and every button means ----------------------
	{ kVoice, Kind::Switch, Accent::Amber, 436, 81, 126, 0, "Voice", kTitleSwitchHeight },

	// ---- the glide, beside the buttons that start one -------------------
	{ kGlide, Kind::Knob, Accent::Violet, 388, 524, 64, 48, "Glide" },

	// ---- the formants: a column each, Freq / Width / Level --------------
	{ kF1Freq,      Kind::Knob, Accent::Amber,  formantCol (0) + 2,   kFormantKnobTop, kFormantKnobW, kFormantKnobDia, "Freq"  },
	{ kF1Bandwidth, Kind::Knob, Accent::Amber,  formantCol (0) + 62,  kFormantKnobTop, kFormantKnobW, kFormantKnobDia, "Width" },
	{ kF1Level,     Kind::Knob, Accent::Amber,  formantCol (0) + 120, kFormantKnobTop, kFormantKnobW, kFormantKnobDia, "Level" },
	{ kF2Freq,      Kind::Knob, Accent::Blue,   formantCol (1) + 2,   kFormantKnobTop, kFormantKnobW, kFormantKnobDia, "Freq"  },
	{ kF2Bandwidth, Kind::Knob, Accent::Blue,   formantCol (1) + 62,  kFormantKnobTop, kFormantKnobW, kFormantKnobDia, "Width" },
	{ kF2Level,     Kind::Knob, Accent::Blue,   formantCol (1) + 120, kFormantKnobTop, kFormantKnobW, kFormantKnobDia, "Level" },
	{ kF3Freq,      Kind::Knob, Accent::Violet, formantCol (2) + 2,   kFormantKnobTop, kFormantKnobW, kFormantKnobDia, "Freq"  },
	{ kF3Bandwidth, Kind::Knob, Accent::Violet, formantCol (2) + 62,  kFormantKnobTop, kFormantKnobW, kFormantKnobDia, "Width" },
	{ kF3Level,     Kind::Knob, Accent::Violet, formantCol (2) + 120, kFormantKnobTop, kFormantKnobW, kFormantKnobDia, "Level" },

	// ---- the output -----------------------------------------------------
	{ kMix,        Kind::Knob, Accent::Mint, 786, 538, 72, 34, "Dry / Wet" },
	{ kOutputTrim, Kind::Knob, Accent::Mint, 876, 538, 72, 34, "Trim" },

	// LEVEL COMP: a rotary selector, OFF at -45 and ON at +45, with the
	// make-up it is applying read out under it. The chicken head is
	// centred on the knobs' centre line (555) and the rectangle ends on
	// their bottom (599), so the reading and the name sit on the same
	// lines as Dry / Wet's and Trim's.
	{ kLevelComp,  Kind::Selector, Accent::Mint, 1018, 534, 100, 30, "Level Comp", 65 },
};
constexpr int kNumControls = static_cast<int> (sizeof (kControls) / sizeof (kControls[0]));

//------------------------------------------------------------------------
// The rotary selector - R2Synth's geometry, with VocalFilter's band
//------------------------------------------------------------------------
/** A selector's geometry: its knob, where its positions point (degrees
    from twelve o'clock, clockwise - 45 either side, and straight up for
    the middle of three), the radius its position names hang from, and
    the lines its reading and name sit on. */
struct Selector
{
	double cx, cy, radius, labelRadius;
	int positions;
	double angle[3];
	double readingCentreY, nameCentreY;
};

inline Selector selectorOf (const Control& c, int positions)
{
	const Rect r = controlRect (c);
	Selector s {};
	s.radius = c.diameter / 2.0;
	s.cx = (r.left + r.right) / 2.0;
	s.cy = r.top + kSelectorTopBand + s.radius;
	s.labelRadius = s.radius + 12.0;
	s.positions = (positions < 2) ? 2 : (positions > 3 ? 3 : positions);
	if (s.positions == 2)
	{
		s.angle[0] = -45.0;
		s.angle[1] = 45.0;
		s.angle[2] = 0.0;
	}
	else
	{
		s.angle[0] = -45.0;
		s.angle[1] = 0.0;
		s.angle[2] = 45.0;
	}
	const double textTop = r.bottom - kSelectorReadingBand;
	s.readingCentreY = textTop + kKnobValueH / 2.0;
	s.nameCentreY = textTop + kKnobValueH + kKnobLabelH / 2.0;
	return s;
}

/** A point at `degrees` (from twelve o'clock, clockwise) and `radius`. */
inline void selectorPoint (const Selector& s, double radius, double degrees, double& x, double& y)
{
	const double t = degrees * 3.14159265358979323846 / 180.0;
	x = s.cx + radius * std::sin (t);
	y = s.cy - radius * std::cos (t);
}

/** WHERE A POSITION'S NAME IS PRINTED: hung from labelRadius along its
    angle - above the knob for the middle one, ending short of the line
    on the left, starting past it on the right. (R2Synth's, unchanged.) */
inline Box selectorLabelBox (const Selector& s, int index, const std::string& text)
{
	const double t = s.angle[index];
	double px = 0.0, py = 0.0;
	selectorPoint (s, s.labelRadius, t, px, py);
	const double w = Lettering::width (text, kTextLabel);
	const double h = kTextLabel.height ();
	double left = 0.0, centreY = 0.0;
	if (std::fabs (t) < 10.0)
	{
		left = px - w / 2.0;
		centreY = py - 8.0;
	}
	else if (t < 0.0)
	{
		left = px - 2.0 - w;
		centreY = py + 2.0;
	}
	else
	{
		left = px + 2.0;
		centreY = py + 2.0;
	}
	return { left, centreY - h / 2.0, left + w, centreY + h / 2.0 };
}

/** THE POSITION A CLICK PICKS: on the knob itself, -1 (the knob steps
    on); anywhere else in the selector, the position whose angle is
    nearest the click's. (R2Synth's, unchanged.) */
inline int selectorPositionAt (const Selector& s, double x, double y)
{
	const double dx = x - s.cx, dy = y - s.cy;
	if (dx * dx + dy * dy <= s.radius * s.radius)
		return -1;
	const double t = std::atan2 (dx, -dy) * 180.0 / 3.14159265358979323846;
	int best = 0;
	for (int i = 1; i < s.positions; ++i)
		if (std::fabs (s.angle[i] - t) < std::fabs (s.angle[best] - t))
			best = i;
	return best;
}

//------------------------------------------------------------------------
// The views that are not parameter controls
//------------------------------------------------------------------------

/** The vowel chart. */
constexpr Rect kPadView { 34, 116, 558, 496 };

/** The five vowel buttons, under the chart, and Manual under them. */
constexpr int kVowelButtonLeft  = 34;
constexpr int kVowelButtonTop   = 512;
constexpr int kVowelButtonW     = 58;
constexpr int kVowelButtonH     = 46;
constexpr int kVowelButtonPitch = 64;

constexpr Rect vowelButtonRect (int index)
{
	return { kVowelButtonLeft + index * kVowelButtonPitch, kVowelButtonTop,
	         kVowelButtonLeft + index * kVowelButtonPitch + kVowelButtonW,
	         kVowelButtonTop + kVowelButtonH };
}

constexpr Rect kManualButton { 34, 566, 348, 608 };

/** How far through the current glide the DSP is. */
constexpr Rect kGlideMeter { 466, 538, 558, 576 };

/** The response. */
constexpr Rect kSpectrumView { 606, 104, 1170, 320 };

/** Points per curve on the spectrum: one every three pixels or better. */
constexpr int kSpectrumPoints = 240;

/** The spectrum's axes. 80 Hz..8 kHz covers every formant in both
    tables with room either side. The floor is -66 dB, not the -48 of the
    design, for the reason SpyResponseDisplay's banner gives: where two
    formants meet in antiphase the sum digs a notch to -61.5 dB, and at
    -48 those troughs run off the bottom and the rise between two of them
    reads as the curve turning round. */
constexpr double kSpectrumMinHz = 80.0;
constexpr double kSpectrumMaxHz = 8000.0;
constexpr double kSpectrumTopDb = 12.0;
constexpr double kSpectrumFloorDb = -66.0;

//------------------------------------------------------------------------
// The header
//------------------------------------------------------------------------
/** VOCAL FILTER in the heavy face, with the inline cut. Wider than the
    old wordmark, so the divider and the engine line moved right. */
constexpr Rect kWordmark      { 24,  0,   292, kHeaderHeight };
constexpr int  kWordmarkGap   = 12;     // between VOCAL and FILTER
constexpr int  kHeaderDivider = 306;
constexpr Rect kEngineLine    { 320, 20,  1020, 36 };
constexpr Rect kBypassPill    { 1056, 14, 1184, 42 };

/** The four corner screws. */
constexpr Point kScrews[] = { { 8, 8 }, { kEditorWidth - 8, 8 }, { 8, kEditorHeight - 8 },
                              { kEditorWidth - 8, kEditorHeight - 8 } };

/** The Manual button's hint, beside the word. */
constexpr const char* kManualHint = "drag the chart to leave presets";

//------------------------------------------------------------------------
// Fixed lettering and rules
//------------------------------------------------------------------------
enum class CaptionStyle
{
	Block,    // a block's name, small capitals in its colour
	Note      // ordinary text, grey
};

struct Caption
{
	const char* text;
	CaptionStyle style;
	Accent accent;
	Rect r;
};

constexpr Caption kCaptions[] = {
	{ "F1", CaptionStyle::Block, Accent::Amber,  { formantCol (0) + 2, 390, formantCol (0) + 26,  404 } },
	{ "on the pad", CaptionStyle::Note, Accent::Amber, { formantCol (0) + 28, 390, formantCol (0) + 176, 404 } },
	{ "F2", CaptionStyle::Block, Accent::Blue,   { formantCol (1) + 2, 390, formantCol (1) + 26,  404 } },
	{ "on the pad, inverted", CaptionStyle::Note, Accent::Blue, { formantCol (1) + 28, 390, formantCol (1) + 184, 404 } },
	{ "F3", CaptionStyle::Block, Accent::Violet, { formantCol (2) + 2, 390, formantCol (2) + 26,  404 } },
	{ "knob only", CaptionStyle::Note, Accent::Violet, { formantCol (2) + 28, 390, formantCol (2) + 176, 404 } },

	// The note that used to sit right of the rule moved under the section's
	// title when Level Comp took its place.
	{ "stereo, one model",              CaptionStyle::Note, Accent::Mint, { 606, 564, 770, 578 } },
	{ "Mix + Trim ease in;",            CaptionStyle::Note, Accent::Mint, { 606, 580, 770, 594 } },
	{ "Comp follows the Glide",         CaptionStyle::Note, Accent::Mint, { 606, 596, 770, 610 } },
};
constexpr int kNumCaptions = static_cast<int> (sizeof (kCaptions) / sizeof (kCaptions[0]));

/** Hairlines: between the buttons and the glide, and between the formant
    columns. */
constexpr Rect kRules[] = {
	{ 370, 516, 371, 606 },
	{ formantCol (1) - 8, 388, formantCol (1) - 7, 500 },
	{ formantCol (2) - 8, 388, formantCol (2) - 7, 500 },
	{ 964, 548, 965, 606 },
};

//------------------------------------------------------------------------
// The vowel chart's axes - the parameter ranges, log-scaled
//------------------------------------------------------------------------

/** The plot inside kPadView: room on the left for the F1 figures, at the
    top for the axis captions, at the foot for the F2 figures. */
constexpr int kPadInsetLeft   = 40;
constexpr int kPadInsetRight  = 18;
constexpr int kPadInsetTop    = 30;
constexpr int kPadInsetBottom = 24;

constexpr Rect padPlot ()
{
	return { kPadView.left + kPadInsetLeft, kPadView.top + kPadInsetTop,
	         kPadView.right - kPadInsetRight, kPadView.bottom - kPadInsetBottom };
}

constexpr double kPadF1Min = kFormantFreqMin[0];
constexpr double kPadF1Max = kFormantFreqMax[0];
constexpr double kPadF2Min = kFormantFreqMin[1];
constexpr double kPadF2Max = kFormantFreqMax[1];

/** 0..1 across the plot: 0 at the HIGHEST F2 (front vowels, left). */
inline double padU (double f2Hz)
{
	return std::log (kPadF2Max / f2Hz) / std::log (kPadF2Max / kPadF2Min);
}

/** 0..1 down the plot: 0 at the LOWEST F1 (close vowels, top). */
inline double padV (double f1Hz)
{
	return std::log (f1Hz / kPadF1Min) / std::log (kPadF1Max / kPadF1Min);
}

inline double f2OfPadU (double u)
{
	return kPadF2Max * std::pow (kPadF2Min / kPadF2Max, u);
}

inline double f1OfPadV (double v)
{
	return kPadF1Min * std::pow (kPadF1Max / kPadF1Min, v);
}

/** The vowel circles' radius, which is also how near a click must be to
    pick that vowel rather than start a drag. */
constexpr int kVowelCircleRadius = 17;

/** The IPA symbol each vowel button and circle carries, as UTF-8. */
constexpr const char* const kVowelIpa[kVowelCount] = { "\xC9\x91", "i", "\xC9\xAA", "o", "u" };

/** The order the corner vowels are joined in, round the chart: /i/ /I/
    /a/ /o/ /u/ - front close, front, open, back, back close. */
constexpr int kTrapezoidOrder[kVowelCount] = { 1, 2, 0, 3, 4 };

//------------------------------------------------------------------------
// The arithmetic, asserted rather than trusted
//------------------------------------------------------------------------
static_assert (kVowelRect.bottom + kMargin == kEditorHeight, "the vowel section ends a margin above the bottom");
static_assert (kOutputRect.bottom + kMargin == kEditorHeight, "the output section ends a margin above the bottom");
static_assert (kSpectrumRect.right + kMargin == kEditorWidth, "the right column ends a margin from the right");
static_assert (vowelButtonRect (kVowelCount - 1).right <= kRules[0].left - 8,
               "the five vowel buttons stop short of the rule before the glide");
static_assert (kWordmark.right < kHeaderDivider && kHeaderDivider < kEngineLine.left,
               "the divider sits between the wordmark and the engine line");
static_assert (kEditorWidth == 1200 && kEditorHeight == 640,
               "the panel changed size - update the banner, docs/panel.png and this assert together");

//------------------------------------------------------------------------
} // namespace Layout
} // namespace VocalFilter
