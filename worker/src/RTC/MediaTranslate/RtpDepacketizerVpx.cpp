#define MS_CLASS "RTC::RtpDepacketizerVpx"
#include "RTC/MediaTranslate/RtpDepacketizerVpx.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/Codecs/VP8.hpp"
#include "RTC/Codecs/VP9.hpp"
#include "RTC/RtpPacket.hpp"
#include "Logger.hpp"

namespace {

using namespace RTC;

inline RtpCodecMimeType::Subtype GetType(bool vp8) {
    return vp8 ? RtpCodecMimeType::Subtype::VP8 : RtpCodecMimeType::Subtype::VP9;
}

}

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


RtpDepacketizerVpx::RtpDepacketizerVpx(uint32_t clockRate, bool vp8,
                                       const std::shared_ptr<BufferAllocator>& allocator)
    : RtpVideoDepacketizer(GetType(vp8), clockRate, allocator)
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

bool RtpDepacketizerVpx::ParseVp8VideoConfig(const RtpPacket* packet, VideoFrameConfig& applyTo)
{
    if (const auto pds = GetPayloadDescriptorSize(packet)) {
        if (const auto payload = packet->GetPayload()) {
            const auto offset = pds.value();
            const auto len = packet->GetPayloadLength();
            if (len >= offset + 10U) {
                // Start code for VP8 key frame:
                // Read comon 3 bytes
                //   0 1 2 3 4 5 6 7
                //  +-+-+-+-+-+-+-+-+
                //  |Size0|H| VER |P|
                //  +-+-+-+-+-+-+-+-+
                //  |     Size1     |
                //  +-+-+-+-+-+-+-+-+
                //  |     Size2     |
                //  +-+-+-+-+-+-+-+-+
                // Keyframe header consists of a three-byte sync code
                // followed by the width and height and associated scaling factors
                if (payload[offset + 3U] == 0x9d &&
                    payload[offset + 4U] == 0x01 &&
                    payload[offset + 5U] == 0x2a) {
                    const uint16_t hor = payload[offset + 7U] << 8 | payload[offset + 6U];
                    const uint16_t ver = payload[offset + 9U] << 8 | payload[offset + 8U];
                    applyTo.SetWidth(hor & 0x3fff);
                    //_videoConfig._widthScale = hor >> 14;
                    applyTo.SetHeight(ver & 0x3fff);
                    //_videoConfig._heightScale = ver >> 14;
                    return true;
                }
            }
        }
    }
    return false;
}

bool RtpDepacketizerVpx::ParseVp9VideoConfig(const RtpPacket* packet, VideoFrameConfig& applyTo)
{
    if (const auto pds = GetPayloadDescriptorSize(packet)) {
        if (const auto payload = packet->GetPayload()) {
            //const auto offset = pds.value();
            /*vp9_parser::Vp9HeaderParser parser;
             if (parser.ParseUncompressedHeader(payload, packet->GetPayloadLength())) {
             videoConfig._width = parser.width();
             videoConfig._height = parser.height();
             return true;
             }*/
        }
    }
    return false;
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
                const auto error = GetStreamInfoString(_depacketizer->GetMime(), packet->GetSsrc());
                MS_ERROR_STD("failed to parse video config for stream [%s]", error.c_str());
            }
        }
        else {
            const auto error = GetStreamInfoString(_depacketizer->GetMime(), packet->GetSsrc());
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
            switch (_depacketizer->GetMime().GetSubtype()) {
                case RtpCodecMimeType::Subtype::VP8:
                    ok = RtpDepacketizerVpx::ParseVp8VideoConfig(packet, config);
                    break;
                case RtpCodecMimeType::Subtype::VP9:
                    ok = RtpDepacketizerVpx::ParseVp9VideoConfig(packet, config);
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
