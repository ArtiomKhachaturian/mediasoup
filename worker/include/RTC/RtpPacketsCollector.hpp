#ifndef MS_RTC_RTP_PACKETS_COLLECTOR_HPP
#define MS_RTC_RTP_PACKETS_COLLECTOR_HPP

namespace RTC
{

class RtpPacket;

class RtpPacketsCollector
{
public:
	virtual bool AddPacket(const RtpPacket* packet) = 0;
protected:
	virtual ~RtpPacketsCollector() = default;
};

} // namespace RTC

#endif
