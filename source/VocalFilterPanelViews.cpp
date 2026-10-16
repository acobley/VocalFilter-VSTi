//------------------------------------------------------------------------
// VocalFilter - the "vowel space" panel's views
//------------------------------------------------------------------------

#include "VocalFilterPanelViews.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

using namespace VSTGUI;

namespace VocalFilter {
namespace Panel {

//------------------------------------------------------------------------
namespace {

constexpr double kPi = 3.14159265358979323846;

/** VSTGUI's angles: degrees, 0 at three o'clock, clockwise (y is down). */
CPoint pointAt (const CPoint& centre, double radius, double degrees)
{
	const double t = degrees * kPi / 180.0;
	return CPoint (centre.x + radius * std::cos (t), centre.y + radius * std::sin (t));
}

CRect circleRect (const CPoint& centre, double radius)
{
	return CRect (centre.x - radius, centre.y - radius, centre.x + radius, centre.y + radius);
}

CRect toCRect (const Layout::Rect& r)
{
	return CRect (r.left, r.top, r.right, r.bottom);
}

/** An arc from `from` to `to` degrees, stroked with round caps. */
void strokeArc (CDrawContext* context, const CPoint& centre, double radius, double from, double to,
                const CColor& colour, CCoord width)
{
	if (to - from < 0.5)
		return;
	auto path = owned (context->createGraphicsPath ());
	if (!path)
		return;
	path->addArc (circleRect (centre, radius), from, to, true);
	context->setFrameColor (colour);
	context->setLineWidth (width);
	context->setLineStyle (CLineStyle (CLineStyle::kLineCapRound, CLineStyle::kLineJoinRound));
	context->drawGraphicsPath (path, CDrawContext::kPathStroked);
}

void fillRoundRect (CDrawContext* context, const CRect& r, CCoord radius, const CColor& fill,
                    const CColor* stroke = nullptr)
{
	auto path = owned (context->createGraphicsPath ());
	if (!path)
		return;
	path->addRoundRect (r, radius);
	context->setFillColor (fill);
	context->drawGraphicsPath (path, CDrawContext::kPathFilled);
	if (stroke)
	{
		context->setFrameColor (*stroke);
		context->setLineWidth (1.);
		context->setLineStyle (kLineSolid);
		context->drawGraphicsPath (path, CDrawContext::kPathStroked);
	}
}

void setDashed (CDrawContext* context, CCoord on, CCoord off)
{
	const CCoord dashes[] = { on, off };
	context->setLineStyle (CLineStyle (CLineStyle::kLineCapButt, CLineStyle::kLineJoinMiter, 0., 2, dashes));
}

void setRound (CDrawContext* context)
{
	context->setLineStyle (CLineStyle (CLineStyle::kLineCapRound, CLineStyle::kLineJoinRound));
}

/** A lamp: glowing when on, a dim disc with an edge when off. */
void drawLamp (CDrawContext* context, const CPoint& centre, bool on, const CColor& colour)
{
	if (on)
	{
		context->setFillColor (Palette::withAlpha (colour, 60));
		context->drawEllipse (circleRect (centre, 7.5), kDrawFilled);
		context->setFillColor (colour);
		context->drawEllipse (circleRect (centre, 4.5), kDrawFilled);
	}
	else
	{
		context->setFillColor (Palette::withAlpha (colour, 40));
		context->setFrameColor (Palette::withAlpha (colour, 115));
		context->setLineWidth (1.);
		context->setLineStyle (kLineSolid);
		context->drawEllipse (circleRect (centre, 4.5), kDrawFilledAndStroked);
	}
}

CFontRef fontSegment ()
{
	static SharedPointer<CFontDesc> font = makeOwned<CFontDesc> ("Helvetica Neue", 12, kBoldFace);
	return font;
}

bool inside (const CRect& r, const CPoint& p)
{
	return p.x >= r.left && p.x < r.right && p.y >= r.top && p.y < r.bottom;
}

} // anonymous namespace

//------------------------------------------------------------------------
// Palette and faces
//------------------------------------------------------------------------
namespace Palette {

CColor accent (Layout::Accent a)
{
	switch (a)
	{
		case Layout::Accent::Amber:  return CColor (242, 165,  65, 255);
		case Layout::Accent::Blue:   return CColor ( 86, 168, 245, 255);
		case Layout::Accent::Violet: return CColor (176, 140, 255, 255);
		case Layout::Accent::Mint:   return CColor ( 95, 208, 160, 255);
	}
	return kText;
}

CColor accentAlpha (Layout::Accent a, uint8_t alpha)
{
	return withAlpha (accent (a), alpha);
}

CColor accentInk (Layout::Accent a)
{
	switch (a)
	{
		case Layout::Accent::Amber:  return CColor ( 26,  18,   5, 255);
		case Layout::Accent::Blue:   return CColor (  6,  20,  36, 255);
		case Layout::Accent::Violet: return CColor ( 22,  15,  38, 255);
		case Layout::Accent::Mint:   return CColor (  6,  30,  20, 255);
	}
	return kGround;
}

CColor withAlpha (CColor c, uint8_t alpha)
{
	c.alpha = alpha;
	return c;
}

} // namespace Palette

CFontRef fontWordmark ()
{
	static SharedPointer<CFontDesc> font = makeOwned<CFontDesc> ("Avenir Next Condensed", 23, kBoldFace);
	return font;
}
CFontRef fontTitle ()
{
	static SharedPointer<CFontDesc> font = makeOwned<CFontDesc> ("Helvetica Neue", 13, kBoldFace);
	return font;
}
CFontRef fontSubtitle ()
{
	static SharedPointer<CFontDesc> font = makeOwned<CFontDesc> ("Helvetica Neue", 12);
	return font;
}
CFontRef fontLabel ()
{
	static SharedPointer<CFontDesc> font = makeOwned<CFontDesc> ("Helvetica Neue", 10, kBoldFace);
	return font;
}
CFontRef fontValue ()
{
	static SharedPointer<CFontDesc> font = makeOwned<CFontDesc> ("Menlo", 10);
	return font;
}
CFontRef fontNote ()
{
	static SharedPointer<CFontDesc> font = makeOwned<CFontDesc> ("Helvetica Neue", 11);
	return font;
}
CFontRef fontGlyph ()
{
	static SharedPointer<CFontDesc> font = makeOwned<CFontDesc> ("Helvetica Neue", 16, kBoldFace);
	return font;
}
CFontRef fontGlyphSmall ()
{
	static SharedPointer<CFontDesc> font = makeOwned<CFontDesc> ("Helvetica Neue", 15, kBoldFace);
	return font;
}

std::string upper (const std::string& s)
{
	std::string out (s);
	for (auto& c : out)
		c = static_cast<char> (std::toupper (static_cast<unsigned char> (c)));
	return out;
}

std::string formatHz (double hz)
{
	char buffer[32];
	if (hz >= 1000.0)
		std::snprintf (buffer, sizeof (buffer), "%.2fk", hz / 1000.0);
	else
		std::snprintf (buffer, sizeof (buffer), "%.0f Hz", hz);
	return buffer;
}

//------------------------------------------------------------------------
// Backdrop
//------------------------------------------------------------------------
Backdrop::Backdrop (const CRect& size)
: CView (size)
{
	setMouseEnabled (false);
}

void Backdrop::draw (CDrawContext* context)
{
	using namespace Layout;
	const CRect all = getViewSize ();

	context->setDrawMode (kAntiAliasing | kNonIntegralMode);

	// ---- the ground and the header band --------------------------------
	context->setFillColor (Palette::kGround);
	context->drawRect (all, kDrawFilled);
	context->setFillColor (Palette::kHeader);
	context->drawRect (CRect (all.left, all.top, all.right, all.top + kHeaderHeight), kDrawFilled);
	context->setFillColor (Palette::kBorder);
	context->drawRect (CRect (all.left, kHeaderHeight - 1, all.right, kHeaderHeight), kDrawFilled);

	// ---- the wordmark: VOCAL in the text colour, FILTER in amber --------
	{
		context->setFont (fontWordmark ());
		const CCoord prefixWidth = context->getStringWidth ("VOCAL");
		CRect r = toCRect (kWordmark);
		context->setFontColor (Palette::kText);
		context->drawString ("VOCAL", r, kLeftText, true);
		r.left += prefixWidth + 1;
		context->setFontColor (Palette::accent (Accent::Amber));
		context->drawString ("FILTER", r, kLeftText, true);

		context->setFillColor (Palette::kBorder);
		context->drawRect (CRect (kWordmark.right + 4, 16, kWordmark.right + 5, 40), kDrawFilled);
	}

	// ---- the sections ---------------------------------------------------
	for (const auto& s : kSections)
	{
		CRect r = toCRect (s.r);
		r.inset (0.5, 0.5);
		fillRoundRect (context, r, kSectionRadius, Palette::kSection, &Palette::kBorder);

		const CCoord rowTop = s.r.top + kSectionPadTop;
		const CPoint dot (s.r.left + kSectionPadX + 4, rowTop + kSectionTitleH / 2.);
		context->setFillColor (Palette::accent (s.accent));
		context->drawEllipse (circleRect (dot, 4), kDrawFilled);

		context->setFont (fontTitle ());
		context->setFontColor (Palette::kTitle);
		const CCoord titleLeft = s.r.left + kSectionPadX + 16;
		const CRect titleRect (titleLeft, rowTop, s.r.right - kSectionPadX, rowTop + kSectionTitleH);
		context->drawString (s.title, titleRect, kLeftText, true);

		if (s.subtitle && *s.subtitle)
		{
			const CCoord w = context->getStringWidth (s.title);
			context->setFont (fontSubtitle ());
			context->setFontColor (Palette::kSubtle);
			CRect sub (titleRect);
			sub.left += w + 12;
			context->drawString (s.subtitle, sub, kLeftText, true);
		}
	}

	// ---- the rules -------------------------------------------------------
	context->setFillColor (Palette::kRule);
	for (const auto& r : kRules)
		context->drawRect (toCRect (r), kDrawFilled);

	// ---- the fixed captions ---------------------------------------------
	for (const auto& c : kCaptions)
	{
		const CRect r = toCRect (c.r);
		switch (c.style)
		{
			case CaptionStyle::Block:
				context->setFont (fontTitle ());
				context->setFontColor (Palette::accent (c.accent));
				context->drawString (c.text, r, kLeftText, true);
				break;
			case CaptionStyle::Note:
				context->setFont (fontNote ());
				context->setFontColor (Palette::kSubtle);
				context->drawString (c.text, r, kLeftText, true);
				break;
		}
	}

	setDirty (false);
}

//------------------------------------------------------------------------
// Knob - R2Synth's, unchanged
//------------------------------------------------------------------------
namespace {
/** Pixels of vertical drag for the whole range. */
constexpr float kDragPixels = 200.f;
} // anonymous namespace

Knob::Knob (const CRect& size, IControlListener* listener, int32_t tag,
            Layout::Accent accent, int diameter, bool bipolar)
: CControl (size, listener, tag)
, mAccent (accent)
, mDiameter (diameter)
, mBipolar (bipolar)
{
}

void Knob::draw (CDrawContext* context)
{
	const CRect r = getViewSize ();
	context->setDrawMode (kAntiAliasing | kNonIntegralMode);

	const double v = std::clamp (static_cast<double> (getValueNormalized ()), 0.0, 1.0);
	const CCoord stroke = (mDiameter >= 60) ? 5. : 4.;
	const CPoint centre (r.left + r.getWidth () / 2., r.top + mDiameter / 2.);
	const double radius = mDiameter / 2. - stroke / 2. - 1.;

	// ---- the track and the value arc ------------------------------------
	strokeArc (context, centre, radius, 135.0, 405.0, Palette::kTrack, stroke);
	const double a = angleOf (v);
	if (mBipolar)
		strokeArc (context, centre, radius, std::min (a, 270.0), std::max (a, 270.0),
		           Palette::accent (mAccent), stroke);
	else
		strokeArc (context, centre, radius, 135.0, a, Palette::accent (mAccent), stroke);

	if (mBipolar && std::fabs (a - 270.0) < 0.5)
	{
		context->setFillColor (Palette::accent (mAccent));
		context->drawEllipse (circleRect (pointAt (centre, radius, 270.0), stroke / 2.), kDrawFilled);
	}

	// ---- the cap and its pointer -----------------------------------------
	const double capRadius = radius - stroke / 2. - 4.;
	context->setFillColor (Palette::kCap);
	context->setFrameColor (Palette::kCapEdge);
	context->setLineWidth (1.);
	context->setLineStyle (kLineSolid);
	context->drawEllipse (circleRect (centre, capRadius), kDrawFilledAndStroked);

	context->setFrameColor (Palette::kText);
	context->setLineWidth (2.);
	setRound (context);
	context->drawLine (pointAt (centre, capRadius * 0.2, a), pointAt (centre, capRadius - 2., a));

	// ---- the reading and the label ----------------------------------------
	const CCoord textTop = r.top + mDiameter + Layout::kKnobGap;
	if (mFormatter)
	{
		context->setFont (fontValue ());
		context->setFontColor (mDragging ? Palette::accent (mAccent) : Palette::kText);
		context->drawString (mFormatter (getValueNormalized ()).c_str (),
		                     CRect (r.left - 4, textTop, r.right + 4, textTop + Layout::kKnobValueH),
		                     kCenterText, true);
	}
	if (!mLabel.empty ())
	{
		const CCoord labelTop = textTop + Layout::kKnobValueH;
		context->setFont (fontLabel ());
		context->setFontColor (Palette::kMuted);
		context->drawString (mLabel.c_str (),
		                     CRect (r.left - 4, labelTop, r.right + 4, labelTop + Layout::kKnobLabelH),
		                     kCenterText, true);
	}

	setDirty (false);
}

void Knob::onMouseDownEvent (MouseDownEvent& event)
{
	if (!event.buttonState.isLeft ())
		return;

	// DOUBLE-CLICK: the default. The first click of the pair began and
	// ended an ordinary (empty) drag, so this is one edit of its own.
	if (event.clickCount >= 2)
	{
		beginEdit ();
		setValue (getDefaultValue ());
		valueChanged ();
		endEdit ();
		invalid ();
		event.consumed = true;
		return;
	}

	mDragging = true;
	mLastY = event.mousePosition.y;
	beginEdit ();
	invalid ();
	event.consumed = true;
}

void Knob::onMouseMoveEvent (MouseMoveEvent& event)
{
	if (!mDragging)
		return;

	const CCoord dy = mLastY - event.mousePosition.y;   // up is more
	if (std::fabs (dy) < 0.5)
		return;
	mLastY = event.mousePosition.y;

	const float scale = event.modifiers.has (ModifierKey::Shift) ? 0.1f : 1.f;
	setValueNormalized (std::clamp (
		getValueNormalized () + static_cast<float> (dy) / kDragPixels * scale, 0.f, 1.f));
	valueChanged ();
	invalid ();
	event.consumed = true;
}

void Knob::onMouseUpEvent (MouseUpEvent& event)
{
	if (!mDragging)
		return;
	mDragging = false;
	endEdit ();
	invalid ();
	event.consumed = true;
}

void Knob::onMouseCancelEvent (MouseCancelEvent& event)
{
	if (mDragging)
	{
		mDragging = false;
		endEdit ();
		invalid ();
	}
	event.consumed = true;
}

void Knob::onMouseWheelEvent (MouseWheelEvent& event)
{
	const float step = event.modifiers.has (ModifierKey::Shift) ? 0.001f : 0.01f;
	beginEdit ();
	setValueNormalized (std::clamp (
		getValueNormalized () + static_cast<float> (event.deltaY) * step, 0.f, 1.f));
	valueChanged ();
	endEdit ();
	invalid ();
	event.consumed = true;
}

//------------------------------------------------------------------------
// SegmentSwitch - R2Synth's, unchanged
//------------------------------------------------------------------------
SegmentSwitch::SegmentSwitch (const CRect& size, IControlListener* listener, int32_t tag,
                              Layout::Accent accent)
: CControl (size, listener, tag)
, mAccent (accent)
{
}

int SegmentSwitch::currentIndex () const
{
	if (count () < 2)
		return 0;
	const int i = static_cast<int> (std::floor (getValueNormalized () * (count () - 1) + 0.5f));
	return std::clamp (i, 0, count () - 1);
}

void SegmentSwitch::select (int index)
{
	if (count () < 2)
		return;
	index = std::clamp (index, 0, count () - 1);
	beginEdit ();
	setValueNormalized (static_cast<float> (index) / static_cast<float> (count () - 1));
	valueChanged ();
	endEdit ();
	invalid ();
}

void SegmentSwitch::draw (CDrawContext* context)
{
	const CRect r = getViewSize ();
	context->setDrawMode (kAntiAliasing | kNonIntegralMode);

	CRect outer (r);
	outer.inset (0.5, 0.5);
	fillRoundRect (context, outer, 7., Palette::kSegBack, &Palette::kBorder);

	const int n = count ();
	if (n == 0)
	{
		setDirty (false);
		return;
	}

	const CCoord inner = 2.;
	const CCoord segWidth = (r.getWidth () - 2. * inner) / n;
	const int current = currentIndex ();

	context->setFont (fontSegment ());
	for (int i = 0; i < n; ++i)
	{
		const CRect seg (r.left + inner + i * segWidth, r.top + inner,
		                 r.left + inner + (i + 1) * segWidth, r.bottom - inner);
		if (i == current)
		{
			fillRoundRect (context, seg, 5., Palette::accent (mAccent));
			context->setFontColor (Palette::accentInk (mAccent));
		}
		else
			context->setFontColor (Palette::kMuted);
		context->drawString (mNames[static_cast<size_t> (i)].c_str (), seg, kCenterText, true);
	}

	setDirty (false);
}

void SegmentSwitch::onMouseDownEvent (MouseDownEvent& event)
{
	if (!event.buttonState.isLeft () || count () == 0)
		return;
	const CRect r = getViewSize ();
	const double fraction = (event.mousePosition.x - r.left) / r.getWidth ();
	select (static_cast<int> (fraction * count ()));
	event.consumed = true;
}

void SegmentSwitch::onMouseUpEvent (MouseUpEvent& event)
{
	event.consumed = true;
}

void SegmentSwitch::onMouseWheelEvent (MouseWheelEvent& event)
{
	if (event.deltaY > 0.)
		select (currentIndex () + 1);
	else if (event.deltaY < 0.)
		select (currentIndex () - 1);
	event.consumed = true;
}

//------------------------------------------------------------------------
// VowelPad
//------------------------------------------------------------------------
namespace {
/** A press that moves this far off a vowel circle becomes a drag. */
constexpr CCoord kDragThreshold = 4.;
} // anonymous namespace

VowelPad::VowelPad (const CRect& size, IVowelPadListener* listener)
: CView (size)
, mListener (listener)
{
}

bool VowelPad::setState (const State& s)
{
	const State& o = mState;
	const bool changed =
		s.voice != o.voice || s.selector != o.selector ||
		std::fabs (s.f1 - o.f1) > 0.05 || std::fabs (s.f2 - o.f2) > 0.05 ||
		s.gliding != o.gliding ||
		(s.gliding && (s.fromF1 != o.fromF1 || s.fromF2 != o.fromF2 ||
		               s.toF1 != o.toF1 || s.toF2 != o.toF2));
	mState = s;
	if (changed)
		invalid ();
	return changed;
}

CRect VowelPad::plot () const
{
	const CRect v = getViewSize ();
	return CRect (v.left + Layout::kPadInsetLeft, v.top + Layout::kPadInsetTop,
	              v.right - Layout::kPadInsetRight, v.bottom - Layout::kPadInsetBottom);
}

CPoint VowelPad::pointOf (double f1Hz, double f2Hz) const
{
	const CRect p = plot ();
	const double u = std::clamp (Layout::padU (f2Hz), 0.0, 1.0);
	const double v = std::clamp (Layout::padV (f1Hz), 0.0, 1.0);
	return CPoint (p.left + u * p.getWidth (), p.top + v * p.getHeight ());
}

void VowelPad::hzOf (const CPoint& pt, double& f1Hz, double& f2Hz) const
{
	const CRect p = plot ();
	const double u = std::clamp ((pt.x - p.left) / p.getWidth (), 0.0, 1.0);
	const double v = std::clamp ((pt.y - p.top) / p.getHeight (), 0.0, 1.0);
	f2Hz = Layout::f2OfPadU (u);
	f1Hz = Layout::f1OfPadV (v);
}

int VowelPad::vowelAt (const CPoint& pt) const
{
	const VowelPreset* table = vowelTable (mState.voice);
	int best = -1;
	double bestDistance = Layout::kVowelCircleRadius + 2.;
	for (int i = 0; i < kVowelCount; ++i)
	{
		const CPoint c = pointOf (table[i].formants[0].freqHz, table[i].formants[1].freqHz);
		const double d = std::hypot (pt.x - c.x, pt.y - c.y);
		if (d < bestDistance)
		{
			bestDistance = d;
			best = i;
		}
	}
	return best;
}

void VowelPad::draw (CDrawContext* context)
{
	const CRect r = getViewSize ();
	context->setDrawMode (kAntiAliasing | kNonIntegralMode);

	// ---- the plate and its grid ------------------------------------------
	CRect plate (r);
	plate.inset (0.5, 0.5);
	fillRoundRect (context, plate, 6., Palette::kScreen);
	{
		ConcatClip clip (*context, plate);

		context->setFillColor (Palette::kGrid);
		for (int i = 1; i < 8; ++i)
		{
			const CCoord x = std::round (r.left + i * r.getWidth () / 8.);
			context->drawRect (CRect (x, r.top + 1, x + 1, r.bottom - 1), kDrawFilled);
		}
		for (int j = 1; j < 6; ++j)
		{
			const CCoord y = std::round (r.top + j * r.getHeight () / 6.);
			context->drawRect (CRect (r.left + 1, y, r.right - 1, y + 1), kDrawFilled);
		}

		// ---- the axes, in words and figures ----------------------------------
		context->setFont (fontValue ());
		context->setFontColor (Palette::accent (Layout::Accent::Amber));
		context->drawString ("F1 \xE2\x86\x93  close \xE2\x80\xA6 open",
		                     CRect (r.left + 10, r.top + 8, r.left + 260, r.top + 22), kLeftText, true);
		context->setFontColor (Palette::accent (Layout::Accent::Blue));
		context->drawString ("\xE2\x86\x90 F2  front \xE2\x80\xA6 back",
		                     CRect (r.right - 260, r.top + 8, r.right - 10, r.top + 22), kRightText, true);

		context->setFontColor (Palette::kDim);
		for (double f2 : { 2500.0, 2000.0, 1500.0, 1000.0, 700.0 })
		{
			const CCoord x = pointOf (Layout::kPadF1Min, f2).x;
			char text[16];
			if (f2 >= 1000.0)
				std::snprintf (text, sizeof (text), "%gk", f2 / 1000.0);
			else
				std::snprintf (text, sizeof (text), "%.0f", f2);
			context->drawString (text, CRect (x - 24, r.bottom - 18, x + 24, r.bottom - 4), kCenterText, true);
		}
		for (double f1 : { 300.0, 500.0, 700.0, 1000.0 })
		{
			const CCoord y = pointOf (f1, 1000.0).y;
			char text[16];
			std::snprintf (text, sizeof (text), "%.0f", f1);
			context->drawString (text, CRect (r.left + 8, y - 7, r.left + 40, y + 7), kLeftText, true);
		}

		const int other = (mState.voice == kVoiceMale) ? kVoiceFemale : kVoiceMale;
		const VowelPreset* mine = vowelTable (mState.voice);
		const VowelPreset* theirs = vowelTable (other);

		// ---- the outline round each voice's corner vowels --------------------
		auto outline = [&] (const VowelPreset* table, const CColor& colour, bool dashed) {
			auto path = owned (context->createGraphicsPath ());
			if (!path)
				return;
			for (int n = 0; n < kVowelCount; ++n)
			{
				const auto& f = table[Layout::kTrapezoidOrder[n]].formants;
				const CPoint pt = pointOf (f[0].freqHz, f[1].freqHz);
				if (n == 0)
					path->beginSubpath (pt);
				else
					path->addLine (pt);
			}
			context->setFrameColor (colour);
			context->setLineWidth (1.2);
			if (dashed)
				setDashed (context, 3., 4.);
			else
				setRound (context);
			context->drawGraphicsPath (path, CDrawContext::kPathStroked);
		};
		outline (theirs, Palette::withAlpha (Palette::kText, 42), true);
		outline (mine, Palette::accentAlpha (Layout::Accent::Amber, 90), false);

		// ---- the other voice's vowels: faint hollow dots --------------------
		context->setLineStyle (kLineSolid);
		context->setLineWidth (1.2);
		context->setFrameColor (Palette::withAlpha (Palette::kText, 76));
		for (int i = 0; i < kVowelCount; ++i)
		{
			const auto& f = theirs[i].formants;
			context->drawEllipse (circleRect (pointOf (f[0].freqHz, f[1].freqHz), 5.), kDrawStroked);
		}
		context->setFont (fontValue ());
		context->setFontColor (Palette::kDim);
		context->drawString (other == kVoiceFemale ? "\xE2\x97\x8B female means" : "\xE2\x97\x8B male means",
		                     CRect (r.right - 200, r.bottom - 34, r.right - 10, r.bottom - 20), kRightText, true);

		// ---- this voice's vowels: labelled circles ------------------------------
		for (int i = 0; i < kVowelCount; ++i)
		{
			const auto& f = mine[i].formants;
			const CPoint c = pointOf (f[0].freqHz, f[1].freqHz);
			const bool lit = (i + 1 == mState.selector);
			const CCoord rad = Layout::kVowelCircleRadius;

			context->setFillColor (lit ? Palette::accentAlpha (Layout::Accent::Amber, 56) : Palette::kSegBack);
			context->setFrameColor (lit ? Palette::accent (Layout::Accent::Amber) : Palette::kCapEdge);
			context->setLineWidth (1.2);
			context->setLineStyle (kLineSolid);
			context->drawEllipse (circleRect (c, rad), kDrawFilledAndStroked);

			context->setFont (fontGlyphSmall ());
			context->setFontColor (lit ? Palette::accent (Layout::Accent::Amber) : Palette::kText);
			context->drawString (Layout::kVowelIpa[i], circleRect (c, rad), kCenterText, true);

			context->setFont (fontLabel ());
			context->setFontColor (Palette::kSubtle);
			context->drawString (mine[i].name, CRect (c.x - 30, c.y + rad + 3, c.x + 30, c.y + rad + 15),
			                     kCenterText, true);
		}

		// ---- the glide: where it started, where it is going --------------------
		const CPoint dot = pointOf (mState.f1, mState.f2);
		const CColor violet = Palette::accent (Layout::Accent::Violet);
		if (mState.gliding)
		{
			const CPoint from = pointOf (mState.fromF1, mState.fromF2);
			const CPoint to = pointOf (mState.toF1, mState.toF2);
			context->setFrameColor (Palette::withAlpha (violet, 95));
			context->setLineWidth (2.);
			setDashed (context, 4., 4.);
			context->drawLine (from, to);
			context->setFrameColor (violet);
			context->setLineWidth (3.);
			setRound (context);
			context->drawLine (from, dot);
		}

		// ---- the dot, and what it is set to ----------------------------------------
		context->setFillColor (Palette::withAlpha (Palette::kText, 20));
		context->drawEllipse (circleRect (dot, 18.), kDrawFilled);
		context->setFillColor (Palette::kText);
		context->setFrameColor (Palette::kGround);
		context->setLineWidth (2.);
		context->setLineStyle (kLineSolid);
		context->drawEllipse (circleRect (dot, 9.), kDrawFilledAndStroked);

		{
			const CCoord w = 112., h = 36.;
			// Beside the dot, level with it: above or below would sit on
			// the name of the vowel the dot is resting on.
			CCoord left = dot.x + 24., top = dot.y - h / 2.;
			if (left + w > r.right - 4.)
				left = dot.x - 24. - w;
			top = std::clamp (top, r.top + 26., r.bottom - h - 4.);
			const CRect tip (left, top, left + w, top + h);
			const CColor edge (Palette::kBorder);
			fillRoundRect (context, tip, 6., Palette::withAlpha (Palette::kScreen, 235), &edge);
			context->setFont (fontValue ());
			context->setFontColor (Palette::accent (Layout::Accent::Amber));
			context->drawString (("F1 " + formatHz (mState.f1)).c_str (),
			                     CRect (left + 8, top + 4, left + w - 4, top + 17), kLeftText, true);
			context->setFontColor (Palette::accent (Layout::Accent::Blue));
			context->drawString (("F2 " + formatHz (mState.f2)).c_str (),
			                     CRect (left + 8, top + 18, left + w - 4, top + 31), kLeftText, true);
		}
	}

	// The edge last, so the corners stay clean.
	context->setLineStyle (kLineSolid);
	fillRoundRect (context, plate, 6., CColor (0, 0, 0, 0), &Palette::kScreenEdge);

	setDirty (false);
}

void VowelPad::beginDrag (const CPoint& pt, bool fine)
{
	mPress = Press::Dragging;
	const CRect p = plot ();
	if (fine)
	{
		// Fine drags start from the DOT, not the pointer.
		const CPoint dot = pointOf (mState.f1, mState.f2);
		mDragU = (dot.x - p.left) / p.getWidth ();
		mDragV = (dot.y - p.top) / p.getHeight ();
	}
	else
	{
		mDragU = std::clamp ((pt.x - p.left) / p.getWidth (), 0.0, 1.0);
		mDragV = std::clamp ((pt.y - p.top) / p.getHeight (), 0.0, 1.0);
	}
	mLastPoint = pt;

	if (mListener)
	{
		mListener->padDragBegin ();
		mListener->padDragMove (Layout::f1OfPadV (mDragV), Layout::f2OfPadU (mDragU));
	}
}

void VowelPad::dragTo (const CPoint& pt, bool fine)
{
	const CRect p = plot ();
	if (fine)
	{
		mDragU = std::clamp (mDragU + 0.1 * (pt.x - mLastPoint.x) / p.getWidth (), 0.0, 1.0);
		mDragV = std::clamp (mDragV + 0.1 * (pt.y - mLastPoint.y) / p.getHeight (), 0.0, 1.0);
	}
	else
	{
		mDragU = std::clamp ((pt.x - p.left) / p.getWidth (), 0.0, 1.0);
		mDragV = std::clamp ((pt.y - p.top) / p.getHeight (), 0.0, 1.0);
	}
	mLastPoint = pt;

	if (mListener)
		mListener->padDragMove (Layout::f1OfPadV (mDragV), Layout::f2OfPadU (mDragU));
}

void VowelPad::onMouseDownEvent (MouseDownEvent& event)
{
	if (!event.buttonState.isLeft ())
		return;

	mDownPoint = event.mousePosition;
	mPressedVowel = vowelAt (event.mousePosition);
	if (mPressedVowel >= 0)
		mPress = Press::OnVowel;
	else
		beginDrag (event.mousePosition, event.modifiers.has (ModifierKey::Shift));
	event.consumed = true;
}

void VowelPad::onMouseMoveEvent (MouseMoveEvent& event)
{
	const bool fine = event.modifiers.has (ModifierKey::Shift);
	if (mPress == Press::OnVowel)
	{
		const CPoint d = event.mousePosition - mDownPoint;
		if (std::hypot (d.x, d.y) > kDragThreshold)
			beginDrag (event.mousePosition, fine);
		event.consumed = true;
	}
	else if (mPress == Press::Dragging)
	{
		dragTo (event.mousePosition, fine);
		event.consumed = true;
	}
}

void VowelPad::onMouseUpEvent (MouseUpEvent& event)
{
	if (mPress == Press::OnVowel)
	{
		if (mListener && mPressedVowel >= 0)
			mListener->padVowelClicked (mPressedVowel + 1);
	}
	else if (mPress == Press::Dragging)
	{
		if (mListener)
			mListener->padDragEnd ();
	}
	mPress = Press::None;
	mPressedVowel = -1;
	event.consumed = true;
}

void VowelPad::onMouseCancelEvent (MouseCancelEvent& event)
{
	if (mPress == Press::Dragging && mListener)
		mListener->padDragEnd ();
	mPress = Press::None;
	mPressedVowel = -1;
	event.consumed = true;
}

//------------------------------------------------------------------------
// Spectrum
//------------------------------------------------------------------------
namespace {
/** Grid lines on the frequency axis, and which carry a figure. */
constexpr double kSpectrumDecades[] = { 100.0, 200.0, 300.0, 500.0, 1000.0, 2000.0, 3000.0, 5000.0 };
constexpr double kSpectrumLabelled[] = { 100.0, 300.0, 1000.0, 3000.0 };
} // anonymous namespace

Spectrum::Spectrum (const CRect& size)
: CView (size)
{
	setMouseEnabled (false);
	for (int k = 0; k < kFormantCount; ++k)
		mFormants[k] = kAaaFormants[k];
}

bool Spectrum::setFormants (const FormantSetting* formants, double sampleRate)
{
	if (formants == nullptr)
		return false;

	bool changed = (sampleRate != mSampleRate);
	mSampleRate = sampleRate;
	for (int k = 0; k < kFormantCount; ++k)
	{
		if (mFormants[k].freqHz != formants[k].freqHz ||
		    mFormants[k].bandwidthHz != formants[k].bandwidthHz ||
		    mFormants[k].levelDb != formants[k].levelDb)
			changed = true;
		mFormants[k] = formants[k];
	}
	if (changed)
		invalid ();
	return changed;
}

void Spectrum::setCaption (const std::string& caption)
{
	if (caption != mCaption)
	{
		mCaption = caption;
		invalid ();
	}
}

void Spectrum::setAnnotation (const std::string& annotation)
{
	if (annotation != mAnnotation)
	{
		mAnnotation = annotation;
		invalid ();
	}
}

CRect Spectrum::plot () const
{
	CRect p = getViewSize ();
	p.left += 6;
	p.right -= 6;
	p.top += 24;
	p.bottom -= 18;
	return p;
}

CCoord Spectrum::xOf (double hz, const CRect& p) const
{
	const double t = std::log (hz / Layout::kSpectrumMinHz)
	               / std::log (Layout::kSpectrumMaxHz / Layout::kSpectrumMinHz);
	return p.left + t * p.getWidth ();
}

CCoord Spectrum::yOf (double db, const CRect& p) const
{
	const double t = (Layout::kSpectrumTopDb - db) / (Layout::kSpectrumTopDb - Layout::kSpectrumFloorDb);
	return p.top + t * p.getHeight ();
}

void Spectrum::strokeCurve (CDrawContext* context, const CRect& p,
                            const std::function<double (double)>& dbAt,
                            const CColor& colour, CCoord width, bool fill)
{
	const int n = Layout::kSpectrumPoints;
	auto hzAt = [n] (int i) {
		return Layout::kSpectrumMinHz
		     * std::pow (Layout::kSpectrumMaxHz / Layout::kSpectrumMinHz, static_cast<double> (i) / n);
	};

	if (fill)
	{
		auto area = owned (context->createGraphicsPath ());
		if (area)
		{
			area->beginSubpath (CPoint (p.left, p.bottom));
			for (int i = 0; i <= n; ++i)
			{
				const double f = hzAt (i);
				const double db = std::clamp (dbAt (f), Layout::kSpectrumFloorDb, Layout::kSpectrumTopDb);
				area->addLine (CPoint (xOf (f, p), yOf (db, p)));
			}
			area->addLine (CPoint (p.right, p.bottom));
			area->closeSubpath ();
			context->setFillColor (Palette::withAlpha (colour, 16));
			context->drawGraphicsPath (area, CDrawContext::kPathFilled);
		}
	}

	// Where the line leaves the scale it is drawn DOWN TO THE BOTTOM EDGE
	// and broken there, and it comes back up from the edge - never run
	// along the floor, and never stopped short at the last sample that
	// happened to be on the scale, which left an arc floating mid-plot
	// that read as the curve turning back up (docs/README.md).
	auto path = owned (context->createGraphicsPath ());
	if (!path)
		return;
	bool open = false;
	bool any = false;
	for (int i = 0; i <= n; ++i)
	{
		const double f = hzAt (i);
		const double db = dbAt (f);
		const CCoord x = xOf (f, p);
		if (!std::isfinite (db) || db < Layout::kSpectrumFloorDb)
		{
			if (open)
				path->addLine (CPoint (x, p.bottom));
			open = false;
			continue;
		}
		const CPoint pt (x, yOf (std::min (db, Layout::kSpectrumTopDb), p));
		if (open)
			path->addLine (pt);
		else
		{
			if (i > 0)
			{
				path->beginSubpath (CPoint (x, p.bottom));
				path->addLine (pt);
			}
			else
				path->beginSubpath (pt);
		}
		open = true;
		any = true;
	}
	if (!any)
		return;
	context->setFrameColor (colour);
	context->setLineWidth (width);
	setRound (context);
	context->drawGraphicsPath (path, CDrawContext::kPathStroked);
}

void Spectrum::draw (CDrawContext* context)
{
	const CRect r = getViewSize ();
	const CRect p = plot ();
	context->setDrawMode (kAntiAliasing | kNonIntegralMode);

	CRect plate (r);
	plate.inset (0.5, 0.5);
	fillRoundRect (context, plate, 6., Palette::kScreen);
	{
		ConcatClip clip (*context, plate);

		// ---- the grid: frequency lines, and every 12 dB ----------------------
		context->setFillColor (Palette::kGrid);
		for (double f : kSpectrumDecades)
		{
			const CCoord x = std::round (xOf (f, p));
			context->drawRect (CRect (x, r.top + 1, x + 1, r.bottom - 1), kDrawFilled);
		}
		for (double db = 0.0; db >= Layout::kSpectrumFloorDb; db -= 12.0)
		{
			const CCoord y = std::round (yOf (db, p));
			context->drawRect (CRect (r.left + 1, y, r.right - 1, y + 1), kDrawFilled);
		}

		context->setFont (fontValue ());
		context->setFontColor (Palette::kDim);
		for (double f : kSpectrumLabelled)
		{
			const CCoord x = xOf (f, p);
			char text[16];
			if (f >= 1000.0)
				std::snprintf (text, sizeof (text), "%gk", f / 1000.0);
			else
				std::snprintf (text, sizeof (text), "%.0f", f);
			context->drawString (text, CRect (x - 24, r.bottom - 16, x + 24, r.bottom - 3), kCenterText, true);
		}

		context->setFontColor (Palette::kSubtle);
		const CRect top (r.left + 10, r.top + 5, r.right - 10, r.top + 19);
		if (!mCaption.empty ())
			context->drawString (mCaption.c_str (), top, kLeftText, true);
		if (!mAnnotation.empty ())
			context->drawString (mAnnotation.c_str (), top, kRightText, true);

		// ---- the three branches, then their sum over them ----------------------
		const double sr = mSampleRate;
		for (int k = 0; k < kFormantCount; ++k)
		{
			const FormantSetting f = mFormants[k];
			strokeCurve (context, p, [f, sr] (double hz) {
				const double m = bandpassMagnitude (f.freqHz, f.bandwidthHz, hz, sr);
				return f.levelDb + 20.0 * std::log10 (std::max (m, 1e-9));
			}, Palette::accentAlpha (Layout::formantAccent (k), 150), 1.2, false);
		}

		const FormantSetting* bank = mFormants;
		strokeCurve (context, p, [bank, sr] (double hz) {
			return 20.0 * std::log10 (std::max (bankMagnitude (bank, kFormantCount, hz, sr), 1e-9));
		}, Palette::kText, 2.2, true);

		// ---- a handle on each formant, its width as a bracket under it ----------
		for (int k = 0; k < kFormantCount; ++k)
		{
			const FormantSetting f = mFormants[k];
			const CColor colour = Palette::accent (Layout::formantAccent (k));
			const double peakDb = f.levelDb + 20.0 * std::log10 (std::max (
				bandpassMagnitude (f.freqHz, f.bandwidthHz, f.freqHz, sr), 1e-9));
			const CPoint h (xOf (f.freqHz, p), yOf (std::clamp (peakDb, Layout::kSpectrumFloorDb,
			                                                    Layout::kSpectrumTopDb), p));

			context->setFrameColor (Palette::withAlpha (colour, 80));
			context->setLineWidth (1.);
			setDashed (context, 2., 3.);
			context->drawLine (h, CPoint (h.x, p.bottom));

			const CCoord lo = xOf (std::max (Layout::kSpectrumMinHz, f.freqHz - f.bandwidthHz / 2.), p);
			const CCoord hi = xOf (std::min (Layout::kSpectrumMaxHz, f.freqHz + f.bandwidthHz / 2.), p);
			const CCoord yb = h.y + 9.;
			context->setFrameColor (colour);
			context->setLineWidth (2.);
			context->setLineStyle (kLineSolid);
			context->drawLine (CPoint (lo, yb), CPoint (hi, yb));
			context->setLineWidth (1.5);
			context->drawLine (CPoint (lo, yb - 4.), CPoint (lo, yb + 4.));
			context->drawLine (CPoint (hi, yb - 4.), CPoint (hi, yb + 4.));

			context->setFillColor (Palette::kScreen);
			context->setLineWidth (2.);
			context->drawEllipse (circleRect (h, 6.5), kDrawFilledAndStroked);
		}
	}

	context->setLineStyle (kLineSolid);
	fillRoundRect (context, plate, 6., CColor (0, 0, 0, 0), &Palette::kScreenEdge);

	setDirty (false);
}

//------------------------------------------------------------------------
// VowelButton
//------------------------------------------------------------------------
VowelButton::VowelButton (const CRect& size, const std::string& glyph, const std::string& name)
: CView (size)
, mGlyph (glyph)
, mName (name)
{
}

void VowelButton::setSelected (bool selected)
{
	if (selected != mSelected)
	{
		mSelected = selected;
		invalid ();
	}
}

void VowelButton::draw (CDrawContext* context)
{
	const CRect r = getViewSize ();
	context->setDrawMode (kAntiAliasing | kNonIntegralMode);

	const CColor amber = Palette::accent (Layout::Accent::Amber);
	const bool down = mPressed && mInside;
	CRect box (r);
	box.inset (0.5, 0.5);
	if (mSelected)
		fillRoundRect (context, box, 8., down ? Palette::withAlpha (amber, 200) : amber, &amber);
	else
	{
		const CColor edge = down ? Palette::kCapEdge : Palette::kBorder;
		fillRoundRect (context, box, 8., down ? Palette::kCap : Palette::kSegBack, &edge);
	}

	const CColor ink = mSelected ? Palette::accentInk (Layout::Accent::Amber) : Palette::kText;
	context->setFont (fontGlyph ());
	context->setFontColor (ink);
	context->drawString (mGlyph.c_str (), CRect (r.left, r.top + 5, r.right, r.top + 26), kCenterText, true);
	context->setFont (fontLabel ());
	context->drawString (mName.c_str (), CRect (r.left, r.bottom - 19, r.right, r.bottom - 6), kCenterText, true);

	setDirty (false);
}

void VowelButton::onMouseDownEvent (MouseDownEvent& event)
{
	if (!event.buttonState.isLeft ())
		return;
	mPressed = true;
	mInside = true;
	invalid ();
	event.consumed = true;
}

void VowelButton::onMouseMoveEvent (MouseMoveEvent& event)
{
	if (!mPressed)
		return;
	const bool in = inside (getViewSize (), event.mousePosition);
	if (in != mInside)
	{
		mInside = in;
		invalid ();
	}
	event.consumed = true;
}

void VowelButton::onMouseUpEvent (MouseUpEvent& event)
{
	if (!mPressed)
		return;
	const bool fire = inside (getViewSize (), event.mousePosition);
	mPressed = false;
	mInside = false;
	invalid ();
	event.consumed = true;
	if (fire && mHandler)
		mHandler ();
}

void VowelButton::onMouseCancelEvent (MouseCancelEvent& event)
{
	mPressed = false;
	mInside = false;
	invalid ();
	event.consumed = true;
}

//------------------------------------------------------------------------
// ManualButton
//------------------------------------------------------------------------
ManualButton::ManualButton (const CRect& size, const std::string& hint)
: CView (size)
, mHint (hint)
{
}

void ManualButton::setLit (bool lit)
{
	if (lit != mLit)
	{
		mLit = lit;
		invalid ();
	}
}

void ManualButton::draw (CDrawContext* context)
{
	const CRect r = getViewSize ();
	context->setDrawMode (kAntiAliasing | kNonIntegralMode);

	const CColor amber = Palette::accent (Layout::Accent::Amber);
	const bool down = mPressed && mInside;
	CRect box (r);
	box.inset (0.5, 0.5);
	const CColor edge = mLit ? Palette::accentAlpha (Layout::Accent::Amber, 150)
	                         : (down ? Palette::kCapEdge : Palette::kBorder);
	fillRoundRect (context, box, 8., down ? Palette::kCap : Palette::kSegBack, &edge);

	const CPoint lamp (r.left + 18., (r.top + r.bottom) / 2.);
	drawLamp (context, lamp, mLit, amber);

	context->setFont (fontLabel ());
	context->setFontColor (mLit ? amber : Palette::kMuted);
	context->drawString ("MANUAL", CRect (r.left + 32, r.top, r.left + 140, r.bottom), kLeftText, true);

	context->setFont (fontNote ());
	context->setFontColor (Palette::kSubtle);
	context->drawString (mHint.c_str (), CRect (r.left + 100, r.top, r.right - 14, r.bottom), kRightText, true);

	setDirty (false);
}

void ManualButton::onMouseDownEvent (MouseDownEvent& event)
{
	if (!event.buttonState.isLeft ())
		return;
	mPressed = true;
	mInside = true;
	invalid ();
	event.consumed = true;
}

void ManualButton::onMouseMoveEvent (MouseMoveEvent& event)
{
	if (!mPressed)
		return;
	const bool in = inside (getViewSize (), event.mousePosition);
	if (in != mInside)
	{
		mInside = in;
		invalid ();
	}
	event.consumed = true;
}

void ManualButton::onMouseUpEvent (MouseUpEvent& event)
{
	if (!mPressed)
		return;
	const bool fire = inside (getViewSize (), event.mousePosition);
	mPressed = false;
	mInside = false;
	invalid ();
	event.consumed = true;
	if (fire && mHandler)
		mHandler ();
}

void ManualButton::onMouseCancelEvent (MouseCancelEvent& event)
{
	mPressed = false;
	mInside = false;
	invalid ();
	event.consumed = true;
}

//------------------------------------------------------------------------
// GlideMeter
//------------------------------------------------------------------------
GlideMeter::GlideMeter (const CRect& size)
: CView (size)
{
	setMouseEnabled (false);
}

void GlideMeter::setProgress (double progress, const std::string& text)
{
	if (std::fabs (progress - mProgress) > 0.004 || text != mText)
	{
		mProgress = progress;
		mText = text;
		invalid ();
	}
}

void GlideMeter::draw (CDrawContext* context)
{
	const CRect r = getViewSize ();
	context->setDrawMode (kAntiAliasing | kNonIntegralMode);

	const CColor violet = Palette::accent (Layout::Accent::Violet);
	const CRect track (r.left, r.top + 2, r.right, r.top + 10);
	fillRoundRect (context, track, 4., Palette::kTrack);
	if (mProgress > 0.0)
	{
		CRect bar (track);
		bar.right = track.left + std::max (8.0, track.getWidth () * std::min (mProgress, 1.0));
		fillRoundRect (context, bar, 4., violet);
	}

	context->setFont (fontValue ());
	context->setFontColor (mProgress >= 0.0 ? violet : Palette::kSubtle);
	context->drawString (mText.c_str (), CRect (r.left - 6, r.top + 16, r.right + 6, r.top + 30), kCenterText, true);

	setDirty (false);
}

//------------------------------------------------------------------------
// BypassPill
//------------------------------------------------------------------------
BypassPill::BypassPill (const CRect& size, IControlListener* listener, int32_t tag)
: CControl (size, listener, tag)
{
}

void BypassPill::draw (CDrawContext* context)
{
	const CRect r = getViewSize ();
	context->setDrawMode (kAntiAliasing | kNonIntegralMode);

	const bool on = getValueNormalized () >= 0.5f;
	CRect pill (r);
	pill.inset (0.5, 0.5);
	const CColor edge = on ? Palette::withAlpha (Palette::kLampOn, 170) : Palette::kBorder;
	fillRoundRect (context, pill, pill.getHeight () / 2., Palette::kSegBack, &edge);

	drawLamp (context, CPoint (r.left + 20., (r.top + r.bottom) / 2.), on,
	          on ? Palette::kLampOn : Palette::accent (Layout::Accent::Mint));

	context->setFont (fontLabel ());
	context->setFontColor (on ? Palette::kLampOn : Palette::kMuted);
	context->drawString (on ? "BYPASS  on" : "BYPASS  off", CRect (r.left + 34, r.top, r.right - 10, r.bottom),
	                     kLeftText, true);

	setDirty (false);
}

void BypassPill::onMouseDownEvent (MouseDownEvent& event)
{
	if (!event.buttonState.isLeft ())
		return;
	beginEdit ();
	setValueNormalized (getValueNormalized () >= 0.5f ? 0.f : 1.f);
	valueChanged ();
	endEdit ();
	invalid ();
	event.consumed = true;
}

void BypassPill::onMouseUpEvent (MouseUpEvent& event)
{
	event.consumed = true;
}

//------------------------------------------------------------------------
} // namespace Panel
} // namespace VocalFilter
