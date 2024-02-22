#define MS_CLASS "RTC::RtpDepacketizer"
#include "RTC/MediaTranslate/RtpDepacketizerOpus.hpp"
#include "RTC/MediaTranslate/RtpDepacketizerVpx.hpp"
#include "RTC/MediaTranslate/RtpMediaFrame.hpp"
#include "RTC/RtpPacket.hpp"
#include "Logger.hpp"

namespace RTC
{

RtpDepacketizer::RtpDepacketizer(const RtpCodecMimeType& mimeType, uint32_t clockRate,
                                 const std::weak_ptr<BufferAllocator>& allocator)
    : BufferAllocations<void>(allocator)
    , _mimeType(mimeType)
    , _clockRate(clockRate)
{
    MS_ASSERT(_mimeType.IsMediaCodec(), "invalid media codec");
}

std::unique_ptr<RtpDepacketizer> RtpDepacketizer::Create(const RtpCodecMimeType& mimeType,
                                                         uint32_t clockRate,
                                                         const std::weak_ptr<BufferAllocator>& allocator)
{
    switch (mimeType.GetType()) {
        case RtpCodecMimeType::Type::AUDIO:
            switch (mimeType.GetSubtype()) {
                case RtpCodecMimeType::Subtype::MULTIOPUS:
                case RtpCodecMimeType::Subtype::OPUS:
                    return std::make_unique<RtpDepacketizerOpus>(mimeType, clockRate, allocator);
                default:
                    break;
            }
            break;
        case RtpCodecMimeType::Type::VIDEO:
            switch (mimeType.GetSubtype()) {
                case RtpCodecMimeType::Subtype::VP8:
                case RtpCodecMimeType::Subtype::VP9:
                    return std::make_unique<RtpDepacketizerVpx>(mimeType, clockRate, allocator);
                default:
                    break;
            }
            break;
        default:
            break;
    }
    return nullptr;
}

std::shared_ptr<RtpMediaFrame> RtpDepacketizer::CreateMediaFrame() const
{
    return std::make_shared<RtpMediaFrame>(GetMimeType(), GetClockRate(), GetAllocator());
}

std::shared_ptr<RtpMediaFrame> RtpDepacketizer::CreateMediaFrame(const RtpPacket* packet) const
{
    return RtpMediaFrame::Create(packet, GetMimeType(), GetClockRate(), GetAllocator());
}

} // namespace RTC
