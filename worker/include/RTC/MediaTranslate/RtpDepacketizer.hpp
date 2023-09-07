#pragma once

#include "RTC/RtpDictionaries.hpp"
#include <memory>

namespace RTC
{

class RtpMediaFrame;
class RtpMediaFrameSerializer;
class RtpPacket;

class RtpDepacketizer
{
public:
    virtual ~RtpDepacketizer() = default;
    virtual std::shared_ptr<RtpMediaFrame> AddPacket(const RtpPacket* packet) = 0;
    const RtpCodecMimeType& GetCodecMimeType() const { return _codecMimeType; }
    static std::unique_ptr<RtpDepacketizer> create(const RtpCodecMimeType& mimeType,
                                                   uint32_t sampleRate);
protected:
    RtpDepacketizer(const RtpCodecMimeType& codecMimeType);
private:
    const RtpCodecMimeType _codecMimeType;
};

} // namespace RTC
