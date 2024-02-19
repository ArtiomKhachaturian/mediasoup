#define MS_CLASS "RTC::RtpDepacketizerVpx"
#include "RTC/MediaTranslate/RtpDepacketizerVpx.hpp"
#include "RTC/MediaTranslate/RtpMediaFrame.hpp"
#include "RTC/MediaTranslate/SimpleMemoryBuffer.hpp"
#include "RTC/MediaTranslate/VideoFrameConfig.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/Codecs/VP8.hpp"
#include "RTC/Codecs/VP9.hpp"
#include "RTC/RtpPacket.hpp"
#include "Logger.hpp"

namespace RTC
{

class RtpDepacketizerVpx::RtpAssembly
{
public:
    RtpAssembly(const RtpCodecMimeType& mime, uint32_t clockRate,
                const std::allocator<uint8_t>& payloadAllocator);
    std::shared_ptr<const RtpMediaFrame> AddPacket(const RtpPacket* packet);
private:
    bool AddPayload(const RtpPacket* packet);
    bool ParseVideoConfig(const RtpPacket* packet);
    std::shared_ptr<const RtpMediaFrame> ResetFrame();
    void SetLastTimeStamp(uint32_t lastTimeStamp);
private:
    const RtpCodecMimeType _mime;
    const uint32_t _clockRate;
    const std::allocator<uint8_t>& _payloadAllocator;
    uint32_t _lastTimeStamp = 0U;
    std::shared_ptr<RtpMediaFrame> _payload;
    std::shared_ptr<VideoFrameConfig> _config;
};


RtpDepacketizerVpx::RtpDepacketizerVpx(const RtpCodecMimeType& mimeType, uint32_t clockRate)
    : RtpDepacketizer(mimeType, clockRate)
{
}

RtpDepacketizerVpx::~RtpDepacketizerVpx()
{
}

std::shared_ptr<const MediaFrame> RtpDepacketizerVpx::AddPacket(const RtpPacket* packet)
{
    if (packet && packet->GetPayload() && packet->GetPayloadLength()) {
        auto it = _assemblies.find(packet->GetSsrc());
        if (it == _assemblies.end()) {
            auto assembly = std::make_unique<RtpAssembly>(GetMimeType(),
                                                          GetClockRate(),
                                                          GetPayloadAllocator());
            it = _assemblies.insert({packet->GetSsrc(), std::move(assembly)}).first;
        }
        return it->second->AddPacket(packet);
    }
    return nullptr;
}

RtpDepacketizerVpx::RtpAssembly::RtpAssembly(const RtpCodecMimeType& mime, uint32_t clockRate,
                                             const std::allocator<uint8_t>& payloadAllocator)
    : _mime(mime)
    , _clockRate(clockRate)
    , _payloadAllocator(payloadAllocator)
{
}

std::shared_ptr<const RtpMediaFrame> RtpDepacketizerVpx::RtpAssembly::
    AddPacket(const RtpPacket* packet)
{
    if (packet && packet->GetPayload() && packet->GetPayloadLength()) {
        SetLastTimeStamp(packet->GetTimestamp());
        // Add payload
        if (AddPayload(packet)) {
            if (ParseVideoConfig(packet)) {
                if (packet->HasMarker()) {
                    return ResetFrame();
                }
            }
            else {
                const auto error = GetStreamInfoString(_mime, packet->GetSsrc());
                MS_ERROR("failed to parse video config for stream [%s]", error.c_str());
            }
        }
        else {
            const auto error = GetStreamInfoString(_mime, packet->GetSsrc());
            MS_ERROR("failed to add payload for stream [%s]", error.c_str());
        }
    }
    return nullptr;
}

bool RtpDepacketizerVpx::RtpAssembly::AddPayload(const RtpPacket* packet)
{
    if (const auto pds = RtpMediaFrame::GetPayloadDescriptorSize(packet)) {
        if (const auto payload = packet->GetPayload()) {
            const auto len = packet->GetPayloadLength();
            if (len > pds.value()) {
                const auto data = packet->GetPayload() + pds.value();
                return _payload && _payload->AddPacket(packet, data,
                                                       len - pds.value(),
                                                       _payloadAllocator);
            }
        }
    }
    return false;
}

bool RtpDepacketizerVpx::RtpAssembly::ParseVideoConfig(const RtpPacket* packet)
{
    bool ok = false;
    if (packet) {
        if (packet->IsKeyFrame()) {
            _config = std::make_shared<VideoFrameConfig>();
            switch (_mime.GetSubtype()) {
                case RtpCodecMimeType::Subtype::VP8:
                    ok = RtpMediaFrame::ParseVp8VideoConfig(packet, _config);
                    break;
                case RtpCodecMimeType::Subtype::VP9:
                    ok = RtpMediaFrame::ParseVp9VideoConfig(packet, _config);
                    break;
                default:
                    break;
            }
            if (!ok) {
                _config.reset();
            }
        }
        else {
            ok = true; // skip it
        }
    }
    return ok;
}

std::shared_ptr<const RtpMediaFrame> RtpDepacketizerVpx::RtpAssembly::ResetFrame()
{
    auto finalPayload = std::move(_payload);
    finalPayload->SetVideoConfig(std::move(_config));
    _payload = std::make_shared<RtpMediaFrame>(_mime, _clockRate);
    return finalPayload;
}

void RtpDepacketizerVpx::RtpAssembly::SetLastTimeStamp(uint32_t lastTimeStamp)
{
    if (_lastTimeStamp != lastTimeStamp) {
        _lastTimeStamp = lastTimeStamp;
        _payload = std::make_shared<RtpMediaFrame>(_mime, _clockRate);
    }
}

} // namespace RTC
