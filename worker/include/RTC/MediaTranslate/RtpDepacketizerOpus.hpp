#ifndef MS_RTC_RTP_DEPACKETIZER_OPUS_HPP
#define MS_RTC_RTP_DEPACKETIZER_OPUS_HPP

#include "RTC/MediaTranslate/RtpDepacketizer.hpp"

namespace RTC
{

class RtpDepacketizerOpus : public RtpDepacketizer
{
public:
    RtpDepacketizerOpus(const RtpCodecMimeType& codecMimeType);
protected:
    // impl. of RtpDepacketizer
    std::shared_ptr<RtpMediaFrame> Assemble(const std::list<const RtpPacket*>& packets) const final;
private:
    std::allocator<uint8_t> _payloadAllocator;
};

} // namespace RTC

#endif
