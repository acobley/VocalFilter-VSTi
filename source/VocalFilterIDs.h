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
// Every message here travels on the UI THREAD. sendMessage from process()
// returns success and is then silently discarded by the host's connection
// proxy - anything the DSP produces per block goes out through
// data.outputParameterChanges instead (see kLiveBase in the parameter
// header). Messages are fine from setActive, setState and notify, which is
// where the one below is sent from.
//------------------------------------------------------------------------

/** Processor -> controller, from setActive: the sample rate the DSP is
    actually running at.

    The response display needs it because a bandpass's shape is a function
    of f/fs, so a curve drawn at an assumed 44.1 k while the DSP runs at
    96 k is drawing a filter nobody is hearing. Sent from setActive, which
    VST3 documents as UI-thread. */
static const char* const kVocalFilterSampleRateMessage   = "VocalFilterSampleRate";
static const char* const kVocalFilterSampleRateAttribute = "SampleRate";

//------------------------------------------------------------------------

//------------------------------------------------------------------------
} // namespace VocalFilter
