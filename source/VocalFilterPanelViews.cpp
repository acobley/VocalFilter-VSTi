//------------------------------------------------------------------------
// VocalFilter - the "vowel space" panel's views (the black panel)
//------------------------------------------------------------------------

#include "VocalFilterPanelViews.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <map>

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

/** The layout's angles: degrees from twelve o'clock, clockwise. */
CPoint pointFromTop (const CPoint& centre, double radius, double degrees)
{
	return pointAt (centre, radius, degrees - 90.0);
}

CRect circleRect (const CPoint& centre, double radius)
{
	return CRect (centre.x - radius, centre.y - radius, centre.x + radius, centre.y + radius);
}

CRect toCRect (const Layout::Rect& r)
{
	return CRect (r.left, r.top, r.right, r.bottom);
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

/** A lamp: a red glow when on, a dark red disc with a black edge when
    off. */
void drawLamp (CDrawContext* context, const CPoint& centre, double radius, bool on)
{
	if (on)
	{
		context->setFillColor (Palette::withAlpha (Palette::kLampOn, 64));
		context->drawEllipse (circleRect (centre, radius + 3.0), kDrawFilled);
	}
	context->setFillColor (on ? Palette::kLampOn : Palette::kLampOff);
	context->setFrameColor (CColor (0, 0, 0, 255));
	context->setLineWidth (0.6);
	context->setLineStyle (kLineSolid);
	context->drawEllipse (circleRect (centre, radius), kDrawFilledAndStroked);
}

CLineStyle roundLine ()
{
	return CLineStyle (CLineStyle::kLineCapRound, CLineStyle::kLineJoinRound);
}

void strokeLine (CDrawContext* context, const CPoint& from, const CPoint& to, const CColor& colour, CCoord width)
{
	context->setFrameColor (colour);
	context->setLineWidth (width);
	context->setLineStyle (roundLine ());
	context->drawLine (from, to);
}

void fillCircle (CDrawContext* context, const CPoint& centre, double radius, const CColor& fill)
{
	context->setFillColor (fill);
	context->drawEllipse (circleRect (centre, radius), kDrawFilled);
}

void strokeCircle (CDrawContext* context, const CPoint& centre, double radius, const CColor& colour, CCoord width)
{
	context->setFrameColor (colour);
	context->setLineWidth (width);
	context->setLineStyle (kLineSolid);
	context->drawEllipse (circleRect (centre, radius), kDrawStroked);
}

/** A black knob's cap: lit from the top left, a near-black at the rim. */
void fillCap (CDrawContext* context, CGraphicsPath* path, const CPoint& centre, double radius)
{
	auto gradient = owned (CGradient::create (0.0, 1.0, CColor (58, 58, 58, 255), CColor (5, 5, 5, 255)));
	if (!gradient)
		return;
	gradient->addColorStop (0.6, CColor (22, 22, 22, 255));
	context->fillRadialGradient (path, *gradient, centre, radius * 1.4,
	                             CPoint (-radius * 0.25, -radius * 0.35));
}

/** A key: a soft shadow under it, a black top with a white frame-line,
    lit faintly from above. */
void drawKey (CDrawContext* context, const CRect& r, bool down)
{
	CRect shadow (r);
	shadow.offset (0, 1.5);
	fillRoundRect (context, shadow, 4., CColor (0, 0, 0, 90));
	CRect box (r);
	box.inset (0.5, 0.5);
	if (down)
		box.offset (0, 0.75);
	fillRoundRect (context, box, 4., down ? Palette::kKeyDown : Palette::kKey, &Palette::kKeyEdge);
	auto path = owned (context->createGraphicsPath ());
	auto gradient = owned (CGradient::create (0.0, 1.0, CColor (255, 255, 255, 18), CColor (0, 0, 0, 46)));
	if (path && gradient)
	{
		path->addRoundRect (box, 4.);
		context->fillLinearGradient (path, *gradient, CPoint (box.left, box.top), CPoint (box.left, box.bottom));
	}
}

/** A screen's glass, inside its view: a black cut-out with a grey edge,
    the glass 2.5 px in. Returns the glass. */
CRect drawGlass (CDrawContext* context, const CRect& r)
{
	CRect bezel (r);
	bezel.inset (0.5, 0.5);
	fillRoundRect (context, bezel, 4., Palette::kBezel, &Palette::kBezelEdge);
	CRect glass (r);
	glass.inset (2.5, 2.5);
	fillRoundRect (context, glass, 2.5, Palette::kScreen);
	return glass;
}

//------------------------------------------------------------------------
// The wear (PanelWear.h): the marks as paths
//------------------------------------------------------------------------
CColor wearColour (const Wear::Rgb& c, uint8_t alpha)
{
	return CColor (c.r, c.g, c.b, alpha);
}

/** Does the box (l, t, r, b) touch the area being drawn? */
bool touches (const CRect& clip, double l, double t, double r, double b)
{
	return r >= clip.left && l <= clip.right && b >= clip.top && t <= clip.bottom;
}

bool lineTouches (const CRect& clip, const std::vector<Wear::Pt>& pts, double pad)
{
	if (pts.empty ())
		return false;
	double l = pts[0].x, t = pts[0].y, r = l, b = t;
	for (const auto& p : pts)
	{
		l = std::min (l, p.x); r = std::max (r, p.x);
		t = std::min (t, p.y); b = std::max (b, p.y);
	}
	return touches (clip, l - pad, t - pad, r + pad, b + pad);
}

void polyline (CDrawContext* context, const std::vector<Wear::Pt>& pts, size_t from, size_t to,
               const CColor& colour, CCoord width)
{
	if (to <= from || to >= pts.size () + 1)
		return;
	auto path = owned (context->createGraphicsPath ());
	if (!path)
		return;
	path->beginSubpath (CPoint (pts[from].x, pts[from].y));
	for (size_t i = from + 1; i < to; ++i)
		path->addLine (CPoint (pts[i].x, pts[i].y));
	context->setFrameColor (colour);
	context->setLineWidth (width);
	context->setLineStyle (roundLine ());
	context->drawGraphicsPath (path, CDrawContext::kPathStroked);
}

/** A hairline: half strength in its first and last thirds, so it fades
    in and out. */
void wearLine (CDrawContext* context, const Wear::Line& line, const Wear::Rgb& rgb)
{
	const size_t n = line.pts.size ();
	if (n < 2)
		return;
	const size_t a = std::max<size_t> (1, n / 3), b = std::max (a + 1, n - n / 3);
	const uint8_t half = static_cast<uint8_t> (line.alpha / 2);
	polyline (context, line.pts, 0, a + 1, wearColour (rgb, half), line.width);
	polyline (context, line.pts, a, std::min (b + 1, n), wearColour (rgb, line.alpha), line.width);
	if (b < n - 1)
		polyline (context, line.pts, b, n, wearColour (rgb, half), line.width);
}

/** A soft round patch: `alpha` at the centre, none at the edge. */
void softDisc (CDrawContext* context, const CPoint& centre, double radius, const Wear::Rgb& rgb, uint8_t alpha)
{
	auto path = owned (context->createGraphicsPath ());
	auto gradient = owned (CGradient::create (0.0, 1.0, wearColour (rgb, alpha), wearColour (rgb, 0)));
	if (!path || !gradient)
		return;
	gradient->addColorStop (0.5, wearColour (rgb, static_cast<uint8_t> (alpha * 0.55)));
	path->addEllipse (circleRect (centre, radius));
	context->fillRadialGradient (path, *gradient, centre, radius);
}

/** Dust round a knob's foot: a ring that fades both ways from `peak`. */
void footRing (CDrawContext* context, const Wear::FootRing& f)
{
	auto path = owned (context->createGraphicsPath ());
	const CColor clear = wearColour (Wear::kDust, 0);
	auto gradient = owned (CGradient::create (0.0, 1.0, clear, clear));
	if (!path || !gradient)
		return;
	gradient->addColorStop (f.inner / f.outer, clear);
	gradient->addColorStop (f.peak / f.outer, wearColour (Wear::kDust, f.alpha));
	const CPoint centre (f.cx, f.cy);
	path->addEllipse (circleRect (centre, f.outer));
	context->fillRadialGradient (path, *gradient, centre, f.outer);
}

/** Dust drifted against a box edge: fades from the edge inwards. */
void drift (CDrawContext* context, const Wear::Drift& d)
{
	auto path = owned (context->createGraphicsPath ());
	auto gradient = owned (CGradient::create (0.0, 1.0, wearColour (Wear::kDust, d.alpha), wearColour (Wear::kDust, 0)));
	if (!path || !gradient)
		return;
	const CRect r (d.r.left, d.r.top, d.r.right, d.r.bottom);
	path->addRect (r);
	const CPoint mid ((r.left + r.right) / 2., (r.top + r.bottom) / 2.);
	CPoint from = mid, to = mid;
	if (d.fromX < 0) { from.x = r.left;  to.x = r.right; }
	if (d.fromX > 0) { from.x = r.right; to.x = r.left; }
	if (d.fromY < 0) { from.y = r.top;    to.y = r.bottom; }
	if (d.fromY > 0) { from.y = r.bottom; to.y = r.top; }
	context->fillLinearGradient (path, *gradient, from, to);
}

/** The glyphs, parsed once each. */
const std::vector<Lettering::Command>& commandsOf (const Lettering::Glyph& glyph)
{
	static std::map<const char*, std::vector<Lettering::Command>> cache;
	auto it = cache.find (glyph.path);
	if (it == cache.end ())
		it = cache.emplace (glyph.path, Lettering::parse (glyph.path)).first;
	return it->second;
}

bool inside (const CRect& r, const CPoint& p)
{
	return p.x >= r.left && p.x < r.right && p.y >= r.top && p.y < r.bottom;
}

/** The dust, a fibre and a fingerprint on a screen's glass - over the
    picture, because they are on the glass, not in it. In panel units, so
    drawn as they are. */
void drawGlassWear (CDrawContext* context, const Wear::GlassWear& w)
{
	for (const auto& p : w.prints)
		for (const auto& ridge : p.ridges)
			polyline (context, ridge.pts, 0, ridge.pts.size (), wearColour (Wear::kPrint, ridge.alpha), ridge.width);
	for (const auto& d : w.dust)
		fillCircle (context, CPoint (d.x, d.y), d.r, wearColour (Wear::kGlassDust, d.alpha));
	for (const auto& l : w.fibres)
		wearLine (context, l, Wear::kGlassDust);
}

} // anonymous namespace

