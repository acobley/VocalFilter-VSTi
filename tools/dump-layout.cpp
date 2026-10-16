//------------------------------------------------------------------------
// VocalFilter - dump the panel's layout and state as JSON
//
// tools/render-panel.py draws docs/panel.png from this. The geometry
// comes from VocalFilterLayout.h - the tables the editor builds from - and
// every curve and value from the DSP's own functions, so the picture is
// the panel as built, not a drawing of it.
//
//     c++ -std=c++17 -O2 -Isource -Iexternal/vst3sdk tools/dump-layout.cpp
//         source/VocalFilterParams.cpp source/VocalFilterDsp.cpp -o /tmp/dump-layout
//     /tmp/dump-layout [selector 0..5] [voice 0..1] > layout.json
//------------------------------------------------------------------------

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

} // namespace

int main (int argc, char** argv)
{
	const int selector = (argc > 1) ? std::atoi (argv[1]) : kVowelManual;
	const int voice = (argc > 2) ? std::atoi (argv[2]) : kVoiceMale;
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

	std::printf ("\"sections\": [\n");
	for (int i = 0; i < kNumSections; ++i)
	{
		const auto& s = kSections[i];
		std::printf ("  {\"title\": \"%s\", \"subtitle\": \"%s\", \"accent\": \"%s\", \"r\": [%d, %d, %d, %d]}%s\n",
		             esc (s.title).c_str (), esc (s.subtitle).c_str (), accentName (s.accent),
		             s.r.left, s.r.top, s.r.right, s.r.bottom, i + 1 < kNumSections ? "," : "");
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
		if (preset && isFormantParam (c.tag))
		{
			const int k = (c.tag - kFormantBase) / 3, field = (c.tag - kFormantBase) % 3;
			const double plain = field == 0 ? preset[k].freqHz : field == 1 ? preset[k].bandwidthHz : preset[k].levelDb;
			normalized = def.toNormalized (plain);
		}
		std::printf ("  {\"kind\": \"%s\", \"accent\": \"%s\", \"label\": \"%s\", \"diameter\": %d, "
		             "\"value\": %.6f, \"reading\": \"%s\", \"r\": [%d, %d, %d, %d]}%s\n",
		             c.kind == Kind::Knob ? "knob" : "switch", accentName (c.accent), esc (c.label).c_str (),
		             c.diameter, normalized, readout (def, normalized).c_str (),
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
