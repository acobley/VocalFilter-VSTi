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

CRect toCRect (const Layout::Rect& r)
{
	return CRect (r.left, r.top, r.right, r.bottom);
}

/** A reading in the parameter's own unit, from the SAME table the host
    formats from, so the panel and an automation lane cannot disagree.
    Hertz, per cent and milliseconds read whole; decibels to a tenth. */
std::string readout (const ParamDef& def, double normalized)
{
	char buffer[32];
	const double plain = def.toPlain (normalized);
	const std::string units = def.units ? def.units : "";
	if (units == "dB")
		std::snprintf (buffer, sizeof (buffer), "%.1f dB", plain);
	else if (units.empty ())
		std::snprintf (buffer, sizeof (buffer), "%.0f", plain);
	else
		std::snprintf (buffer, sizeof (buffer), "%.0f %s", plain, units.c_str ());
	return buffer;
}

/** The nine values as one point, each field scaled by its parameter
    range, so a glide's progress can be measured as a distance. */
double distance (const FormantSetting* a, const FormantSetting* b)
{
	double sum = 0.0;
	for (int k = 0; k < kFormantCount; ++k)
	{
		const double df = (a[k].freqHz - b[k].freqHz) / (kFormantFreqMax[k] - kFormantFreqMin[k]);
		const double db = (a[k].bandwidthHz - b[k].bandwidthHz) / (kBandwidthMax - kBandwidthMin);
		const double dl = (a[k].levelDb - b[k].levelDb) / (kLevelMaxDb - kLevelMinDb);
		sum += df * df + db * db + dl * dl;
	}
	return std::sqrt (sum);
}

const char* const kSeparator = "  \xC2\xB7  ";   // "  ·  "

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
void VocalFilterEditor::setParameter (ParamID tag, double plain)
{
	if (mController == nullptr)
		return;

	const ParamDef& def = paramDef (tag);
	const double normalized = std::min (1.0, std::max (0.0, def.toNormalized (plain)));

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
int VocalFilterEditor::currentVoice () const
{
	if (mController == nullptr)
		return kVoiceMale;
	return static_cast<int> (
		kParams[kVoice].toInternal (mController->getParamNormalized (kVoice)) + 0.5);
}

//------------------------------------------------------------------------
void VocalFilterEditor::selectVowel (int selector)
{
	setParameter (kVowel, static_cast<double> (selector));
}

//------------------------------------------------------------------------
void VocalFilterEditor::captureAndGoManual ()
{
	const FormantSetting* preset = vowelSelection (currentVowel (), currentVoice ());
	if (preset == nullptr)
		return;                        // already Manual

	// The nine FIRST, the mode second, so a host recording this gesture
	// records the values arriving before the mode that makes them matter.
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
	const FormantSetting* preset = vowelSelection (selector, currentVoice ());

	auto voice = mControls.find (kVoice);
	if (voice != mControls.end ())
		showValue (voice->second, mController->getParamNormalized (kVoice));

	for (int v = 0; v < kVowelCount; ++v)
		if (mVowelButtons[v])
			mVowelButtons[v]->setSelected (v + 1 == selector);
	if (mManualButton)
		mManualButton->setLit (selector == kVowelManual);

	// The nine knobs show WHAT THE DSP IS USING, which on a preset is the
	// preset and not the parameters. Display only: the parameters still
	// hold the manual values, and pretending otherwise is what
	// captureAndGoManual is for.
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
			showValue (it->second, shown);
		}
	}
}

