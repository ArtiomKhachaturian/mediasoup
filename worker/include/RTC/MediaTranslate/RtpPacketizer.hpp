#pragma once
#include "RTC/RtpDictionaries.hpp"
#include <memory>

namespace RTC
{

class RtpPacket;
class MediaFrame;

class RtpPacketizer
{
public:
    virtual ~RtpPacketizer() = default;
    virtual RtpCodecMimeType GetType() const = 0;
    virtual RtpPacket* AddFrame(const std::shared_ptr<const MediaFrame>& frame,
                                bool setPacketTimestamp = false) = 0;
protected:
    RtpPacketizer() = default;
};

} // namespace RTC
