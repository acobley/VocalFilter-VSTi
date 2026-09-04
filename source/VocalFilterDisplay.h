//------------------------------------------------------------------------
// VocalFilter - the response display
//
// The stand-in for ForTran's FtCurveView, and built in the same idiom: a
// dark plate with a thin border, a caption in the top-left corner, and
// polylines over it. What it draws is different enough to be its own
// class - three overlaid curves on a log frequency axis with a decibel
// grid, rather than one polyline over an index.
//
// It is NOT in VocalFilterControls.*, deliberately. That file is SpyBand's
// control set carried across almost unchanged and it knows nothing about
// this plug-in; this view knows what a formant is.
//
// THE CURVES COME FROM bandpassMagnitude(), the same function the DSP's
// coefficients come from. Anything the editor displays that the DSP also
// computes must come from ONE shared function, and that is the whole
// reason this display can be trusted:
// a private copy of the response maths here would agree with the filter
// today and diverge at some sample rate nobody tests.
//------------------------------------------------------------------------

#pragma once

#include "VocalFilterDsp.h"

#include "vstgui/vstgui.h"

#include <functional>

namespace VocalFilter {

//------------------------------------------------------------------------
class SpyResponseDisplay : public VSTGUI::CView
{
public:
	explicit SpyResponseDisplay (const VSTGUI::CRect& size);

	/** The three formants as they are RIGHT NOW - mid-glide, not their
	    targets - and the rate the DSP is running at, because a bandpass's
	    shape is a function of f/fs and a curve drawn at an assumed
	    44.1 kHz while the DSP runs at 96 k is a filter nobody is hearing.

	    Returns true if anything moved, so the editor's timer can skip the
	    redraw when nothing has. */
	bool setFormants (const FormantSetting* formants, double sampleRate);

	void draw (VSTGUI::CDrawContext* context) override;

	CLASS_METHODS (SpyResponseDisplay, VSTGUI::CView)

	/** The axes. 80 Hz to 8 kHz covers every formant in the table with
	    room either side; +12 to -48 dB covers the Level range plus the
	    headroom a peak can reach. */
	static constexpr double kMinHz  =    80.0;
	static constexpr double kMaxHz  =  8000.0;
	static constexpr double kMaxDb  =    12.0;
	static constexpr double kMinDb  =   -48.0;

private:
	VSTGUI::CCoord xOf (double hz, const VSTGUI::CRect& plot) const;
	VSTGUI::CCoord yOf (double db, const VSTGUI::CRect& plot) const;

	void drawGrid (VSTGUI::CDrawContext* context, const VSTGUI::CRect& plot);

	/** One broken polyline across the plot. `sampler` returns decibels for
	    a frequency; anything below the floor breaks the line rather than
	    being clamped to it. Both the three formants and the summed
	    response go through here, so they cannot end up drawn by two
	    slightly different pieces of code. */
	void drawPolyline (VSTGUI::CDrawContext* context, const VSTGUI::CRect& plot,
	                   const std::function<double (double)>& sampler,
	                   const VSTGUI::CColor& colour, VSTGUI::CCoord width);

	FormantSetting mFormants[kFormantCount] = {};
	double mSampleRate = 44100.0;
};

//------------------------------------------------------------------------
} // namespace VocalFilter