//------------------------------------------------------------------------
void VocalFilterEditor::addControl (const Layout::Control& c)
{
	const CRect r = toCRect (Layout::controlRect (c));
	const ParamID tag = c.tag;
	CControl* control = nullptr;

	if (c.kind == Layout::Kind::Switch)
	{
		auto* sw = new Panel::SegmentSwitch (r, this, static_cast<int32_t> (tag), c.accent);
		// The only switch is Voice, and its names are voiceName()'s - the
		// same words the controller gives the host.
		std::vector<std::string> names;
		for (int i = 0; i < kVoiceCount; ++i)
			names.push_back (voiceName (i));
		sw->setNames (names);
		control = sw;
	}
	else
	{
		auto* knob = new Panel::Knob (r, this, static_cast<int32_t> (tag), c.accent, c.diameter, false);
		knob->setLabel (c.label ? c.label : paramDef (tag).title);
		const ParamDef& def = paramDef (tag);
		knob->setFormatter ([def] (float normalized) { return readout (def, normalized); });
		control = knob;
	}

	// Double-click on a knob returns to the table's default.
	control->setDefaultValue (static_cast<float> (paramDef (tag).defaultNormalized ()));

	mControls[tag] = control;
	if (mController)
		showValue (control, mController->getParamNormalized (tag));
	frame->addView (control);
}

//------------------------------------------------------------------------
CTextLabel* VocalFilterEditor::addTextLine (const Layout::Rect& r, CFontRef font, const CColor& colour)
{
	auto* label = new CTextLabel (toCRect (r));
	label->setFont (font);
	label->setFontColor (colour);
	label->setBackColor (kTransparentCColor);
	label->setFrameColor (kTransparentCColor);
	label->setStyle (CParamDisplay::kNoFrame);
	label->setHoriAlign (kLeftText);
	label->setMouseEnabled (false);
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
	frame->setBackgroundColor (Panel::Palette::kGround);

	// ---- everything that never changes, behind everything else ----------
	frame->addView (new Panel::Backdrop (frameSize));

	// ---- the header: the engine line and bypass -------------------------
	mEngineLine = addTextLine (Layout::kEngineLine, Panel::fontSubtitle (), Panel::Palette::kSubtle);
	{
		auto* bypass = new Panel::BypassPill (toCRect (Layout::kBypassPill), this,
		                                      static_cast<int32_t> (kBypass));
		mControls[kBypass] = bypass;
		if (mController)
			showValue (bypass, mController->getParamNormalized (kBypass));
		frame->addView (bypass);
	}

	// ---- the vowel chart ---------------------------------------------------
	mPad = new Panel::VowelPad (toCRect (Layout::kPadView), this);
	frame->addView (mPad);

	// ---- the five vowel buttons, and Manual --------------------------------
	//
	// Each button writes the VOWEL SELECTOR - one parameter, not nine. The
	// handler captures an INDEX, not a pointer into the table.
	for (int v = 0; v < kVowelCount; ++v)
	{
		auto* button = new Panel::VowelButton (toCRect (Layout::vowelButtonRect (v)),
		                                       Layout::kVowelIpa[v], vowelSelectionName (v + 1));
		button->setHandler ([this, v] { selectVowel (v + 1); });   // 0 is Manual
		mVowelButtons[v] = button;
		frame->addView (button);
	}
	mManualButton = new Panel::ManualButton (toCRect (Layout::kManualButton),
	                                         "drag on the chart to leave the presets");
	mManualButton->setHandler ([this] { captureAndGoManual (); });
	frame->addView (mManualButton);

	mGlideMeter = new Panel::GlideMeter (toCRect (Layout::kGlideMeter));
	frame->addView (mGlideMeter);

	// ---- the spectrum ---------------------------------------------------------
	mSpectrum = new Panel::Spectrum (toCRect (Layout::kSpectrumView));
	frame->addView (mSpectrum);

	// ---- every knob and switch, from the table ----------------------------
	for (const auto& c : Layout::kControls)
		addControl (c);

	refreshVowelState ();
	refreshDisplay ();

	// The displays follow the DSP, which moves without anything on the
	// panel being touched - a glide, or a host automating Vowel - so they
	// are pulled on a timer rather than pushed by a control.
	mTimer = makeOwned<CVSTGUITimer> ([this] (CVSTGUITimer*) { refreshDisplay (); },
	                                  kTimerMs, true);

	frame->open (parent, platformType);
	return true;
}

