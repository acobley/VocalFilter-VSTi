//------------------------------------------------------------------------
// VocalFilter - the parameter table
//------------------------------------------------------------------------

#include "VocalFilterParams.h"

namespace VocalFilter {

//------------------------------------------------------------------------
// id            title           units  type               plainMin      plainMax      plainDefault     internalMin   internalMax  steps smoothed
//------------------------------------------------------------------------
const ParamDef kParams[kNumParams] =
{
	{ kOutputTrim, "Output Trim", "dB",  ParamType::Float,  kTrimMinDb,   kTrimMaxDb,   kTrimDefaultDb,  kTrimMinDb,   kTrimMaxDb,  0,    true },
};

//------------------------------------------------------------------------
const ParamDef& paramDef (Steinberg::Vst::ParamID id)
{
	if (id < kNumParams)
		return kParams[id];
	return kParams[kOutputTrim];
}

//------------------------------------------------------------------------
} // namespace VocalFilter