//------------------------------------------------------------------------
// Palette and lettering
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

std::string upper (const std::string& s)
{
	std::string out (s);
	for (auto& c : out)
		c = static_cast<char> (std::toupper (static_cast<unsigned char> (c)));
	return out;
}

void drawLettering (CDrawContext* context, const std::string& text, const Lettering::Style& style,
                    const CColor& colour, CCoord x, CCoord centreY, Align align, double strokeUnits)
{
	double endUnits = 0.0;
	const auto placed = Lettering::layout (text, style, &endUnits);
	if (placed.empty ())
		return;

	const double s = style.scale ();
	const double width = Lettering::width (text, style);
	double left = x;
	if (align == Align::Centre)
		left = x - width / 2.0;
	else if (align == Align::Right)
		left = x - width;
	const double top = centreY - style.height () / 2.0;
	const double half = style.weight / 2.0;

	auto path = owned (context->createGraphicsPath ());
	if (!path)
		return;

	for (const auto& p : placed)
	{
		const double ox = left + (p.x + half) * s;
		const double oy = top + half * s;
		auto at = [&] (double gx, double gy) { return CPoint (ox + gx * s, oy + gy * s); };

		CPoint current, start;
		for (const auto& c : commandsOf (Lettering::glyphOrDot (p.code)))
		{
			switch (c.op)
			{
				case 'M':
					current = start = at (c.x, c.y);
					path->beginSubpath (current);
					break;
				case 'L':
					current = at (c.x, c.y);
					path->addLine (current);
					break;
				case 'Q':
				{
					// The quadratic as a cubic: c1 = p0 + 2/3 (q - p0),
					// c2 = p2 + 2/3 (q - p2).
					const CPoint q = at (c.x1, c.y1);
					const CPoint end = at (c.x, c.y);
					const CPoint c1 (current.x + 2.0 / 3.0 * (q.x - current.x), current.y + 2.0 / 3.0 * (q.y - current.y));
					const CPoint c2 (end.x + 2.0 / 3.0 * (q.x - end.x), end.y + 2.0 / 3.0 * (q.y - end.y));
					path->addBezierCurve (c1, c2, end);
					current = end;
					break;
				}
				case 'Z':
					path->closeSubpath ();
					current = start;
					break;
				default:
					break;
			}
		}
	}

	context->setFrameColor (colour);
	context->setLineWidth ((strokeUnits > 0.0 ? strokeUnits : style.weight) * s);
	context->setLineStyle (roundLine ());
	context->drawGraphicsPath (path, CDrawContext::kPathStroked);
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
, mWear (Wear::panelWear ())
{
	setMouseEnabled (false);
}

void Backdrop::drawWear (CDrawContext* context)
{
	CRect clip;
	context->getClipRect (clip);

	// ---- the haze and the drifts: soft, underneath the sharper marks -------
	for (const auto& b : mWear.haze)
		if (touches (clip, b.x - b.r, b.y - b.r, b.x + b.r, b.y + b.r))
			softDisc (context, CPoint (b.x, b.y), b.r, Wear::kDust, b.alpha);
	for (const auto& d : mWear.drifts)
		if (touches (clip, d.r.left, d.r.top, d.r.right, d.r.bottom))
			drift (context, d);
	for (const auto& f : mWear.rings)
		if (touches (clip, f.cx - f.outer, f.cy - f.outer, f.cx + f.outer, f.cy + f.outer))
			footRing (context, f);

	// ---- hairlines through the paint, and the printing ------------------------
	for (const auto& l : mWear.scratches)
		if (lineTouches (clip, l.pts, l.width))
			wearLine (context, l, Wear::kScratch);

	// ---- specks and fibres ------------------------------------------------------
	for (const auto& sp : mWear.specks)
		if (touches (clip, sp.x - sp.r, sp.y - sp.r, sp.x + sp.r, sp.y + sp.r))
			fillCircle (context, CPoint (sp.x, sp.y), sp.r, wearColour (Wear::kSpeck, sp.alpha));
	for (const auto& l : mWear.fibres)
		if (lineTouches (clip, l.pts, l.width))
			wearLine (context, l, Wear::kSpeck);
}

void Backdrop::draw (CDrawContext* context)
{
	using namespace Layout;
	const CRect all = getViewSize ();

	context->setDrawMode (kAntiAliasing | kNonIntegralMode);

	// ---- the ground, the header band and the white rule under it --------
	context->setFillColor (Palette::kGround);
	context->drawRect (all, kDrawFilled);
	context->setFillColor (Palette::kHeader);
	context->drawRect (CRect (all.left, all.top, all.right, all.top + kHeaderHeight), kDrawFilled);
	context->setFillColor (Palette::kHeaderRule);
	context->drawRect (CRect (all.left, kHeaderHeight - 2, all.right, kHeaderHeight), kDrawFilled);

	// ---- the corner screws ------------------------------------------------
	for (const auto& sc : kScrews)
	{
		const CPoint c (sc.x, sc.y);
		fillCircle (context, c, 3.2, Palette::kScrew);
		strokeCircle (context, c, 3.2, CColor (0, 0, 0, 255), 0.6);
		strokeLine (context, CPoint (c.x - 2.2, c.y - 2.2), CPoint (c.x + 2.2, c.y + 2.2), CColor (34, 34, 34, 255), 0.8);
	}

	// ---- the wordmark: VOCAL FILTER, heavy, with the inline cut -----------
	{
		const double centreY = kHeaderHeight / 2.0;
		const double second = kWordmark.left + Lettering::width ("VOCAL", kTextWordmark) + kWordmarkGap;
		for (int pass = 0; pass < 2; ++pass)
		{
			const CColor ink = pass ? Palette::kHeader : Palette::kInk;
			const double stroke = pass ? 0.32 : -1.0;
			drawLettering (context, "VOCAL", kTextWordmark, ink, kWordmark.left, centreY, Align::Left, stroke);
			drawLettering (context, "FILTER", kTextWordmark, ink, second, centreY, Align::Left, stroke);
		}
		context->setFillColor (Palette::kDivider);
		context->drawRect (CRect (kHeaderDivider, 16, kHeaderDivider + 1, 40), kDrawFilled);
	}

	// ---- the four sections: boxed in white, the title in its own box -----
	for (const auto& sec : kSections)
	{
		CRect r = toCRect (sec.r);
		r.inset (0.5, 0.5);
		fillRoundRect (context, r, kSectionRadius, Palette::kSection, &Palette::kSectionEdge);

		const Box b = titleBox (sec);
		const CColor edge = Palette::kSectionEdge;
		fillRoundRect (context, CRect (b.left, b.top, b.right, b.bottom), 1.5, Palette::kSection, &edge);
		const double centre = (b.top + b.bottom) / 2.0;
		drawLettering (context, sec.title, kTextTitle, Palette::kInk, b.left + 9.0, centre);
		if (sec.subtitle && *sec.subtitle)
			drawLettering (context, sec.subtitle, kTextSubtitle, Palette::kInkSoft, subtitleLeft (sec), centre);
	}

	// ---- the rules -------------------------------------------------------
	context->setFillColor (Palette::kRule);
	for (const auto& r : kRules)
		context->drawRect (toCRect (r), kDrawFilled);

	// ---- the fixed captions: F1 / F2 / F3 in their colours, notes grey --
	for (const auto& c : kCaptions)
	{
		const CRect r = toCRect (c.r);
		const double centre = (r.top + r.bottom) / 2.0;
		switch (c.style)
		{
			case CaptionStyle::Block:
				drawLettering (context, c.text, kTextBlock, Palette::accent (c.accent), r.left, centre);
				break;
			case CaptionStyle::Note:
				drawLettering (context, c.text, kTextNote, Palette::kInkSoft, r.left, centre);
				break;
		}
	}

	// ---- the wear, over the printing ----------------------------------------
	drawWear (context);

	setDirty (false);
}

//------------------------------------------------------------------------
// Knob - R2Synth's black-panel knob
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
	(void) mAccent;   // every knob on the black panel is black and white
	const CRect r = getViewSize ();
	context->setDrawMode (kAntiAliasing | kNonIntegralMode);

	const double v = std::clamp (static_cast<double> (getValueNormalized ()), 0.0, 1.0);
	const CPoint centre (r.left + r.getWidth () / 2., r.top + mDiameter / 2.);
	const double outer = mDiameter / 2.0;
	const double body = outer - ((mDiameter >= 44) ? 6.0 : 5.0);
	const double a = angleOf (v);

	// ---- the printed scale: eleven ticks, long at the ends and the middle
	for (int i = 0; i <= 10; ++i)
	{
		const double t = 135.0 + 27.0 * i;
		const bool longTick = (i == 0 || i == 5 || i == 10);
		strokeLine (context, pointAt (centre, body + 1.5, t), pointAt (centre, outer - (longTick ? 0.0 : 1.8), t),
		            Palette::kTick, longTick ? 1.3 : 0.9);
	}
	if (mDiameter >= 52)
	{
		const CPoint lo = pointFromTop (centre, outer + 2.0, -138.0);
		const CPoint hi = pointFromTop (centre, outer + 2.0, 138.0);
		drawLettering (context, mBipolar ? "-" : "0", Layout::kTextScale, Palette::kTick, lo.x - 2., lo.y + 4.,
		               Align::Right);
		drawLettering (context, mBipolar ? "+" : "10", Layout::kTextScale, Palette::kTick, hi.x + 2., hi.y + 4.,
		               Align::Left);
	}

	// ---- the knurled body ---------------------------------------------------
	{
		auto rim = owned (context->createGraphicsPath ());
		if (rim)
		{
			constexpr int kTeeth = 24;
			for (int i = 0; i < kTeeth * 2; ++i)
			{
				const CPoint p = pointAt (centre, (i % 2 == 0) ? body : body - 1.4, 360.0 * i / (kTeeth * 2));
				if (i == 0)
					rim->beginSubpath (p);
				else
					rim->addLine (p);
			}
			rim->closeSubpath ();
			context->setFillColor (Palette::kKnobBody);
			context->drawGraphicsPath (rim, CDrawContext::kPathFilled);
			context->setFrameColor (Palette::kKnobRim);
			context->setLineWidth (0.6);
			context->setLineStyle (kLineSolid);
			context->drawGraphicsPath (rim, CDrawContext::kPathStroked);
		}
		auto cap = owned (context->createGraphicsPath ());
		if (cap)
		{
			cap->addEllipse (circleRect (centre, body * 0.78));
			fillCap (context, cap, centre, body * 0.78);
			context->setFrameColor (CColor (0, 0, 0, 255));
			context->setLineWidth (0.8);
			context->setLineStyle (kLineSolid);
			context->drawGraphicsPath (cap, CDrawContext::kPathStroked);
		}
	}

	// ---- the wear: dust in the knurling, the polished cap, turning marks ------
	// Everything but the sheen is placed relative to the pointer (`a`), so
	// it turns with the knob; the sheen is where the light is, so it stays.
	{
		const double capR = body * 0.78;
		if (mWear.knurlAlpha > 0)
			strokeCircle (context, centre, (capR + body) / 2.0, wearColour (Wear::kKnurlDust, mWear.knurlAlpha),
			              body - capR);
		if (mWear.sheen > 0)
			softDisc (context, CPoint (centre.x - capR * 0.25, centre.y - capR * 0.3), capR * 0.55, Wear::kSheen,
			          mWear.sheen);
		for (const auto& s : mWear.scuffs)
		{
			auto arc = owned (context->createGraphicsPath ());
			if (!arc)
				continue;
			const double start = a + s.start;
			arc->addArc (circleRect (centre, capR * s.radius), start, start + s.span, true);
			context->setFrameColor (wearColour (Wear::kScuff, s.alpha));
			context->setLineWidth (s.width);
			context->setLineStyle (roundLine ());
			context->drawGraphicsPath (arc, CDrawContext::kPathStroked);
		}
		for (const auto& g : mWear.grit)
			fillCircle (context, pointAt (centre, body * g.radius, a + g.angle), g.r,
			            wearColour (Wear::kKnurlDust, g.alpha));
	}

	// ---- the white line -------------------------------------------------------
	strokeLine (context, pointAt (centre, body * 0.15, a), pointAt (centre, body - 0.5, a), Palette::kInk,
	            (mDiameter >= 44) ? 2.2 : 1.8);

	// ---- the reading and the label --------------------------------------------
	const CCoord textTop = r.top + mDiameter + Layout::kKnobGap;
	const CCoord cx = centre.x;
	if (mFormatter)
		drawLettering (context, mFormatter (getValueNormalized ()), Layout::kTextValue,
		               Palette::kInk, cx,
		               textTop + Layout::kKnobValueH / 2., Align::Centre);
	if (!mLabel.empty ())
		drawLettering (context, mLabel, Layout::kTextLabel, Palette::kLabel, cx,
		               textTop + Layout::kKnobValueH + Layout::kKnobLabelH / 2., Align::Centre);

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
// SegmentSwitch - R2Synth's, plus the reading and label under the slot
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

void SegmentSwitch::setReading (const std::string& reading, bool lit)
{
	if (reading != mReading || lit != mReadingLit)
	{
		mReading = reading;
		mReadingLit = lit;
		invalid ();
	}
}

void SegmentSwitch::draw (CDrawContext* context)
{
	const CRect whole = getViewSize ();
	context->setDrawMode (kAntiAliasing | kNonIntegralMode);

	// With a label, the slot is the top of the rectangle and the reading
	// and label sit at its foot; without one the slot is all of it.
	CRect r (whole);
	if (!mLabel.empty ())
	{
		r.bottom = r.top + Layout::kSwitchSlotHeight;

		const CCoord cx = (whole.left + whole.right) / 2.;
		const CCoord textTop = whole.bottom - Layout::kSwitchReadingBand;
		if (!mReading.empty ())
			drawLettering (context, mReading, Layout::kTextValue,
			               mReadingLit ? Palette::kInk : Palette::kLabel, cx,
			               textTop + Layout::kKnobValueH / 2., Align::Centre);
		drawLettering (context, mLabel, Layout::kTextLabel, Palette::kLabel, cx,
		               textTop + Layout::kKnobValueH + Layout::kKnobLabelH / 2., Align::Centre);
	}

	CRect outer (r);
	outer.inset (0.6, 0.6);
	fillRoundRect (context, outer, 3., Palette::kHeader, &Palette::kSectionEdge);

	const int n = count ();
	if (n == 0)
	{
		setDirty (false);
		return;
	}

	const CCoord inner = 2.;
	const CCoord segWidth = (r.getWidth () - 2. * inner) / n;
	const int current = currentIndex ();

	for (int i = 0; i < n; ++i)
	{
		const CRect seg (r.left + inner + i * segWidth, r.top + inner,
		                 r.left + inner + (i + 1) * segWidth, r.bottom - inner);
		if (i == current)
			fillRoundRect (context, seg, 2., Palette::kInk);
		drawLettering (context, mNames[static_cast<size_t> (i)], Layout::kTextPill,
		               i == current ? Palette::kGround : Palette::kInkDim,
		               (seg.left + seg.right) / 2., (seg.top + seg.bottom) / 2., Align::Centre);
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

	// ---- the glass in its cut-out, and its grid --------------------------
	const CRect plate = drawGlass (context, r);
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
		drawLettering (context, "F1 \xE2\x86\x93 close \xE2\x80\xA6 open", Layout::kTextScreen,
		               Palette::accent (Layout::Accent::Amber), r.left + 10, r.top + 15);
		drawLettering (context, "\xE2\x86\x90 F2 front \xE2\x80\xA6 back", Layout::kTextScreen,
		               Palette::accent (Layout::Accent::Blue), r.right - 10, r.top + 15, Align::Right);

		for (double f2 : { 2500.0, 2000.0, 1500.0, 1000.0, 700.0 })
		{
			const CCoord x = pointOf (Layout::kPadF1Min, f2).x;
			char text[16];
			if (f2 >= 1000.0)
				std::snprintf (text, sizeof (text), "%gk", f2 / 1000.0);
			else
				std::snprintf (text, sizeof (text), "%.0f", f2);
			drawLettering (context, text, Layout::kTextScale, Palette::kScreenDim, x, r.bottom - 11, Align::Centre);
		}
		for (double f1 : { 300.0, 500.0, 700.0, 1000.0 })
		{
			const CCoord y = pointOf (f1, 1000.0).y;
			char text[16];
			std::snprintf (text, sizeof (text), "%.0f", f1);
			drawLettering (context, text, Layout::kTextScale, Palette::kScreenDim, r.left + 8, y);
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
		outline (theirs, Palette::withAlpha (Palette::kInk, 42), true);
		outline (mine, Palette::accentAlpha (Layout::Accent::Amber, 90), false);

		// ---- the other voice's vowels: faint hollow dots --------------------
		context->setLineStyle (kLineSolid);
		context->setLineWidth (1.2);
		context->setFrameColor (Palette::withAlpha (Palette::kInk, 76));
		for (int i = 0; i < kVowelCount; ++i)
		{
			const auto& f = theirs[i].formants;
			context->drawEllipse (circleRect (pointOf (f[0].freqHz, f[1].freqHz), 5.), kDrawStroked);
		}
		drawLettering (context, other == kVoiceFemale ? "\xE2\x97\x8B female means" : "\xE2\x97\x8B male means",
		               Layout::kTextScale, Palette::kScreenDim, r.right - 10, r.bottom - 27, Align::Right);

		// ---- this voice's vowels: labelled circles ------------------------------
		for (int i = 0; i < kVowelCount; ++i)
		{
			const auto& f = mine[i].formants;
			const CPoint c = pointOf (f[0].freqHz, f[1].freqHz);
			const bool lit = (i + 1 == mState.selector);
			const CCoord rad = Layout::kVowelCircleRadius;

			context->setFillColor (lit ? Palette::accentAlpha (Layout::Accent::Amber, 56) : Palette::kVowelDisc);
			context->setFrameColor (lit ? Palette::accent (Layout::Accent::Amber) : Palette::kVowelEdge);
			context->setLineWidth (1.2);
			context->setLineStyle (kLineSolid);
			context->drawEllipse (circleRect (c, rad), kDrawFilledAndStroked);

			// The IPA sits on the x-height (2..6 of the grid): centre that.
			drawLettering (context, Layout::kVowelIpa[i], Layout::kTextIpaChart,
			               lit ? Palette::accent (Layout::Accent::Amber) : Palette::kInk, c.x,
			               c.y - Layout::kTextIpaChart.cap / 6.0, Align::Centre);
			drawLettering (context, mine[i].name, Layout::kTextLabel, Palette::kScreenText, c.x, c.y + rad + 9,
			               Align::Centre);
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
		context->setFillColor (Palette::withAlpha (Palette::kInk, 20));
		context->drawEllipse (circleRect (dot, 18.), kDrawFilled);
		context->setFillColor (Palette::kInk);
		context->setFrameColor (CColor (0, 0, 0, 255));
		context->setLineWidth (2.);
		context->setLineStyle (kLineSolid);
		context->drawEllipse (circleRect (dot, 9.), kDrawFilledAndStroked);

		{
			const CCoord w = 118., h = 36.;
			// Beside the dot, level with it: above or below would sit on
			// the name of the vowel the dot is resting on.
			CCoord left = dot.x + 24., top = dot.y - h / 2.;
			if (left + w > r.right - 4.)
				left = dot.x - 24. - w;
			top = std::clamp (top, r.top + 26., r.bottom - h - 4.);
			const CRect tip (left, top, left + w, top + h);
			const CColor edge (Palette::kVowelEdge);
			fillRoundRect (context, tip, 3., Palette::withAlpha (Palette::kScreen, 235), &edge);
			drawLettering (context, "F1 " + formatHz (mState.f1), Layout::kTextScreen,
			               Palette::accent (Layout::Accent::Amber), left + 9, top + 10.5);
			drawLettering (context, "F2 " + formatHz (mState.f2), Layout::kTextScreen,
			               Palette::accent (Layout::Accent::Blue), left + 9, top + 24.5);
		}
	}

	// The marks on the glass, over the picture.
	{
		ConcatClip clip (*context, plate);
		drawGlassWear (context, mWear);
	}

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

	const CRect plate = drawGlass (context, r);
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

		for (double f : kSpectrumLabelled)
		{
			const CCoord x = xOf (f, p);
			char text[16];
			if (f >= 1000.0)
				std::snprintf (text, sizeof (text), "%gk", f / 1000.0);
			else
				std::snprintf (text, sizeof (text), "%.0f", f);
			drawLettering (context, text, Layout::kTextScale, Palette::kScreenDim, x, r.bottom - 9.5, Align::Centre);
		}

		if (!mCaption.empty ())
			drawLettering (context, mCaption, Layout::kTextScreen, Palette::kScreenText, r.left + 10, r.top + 12);
		if (!mAnnotation.empty ())
			drawLettering (context, mAnnotation, Layout::kTextScreen, Palette::kScreenText, r.right - 10, r.top + 12,
			               Align::Right);

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
		}, Palette::kInk, 2.2, true);

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

	{
		ConcatClip clip (*context, plate);
		drawGlassWear (context, mWear);
	}

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

	const bool down = mPressed && mInside;
	drawKey (context, r, down);
	const CCoord dy = down ? 0.75 : 0.;

	// The lamp in the corner: lit while this vowel is the one selected.
	drawLamp (context, CPoint (r.right - 8., r.top + 8. + dy), 2.6, mSelected);

	const CCoord cx = (r.left + r.right) / 2.;
	drawLettering (context, mGlyph, Layout::kTextIpaKey, Palette::kInk, cx,
	               r.top + 16. + dy - Layout::kTextIpaKey.cap / 6.0, Align::Centre);
	drawLettering (context, mName, Layout::kTextLabel, Palette::kInk, cx, r.bottom - 11.5 + dy, Align::Centre);

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

	const bool down = mPressed && mInside;
	drawKey (context, r, down);
	const CCoord dy = down ? 0.75 : 0.;
	const CCoord my = (r.top + r.bottom) / 2. + dy;

	drawLamp (context, CPoint (r.left + 18., my), 4.5, mLit);
	drawLettering (context, "MANUAL", Layout::kTextPill, Palette::kInk, r.left + 32., my);
	drawLettering (context, mHint, Layout::kTextHint, Palette::kInkSoft, r.right - 12., my, Align::Right);

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
	CRect slot (r.left, r.top + 2, r.right, r.top + 10);
	slot.inset (0.5, 0.);
	fillRoundRect (context, slot, 2., Palette::kBezel, &Palette::kBezelEdge);
	if (mProgress > 0.0)
	{
		CRect bar (slot);
		bar.inset (1.5, 1.5);
		bar.right = bar.left + std::max (4.0, bar.getWidth () * std::min (mProgress, 1.0));
		fillRoundRect (context, bar, 1., violet);
	}
	context->setFillColor (Palette::kGrid);
	for (int i = 1; i < 10; ++i)
	{
		const CCoord x = std::round (slot.left + i * slot.getWidth () / 10.);
		context->drawRect (CRect (x, slot.top + 1.5, x + 1, slot.bottom - 1.5), kDrawFilled);
	}

	drawLettering (context, mText, Layout::kTextValue, mProgress >= 0.0 ? violet : Palette::kLabel,
	               (r.left + r.right) / 2., r.top + 23., Align::Centre);

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
	CRect window (r);
	window.inset (0.6, 0.6);
	fillRoundRect (context, window, 3., Palette::kHeader, &Palette::kRed);
	context->setLineWidth (1.);

	const CCoord cy = (r.top + r.bottom) / 2.;
	drawLamp (context, CPoint (r.left + 18., cy), 4.5, on);
	drawLettering (context, on ? "BYPASS  ON" : "BYPASS  OFF", Layout::kTextPill, Palette::kInk, r.left + 32., cy);

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
// TextLine
//------------------------------------------------------------------------
TextLine::TextLine (const CRect& size, const Lettering::Style& style, const CColor& colour)
: CView (size)
, mStyle (style)
, mColour (colour)
{
	setMouseEnabled (false);
}

void TextLine::setText (const std::string& text)
{
	if (text != mText)
	{
		mText = text;
		invalid ();
	}
}

void TextLine::draw (CDrawContext* context)
{
	const CRect r = getViewSize ();
	context->setDrawMode (kAntiAliasing | kNonIntegralMode);
	drawLettering (context, mText, mStyle, mColour, r.left, (r.top + r.bottom) / 2.);
	setDirty (false);
}

//------------------------------------------------------------------------
} // namespace Panel
} // namespace VocalFilter