//------------------------------------------------------------------------
void VocalFilterEditor::refreshDisplay ()
{
	if (mSpectrum == nullptr || mPad == nullptr || mController == nullptr)
		return;

	// WHERE THE DSP ACTUALLY IS, if the processor's published values have
	// reached us, and where it is HEADED otherwise. The fallback is not
	// decoration: a host that never forwards outputParameterChanges would
	// otherwise leave the display on the default patch for ever.
	const bool live = mController->hasLiveValues ();
	const int selector = currentVowel ();
	const int voice = currentVoice ();
	const FormantSetting* preset = vowelSelection (selector, voice);

	FormantSetting target[kFormantCount];
	FormantSetting shown[kFormantCount];
	for (int k = 0; k < kFormantCount; ++k)
	{
		if (preset != nullptr)
			target[k] = preset[k];
		else
		{
			target[k].freqHz      = plainOf (formantParam (k, kFieldFreq));
			target[k].bandwidthHz = plainOf (formantParam (k, kFieldBandwidth));
			target[k].levelDb     = plainOf (formantParam (k, kFieldLevel));
		}

		if (live)
		{
			shown[k].freqHz      = plainOf (liveParam (k, kFieldFreq));
			shown[k].bandwidthHz = plainOf (liveParam (k, kFieldBandwidth));
			shown[k].levelDb     = plainOf (liveParam (k, kFieldLevel));
		}
		else
			shown[k] = target[k];
	}

	// ---- glide tracking: a new target starts a glide from wherever the
	// display last was. Display only - the DSP's ramp is the authority. --
	if (!mHaveTarget || distance (target, mTarget) > 1e-6)
	{
		if (mHaveTarget && live)
		{
			for (int k = 0; k < kFormantCount; ++k)
				mGlideFrom[k] = mShown[k];
			mGliding = distance (mGlideFrom, target) > 1e-4;
		}
		for (int k = 0; k < kFormantCount; ++k)
			mTarget[k] = target[k];
		mHaveTarget = true;
	}

	double progress = -1.0;
	if (mGliding)
	{
		const double span = distance (mGlideFrom, target);
		const double left = distance (shown, target);
		if (span <= 1e-6 || left <= 1e-4)
			mGliding = false;
		else
			progress = std::clamp (1.0 - left / span, 0.0, 1.0);
	}
	for (int k = 0; k < kFormantCount; ++k)
		mShown[k] = shown[k];

	const double glideMs = std::max (plainOf (kGlide), kGlideFloorMs);

	// ---- the spectrum -----------------------------------------------------
	mSpectrum->setFormants (shown, mController->dspSampleRate ());
	mSpectrum->setCaption (!live ? "targets  \xC2\xB7  no live values from the host"
	                             : (mGliding ? "live  \xC2\xB7  mid-glide" : "live"));
	mSpectrum->setAnnotation ("F3 " + Panel::formatHz (shown[2].freqHz));

	// ---- the chart --------------------------------------------------------
	Panel::VowelPad::State state;
	state.voice = voice;
	state.selector = selector;
	state.f1 = shown[0].freqHz;
	state.f2 = shown[1].freqHz;
	state.gliding = mGliding;
	state.fromF1 = mGlideFrom[0].freqHz;
	state.fromF2 = mGlideFrom[1].freqHz;
	state.toF1 = target[0].freqHz;
	state.toF2 = target[1].freqHz;
	mPad->setState (state);

	// ---- the glide meter and the engine line ------------------------------
	char elapsed[32] = {};
	if (mGliding)
		std::snprintf (elapsed, sizeof (elapsed), "%.0f ms", progress * glideMs);
	if (mGlideMeter)
		mGlideMeter->setProgress (progress, mGliding ? elapsed : "at rest");

	if (mEngineLine)
	{
		char rate[48];
		std::snprintf (rate, sizeof (rate), "Engine: %.0f Hz", mController->dspSampleRate ());
		std::string line = rate;
		line += kSeparator;
		if (mGliding)
		{
			char of[48];
			std::snprintf (of, sizeof (of), "%.0f of %.0f ms", progress * glideMs, glideMs);
			line += std::string ("gliding to ") + vowelSelectionName (selector) + kSeparator + of;
		}
		else if (selector == kVowelManual)
			line += "Manual";
		else
			line += std::string (vowelSelectionName (selector)) + ", "
			      + (voice == kVoiceFemale ? "female" : "male");

		if (line != mEngineLine->getText ().getString ())
			mEngineLine->setText (line.c_str ());
	}
}

