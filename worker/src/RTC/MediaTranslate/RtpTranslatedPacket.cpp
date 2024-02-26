#define MS_CLASS "RTC::RtpTranslatedPacket"
#include "RTC/MediaTranslate/RtpTranslatedPacket.hpp"
#include "RTC/Codecs/Tools.hpp"
#include "RTC/RtpPacket.hpp"
#include "Logger.hpp"

namespace {

using namespace RTC;

inline RtpPacket::Header* GetRtpHeader(uint8_t* buffer) {
    return reinterpret_cast<RtpPacket::Header*>(buffer);
}

inline const uint8_t* GetRtpPayload(const uint8_t* buffer, size_t offset) {
    return buffer + offset;
}

inline const uint8_t* GetRtpPayload(const std::shared_ptr<Buffer>& buffer, size_t offset) {
    return buffer ? GetRtpPayload(buffer->GetData(), offset) : nullptr;
}

inline RtpPacket::Header* GetRtpHeader(const std::shared_ptr<Buffer>& buffer) {
    return buffer ? GetRtpHeader(buffer->GetData()) : nullptr;
}

}

namespace RTC
{

class RtpTranslatedPacket::BufferedRtpPacket : public RtpPacket
{
public:
    BufferedRtpPacket(std::shared_ptr<Buffer> buffer, size_t payloadOffset,
                      size_t payloadLength);
private:
    const std::shared_ptr<Buffer> _buffer;
};

RtpTranslatedPacket::RtpTranslatedPacket(const RtpCodecMimeType& mime,
                                         Timestamp timestampOffset,
                                         std::shared_ptr<Buffer> buffer,
                                         size_t payloadOffset, size_t payloadLength)
    : _timestampOffset(std::move(timestampOffset))
    , _impl(std::make_unique<BufferedRtpPacket>(std::move(buffer), payloadOffset, payloadLength))
{
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
    _impl->SetMarker(set);
}

void RtpTranslatedPacket::SetSsrc(uint32_t ssrc)
{
    _impl->SetSsrc(ssrc);
}

void RtpTranslatedPacket::SetPayloadType(uint8_t type)
{
    _impl->SetPayloadType(type);
}

RtpTranslatedPacket::BufferedRtpPacket::BufferedRtpPacket(std::shared_ptr<Buffer> buffer,
                                                          size_t payloadOffset,
                                                          size_t payloadLength)
    : RtpPacket(GetRtpHeader(buffer), nullptr, GetRtpPayload(buffer, payloadOffset),
                payloadLength, 0U, buffer ? buffer->GetSize() : 0U)
    , _buffer(std::move(buffer))
{
    MS_ASSERT(_buffer, "buffer must not be null");
    MS_ASSERT(payloadOffset >= RtpPacket::HeaderSize, "payload offset is too small");
    MS_ASSERT(payloadOffset + payloadLength <= _buffer->GetSize(),
              "payload data is out of buffer's range");
    SetTranslated(true);
}

} // namespace RTC
