#define MS_CLASS "RTC::RtpDepacketizer"
#include "RTC/MediaTranslate/RtpDepacketizerOpus.hpp"
#include "RTC/RtpPacket.hpp"
#include "Logger.hpp"

namespace RTC
{

RtpDepacketizer::RtpDepacketizer(const RtpCodecMimeType& codecMimeType)
    : _codecMimeType(codecMimeType)
{
}

std::shared_ptr<RtpMediaFrame> RtpDepacketizer::AddPacket(const RtpPacket* packet)
{
    std::shared_ptr<RtpMediaFrame> frame;
    if (packet && packet->GetPayload()) {
        _packetsChain.push_back(packet);
        frame = Assemble(_packetsChain);
        _packetsChain.pop_back();
        if (frame) {
            DestroyPacketsChain();
        }
        else {
            const auto packetCopy = packet->Clone();
            MS_ASSERT(packetCopy != nullptr, "RTP packet clone failed");
            _packetsChain.push_back(packetCopy);
        }
    }
    return frame;
}

std::shared_ptr<RtpDepacketizer> RtpDepacketizer::create(const RtpCodecMimeType& mimeType)
{
    switch (mimeType.type) {
        case RtpCodecMimeType::Type::UNSET:
            break;
        case RtpCodecMimeType::Type::AUDIO:
            switch (mimeType.subtype) {
                case RtpCodecMimeType::Subtype::MULTIOPUS:
                case RtpCodecMimeType::Subtype::OPUS:
                    return std::make_shared<RtpDepacketizerOpus>(mimeType);
                default:
                    break;
            }
            break;
        case RtpCodecMimeType::Type::VIDEO:
            break;
    }
    return nullptr;
}

void RtpDepacketizer::DestroyPacketsChain()
{
    if (!_packetsChain.empty()) {
        std::for_each(_packetsChain.begin(), _packetsChain.end(),
                      [](const RtpPacket* packet) { delete packet; });
        _packetsChain.clear();
    }
}


} // namespace RTC
