//------------------------------------------------------------------------
// VocalFilter - layout tests
//
// The only way to test a panel from a session that cannot run a host:
// hold the layout TABLES to the things a broken layout gets wrong.
// Everything here reads VocalFilterLayout.h, which includes no VSTGUI, so
// this links against VocalFilterParams.cpp and VocalFilterDsp.cpp only:
//
//     c++ -std=c++17 -O2 -Isource -Iexternal/vst3sdk tests/LayoutTests.cpp
//         source/VocalFilterParams.cpp source/VocalFilterDsp.cpp
//         -o /tmp/layouttests && /tmp/layouttests
//
// THE BLACK PANEL (10 Nov 2026): every fit is measured with
// Lettering::width() - the width of the strokes the views draw - instead
// of average character widths; the face itself is tested (every glyph
// parses and stays near its grid, every character the panel prints has a
// glyph, the IPA symbols keep their case); and the wear (PanelWear.h) is
// held to the panel, off the glass, and faint.
//------------------------------------------------------------------------

#include "PanelWear.h"
#include "VocalFilterLayout.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace VocalFilter;
using namespace VocalFilter::Layout;
using Steinberg::Vst::ParamID;

namespace {

int gFailures = 0;

void check (bool condition, const std::string& what)
{
	std::printf ("  %-70s %s\n", what.c_str (), condition ? "ok" : "FAILED");
	if (!condition) ++gFailures;
}

void section (const char* title) { std::printf ("\n%s\n", title); }

bool contains (const Rect& outer, const Rect& inner)
{
	return inner.left >= outer.left && inner.right <= outer.right &&
	       inner.top >= outer.top && inner.bottom <= outer.bottom;
}

bool overlaps (const Rect& a, const Rect& b)
{
	return a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom;
}

/** A section's inside: under its title row and within its padding. The
    title row itself is allowed for controls that sit in it (the Voice
    switch). */
Rect inside (const Rect& s)
{
	return { s.left + 8, s.top + 6, s.right - 8, s.bottom - 8 };
}

const char* nameOf (ParamID tag) { return paramDef (tag).title; }

/** Everything that is placed on the panel, by name. */
struct Placed
{
	std::string name;
	Rect r;
};

/** The widest reading a knob can show, in characters. */
/** The widest reading a knob can show, in pixels of lettering: the
    editor's readout() over the whole range, 401 steps and the ends. */
double widestReading (ParamID tag, std::string* which = nullptr)
{
	const ParamDef& def = paramDef (tag);
	const std::string units = def.units ? def.units : "";
	double widest = 0.0;
	for (int i = 0; i <= 401; ++i)
	{
		const double plain = (i == 401) ? def.plainDefault : def.toPlain (i / 400.0);
		char buffer[32];
		if (units == "dB")
			std::snprintf (buffer, sizeof (buffer), "%.1f dB", plain);
		else if (units.empty ())
			std::snprintf (buffer, sizeof (buffer), "%.0f", plain);
		else
			std::snprintf (buffer, sizeof (buffer), "%.0f %s", plain, units.c_str ());
		const double w = Lettering::width (buffer, kTextValue);
		if (w > widest)
		{
			widest = w;
			if (which)
				*which = buffer;
		}
	}
	return widest;
}

/** Every character of `text` has a glyph of its own - none drawn as the
    dot. */
bool allDrawn (const std::string& text, bool keepCase = false)
{
	for (uint32_t c : Lettering::codePoints (text, keepCase))
		if (c != ' ' && !Lettering::glyphFor (c))
			return false;
	return true;
}

std::string px (double v)
{
	char t[16];
	std::snprintf (t, sizeof (t), "%.1f", v);
	return t;
}

} // namespace

