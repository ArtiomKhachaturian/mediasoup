#pragma once

#include "RTC/RtpDictionaries.hpp"
#include <memory>
#include <list>

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
    RtpDepacketizer(const RtpCodecMimeType& codecMimeType, uint32_t sampleRate);
    uint32_t GetSampleRate() const { return _sampleRate; }
private:
    const RtpCodecMimeType _codecMimeType;
    const uint32_t _sampleRate;
};

} // namespace RTC
