#pragma once
#include "RTC/MediaTranslate/RtpTranslatedPacket.hpp"
#include <cstdint>

namespace RTC
{

class RtpTranslatedPacket;

class RtpPacketsPlayerCallback
{
public:
    virtual void OnPlayStarted(uint64_t /*mediaId*/, uint64_t /*mediaSourceId*/,
                               uint32_t /*ssrc*/) {}
    // 1st packet has zero timestamp offset
	virtual void OnPlay(uint64_t mediaId, uint64_t mediaSourceId,
                        RtpTranslatedPacket packet) = 0;
    virtual void OnPlayFinished(uint64_t /*mediaId*/, uint64_t /*mediaSourceId*/,
                                uint32_t /*ssrc*/) {}
protected:
	virtual ~RtpPacketsPlayerCallback() = default;
};

} // namespace RTC
