#pragma once
#include <set>

namespace RTC
{

class RtpPacket;	

class RtpPacketsPlayerCallback
{
public:
	virtual void OnPlayStarted(uint64_t playbackId, uint64_t mediaSeqNum,
							   const std::set<uint32_t>& ssrcs) = 0;
	virtual void OnPlay(uint64_t playbackId, uint32_t rtpTimestampOffset, RtpPacket* packet) = 0;
	virtual void OnPlayFinished(uint64_t playbackId) = 0;
protected:
	virtual ~RtpPacketsPlayerCallback() = default;
};

} // namespace RTC