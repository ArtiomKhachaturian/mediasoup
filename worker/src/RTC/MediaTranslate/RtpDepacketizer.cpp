#define MS_CLASS "RTC::RtpDepacketizer"
#include "RTC/MediaTranslate/RtpDepacketizerOpus.hpp"
#include "RTC/MediaTranslate/RtpDepacketizerVpx.hpp"
#include "RTC/RtpPacket.hpp"
#include "Logger.hpp"

namespace RTC
{

class RtpDepacketizer::PayloadBufferView : public Buffer
{
public:
    PayloadBufferView(uint8_t* data, size_t len);
    // impl. of Buffer
    size_t GetSize() const final { return _len; }
    uint8_t* GetData() final { return _data; }
    const uint8_t* GetData() const final { return _data; }
private:
    uint8_t* const _data;
    const size_t _len;
};

RtpDepacketizer::RtpDepacketizer(const RtpCodecMimeType& mime, uint32_t clockRate,
                                 const std::shared_ptr<BufferAllocator>& allocator)
    : BufferAllocations<void>(allocator)
    , _mime(mime)
    , _clockRate(clockRate)
{
    MS_ASSERT(_mime.IsMediaCodec(), "invalid media codec: %s", _mime.ToString().c_str());
}

std::optional<MediaFrame> RtpDepacketizer::CreateFrameFromPacket(const RtpPacket* packet,
                                                                 bool* configWasChanged)
{
    if (packet) {
        const auto payload = AllocateBuffer(packet->GetPayloadLength(), packet->GetPayload());
        return CreateFrameFromPacket(packet->GetSsrc(), packet->GetTimestamp(),
                                     packet->IsKeyFrame(), payload);
    }
    return std::nullopt;
}

std::unique_ptr<RtpDepacketizer> RtpDepacketizer::Create(const RtpCodecMimeType& mime,
                                                         uint32_t clockRate,
                                                         const std::shared_ptr<BufferAllocator>& allocator)
{
    switch (mime.GetSubtype()) {
        case RtpCodecMimeType::Subtype::MULTIOPUS: // not yet tested
            return std::make_unique<RtpDepacketizerOpus>(clockRate, true, allocator);
        case RtpCodecMimeType::Subtype::OPUS:
            return std::make_unique<RtpDepacketizerOpus>(clockRate, false, allocator);
        case RtpCodecMimeType::Subtype::VP8:
            return std::make_unique<RtpDepacketizerVpx>(clockRate, true, allocator);
        case RtpCodecMimeType::Subtype::VP9:
            break; // not yet tested
        default:
            break;
    }
    return nullptr;
}

MediaFrame RtpDepacketizer::CreateFrame() const
{
    return MediaFrame(GetMime(), GetClockRate(), GetAllocator());
}

bool RtpDepacketizer::AddPacketToFrame(uint32_t ssrc, uint32_t rtpTimestamp, bool keyFrame,
                                       const std::shared_ptr<Buffer>& payload, MediaFrame* frame)
{
    if (frame) {
        frame->AddPayload(payload);
        if (frame->GetTimestamp().GetRtpTime() > rtpTimestamp) {
            MS_WARN_TAG(rtp, "time stamp of new packet is less than previous, SSRC = %du", ssrc);
        }
        else {
            frame->SetTimestamp(rtpTimestamp);
        }
        if (keyFrame) {
            frame->SetKeyFrame(true);
        }
        return true;
    }
    return false;
}

bool RtpDepacketizer::AddPacketToFrame(const RtpPacket* packet, MediaFrame* frame) const
{
    if (packet && frame) {
        const auto payload = AllocateBuffer(packet->GetPayloadLength(), packet->GetPayload());
        return AddPacketToFrame(packet->GetSsrc(), packet->GetTimestamp(),
                                packet->IsKeyFrame(), payload, frame);
    }
}

std::optional<size_t> RtpDepacketizer::GetPayloadDescriptorSize(const RtpPacket* packet)
{
    if (packet) {
        if (const auto pdh = packet->GetPayloadDescriptorHandler()) {
            return pdh->GetPayloadDescriptorSize();
        }
    }
    return std::nullopt;
}

RtpAudioDepacketizer::RtpAudioDepacketizer(RtpCodecMimeType::Subtype type, uint32_t clockRate,
                                           const std::shared_ptr<BufferAllocator>& allocator)
    : RtpDepacketizer(RtpCodecMimeType(RtpCodecMimeType::Type::AUDIO, type), clockRate, allocator)
{
    MS_ASSERT(GetMime().IsAudioCodec(), "invalid audio codec: %s", GetMime().ToString().c_str());
}

RtpVideoDepacketizer::RtpVideoDepacketizer(RtpCodecMimeType::Subtype type, uint32_t clockRate,
                                           const std::shared_ptr<BufferAllocator>& allocator)
    : RtpDepacketizer(RtpCodecMimeType(RtpCodecMimeType::Type::VIDEO, type), clockRate, allocator)
{
    MS_ASSERT(GetMime().IsVideoCodec(), "invalid video codec: %s", GetMime().ToString().c_str());
}

} // namespace RTC
