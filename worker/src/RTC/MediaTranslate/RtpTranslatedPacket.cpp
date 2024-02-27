#define MS_CLASS "RTC::RtpTranslatedPacket"
#include "RTC/MediaTranslate/RtpTranslatedPacket.hpp"
#include "RTC/Codecs/Tools.hpp"
#include "RTC/RtpPacket.hpp"
#include "Logger.hpp"

namespace RTC
{

RtpTranslatedPacket::RtpTranslatedPacket(const RtpCodecMimeType& mime,
                                         Timestamp timestampOffset,
                                         std::shared_ptr<Buffer> buffer,
                                         size_t payloadOffset,
                                         size_t payloadLength,
                                         const std::weak_ptr<BufferAllocator>& allocator)
    : _timestampOffset(std::move(timestampOffset))
{
    MS_ASSERT(buffer, "buffer must not be null");
    MS_ASSERT(payloadOffset >= RtpPacket::HeaderSize, "payload offset is too small");
    const auto data = buffer->GetData();
    std::memset(data, 0, payloadOffset);
    const auto header = reinterpret_cast<RtpPacket::Header*>(data);
    header->version = RtpPacket::Version; // default
    // workaround for fix issue with memory corruption in direct usage of RtpMemoryBufferPacket
    // TODO: solve this problem for production, maybe problem inside of RtpPacket::SetExtensions
    // because synthetic packet has no extensions and preallocated memory for that
    RtpPacket packet(header, nullptr, data + payloadOffset, payloadLength, 0U,
                     payloadOffset + payloadLength, allocator);
    _impl.reset(packet.Clone());
    _impl->SetTranslated(true);
    Codecs::Tools::ProcessRtpPacket(_impl.get(), mime);
}

RtpTranslatedPacket::RtpTranslatedPacket(RtpTranslatedPacket&& tmp)
    : _timestampOffset(std::move(tmp._timestampOffset))
    , _impl(std::move(tmp._impl))
{
}

RtpTranslatedPacket::~RtpTranslatedPacket()
{
}

RtpTranslatedPacket& RtpTranslatedPacket::operator = (RtpTranslatedPacket&& tmp)
{
    if (&tmp != this) {
        _timestampOffset = std::move(tmp._timestampOffset);
        _impl = std::move(tmp._impl);
    }
    return *this;
}

void RtpTranslatedPacket::SetMarker(bool set)
{
    if (_impl) {
        _impl->SetMarker(set);
    }
}

void RtpTranslatedPacket::SetSsrc(uint32_t ssrc)
{
    if (_impl) {
        _impl->SetSsrc(ssrc);
    }
}

void RtpTranslatedPacket::SetPayloadType(uint8_t type)
{
    if (_impl) {
        _impl->SetPayloadType(type);
    }
}

} // namespace RTC
