//------------------------------------------------------------------------
// VocalFilter - dump the panel's layout and state as JSON
//
// tools/render-panel.py draws docs/panel.png from this. The geometry
// comes from VocalFilterLayout.h - the tables the editor builds from - and
// every curve and value from the DSP's own functions, so the picture is
// the panel as built, not a drawing of it. Since the black panel (10 Nov
// 2026) it also prints the lettering's styles and every glyph, parsed by
// SciFiLettering.h, and every mark of the wear from PanelWear.h, so the
// script keeps no copy of the face and no seed of its own.
//
//     c++ -std=c++17 -O2 -Isource -Iexternal/vst3sdk tools/dump-layout.cpp
//         source/VocalFilterParams.cpp source/VocalFilterDsp.cpp -o /tmp/dump-layout
//     /tmp/dump-layout [selector 0..5] [voice 0..1] > layout.json
//------------------------------------------------------------------------

#include "PanelWear.h"
#include "VocalFilterLayout.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace VocalFilter;
using namespace VocalFilter::Layout;

namespace {

const char* accentName (Accent a)
{
	switch (a)
	{
		case Accent::Amber:  return "amber";
		case Accent::Blue:   return "blue";
		case Accent::Violet: return "violet";
		case Accent::Mint:   return "mint";
	}
	return "amber";
}

std::string esc (const char* s)
{
	std::string out;
	for (; *s; ++s)
	{
		if (*s == '"' || *s == '\\')
			out += '\\';
		out += *s;
	}
	return out;
}

void rect (const char* key, const Rect& r, bool comma = true)
{
	std::printf ("\"%s\": [%d, %d, %d, %d]%s\n", key, r.left, r.top, r.right, r.bottom, comma ? "," : "");
}

/** The editor's readout(), verbatim. */
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

std::string rectStr (const Rect& r)
{
	return "[" + std::to_string (r.left) + "," + std::to_string (r.top) + "," + std::to_string (r.right) + "," +
	       std::to_string (r.bottom) + "]";
}

std::string num (double v)
{
	char t[32];
	std::snprintf (t, sizeof (t), "%.3f", v);
	return t;
}

std::string style (const Lettering::Style& s)
{
	return "{\"cap\": " + num (s.cap) + ", \"weight\": " + num (s.weight) + ", \"track\": " + num (s.track) +
	       ", \"keepCase\": " + (s.keepCase ? "true" : "false") + "}";
}

std::string box (const Box& b)
{
	return "[" + num (b.left) + "," + num (b.top) + "," + num (b.right) + "," + num (b.bottom) + "]";
}

std::string commands (const char* path)
{
	std::string out = "[";
	bool first = true;
	for (const auto& c : Lettering::parse (path))
	{
		out += first ? "" : ",";
		first = false;
		out += "[\"" + std::string (1, c.op) + "\"," + num (c.x1) + "," + num (c.y1) + "," + num (c.x) + "," + num (c.y) + "]";
	}
	return out + "]";
}

std::string pts (const std::vector<Wear::Pt>& v)
{
	std::string out = "[";
	char t[48];
	for (size_t i = 0; i < v.size (); ++i)
	{
		std::snprintf (t, sizeof (t), "%s%.3f,%.3f", i ? "," : "", v[i].x, v[i].y);
		out += t;
	}
	return out + "]";
}

std::string lines (const std::vector<Wear::Line>& v)
{
	std::string out = "[";
	for (size_t i = 0; i < v.size (); ++i)
		out += std::string (i ? "," : "") + "{\"w\": " + num (v[i].width) + ", \"a\": " +
		       std::to_string (v[i].alpha) + ", \"pts\": " + pts (v[i].pts) + "}";
	return out + "]";
}

std::string specks (const std::vector<Wear::Speck>& v)
{
	std::string out = "[";
	for (size_t i = 0; i < v.size (); ++i)
		out += std::string (i ? "," : "") + "[" + num (v[i].x) + "," + num (v[i].y) + "," + num (v[i].r) + "," +
		       std::to_string (v[i].alpha) + "]";
	return out + "]";
}

std::string polar (const std::vector<Wear::PolarSpeck>& v)
{
	std::string out = "[";
	for (size_t i = 0; i < v.size (); ++i)
		out += std::string (i ? "," : "") + "[" + num (v[i].radius) + "," + num (v[i].angle) + "," + num (v[i].r) +
		       "," + std::to_string (v[i].alpha) + "]";
	return out + "]";
}

std::string rgb (const Wear::Rgb& c)
{
	return "[" + std::to_string (c.r) + "," + std::to_string (c.g) + "," + std::to_string (c.b) + "]";
}

void printWear ()
{
	using namespace Wear;
	const PanelWear w = panelWear ();
	std::printf ("  \"wear\": {\n");
	std::printf ("    \"colours\": {\"dust\": %s, \"speck\": %s, \"scratch\": %s, \"scuff\": %s, \"knurlDust\": %s, "
	             "\"glassDust\": %s, \"print\": %s, \"sheen\": %s},\n",
	             rgb (kDust).c_str (), rgb (kSpeck).c_str (), rgb (kScratch).c_str (), rgb (kScuff).c_str (),
	             rgb (kKnurlDust).c_str (), rgb (kGlassDust).c_str (), rgb (kPrint).c_str (), rgb (kSheen).c_str ());

	std::string haze = "[";
	for (size_t i = 0; i < w.haze.size (); ++i)
		haze += std::string (i ? "," : "") + "[" + num (w.haze[i].x) + "," + num (w.haze[i].y) + "," +
		        num (w.haze[i].r) + "," + std::to_string (w.haze[i].alpha) + "]";
	std::printf ("    \"haze\": %s],\n", haze.c_str ());

	std::string drifts = "[";
	for (size_t i = 0; i < w.drifts.size (); ++i)
		drifts += std::string (i ? "," : "") + "[" + rectStr (w.drifts[i].r) + "," + std::to_string (w.drifts[i].fromX) +
		          "," + std::to_string (w.drifts[i].fromY) + "," + std::to_string (w.drifts[i].alpha) + "]";
	std::printf ("    \"drifts\": %s],\n", drifts.c_str ());

	std::string rings = "[";
	for (size_t i = 0; i < w.rings.size (); ++i)
	{
		const auto& f = w.rings[i];
		rings += std::string (i ? "," : "") + "[" + num (f.cx) + "," + num (f.cy) + "," + num (f.inner) + "," +
		         num (f.peak) + "," + num (f.outer) + "," + std::to_string (f.alpha) + "]";
	}
	std::printf ("    \"rings\": %s],\n", rings.c_str ());
	std::printf ("    \"scratches\": %s,\n", lines (w.scratches).c_str ());
	std::printf ("    \"specks\": %s,\n", specks (w.specks).c_str ());
	std::printf ("    \"fibres\": %s,\n", lines (w.fibres).c_str ());

	// one entry per control, in kControls order: a knob's wear, a
	// selector's grit, or nothing for the switch
	std::printf ("    \"controls\": [\n");
	for (int i = 0; i < kNumControls; ++i)
	{
		const char* end = (i + 1 < kNumControls) ? "," : "";
		if (kControls[i].kind == Kind::Knob)
		{
			const KnobWear k = knobWear (i);
			std::string scuffs = "[";
			for (size_t j = 0; j < k.scuffs.size (); ++j)
			{
				const auto& a = k.scuffs[j];
				scuffs += std::string (j ? "," : "") + "[" + num (a.radius) + "," + num (a.start) + "," +
				          num (a.span) + "," + num (a.width) + "," + std::to_string (a.alpha) + "]";
			}
			std::printf ("      {\"scuffs\": %s], \"grit\": %s, \"sheen\": %d, \"knurl\": %d}%s\n", scuffs.c_str (),
			             polar (k.grit).c_str (), k.sheen, k.knurlAlpha, end);
		}
		else if (kControls[i].kind == Kind::Selector)
			std::printf ("      {\"grit\": %s}%s\n", polar (selectorGrit (i)).c_str (), end);
		else
			std::printf ("      {}%s\n", end);
	}
	std::printf ("    ],\n");

	// one entry per screen, in Wear::kGlass order: the chart, the spectrum
	const int screens = Detail::kNumGlass;
	std::printf ("    \"glass\": [\n");
	for (int i = 0; i < screens; ++i)
	{
		const GlassWear g = glassWear (i);
		std::string prints = "[";
		for (size_t j = 0; j < g.prints.size (); ++j)
			prints += std::string (j ? "," : "") + lines (g.prints[j].ridges);
		std::printf ("      {\"dust\": %s, \"fibres\": %s, \"prints\": %s]}%s\n", specks (g.dust).c_str (),
		             lines (g.fibres).c_str (), prints.c_str (), (i + 1 < screens) ? "," : "");
	}
	std::printf ("    ]\n},\n");
}

} // namespace

