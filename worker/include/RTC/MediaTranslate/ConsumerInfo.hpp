#pragma once
#include <memory>

namespace RTC
{

class RtpPacket;
class TranslatorEndPoint;

class ConsumerInfo
{
public:
	virtual ~ConsumerInfo() = default;
    virtual void SaveProducerRtpPacketInfo(const RtpPacket* packet) = 0;
	virtual void AlignProducerRtpPacketInfo(RtpPacket* packet) = 0;
	virtual void AlignTranslatedRtpPacketInfo(uint32_t rtpTimestampOffset, RtpPacket* packet) = 0;
	virtual std::shared_ptr<const TranslatorEndPoint> GetEndPoint() const = 0;
	bool IsConnected() const;
};

} // namespace RTC
