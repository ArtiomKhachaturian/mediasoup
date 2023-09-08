#define MS_CLASS "RTC::RtpDepacketizerVpx"
#include "RTC/MediaTranslate/RtpDepacketizerVpx.hpp"
#include "RTC/MediaTranslate/RtpMediaFrame.hpp"
#include "RTC/MediaTranslate/SimpleMemoryBuffer.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/Codecs/VP8.hpp"
#include "RTC/Codecs/VP9.hpp"
#include "RTC/RtpPacket.hpp"
#include "Logger.hpp"
#include <optional>

namespace RTC
{

class RtpDepacketizerVpx::RtpAssembly
{
public:
    RtpAssembly(const RtpCodecMimeType& mime);
    std::shared_ptr<RtpMediaFrame> AddPacket(const RtpPacket* packet);
private:
    bool AddPayload(const RtpPacket* packet);
private:
    const RtpCodecMimeType _mime;
#ifdef USE_ASSEMBLE_MEDIA_FRAME
    std::shared_ptr<RtpMediaFrame> _payload;
#else
    SimpleMemoryBuffer _payload;
#endif
    uint32_t _lastTimeStamp = 0U;
    std::shared_ptr<RtpVideoFrameConfig> _videoConfig;
};


RtpDepacketizerVpx::RtpDepacketizerVpx(const RtpCodecMimeType& codecMimeType)
    : RtpDepacketizer(codecMimeType)
{
}

RtpDepacketizerVpx::~RtpDepacketizerVpx()
{
}

std::shared_ptr<RtpMediaFrame> RtpDepacketizerVpx::AddPacket(const RtpPacket* packet)
{
    if (packet && packet->GetPayload() && packet->GetPayloadLength()) {
        auto it = _assemblies.find(packet->GetSsrc());
        if (it == _assemblies.end()) {
            auto assembly = std::make_unique<RtpAssembly>(GetCodecMimeType());
            it = _assemblies.insert({packet->GetSsrc(), std::move(assembly)}).first;
        }
        return it->second->AddPacket(packet);
    }
    return nullptr;
}

RtpDepacketizerVpx::RtpAssembly::RtpAssembly(const RtpCodecMimeType& mime)
    : _mime(mime)
{
}

std::shared_ptr<RtpMediaFrame> RtpDepacketizerVpx::RtpAssembly::AddPacket(const RtpPacket* packet)
{
    if (packet && packet->GetPayload() && packet->GetPayloadLength()) {
        if (_lastTimeStamp != packet->GetTimestamp()) {
            _lastTimeStamp = packet->GetTimestamp();
#ifdef USE_ASSEMBLE_MEDIA_FRAME
            _payload = std::make_shared<RtpMediaFrame>(_mime);
#else
            _payload.Resize(0U);
#endif
        }
        // Add payload
        if (AddPayload(packet)) {
            bool ok = true;
            if (packet->IsKeyFrame()) {
                _videoConfig = std::make_shared<RtpVideoFrameConfig>();
                switch (_mime.GetSubtype()) {
                    case RtpCodecMimeType::Subtype::VP8:
                        ok = _videoConfig->ParseVp8VideoConfig(packet);
                        break;
                    case RtpCodecMimeType::Subtype::VP9:
                        ok = _videoConfig->ParseVp9VideoConfig(packet);
                        break;
                    default:
                        break;
                }
                if (!ok) {
                    const auto error = GetStreamInfoString(_mime, packet->GetSsrc());
                    MS_ERROR("failed to parse video config for stream [%s]", error.c_str());
                    _videoConfig.reset();
                }
            }
            if (ok) {
                if (packet->HasMarker()) {
#ifdef USE_ASSEMBLE_MEDIA_FRAME
                    MS_ASSERT(!_payload->IsEmpty(), "empty frame");
                    const auto finalPayload = std::move(_payload);
                    finalPayload->SetKeyFrame(packet->IsKeyFrame() || _videoConfig);
                    finalPayload->SetVideoConfig(std::move(_videoConfig));
                    _payload = std::make_shared<RtpMediaFrame>(_mime);
                    return finalPayload;
#else
                    if (const auto frame = RtpMediaFrame::Create(_mime, packet, _payload.Take())) {
                        frame->SetKeyFrame(packet->IsKeyFrame() || _videoConfig);
                        frame->SetVideoConfig(std::move(_videoConfig));
                        return frame;
                    }
                    else {
                        const auto error = GetStreamInfoString(_mime, packet->GetSsrc());
                        MS_ERROR("failed to take assembled payload for stream [%s]", error.c_str());
                    }
#endif
                }
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
    if (const auto pds = RtpVideoFrameConfig::GetPayloadDescriptorSize(packet)) {
        if (const auto payload = packet->GetPayload()) {
            const auto len = packet->GetPayloadLength();
            if (len > pds.value()) {
                const auto data = packet->GetPayload() + pds.value();
#ifdef USE_ASSEMBLE_MEDIA_FRAME
                return _payload && _payload->AddPacket(packet, data, len);
#else
                return _payload.Append(data, len - pds.value());
#endif
            }
        }
    }
    return false;
}

} // namespace RTC
