#pragma once
#include <cstdint>
#include <set>

namespace RTC
{

class RtpPacket;
class Timestamp;

class RtpPacketsPlayerCallback
{
public:
    virtual void OnPlayStarted(uint32_t /*ssrc*/, uint64_t /*mediaId*/,
                               uint64_t /*mediaSourceId*/) {}
    // timestampOffset is relative value from beginning of the media play,
    // 1st packet has zero time
	virtual void OnPlay(const Timestamp& timestampOffset, RtpPacket* packet,
                        uint64_t mediaId, uint64_t mediaSourceId) = 0;
    virtual void OnPlayFinished(uint32_t /*ssrc*/, uint64_t /*mediaId*/,
                                uint64_t /*mediaSourceId*/) {}
protected:
	virtual ~RtpPacketsPlayerCallback() = default;
};

} // namespace RTC
