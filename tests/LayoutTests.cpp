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
//------------------------------------------------------------------------

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
int widestReading (ParamID tag)
{
	const ParamDef& def = paramDef (tag);
	const std::string units = def.units ? def.units : "";
	int widest = 0;
	for (double plain : { def.plainMin, def.plainMax, def.plainDefault })
	{
		char buffer[32];
		if (units == "dB")
			std::snprintf (buffer, sizeof (buffer), "%.1f dB", plain);
		else
			std::snprintf (buffer, sizeof (buffer), "%.0f %s", plain, units.c_str ());
		widest = std::max (widest, static_cast<int> (std::strlen (buffer)));
	}
	return widest;
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
	}

	//--------------------------------------------------------------------
	section ("3. Every reading and label fits its knob");
	//--------------------------------------------------------------------
	{
		for (const auto& c : kControls)
		{
			if (c.kind != Kind::Knob)
				continue;
			// Knob::draw lets the text run 4 pixels past each side.
			const int room = c.width + 8;
			check (widestReading (c.tag) * kValueCharWidth <= room,
			       std::string (nameOf (c.tag)) + ": widest reading fits");
			check (static_cast<int> (std::strlen (c.label)) * kLabelCharWidth <= room,
			       std::string (nameOf (c.tag)) + ": label fits");
		}
		for (int i = 0; i < kNumCaptions; ++i)
		{
			const auto& cap = kCaptions[i];
			if (cap.style == CaptionStyle::Note)
				check (static_cast<int> (std::strlen (cap.text)) * kNoteCharWidth <= cap.r.width () + 12,
				       std::string ("\"") + cap.text + "\" fits its line");
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

	std::printf ("\n%s - %d failure(s)\n", gFailures == 0 ? "PASS" : "FAIL", gFailures);
	return gFailures == 0 ? 0 : 1;
}