int main (int argc, char** argv)
{
	const int selector = (argc > 1) ? std::atoi (argv[1]) : kVowelManual;
	const int voice = (argc > 2) ? std::atoi (argv[2]) : kVoiceMale;
	const int levelComp = (argc > 3) ? std::atoi (argv[3]) : kLevelCompOff;
	const double sampleRate = 48000.0;

	// What the DSP would be using: the preset, or the parameters' defaults.
	FormantSetting shown[kFormantCount];
	const FormantSetting* preset = vowelSelection (selector, voice);
	for (int k = 0; k < kFormantCount; ++k)
		shown[k] = preset ? preset[k] : kAaaFormants[k];

	std::printf ("{\n");
	std::printf ("\"size\": [%d, %d],\n", kEditorWidth, kEditorHeight);
	std::printf ("\"header\": %d,\n", kHeaderHeight);
	std::printf ("\"selector\": %d, \"voice\": %d,\n", selector, voice);
	std::printf ("\"engine\": \"Engine: %.0f Hz  \xC2\xB7  %s\",\n", sampleRate,
	             selector == kVowelManual ? "Manual"
	                                      : (std::string (vowelSelectionName (selector)) + ", " +
	                                         (voice == kVoiceFemale ? "female" : "male")).c_str ());
	rect ("wordmark", kWordmark);
	rect ("engineLine", kEngineLine);
	rect ("bypass", kBypassPill);
	std::printf ("\"headerDivider\": %d, \"wordmarkGap\": %d, \"sectionRadius\": %d,\n", kHeaderDivider,
	             kWordmarkGap, kSectionRadius);
	std::printf ("\"knobGap\": %d, \"knobValueH\": %d, \"knobLabelH\": %d,\n", kKnobGap, kKnobValueH, kKnobLabelH);
	std::printf ("\"levelComp\": %d,\n", levelComp);
	std::printf ("\"manualHint\": \"%s\",\n", esc (kManualHint).c_str ());
	std::printf ("\"screws\": [");
	for (size_t i = 0; i < sizeof (kScrews) / sizeof (kScrews[0]); ++i)
		std::printf ("%s[%d,%d]", i ? "," : "", kScrews[i].x, kScrews[i].y);
	std::printf ("],\n");

	// ---- the lettering: the styles, and every glyph parsed -----------------
	std::printf ("\"styles\": {\"wordmark\": %s, \"title\": %s, \"subtitle\": %s, \"block\": %s, \"note\": %s,\n"
	             "  \"label\": %s, \"value\": %s, \"engine\": %s, \"pill\": %s, \"hint\": %s, \"screen\": %s,\n"
	             "  \"scale\": %s, \"ipaKey\": %s, \"ipaChart\": %s},\n",
	             style (kTextWordmark).c_str (), style (kTextTitle).c_str (), style (kTextSubtitle).c_str (),
	             style (kTextBlock).c_str (), style (kTextNote).c_str (), style (kTextLabel).c_str (),
	             style (kTextValue).c_str (), style (kTextEngine).c_str (), style (kTextPill).c_str (),
	             style (kTextHint).c_str (), style (kTextScreen).c_str (), style (kTextScale).c_str (),
	             style (kTextIpaKey).c_str (), style (kTextIpaChart).c_str ());
	std::printf ("\"space\": %s,\n\"glyphs\": {\n", num (Lettering::kSpace).c_str ());
	for (uint32_t c = 1; c < 0x2600; ++c)
		if (const Lettering::Glyph* g = Lettering::glyphFor (c))
			std::printf ("  \"%u\": [%s, %s],\n", c, num (g->advance).c_str (), commands (g->path).c_str ());
	std::printf ("  \"missing\": [%s, %s]\n},\n", num (Lettering::missingGlyph ().advance).c_str (),
	             commands (Lettering::missingGlyph ().path).c_str ());

	printWear ();

	std::printf ("\"sections\": [\n");
	for (int i = 0; i < kNumSections; ++i)
	{
		const auto& s = kSections[i];
		std::printf ("  {\"title\": \"%s\", \"subtitle\": \"%s\", \"accent\": \"%s\", \"r\": [%d, %d, %d, %d], "
		             "\"titleBox\": %s, \"subtitleLeft\": %s}%s\n",
		             esc (s.title).c_str (), esc (s.subtitle).c_str (), accentName (s.accent),
		             s.r.left, s.r.top, s.r.right, s.r.bottom, box (titleBox (s)).c_str (),
		             num (subtitleLeft (s)).c_str (), i + 1 < kNumSections ? "," : "");
	}
	std::printf ("],\n");

	std::printf ("\"controls\": [\n");
	for (int i = 0; i < kNumControls; ++i)
	{
		const auto& c = kControls[i];
		const Rect r = controlRect (c);
		const ParamDef& def = paramDef (c.tag);
		double normalized = def.defaultNormalized ();
		if (c.tag == kVoice)
			normalized = voice;
		if (c.tag == kLevelComp)
			normalized = levelComp ? 1.0 : 0.0;
		if (preset && isFormantParam (c.tag))
		{
			const int k = (c.tag - kFormantBase) / 3, field = (c.tag - kFormantBase) % 3;
			const double plain = field == 0 ? preset[k].freqHz : field == 1 ? preset[k].bandwidthHz : preset[k].levelDb;
			normalized = def.toNormalized (plain);
		}
		// A switch or selector: its position NAMES; a selector also its
		// geometry and - Level Comp - its reading, from the same model and
		// format the editor uses, at the factory Dry / Wet.
		std::string names = "[";
		std::string reading = readout (def, normalized);
		std::string geometry = "null";
		if (c.kind == Kind::Selector)
		{
			const int count = static_cast<int> (def.plainMax) + 1;
			const Selector g = selectorOf (c, count);
			geometry = "{\"cx\": " + num (g.cx) + ", \"cy\": " + num (g.cy) + ", \"radius\": " + num (g.radius) +
			           ", \"readingY\": " + num (g.readingCentreY) + ", \"nameY\": " + num (g.nameCentreY) +
			           ", \"angles\": [";
			std::string boxes = "[";
			for (int n = 0; n < count; ++n)
			{
				const std::string nm = enumChoiceName (c.tag, n);
				std::string up = nm;
				for (char& ch : up)
					if (ch >= 'a' && ch <= 'z') ch = static_cast<char> (ch - 'a' + 'A');
				geometry += std::string (n ? ", " : "") + num (g.angle[n]);
				boxes += std::string (n ? ", " : "") + box (selectorLabelBox (g, n, up));
			}
			geometry += "], \"labelBoxes\": " + boxes + "]}";
		}
		if (c.kind != Kind::Knob)
		{
			for (int n = 0; n <= static_cast<int> (def.plainMax); ++n)
				names += std::string (n ? ", " : "") + "\"" + esc (enumChoiceName (c.tag, n)) + "\"";
			normalized = std::floor (normalized * def.plainMax + 0.5);   // the index
			if (c.tag == kLevelComp)
				reading = levelCompReading (levelComp ? levelCompensationDb (shown, kFormantCount, kMixDefault,
				                                                             sampleRate)
				                                      : 0.0);
		}
		names += "]";
		std::printf ("  {\"kind\": \"%s\", \"accent\": \"%s\", \"label\": \"%s\", \"diameter\": %d, "
		             "\"value\": %.6f, \"reading\": \"%s\", \"names\": %s, \"selector\": %s, "
		             "\"r\": [%d, %d, %d, %d]}%s\n",
		             c.kind == Kind::Knob ? "knob" : c.kind == Kind::Selector ? "selector" : "switch",
		             accentName (c.accent), esc (c.label).c_str (),
		             c.diameter, normalized, reading.c_str (), names.c_str (), geometry.c_str (),
		             r.left, r.top, r.right, r.bottom, i + 1 < kNumControls ? "," : "");
	}
	std::printf ("],\n");

	std::printf ("\"captions\": [\n");
	for (int i = 0; i < kNumCaptions; ++i)
	{
		const auto& c = kCaptions[i];
		std::printf ("  {\"text\": \"%s\", \"style\": \"%s\", \"accent\": \"%s\", \"r\": [%d, %d, %d, %d]}%s\n",
		             esc (c.text).c_str (), c.style == CaptionStyle::Block ? "block" : "note",
		             accentName (c.accent), c.r.left, c.r.top, c.r.right, c.r.bottom,
		             i + 1 < kNumCaptions ? "," : "");
	}
	std::printf ("],\n");

	std::printf ("\"rules\": [");
	const int nRules = static_cast<int> (sizeof (kRules) / sizeof (kRules[0]));
	for (int i = 0; i < nRules; ++i)
		std::printf ("[%d, %d, %d, %d]%s", kRules[i].left, kRules[i].top, kRules[i].right, kRules[i].bottom,
		             i + 1 < nRules ? ", " : "");
	std::printf ("],\n");

	rect ("pad", kPadView);
	rect ("padPlot", padPlot ());
	std::printf ("\"vowelRadius\": %d,\n", kVowelCircleRadius);
	std::printf ("\"vowelButtons\": [");
	for (int v = 0; v < kVowelCount; ++v)
	{
		const Rect r = vowelButtonRect (v);
		std::printf ("[%d, %d, %d, %d]%s", r.left, r.top, r.right, r.bottom, v + 1 < kVowelCount ? ", " : "");
	}
	std::printf ("],\n");
	rect ("manual", kManualButton);
	rect ("glideMeter", kGlideMeter);
	rect ("spectrum", kSpectrumView);

	// The vowels, both voices, already placed on the chart (u, v in 0..1).
	std::printf ("\"vowels\": [\n");
	for (int vc = 0; vc < kVoiceCount; ++vc)
	{
		std::printf ("  [");
		for (int v = 0; v < kVowelCount; ++v)
		{
			const auto& f = vowelTable (vc)[v].formants;
			std::printf ("{\"name\": \"%s\", \"ipa\": \"%s\", \"u\": %.6f, \"v\": %.6f}%s",
			             vowelTable (vc)[v].name, kVowelIpa[v], padU (f[1].freqHz), padV (f[0].freqHz),
			             v + 1 < kVowelCount ? ", " : "");
		}
		std::printf ("]%s\n", vc + 1 < kVoiceCount ? "," : "");
	}
	std::printf ("],\n");
	std::printf ("\"trapezoid\": [%d, %d, %d, %d, %d],\n", kTrapezoidOrder[0], kTrapezoidOrder[1],
	             kTrapezoidOrder[2], kTrapezoidOrder[3], kTrapezoidOrder[4]);

	// Chart tick positions.
	std::printf ("\"f2Ticks\": [");
	const double f2t[] = { 2500, 2000, 1500, 1000, 700 };
	for (int i = 0; i < 5; ++i)
		std::printf ("[%g, %.6f]%s", f2t[i], padU (f2t[i]), i < 4 ? ", " : "");
	std::printf ("],\n\"f1Ticks\": [");
	const double f1t[] = { 300, 500, 700, 1000 };
	for (int i = 0; i < 4; ++i)
		std::printf ("[%g, %.6f]%s", f1t[i], padV (f1t[i]), i < 3 ? ", " : "");
	std::printf ("],\n");

	std::printf ("\"dot\": {\"f1\": %.3f, \"f2\": %.3f, \"u\": %.6f, \"v\": %.6f},\n",
	             shown[0].freqHz, shown[1].freqHz, padU (shown[1].freqHz), padV (shown[0].freqHz));

	// The spectrum: axis fractions and the four curves, in dB, from the
	// DSP's functions.
	std::printf ("\"spectrumAxes\": {\"minHz\": %g, \"maxHz\": %g, \"topDb\": %g, \"floorDb\": %g},\n",
	             kSpectrumMinHz, kSpectrumMaxHz, kSpectrumTopDb, kSpectrumFloorDb);
	std::printf ("\"formants\": [");
	for (int k = 0; k < kFormantCount; ++k)
		std::printf ("[%.3f, %.3f, %.3f]%s", shown[k].freqHz, shown[k].bandwidthHz, shown[k].levelDb,
		             k + 1 < kFormantCount ? ", " : "");
	std::printf ("],\n");
	std::printf ("\"curves\": {\"hz\": [");
	const int n = kSpectrumPoints;
	for (int i = 0; i <= n; ++i)
		std::printf ("%.4f%s", kSpectrumMinHz * std::pow (kSpectrumMaxHz / kSpectrumMinHz, double (i) / n),
		             i < n ? ", " : "");
	std::printf ("],\n");
	for (int k = 0; k < kFormantCount; ++k)
	{
		std::printf ("  \"f%d\": [", k + 1);
		for (int i = 0; i <= n; ++i)
		{
			const double hz = kSpectrumMinHz * std::pow (kSpectrumMaxHz / kSpectrumMinHz, double (i) / n);
			const double m = bandpassMagnitude (shown[k].freqHz, shown[k].bandwidthHz, hz, sampleRate);
			std::printf ("%.4f%s", shown[k].levelDb + 20.0 * std::log10 (std::max (m, 1e-9)), i < n ? ", " : "");
		}
		std::printf ("],\n");
	}
	std::printf ("  \"sum\": [");
	for (int i = 0; i <= n; ++i)
	{
		const double hz = kSpectrumMinHz * std::pow (kSpectrumMaxHz / kSpectrumMinHz, double (i) / n);
		std::printf ("%.4f%s", 20.0 * std::log10 (std::max (bankMagnitude (shown, kFormantCount, hz, sampleRate), 1e-9)),
		             i < n ? ", " : "");
	}
	std::printf ("]}\n}\n");
	return 0;
}
