//------------------------------------------------------------------------
// VocalFilter - the response display
//------------------------------------------------------------------------

#include "VocalFilterDisplay.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace VSTGUI;

namespace VocalFilter {

namespace {

/** One colour per formant, as asked for. They are saturated because they
    have to stay apart from each other where three curves cross, and
    because the panel's own lettering is now near-white - so the traces are
    the only saturated thing on the panel and cannot be confused with it. */
const CColor kTrace[kFormantCount] =
{
	CColor (255, 214,  64, 255),    // F1 yellow
	CColor ( 72, 226,  86, 255),    // F2 green
	CColor ( 96, 160, 255, 255),    // F3 blue
};

const CColor kPlate      (  0,   0,   0, 190);
const CColor kPlateEdge  (255, 255, 255, 110);
const CColor kGridLine   (255, 255, 255,  38);
const CColor kGridStrong (255, 255, 255,  70);
const CColor kCaption    (255, 255, 255, 200);

/** Grid lines, and which of them are labelled. */
constexpr double kDecades[] = { 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0 };
constexpr double kDbLines[] = { 0.0, -12.0, -24.0, -36.0 };

/** Points across the plot. One per pixel would be wasteful on a wide panel
    and jagged on a narrow one; a fixed count keeps the curve smooth and
    the cost predictable. */
constexpr int kPoints = 240;

} // namespace

//------------------------------------------------------------------------
SpyResponseDisplay::SpyResponseDisplay (const CRect& size)
: CView (size)
{
	// Nothing here reads the mouse: it is a readout, not a control.
	setMouseEnabled (false);

	for (int k = 0; k < kFormantCount; ++k)
		mFormants[k] = kAaaFormants[k];
}

//------------------------------------------------------------------------
bool SpyResponseDisplay::setFormants (const FormantSetting* formants, double sampleRate)
{
	if (formants == nullptr)
		return false;

	bool changed = (sampleRate != mSampleRate);
	mSampleRate = sampleRate;

	for (int k = 0; k < kFormantCount; ++k)
	{
		if (mFormants[k].freqHz      != formants[k].freqHz ||
		    mFormants[k].bandwidthHz != formants[k].bandwidthHz ||
		    mFormants[k].levelDb     != formants[k].levelDb)
			changed = true;
		mFormants[k] = formants[k];
	}

	if (changed)
		invalid ();
	return changed;
}

//------------------------------------------------------------------------
CCoord SpyResponseDisplay::xOf (double hz, const CRect& plot) const
{
	// LOG frequency. A linear axis spends two thirds of its width above
	// 3 kHz, where nothing in this plug-in lives, and squeezes F1 and F2 -
	// the two that decide which vowel you are hearing - into a thumbnail.
	const double lo = std::log10 (kMinHz), hi = std::log10 (kMaxHz);
	const double t = (std::log10 (std::max (hz, 1.0)) - lo) / (hi - lo);
	return plot.left + plot.getWidth () * std::min (1.0, std::max (0.0, t));
}

CCoord SpyResponseDisplay::yOf (double decibels, const CRect& plot) const
{
	const double t = (kMaxDb - decibels) / (kMaxDb - kMinDb);
	return plot.top + plot.getHeight () * std::min (1.0, std::max (0.0, t));
}

//------------------------------------------------------------------------
void SpyResponseDisplay::drawGrid (CDrawContext* context, const CRect& plot)
{
	context->setLineWidth (1.);

	for (double hz : kDecades)
	{
		const bool decade = (hz == 100.0 || hz == 1000.0);
		context->setFrameColor (decade ? kGridStrong : kGridLine);
		const CCoord x = xOf (hz, plot);
		context->drawLine (CPoint (x, plot.top), CPoint (x, plot.bottom));
	}

	for (double decibels : kDbLines)
	{
		context->setFrameColor (decibels == 0.0 ? kGridStrong : kGridLine);
		const CCoord y = yOf (decibels, plot);
		context->drawLine (CPoint (plot.left, y), CPoint (plot.right, y));
	}
}

//------------------------------------------------------------------------
void SpyResponseDisplay::drawCurve (CDrawContext* context, const CRect& plot,
                                    const FormantSetting& formant, const CColor& colour)
{
	const double gain = dbToLinear (formant.levelDb, kLevelMinDb);

	// A silenced formant draws nothing rather than a flat line along the
	// bottom, which would read as a filter doing something quiet.
	if (gain <= 0.0)
		return;

	context->setFrameColor (colour);
	context->setLineWidth (1.5);

	SharedPointer<CGraphicsPath> path = owned (context->createGraphicsPath ());
	if (path == nullptr)
		return;

	// A curve is BROKEN where it falls off the bottom of the scale rather
	// than clamped to it. Clamping draws a flat line along the floor, which
	// reads as a filter doing something quiet across the whole spectrum;
	// F3 at -26.8 dB in the Aaa patch has skirts below -48 for most of the
	// width and looked exactly like that.
	bool drawing = false;
	for (int i = 0; i < kPoints; ++i)
	{
		const double t = i / static_cast<double> (kPoints - 1);
		const double hz = kMinHz * std::pow (kMaxHz / kMinHz, t);

		// THE SHARED FUNCTION. Not a copy of the response maths.
		const double mag = bandpassMagnitude (formant.freqHz, formant.bandwidthHz,
		                                      hz, mSampleRate) * gain;
		const double decibels = 20.0 * std::log10 (std::max (mag, 1e-9));

		if (decibels < kMinDb)
		{
			drawing = false;
			continue;
		}

		const CPoint p (xOf (hz, plot), yOf (decibels, plot));
		if (! drawing)
		{
			path->beginSubpath (p);
			drawing = true;
		}
		else
		{
			path->addLine (p);
		}
	}

	context->drawGraphicsPath (path, CDrawContext::kPathStroked);
}

//------------------------------------------------------------------------
void SpyResponseDisplay::draw (CDrawContext* context)
{
	const CRect r = getViewSize ();

	context->setDrawMode (kAntiAliasing);

	// The plate, in ForTran's idiom: a dark ground with a thin light edge,
	// so the display reads as a window into the plug-in rather than as a
	// hole in the panel.
	context->setFillColor (kPlate);
	context->setFrameColor (kPlateEdge);
	context->setLineWidth (1.);
	CRect plate (r);
	plate.inset (0.5, 0.5);
	context->drawRect (plate, kDrawFilledAndStroked);

	// The caption gets a band of its own at the top; without it a curve at
	// full level runs through the lettering.
	CRect plot (r);
	plot.inset (6., 5.);
	plot.top += 12.;

	drawGrid (context, plot);

	context->setFont (kNormalFontVerySmall);
	context->setFontColor (kCaption);
	CRect caption (r);
	caption.inset (5., 4.);
	caption.bottom = caption.top + 11.;
	context->drawString ("Filter response", caption, kLeftText, true);

	// The legend doubles as the axis labels' colour key - F1 F2 F3 in the
	// colours their curves are drawn in, so nothing has to be looked up.
	CCoord x = caption.right - 66.;
	for (int k = 0; k < kFormantCount; ++k)
	{
		char name[8];
		std::snprintf (name, sizeof (name), "F%d", k + 1);
		context->setFontColor (kTrace[k]);
		CRect slot (x, caption.top, x + 20., caption.bottom);
		context->drawString (name, slot, kLeftText, true);
		x += 22.;
	}

	// Painted in order, so F1 is under F2 is under F3 where they cross.
	for (int k = 0; k < kFormantCount; ++k)
		drawCurve (context, plot, mFormants[k], kTrace[k]);

	setDirty (false);
}

//------------------------------------------------------------------------
} // namespace VocalFilter
