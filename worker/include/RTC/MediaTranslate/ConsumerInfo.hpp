#pragma once
#include <cstdint>

namespace RTC
{

class RtpPacket;

class ConsumerInfo
{
public:
	virtual ~ConsumerInfo() = default;
    virtual void SaveProducerRtpPacketInfo(const RtpPacket* packet) = 0;
	virtual uint64_t GetEndPointId() const = 0;
    virtual bool IsConnected() const = 0;
};

} // namespace RTC
