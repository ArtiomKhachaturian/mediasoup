#pragma once
#include "RTC/MediaTranslate/RtpTranslatedPacket.hpp"
#include "RTC/Buffers/BufferAllocations.hpp"
#include "RTC/RtpDictionaries.hpp"
#include <memory>
#include <optional>

namespace RTC
{

class Buffer;
class BufferAllocator;
class MediaFrame;
class RtpTranslatedPacket;

class RtpPacketizer : public BufferAllocations<void>
{
public:
    virtual ~RtpPacketizer() = default;
    const RtpCodecMimeType& GetType() const { return _mime; }
    virtual std::optional<RtpTranslatedPacket> Add(size_t payloadOffset,
                                                   size_t payloadLength,
                                                   std::shared_ptr<MediaFrame>&& frame) = 0;
    virtual size_t GetPayloadOffset() const;
protected:
    RtpPacketizer(const RtpCodecMimeType& mime,
                  const std::shared_ptr<BufferAllocator>& allocator = nullptr);
    RtpPacketizer(RtpCodecMimeType::Type type, RtpCodecMimeType::Subtype subtype,
                  const std::shared_ptr<BufferAllocator>& allocator = nullptr);
    std::optional<RtpTranslatedPacket> Create(Timestamp timestampOffset,
                                              std::shared_ptr<Buffer> buffer,
                                              size_t payloadOffset,
                                              size_t payloadLength) const;
private:
    const RtpCodecMimeType _mime;
};

} // namespace RTC