//------------------------------------------------------------------------
void VocalFilterEditor::showValue (CControl* control, double normalized)
{
	if (control == nullptr)
		return;
	control->setValueNormalized (static_cast<float> (normalized));
	control->invalid ();
}

//------------------------------------------------------------------------
double VocalFilterEditor::plainOf (ParamID tag) const
{
	if (mController == nullptr)
		return 0.0;
	return paramDef (tag).toPlain (mController->getParamNormalized (tag));
}

//------------------------------------------------------------------------
void PLUGIN_API VocalFilterEditor::close ()
{
	mTimer = nullptr;

	// A drag still open when the window closes must not leave the host
	// waiting for an endEdit that never comes.
	if (mPadEditing)
		padDragEnd ();

	mControls.clear ();
	for (auto*& button : mVowelButtons)
		button = nullptr;
	mManualButton = nullptr;
	mPad = nullptr;
	mSpectrum = nullptr;
	mGlideMeter = nullptr;
	mEngineLine = nullptr;
	mHaveTarget = false;
	mGliding = false;

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

	// TOUCHING A FORMANT KNOB LEAVES PRESET MODE, capturing first so the
	// eight values you did not touch are the ones you could already hear.
	if (isFormantParam (tag) && currentVowel () != kVowelManual)
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
// The vowel chart
//------------------------------------------------------------------------
void VocalFilterEditor::padDragBegin ()
{
	if (mController == nullptr || mPadEditing)
		return;

	// Leave the preset FIRST, keeping its sound, so F3, the widths and the
	// levels stay where the vowel had them while F1 and F2 follow the
	// pointer.
	if (currentVowel () != kVowelManual)
		captureAndGoManual ();

	mController->beginEdit (kF1Freq);
	mController->beginEdit (kF2Freq);
	mPadEditing = true;
}

//------------------------------------------------------------------------
void VocalFilterEditor::padDragMove (double f1Hz, double f2Hz)
{
	if (mController == nullptr || !mPadEditing)
		return;

	const ParamID tags[2] = { kF1Freq, kF2Freq };
	const double hz[2] = { f1Hz, f2Hz };
	for (int i = 0; i < 2; ++i)
	{
		const double normalized = std::clamp (paramDef (tags[i]).toNormalized (hz[i]), 0.0, 1.0);
		// setParamNormalized comes back through updateControl, which moves
		// the F1 / F2 knobs with the dot.
		mController->setParamNormalized (tags[i], normalized);
		mController->performEdit (tags[i], normalized);
	}
}

//------------------------------------------------------------------------
void VocalFilterEditor::padDragEnd ()
{
	if (mController == nullptr || !mPadEditing)
		return;
	mController->endEdit (kF1Freq);
	mController->endEdit (kF2Freq);
	mPadEditing = false;
}

//------------------------------------------------------------------------
void VocalFilterEditor::padVowelClicked (int selector)
{
	selectVowel (selector);
}

//------------------------------------------------------------------------
void VocalFilterEditor::updateControl (ParamID tag, ParamValue normalized)
{
	if (frame == nullptr)
		return;

	// The published values have no control behind them; the timer picks
	// them up.
	if (isLiveParam (tag))
		return;

	// The selector - or the VOICE, which changes what every preset is -
	// moving is a whole-panel update, not a control one.
	if (tag == kVowel || tag == kVoice)
	{
		refreshVowelState ();
		refreshDisplay ();
		return;
	}

	// While a preset is live the nine knobs are showing the PRESET; a
	// change to the parameters behind them is not something to display.
	if (isFormantParam (tag) && currentVowel () != kVowelManual)
		return;

	auto it = mControls.find (tag);
	if (it == mControls.end () || it->second == nullptr)
		return;

	showValue (it->second, normalized);
}

//------------------------------------------------------------------------
} // namespace VocalFilter
