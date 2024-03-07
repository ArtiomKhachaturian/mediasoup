#define MS_CLASS "DepLibWebRTC"
// #define MS_LOG_DEV_LEVEL 3

#include "DepLibWebRTC.hpp"
#include "Logger.hpp"
#include "Settings.hpp"
#include "system_wrappers/source/field_trial.h" // webrtc::field_trial
#include <mutex>

/* Static. */

static std::once_flag GlobalInitOnce;

/* Static methods. */

void DepLibWebRTC::ClassInit()
{
	MS_TRACE();

	std::call_once(GlobalInitOnce,[] {
        const auto libwebrtcFieldTrials = Settings::configuration.GetLibwebrtcFieldTrials();
        MS_DEBUG_TAG(info, "libwebrtc field trials: \"%s\"", libwebrtcFieldTrials.c_str());
        webrtc::field_trial::InitFieldTrialsFromString(libwebrtcFieldTrials.c_str());
    });
}

void DepLibWebRTC::ClassDestroy()
{
	MS_TRACE();
}
