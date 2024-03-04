#ifndef MS_RTC_RTP_PACKETS_COLLECTOR_HPP
#define MS_RTC_RTP_PACKETS_COLLECTOR_HPP

namespace RTC
{

class RtpPacket;
class Consumer;

class RtpPacketsCollector
{
public:
	virtual void AddPacket(RtpPacket* packet, uint32_t mappedSsrc,
                           bool destroyPacketAfter = true) = 0;
protected:
	virtual ~RtpPacketsCollector() = default;
};

} // namespace RTC

#endif
