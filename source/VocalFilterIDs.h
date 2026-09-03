//------------------------------------------------------------------------
// VocalFilter - class UIDs and message identifiers
//
// These UIDs were generated fresh for this plug-in from os.urandom. NEVER
// CHANGE THEM once a build has been shipped: hosts store them in the
// project file, so a changed UID means every existing session silently
// loses the plug-in.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace VocalFilter {

static const Steinberg::FUID kVocalFilterProcessorUID  (0xE0326336, 0xA4AF838F, 0x5B8E1DC8, 0x1EAD6DB2);
static const Steinberg::FUID kVocalFilterControllerUID (0xF0DB05A3, 0xC1D41B10, 0x33F8F2CE, 0x58110AB3);

// The plug-in category is declared once, in VocalFilterEntry.cpp, using the
// SDK's own PlugType::kFx - not repeated as a string here.

//------------------------------------------------------------------------
// Processor <-> controller messages
//
// None yet. When you add one, remember that every message travels on the UI
// thread: sendMessage from process() returns success and is then discarded
// by the host's connection proxy. Anything the DSP produces per block goes
// out through data.outputParameterChanges with a kIsReadOnly parameter
// instead. Messages are fine from setActive, setState and notify.
//
// static const char* const kVocalFilterSampleRateMessage = "VocalFilterSampleRate";
//------------------------------------------------------------------------

//------------------------------------------------------------------------
} // namespace VocalFilter
