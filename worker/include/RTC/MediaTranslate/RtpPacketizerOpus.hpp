#pragma once
#include "RTC/MediaTranslate/RtpPacketizer.hpp"

namespace RTC
{

class RtpPacketizerOpus : public RtpPacketizer
{
public:
    RtpPacketizerOpus() = default;
    // impl. of RtpPacketizer
    RtpCodecMimeType GetType() const final;
    RtpPacket* AddFrame(const std::shared_ptr<const MediaFrame>& frame,
                        bool setPacketTimestamp) final;
private:
    bool _firstFrame = true;
};

} // namespace RTC
