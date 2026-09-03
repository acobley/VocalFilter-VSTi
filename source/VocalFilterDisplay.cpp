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

/** The summed response - what the three actually add up to, and what is
    heard. White, drawn last and heaviest, because it is the answer and the
    three colours are its components.

    NOT fully opaque. Where the sum sits on top of a component - and below
    F1 it sits almost exactly on it - a solid white line hides the colour
    completely, so "F1 yellow" stops being true wherever it matters most.
    At 205 the trace underneath tints it, which turns the collision into
    information: white-over-yellow means F1 IS the response there. */
const CColor kSumTrace (255, 255, 255, 205);

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
void SpyResponseDisplay::drawPolyline (CDrawContext* context, const CRect& plot,
                                       const std::function<double (double)>& sampler,
                                       const CColor& colour, CCoord width)
{
	context->setFrameColor (colour);
	context->setLineWidth (width);

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
		const double decibels = sampler (hz);

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

	// The legend is the colour key - each name in the colour its curve is
	// drawn in, so nothing has to be looked up. Laid out from the RIGHT
	// EDGE backwards, because adding "Sum" to a row measured from the left
	// would have pushed it off the plate.
	{
		struct { const char* name; CColor colour; CCoord width; } entries[] =
		{
			{ "F1",  kTrace[0],  22. },
			{ "F2",  kTrace[1],  22. },
			{ "F3",  kTrace[2],  22. },
			{ "Sum", kSumTrace,  28. },
		};

		CCoord total = 0.;
		for (const auto& e : entries)
			total += e.width;

		CCoord x = caption.right - total;
		for (const auto& e : entries)
		{
			context->setFontColor (e.colour);
			CRect slot (x, caption.top, x + e.width, caption.bottom);
			context->drawString (e.name, slot, kLeftText, true);
			x += e.width;
		}
	}

	// Painted in order, so F1 is under F2 is under F3 where they cross.
	for (int k = 0; k < kFormantCount; ++k)
	{
		const FormantSetting& f = mFormants[k];
		const double gain = dbToLinear (f.levelDb, kLevelMinDb);

		// A silenced formant draws nothing rather than a flat line along
		// the bottom, which would read as a filter doing something quiet.
		if (gain <= 0.0)
			continue;

		drawPolyline (context, plot,
			[this, &f, gain] (double hz)
			{
				// THE SHARED FUNCTION. Not a copy of the response maths.
				const double mag = bandpassMagnitude (f.freqHz, f.bandwidthHz,
				                                      hz, mSampleRate) * gain;
				return 20.0 * std::log10 (std::max (mag, 1e-9));
			},
			kTrace[k], 1.5);
	}

	// THE SUM, last and heaviest, so it reads as the resultant rather than
	// as a fourth formant. bankMagnitude does the COMPLEX sum, levels and
	// polarity included - the same function tests/DspTests.cpp section 3
	// checks against the running filter, so this white line is what is
	// actually coming out of the bank and not an approximation of it.
	//
	// Dry/Wet and Output Trim are deliberately NOT in it: this panel is
	// captioned "Filter response" and shows the filter, not the mix.
	drawPolyline (context, plot,
		[this] (double hz)
		{
			const double mag = bankMagnitude (mFormants, kFormantCount, hz, mSampleRate);
			return 20.0 * std::log10 (std::max (mag, 1e-9));
		},
		kSumTrace, 2.0);

	setDirty (false);
}

//------------------------------------------------------------------------
} // namespace VocalFilter
