#pragma once
#include "RTC/MediaTranslate/RtpTranslatedPacket.hpp"
#include "RTC/RtpDictionaries.hpp"
#include <memory>
#include <optional>

namespace RTC
{

class RtpPacket;
class Buffer;
class MediaFrame;
class RtpTranslatedPacket;

class RtpPacketizer
{
public:
    virtual ~RtpPacketizer() = default;
    const RtpCodecMimeType& GetType() const { return _mime; }
    virtual std::optional<RtpTranslatedPacket> Add(size_t payloadOffset,
                                                   size_t payloadLength,
                                                   std::shared_ptr<MediaFrame>&& frame) = 0;
    virtual size_t GetRtpPayloadOffset() const { return 0U; }
    virtual size_t GetMinimumBufferSize() const { return 0U; }
protected:
    RtpPacketizer(const RtpCodecMimeType& mime);
    RtpPacketizer(RtpCodecMimeType::Type type, RtpCodecMimeType::Subtype subtype);
    std::optional<RtpTranslatedPacket> Create(Timestamp timestampOffset,
                                              std::shared_ptr<Buffer> buffer,
                                              size_t payloadOffset,
                                              size_t payloadLength) const;
private:
    const RtpCodecMimeType _mime;
};

} // namespace RTC
