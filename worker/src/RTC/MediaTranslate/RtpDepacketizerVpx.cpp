#define MS_CLASS "RTC::RtpDepacketizerVpx"
#include "RTC/MediaTranslate/RtpDepacketizerVpx.hpp"
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
    RtpAssembly(const RtpDepacketizerVpx* depacketizer);
    std::optional<MediaFrame> AddPacket(const RtpPacket* packet, bool* configWasChanged);
    const VideoFrameConfig& GetConfig() const { return _config; }
private:
    bool AddPayload(const RtpPacket* packet);
    bool ParseVideoConfig(const RtpPacket* packet, bool* configWasChanged);
    std::optional<MediaFrame> ResetFrame();
    void SetLastTimeStamp(uint32_t lastTimeStamp);
private:
    const RtpDepacketizerVpx* const _depacketizer;
    uint32_t _lastTimeStamp = 0U;
    std::optional<MediaFrame> _frame;
    VideoFrameConfig _config;
};


RtpDepacketizerVpx::RtpDepacketizerVpx(const RtpCodecMimeType& mimeType, uint32_t clockRate,
                                       const std::shared_ptr<BufferAllocator>& allocator)
    : RtpDepacketizer(mimeType, clockRate, allocator)
{
}

RtpDepacketizerVpx::~RtpDepacketizerVpx()
{
}

std::optional<MediaFrame> RtpDepacketizerVpx::AddPacket(const RtpPacket* packet,
                                                        bool /*makeDeepCopyOfPayload*/,
                                                        bool* configWasChanged)
{
    if (packet && packet->GetPayload() && packet->GetPayloadLength()) {
        auto it = _assemblies.find(packet->GetSsrc());
        if (it == _assemblies.end()) {
            auto assembly = std::make_unique<RtpAssembly>(this);
            it = _assemblies.insert({packet->GetSsrc(), std::move(assembly)}).first;
        }
        return it->second->AddPacket(packet, configWasChanged);
    }
    return std::nullopt;
}

VideoFrameConfig RtpDepacketizerVpx::GetVideoConfig(const RtpPacket* packet) const
{
    if (packet) {
        const auto it = _assemblies.find(packet->GetSsrc());
        if (it != _assemblies.end()) {
            return it->second->GetConfig();
        }
    }
    return RtpDepacketizer::GetVideoConfig(packet);
}

RtpDepacketizerVpx::RtpAssembly::RtpAssembly(const RtpDepacketizerVpx* depacketizer)
    : _depacketizer(depacketizer)
{
}

std::optional<MediaFrame> RtpDepacketizerVpx::RtpAssembly::AddPacket(const RtpPacket* packet,
                                                                     bool* configWasChanged)
{
    if (packet && packet->GetPayload() && packet->GetPayloadLength()) {
        SetLastTimeStamp(packet->GetTimestamp());
        // Add payload
        if (AddPayload(packet)) {
            if (ParseVideoConfig(packet, configWasChanged)) {
                if (packet->HasMarker()) {
                    return ResetFrame();
                }
            }
            else {
                const auto error = GetStreamInfoString(_depacketizer->GetMimeType(), packet->GetSsrc());
                MS_ERROR_STD("failed to parse video config for stream [%s]", error.c_str());
            }
        }
        else {
            const auto error = GetStreamInfoString(_depacketizer->GetMimeType(), packet->GetSsrc());
            MS_ERROR_STD("failed to add payload for stream [%s]", error.c_str());
        }
    }
    return std::nullopt;
}

bool RtpDepacketizerVpx::RtpAssembly::AddPayload(const RtpPacket* packet)
{
    if (_frame) {
        if (const auto pds = RtpDepacketizer::GetPayloadDescriptorSize(packet)) {
            if (const auto payload = packet->GetPayload()) {
                const auto len = packet->GetPayloadLength();
                if (len > pds.value()) {
                    const auto data = packet->GetPayload() + pds.value();
                    return RtpDepacketizer::AddPacket(packet, data,
                                                      len - pds.value(),
                                                      &_frame.value());
                }
            }
        }
    }
    return false;
}

bool RtpDepacketizerVpx::RtpAssembly::ParseVideoConfig(const RtpPacket* packet,
                                                       bool* configWasChanged)
{
    bool ok = false;
    if (packet) {
        if (packet->IsKeyFrame()) {
            VideoFrameConfig config;
            switch (_depacketizer->GetMimeType().GetSubtype()) {
                case RtpCodecMimeType::Subtype::VP8:
                    ok = RtpDepacketizer::ParseVp8VideoConfig(packet, config);
                    break;
                case RtpCodecMimeType::Subtype::VP9:
                    ok = RtpDepacketizer::ParseVp9VideoConfig(packet, config);
                    break;
                default:
                    break;
            }
            if (ok && config != _config) {
                _config = std::move(config);
                if (configWasChanged) {
                    *configWasChanged = true;
                }
            }
        }
        else {
            ok = true; // skip it
        }
    }
    return ok;
}

std::optional<MediaFrame> RtpDepacketizerVpx::RtpAssembly::ResetFrame()
{
    std::optional<MediaFrame> frame(std::move(_frame));
    _frame = _depacketizer->CreateMediaFrame();
    return frame;
}

void RtpDepacketizerVpx::RtpAssembly::SetLastTimeStamp(uint32_t lastTimeStamp)
{
    if (_lastTimeStamp != lastTimeStamp) {
        _lastTimeStamp = lastTimeStamp;
        _frame = _depacketizer->CreateMediaFrame();
    }
}

} // namespace RTC
