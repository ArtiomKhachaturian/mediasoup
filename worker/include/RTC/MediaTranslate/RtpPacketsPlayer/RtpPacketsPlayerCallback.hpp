#pragma once
#include <set>

namespace RTC
{

class RtpPacket;	

class RtpPacketsPlayerCallback
{
public:
    virtual void OnPlayStarted(uint32_t /*ssrc*/, uint64_t /*mediaId*/,
                               const void* /*userData*/ = nullptr) {}
	virtual void OnPlay(uint32_t rtpTimestampOffset, RtpPacket* packet,
                        uint64_t mediaId, const void* userData = nullptr) = 0;
    virtual void OnPlayFinished(uint32_t /*ssrc*/, uint64_t /*mediaId*/,
                                const void* /*userData*/ = nullptr) {}
protected:
	virtual ~RtpPacketsPlayerCallback() = default;
};

} // namespace RTC