//------------------------------------------------------------------------
int main ()
{
	const Rect panel { 0, 0, kEditorWidth, kEditorHeight };

	std::vector<Placed> placed;
	for (const auto& c : kControls)
		placed.push_back ({ nameOf (c.tag), controlRect (c) });
	placed.push_back ({ "the vowel chart", kPadView });
	for (int v = 0; v < kVowelCount; ++v)
		placed.push_back ({ std::string ("vowel button ") + kVowelsMale[v].name, vowelButtonRect (v) });
	placed.push_back ({ "the Manual button", kManualButton });
	placed.push_back ({ "the glide meter", kGlideMeter });
	placed.push_back ({ "the spectrum", kSpectrumView });

	//--------------------------------------------------------------------
	section ("1. Every parameter with a control appears exactly once");
	//--------------------------------------------------------------------
	{
		for (ParamID id = 0; id < kNumStoredParams; ++id)
		{
			int count = 0;
			for (const auto& c : kControls)
				if (c.tag == id)
					++count;
			if (id == kVowel)
				check (count == 0, "Vowel has no table control - the buttons and the chart write it");
			else
				check (count == 1, std::string (nameOf (id)) + " has exactly one control");
		}
		int voice = 0;
		for (const auto& c : kControls)
			if (c.tag == kVoice)
				++voice;
		check (voice == 1, "Voice has exactly one control");

		bool noLive = true;
		for (const auto& c : kControls)
			if (isLiveParam (c.tag))
				noLive = false;
		check (noLive, "no published (read-only) value has a control");
	}

	//--------------------------------------------------------------------
	section ("2. Everything is on the panel, in a section, and nothing overlaps");
	//--------------------------------------------------------------------
	{
		for (const auto& p : placed)
		{
			check (contains (panel, p.r), p.name + " is on the panel");

			bool inSection = false;
			for (const auto& s : kSections)
				if (contains (inside (s.r), p.r))
					inSection = true;
			check (inSection, p.name + " is inside a section");
		}

		for (size_t i = 0; i < placed.size (); ++i)
			for (size_t j = i + 1; j < placed.size (); ++j)
				if (overlaps (placed[i].r, placed[j].r))
					check (false, placed[i].name + " overlaps " + placed[j].name);
		check (true, "pairwise overlap scan done");

		for (int i = 0; i < kNumSections; ++i)
		{
			check (contains (panel, kSections[i].r), std::string (kSections[i].title) + " is on the panel");
			check (kSections[i].r.top >= kHeaderHeight + 8, std::string (kSections[i].title) + " is below the header");
			for (int j = i + 1; j < kNumSections; ++j)
				check (!overlaps (kSections[i].r, kSections[j].r),
				       std::string (kSections[i].title) + " does not overlap " + kSections[j].title);
		}

		check (contains ({ 0, 0, kEditorWidth, kHeaderHeight }, kBypassPill), "the bypass pill is in the header");
		check (contains ({ 0, 0, kEditorWidth, kHeaderHeight }, kEngineLine), "the engine line is in the header");
		check (!overlaps (kEngineLine, kBypassPill), "the engine line stops before the bypass pill");
		check (!overlaps (kEngineLine, kWordmark), "the engine line starts after the wordmark");
		const double wordmark = Lettering::width ("VOCAL", kTextWordmark) + kWordmarkGap +
		                        Lettering::width ("FILTER", kTextWordmark);
		check (kWordmark.left + wordmark <= kWordmark.right,
		       "the wordmark (" + px (wordmark) + " px) fits its box");
		check (kTextWordmark.height () <= kHeaderHeight - 16, "the wordmark clears the header's edges");
	}

	//--------------------------------------------------------------------
	section ("3. Every reading, label and caption fits, in the lettering");
	//--------------------------------------------------------------------
	{
		for (const auto& c : kControls)
		{
			if (c.kind != Kind::Knob)
				continue;
			// Centred under the knob; 2 px either side may run past the
			// rectangle - the knobs beside it are further off than that.
			const double room = c.width + 4.0;
			std::string which;
			const double w = widestReading (c.tag, &which);
			check (w <= room, std::string (nameOf (c.tag)) + ": widest reading \"" + which + "\" (" + px (w) +
			                      " px) fits " + px (room));
			const double lw = Lettering::width (c.label, kTextLabel);
			check (lw <= room, std::string (nameOf (c.tag)) + ": label (" + px (lw) + " px) fits");
		}
		for (int i = 0; i < kNumCaptions; ++i)
		{
			const auto& cap = kCaptions[i];
			const auto& st = (cap.style == CaptionStyle::Note) ? kTextNote : kTextBlock;
			const double w = Lettering::width (cap.text, st);
			check (w <= cap.r.width (), std::string ("\"") + cap.text + "\" (" + px (w) + " px) fits its line");
			check (st.height () <= cap.r.height () + 2, std::string ("\"") + cap.text + "\" fits its line's height");
		}

		// ---- the section titles and subtitles ------------------------------
		for (const auto& sec : kSections)
		{
			const Box b = titleBox (sec);
			check (b.top >= sec.r.top + 4 && b.left >= sec.r.left + 8,
			       std::string (sec.title) + ": title box inside its section");
			double right = sec.r.right - kSectionPadX;
			for (const auto& c : kControls)
				if (c.kind == Kind::Switch && c.y < sec.r.top + 40 && c.x > sec.r.left && c.x < sec.r.right)
					right = std::min<double> (right, c.x - 8);
			const double end = (sec.subtitle && *sec.subtitle)
			                       ? subtitleLeft (sec) + Lettering::width (sec.subtitle, kTextSubtitle)
			                       : b.right;
			check (end <= right, std::string (sec.title) + ": title and subtitle end at " + px (end) +
			                         ", before " + px (right));
		}
		check (titleBox (kSections[1]).bottom <= kSpectrumView.top,
		       "the spectrum's title box stops above its glass");
		check (titleBox (kSections[0]).bottom <= kPadView.top, "the vowel space's title box stops above the chart");

		// ---- the header ------------------------------------------------------
		// The longest engine line the editor writes: the fastest rate, a
		// glide to the longest name at the longest time.
		const std::string engine = "Engine: 384000 Hz  \xC2\xB7  gliding to Uuuu  \xC2\xB7  2000 of 2000 ms";
		const double ew = Lettering::width (engine, kTextEngine);
		check (ew <= kEngineLine.width (), "the longest engine line (" + px (ew) + " px) fits");
		check (allDrawn (engine), "the engine line has a glyph for every character");
		const double bw = 32.0 + Lettering::width ("BYPASS  OFF", kTextPill);
		check (bw <= kBypassPill.width () - 8, "BYPASS OFF fits its window");

		// ---- the keys ----------------------------------------------------------
		for (int v = 0; v < kVowelCount; ++v)
		{
			const Rect r = vowelButtonRect (v);
			const char* name = vowelSelectionName (v + 1);
			check (Lettering::width (name, kTextLabel) <= r.width () - 6, std::string (name) + ": name fits its key");
			check (Lettering::width (kVowelIpa[v], kTextIpaKey) <= r.width () - 20,
			       std::string (name) + ": symbol fits its key, clear of the lamp");
		}
		const double manual = 32.0 + Lettering::width ("MANUAL", kTextPill) + 16.0 +
		                      Lettering::width (kManualHint, kTextHint) + 12.0;
		check (manual <= kManualButton.width (), "MANUAL and its hint fit the key (" + px (manual) + " px)");
		for (int i = 0; i < kVoiceCount; ++i)
		{
			const Control& sw = kControls[0];
			check (Lettering::width (voiceName (i), kTextPill) <= (sw.width - 4) / 2.0 - 8,
			       std::string (voiceName (i)) + " fits its half of the Voice switch");
		}
	}

	//--------------------------------------------------------------------
	section ("4. The vowel chart IS the parameter space");
	//--------------------------------------------------------------------
	{
		check (std::fabs (padU (kPadF2Max)) < 1e-12 && std::fabs (padU (kPadF2Min) - 1.0) < 1e-12,
		       "F2's range spans the chart exactly, highest at the left");
		check (std::fabs (padV (kPadF1Min)) < 1e-12 && std::fabs (padV (kPadF1Max) - 1.0) < 1e-12,
		       "F1's range spans the chart exactly, lowest at the top");

		double worst = 0.0;
		for (double u = 0.0; u <= 1.0; u += 0.01)
			worst = std::max (worst, std::fabs (padU (f2OfPadU (u)) - u));
		for (double v = 0.0; v <= 1.0; v += 0.01)
			worst = std::max (worst, std::fabs (padV (f1OfPadV (v)) - v));
		check (worst < 1e-9, "position -> Hz -> position round-trips");

		bool allOn = true;
		for (int voice = 0; voice < kVoiceCount; ++voice)
			for (int v = 0; v < kVowelCount; ++v)
			{
				const auto& f = vowelTable (voice)[v].formants;
				const double u = padU (f[1].freqHz), w = padV (f[0].freqHz);
				if (u < 0.0 || u > 1.0 || w < 0.0 || w > 1.0)
					allOn = false;
			}
		check (allOn, "all ten vowels (both voices) land on the chart");

		// The vowels a phonetician expects at the corners are there.
		const auto* male = vowelTable (kVoiceMale);
		check (padU (male[1].formants[1].freqHz) < padU (male[4].formants[1].freqHz),
		       "/i/ is left of /u/ - front vowels on the left");
		check (padV (male[1].formants[0].freqHz) < padV (male[0].formants[0].freqHz),
		       "/i/ is above /a/ - close vowels at the top");

		// Two vowel circles never overlap, or a click could not tell them apart.
		const Rect p = padPlot ();
		for (int voice = 0; voice < kVoiceCount; ++voice)
		{
			double closest = 1e9;
			const auto* t = vowelTable (voice);
			for (int a = 0; a < kVowelCount; ++a)
				for (int b = a + 1; b < kVowelCount; ++b)
				{
					const double dx = (padU (t[a].formants[1].freqHz) - padU (t[b].formants[1].freqHz)) * p.width ();
					const double dy = (padV (t[a].formants[0].freqHz) - padV (t[b].formants[0].freqHz)) * p.height ();
					closest = std::min (closest, std::hypot (dx, dy));
				}
			char what[96];
			std::snprintf (what, sizeof (what), "%s vowel circles are apart (closest %.0f px, need %d)",
			               voiceName (voice), closest, 2 * kVowelCircleRadius + 2);
			check (closest >= 2 * kVowelCircleRadius + 2, what);
		}
	}

	//--------------------------------------------------------------------
	section ("5. The 70s sci-fi lettering");
	//--------------------------------------------------------------------
	{
		int parsed = 0, total = 0, onGrid = 0;
		for (uint32_t c = 1; c < 0x2600; ++c)
		{
			const Lettering::Glyph* g = Lettering::glyphFor (c);
			if (!g)
				continue;
			++total;
			const auto cmds = Lettering::parse (g->path);
			if (!cmds.empty ())
				++parsed;
			bool inside = true;
			for (const auto& k : cmds)
				for (double x : { k.x, k.x1 })
					inside = inside && x >= -0.5 && x <= g->advance + 0.5;
			for (const auto& k : cmds)
				for (double y : { k.y, k.y1 })
					inside = inside && y >= -0.5 && y <= 7.1;
			if (inside)
				++onGrid;
		}
		check (parsed == total, "every glyph parses (" + std::to_string (parsed) + " of " + std::to_string (total) + ")");
		check (onGrid == total, "every glyph stays on its grid");
		bool az = true;
		for (char c = 'A'; c <= 'Z'; ++c)
			az = az && Lettering::glyphFor (static_cast<uint32_t> (c));
		for (char c = '0'; c <= '9'; ++c)
			az = az && Lettering::glyphFor (static_cast<uint32_t> (c));
		check (az, "A-Z and 0-9 are all there");

		// The IPA keeps its case; /i/ and /I/ are different glyphs.
		for (int v = 0; v < kVowelCount; ++v)
			check (allDrawn (kVowelIpa[v], true), std::string ("the IPA for ") + vowelSelectionName (v + 1) + " is drawn");
		const auto i = Lettering::codePoints ("i", true), ii = Lettering::codePoints ("\xC9\xAA", true);
		check (i.size () == 1 && ii.size () == 1 && i[0] != ii[0] &&
		       std::string (Lettering::glyphFor (i[0])->path) != Lettering::glyphFor (ii[0])->path,
		       "/i/ and /I/ are drawn differently");
		check (Lettering::codePoints ("i")[0] == 'I', "lower case is still drawn in capitals elsewhere");

		// Every fixed word on the panel has its glyphs.
		bool all = true;
		std::string missing;
		auto need = [&] (const std::string& t) { if (!allDrawn (t)) { all = false; missing = t; } };
		for (const auto& sec : kSections) { need (sec.title); need (sec.subtitle); }
		for (const auto& c : kCaptions) need (c.text);
		for (const auto& c : kControls) need (c.label);
		for (int v = 0; v <= kVowelCount; ++v) need (vowelSelectionName (v));
		need (kManualHint);
		need ("F1 \xE2\x86\x93 close \xE2\x80\xA6 open");
		need ("\xE2\x86\x90 F2 front \xE2\x80\xA6 back");
		need ("\xE2\x97\x8B female means");
		need ("targets  \xC2\xB7  no live values from the host");
		need ("live  \xC2\xB7  mid-glide");
		check (all, "every fixed word has a glyph for every character" + (all ? std::string () : " - not \"" + missing + "\""));
	}

	//--------------------------------------------------------------------
	section ("6. The wear - mild, on the panel, off the glass");
	//--------------------------------------------------------------------
	{
		const Wear::PanelWear a = Wear::panelWear (), b = Wear::panelWear ();
		check (a.specks.size () == b.specks.size () && a.scratches.size () == b.scratches.size () &&
		       !a.specks.empty () && a.specks.back ().x == b.specks.back ().x,
		       "the same marks every time (" + std::to_string (a.specks.size ()) + " specks, " +
		       std::to_string (a.scratches.size ()) + " scratches)");

		bool inPanel = true, offGlass = true, faint = true;
		auto onGlass = [] (double x, double y, double pad) {
			for (const auto& g : Wear::Detail::kGlass)
				if (Wear::Detail::inRect (g, x, y, pad))
					return true;
			return false;
		};
		for (const auto& sp : a.specks)
		{
			inPanel = inPanel && sp.x > 0 && sp.y > 0 && sp.x < kEditorWidth && sp.y < kEditorHeight;
			offGlass = offGlass && !onGlass (sp.x, sp.y, 0.5);
			faint = faint && sp.alpha <= Wear::kMaxMarkAlpha;
		}
		for (const auto* set : { &a.scratches, &a.fibres })
			for (const auto& l : *set)
			{
				faint = faint && l.alpha <= Wear::kMaxMarkAlpha && l.width <= 1.4;
				for (const auto& p : l.pts)
				{
					inPanel = inPanel && p.x > 0 && p.y > 0 && p.x < kEditorWidth && p.y < kEditorHeight;
					offGlass = offGlass && !onGlass (p.x, p.y, 1.0);
				}
			}
		for (const auto& h : a.haze)
			faint = faint && h.alpha <= Wear::kMaxHazeAlpha;
		check (inPanel, "every panel mark is on the panel");
		check (offGlass, "no panel mark on either screen's glass");
		check (faint, "every panel mark is faint");

		bool glassOk = true, printsFaint = true;
		for (int i = 0; i < Wear::Detail::kNumGlass; ++i)
		{
			const Rect g = Wear::Detail::kGlass[i];
			const Wear::GlassWear w = Wear::glassWear (i);
			for (const auto& d : w.dust)
				glassOk = glassOk && Wear::Detail::inRect (g, d.x, d.y);
			for (const auto& p : w.prints)
				for (const auto& r : p.ridges)
				{
					printsFaint = printsFaint && r.alpha <= Wear::kMaxPrintAlpha;
					for (const auto& q : r.pts)
						glassOk = glassOk && Wear::Detail::inRect (g, q.x, q.y);
				}
			check (w.prints.size () == 1, std::string (i ? "the spectrum" : "the vowel chart") + " has one fingerprint");
		}
		check (glassOk, "every glass mark is on its own screen");
		check (printsFaint, "the fingerprints are faint");

		bool knobsOk = true;
		for (int i = 0; i < kNumControls; ++i)
		{
			if (kControls[i].kind != Kind::Knob)
				continue;
			const Wear::KnobWear k = Wear::knobWear (i);
			for (const auto& s : k.scuffs)
				knobsOk = knobsOk && s.radius > 0 && s.radius < 1 && s.alpha <= Wear::kMaxMarkAlpha;
			for (const auto& g : k.grit)
				knobsOk = knobsOk && g.radius > 0 && g.radius < 1;
			knobsOk = knobsOk && k.sheen <= Wear::kMaxHazeAlpha;
		}
		check (knobsOk, "every knob mark is on its cap or knurling, and faint");
		check (Wear::controlIndex (kGlide) >= 0 && Wear::controlIndex (999999) == -1, "controlIndex finds knobs");
	}

	std::printf ("\n%s - %d failure(s)\n", gFailures == 0 ? "PASS" : "FAIL", gFailures);
	return gFailures == 0 ? 0 : 1;
}
