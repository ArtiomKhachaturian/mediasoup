#pragma once
#include "RTC/Buffers/BufferAllocations.hpp"
#include "RTC/RtpDictionaries.hpp"
#include <memory>

namespace RTC
{

class MediaFrame;
class RtpMediaFrame;
class RtpMediaFrameSerializer;
class RtpPacket;

class RtpDepacketizer : public BufferAllocations<void>
{
public:
    virtual ~RtpDepacketizer() = default;
    virtual std::shared_ptr<const MediaFrame> AddPacket(const RtpPacket* packet) = 0;
    const RtpCodecMimeType& GetMimeType() const { return _mimeType; }
    uint32_t GetClockRate() const { return _clockRate; }
    static std::unique_ptr<RtpDepacketizer> Create(const RtpCodecMimeType& mimeType,
                                                   uint32_t clockRate,
                                                   const std::weak_ptr<BufferAllocator>& allocator);
protected:
    RtpDepacketizer(const RtpCodecMimeType& mimeType, uint32_t clockRate,
                    const std::weak_ptr<BufferAllocator>& allocator);
    std::shared_ptr<RtpMediaFrame> CreateMediaFrame() const;
    std::shared_ptr<RtpMediaFrame> CreateMediaFrame(const RtpPacket* packet) const;
private:
    const RtpCodecMimeType _mimeType;
    const uint32_t _clockRate;
};

} // namespace RTC
