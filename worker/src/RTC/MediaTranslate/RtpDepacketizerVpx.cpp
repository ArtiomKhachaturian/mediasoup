#define MS_CLASS "RTC::RtpDepacketizerVpx"
#include "RTC/MediaTranslate/RtpDepacketizerVpx.hpp"
#include "RTC/MediaTranslate/RtpMediaFrame.hpp"
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
    RtpAssembly(const RtpDepacketizerVpx* depacketizer);
    std::shared_ptr<const RtpMediaFrame> AddPacket(const RtpPacket* packet);
private:
    bool AddPayload(const RtpPacket* packet);
    bool ParseVideoConfig(const RtpPacket* packet);
    std::shared_ptr<const RtpMediaFrame> ResetFrame();
    void SetLastTimeStamp(uint32_t lastTimeStamp);
private:
    const RtpDepacketizerVpx* const _depacketizer;
    uint32_t _lastTimeStamp = 0U;
    std::shared_ptr<RtpMediaFrame> _frame;
    std::shared_ptr<VideoFrameConfig> _config;
};


RtpDepacketizerVpx::RtpDepacketizerVpx(const RtpCodecMimeType& mimeType, uint32_t clockRate,
                                       const std::weak_ptr<BufferAllocator>& allocator)
    : RtpDepacketizer(mimeType, clockRate, allocator)
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
            auto assembly = std::make_unique<RtpAssembly>(this);
            it = _assemblies.insert({packet->GetSsrc(), std::move(assembly)}).first;
        }
        return it->second->AddPacket(packet);
    }
    return nullptr;
}

RtpDepacketizerVpx::RtpAssembly::RtpAssembly(const RtpDepacketizerVpx* depacketizer)
    : _depacketizer(depacketizer)
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
                const auto error = GetStreamInfoString(_depacketizer->GetMimeType(), packet->GetSsrc());
                MS_ERROR("failed to parse video config for stream [%s]", error.c_str());
            }
        }
        else {
            const auto error = GetStreamInfoString(_depacketizer->GetMimeType(), packet->GetSsrc());
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
                return _frame && _frame->AddPacket(packet, data, len - pds.value());
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
            switch (_depacketizer->GetMimeType().GetSubtype()) {
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
    std::shared_ptr<RtpMediaFrame> frame(std::move(_frame));
    if (frame) {
        frame->SetVideoConfig(std::move(_config));
    }
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
