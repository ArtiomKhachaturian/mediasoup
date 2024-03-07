#pragma once
#include "RTC/MediaTranslate/RtpTranslatedPacket.hpp"
#include <cstdint>

namespace RTC
{

class RtpTranslatedPacket;

class RtpPacketsPlayerStreamCallback
{
public:
    virtual void OnPlayStarted(uint64_t mediaId, uint64_t mediaSourceId) = 0;
    // 1st packet has zero timestamp offset
	virtual void OnPlay(uint64_t mediaId, uint64_t mediaSourceId, RtpTranslatedPacket packet) = 0;
    virtual void OnPlayFinished(uint64_t mediaId, uint64_t mediaSourceId) = 0;
protected:
	virtual ~RtpPacketsPlayerStreamCallback() = default;
};

} // namespace RTC
