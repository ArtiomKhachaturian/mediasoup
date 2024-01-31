#pragma once
#include <memory>

namespace RTC
{

class RtpPacket;
class MediaFrame;

class RtpPacketizer
{
public:
    virtual ~RtpPacketizer() = default;
    virtual RtpPacket* AddFrame(const std::shared_ptr<const MediaFrame>& frame) = 0;
protected:
    RtpPacketizer() = default;
};

} // namespace RTC
