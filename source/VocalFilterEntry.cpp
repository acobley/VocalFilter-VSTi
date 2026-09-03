//------------------------------------------------------------------------
// VocalFilter - plug-in factory
//------------------------------------------------------------------------

#include "VocalFilterController.h"
#include "VocalFilterIDs.h"
#include "VocalFilterProcessor.h"
#include "version.h"

#include "public.sdk/source/main/pluginfactory.h"

#define stringPluginName "VocalFilter"

using namespace Steinberg::Vst;
using namespace VocalFilter;

//------------------------------------------------------------------------
BEGIN_FACTORY_DEF (stringCompanyName, "https://github.com/", "mailto:aecobley@googlemail.com")

	// An EFFECT: stereo in, stereo out, no event input. PlugType is what a
	// host reads to decide which list to put it in - narrow it to
	// kFxFilter, kFxModulation or whatever it turns out to be once the DSP
	// is in.
	DEF_CLASS2 (INLINE_UID_FROM_FUID (kVocalFilterProcessorUID),
	            PClassInfo::kManyInstances,
	            kVstAudioEffectClass,
	            stringPluginName,
	            Vst::kDistributable,
	            PlugType::kFx,
	            FULL_VERSION_STR,
	            kVstVersionString,
	            VocalFilterProcessor::createInstance)

	DEF_CLASS2 (INLINE_UID_FROM_FUID (kVocalFilterControllerUID),
	            PClassInfo::kManyInstances,
	            kVstComponentControllerClass,
	            stringPluginName "Controller",
	            0,
	            "",
	            FULL_VERSION_STR,
	            kVstVersionString,
	            VocalFilterController::createInstance)

END_FACTORY
