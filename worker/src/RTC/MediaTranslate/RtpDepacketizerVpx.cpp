#define MS_CLASS "RTC::RtpDepacketizerVpx"
#include "RTC/MediaTranslate/RtpDepacketizerVpx.hpp"
#include "RTC/MediaTranslate/RtpMediaFrame.hpp"
#include "RTC/MediaTranslate/SimpleMemoryBuffer.hpp"
#include "RTC/Codecs/VP8.hpp"
#include "RTC/Codecs/VP9.hpp"
#include "RTC/RtpPacket.hpp"
#include "Logger.hpp"
#include <optional>

namespace {

inline std::optional<size_t> GetPayloadDescriptorSize(const RTC::RtpPacket* packet) {
    if (packet) {
        if (const auto pdh = packet->GetPayloadDescriptorHandler()) {
            return pdh->GetPayloadDescriptorSize();
        }
    }
    return std::nullopt;
}

}

namespace RTC
{

class RtpDepacketizerVpx::RtpAssembly
{
public:
    RtpAssembly(const RtpDepacketizerVpx* owner);
    std::shared_ptr<RtpMediaFrame> AddPacket(const RtpPacket* packet);
private:
    static bool ParseVp8VideoConfig(const RtpPacket* packet,
                                    RtpVideoFrameConfig& videoConfig);
    static bool ParseVp9VideoConfig(const RtpPacket* packet,
                                    RtpVideoFrameConfig& videoConfig);
    bool AddVp8Payload(const RtpPacket* packet);
    bool AddVp9Payload(const RtpPacket* packet);
private:
    const RtpDepacketizerVpx* const _owner;
    SimpleMemoryBuffer _payload;
    uint32_t _lastTimeStamp = 0U;
};


RtpDepacketizerVpx::RtpDepacketizerVpx(const RtpCodecMimeType& codecMimeType, uint32_t sampleRate)
    : RtpDepacketizer(codecMimeType, sampleRate)
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
            auto assembly = std::make_unique<RtpAssembly>(this);
            it = _assemblies.insert({packet->GetSsrc(), std::move(assembly)}).first;
        }
        return it->second->AddPacket(packet);
    }
    return nullptr;
}

RtpDepacketizerVpx::RtpAssembly::RtpAssembly(const RtpDepacketizerVpx* owner)
    : _owner(owner)
{
}

std::shared_ptr<RtpMediaFrame> RtpDepacketizerVpx::RtpAssembly::AddPacket(const RtpPacket* packet)
{
    if (packet && packet->GetPayload() && packet->GetPayloadLength()) {
        bool ok = true;
        if (_lastTimeStamp != packet->GetTimestamp()) {
            _lastTimeStamp = packet->GetTimestamp();
            _payload.Resize(0U);
        }
        // Add payload
        switch (_owner->GetCodecMimeType().GetSubtype()) {
            case RtpCodecMimeType::Subtype::VP8:
                ok = AddVp8Payload(packet);
                break;
            case RtpCodecMimeType::Subtype::VP9:
                ok = AddVp9Payload(packet);
                break;
            default:
                break;
        }
        if (ok) {
            RtpVideoFrameConfig videoConfig;
            if (packet->IsKeyFrame()) {
                switch (_owner->GetCodecMimeType().GetSubtype()) {
                    case RtpCodecMimeType::Subtype::VP8:
                        ok = ParseVp8VideoConfig(packet, videoConfig);
                        break;
                    case RtpCodecMimeType::Subtype::VP9:
                        ok = ParseVp9VideoConfig(packet, videoConfig);
                        break;
                    default:
                        break;
                }
                if (ok) {
                    videoConfig._frameRate = 30.; // TODO: replace to real value
                }
            }
            if (ok) {
                if (packet->HasMarker()) {
                    const auto codecType = _owner->GetCodecMimeType().GetSubtype();
                    const auto sampleRate = _owner->GetSampleRate();
                    if (const auto payload = _payload.Take()) {
                        return RtpMediaFrame::CreateVideo(packet, payload, codecType, sampleRate, videoConfig);
                    }
                    else {
                        // TODO: log error
                    }
                }
            }
            else {
                // TODO: log error
            }
        }
        else {
            // TODO: log error
        }
    }
    return nullptr;
}

bool RtpDepacketizerVpx::RtpAssembly::ParseVp8VideoConfig(const RtpPacket* packet,
                                                          RtpVideoFrameConfig& videoConfig)
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
                    videoConfig._width = hor & 0x3fff;
                    //videoConfig._widthScale = hor >> 14;
                    videoConfig._height = ver & 0x3fff;
                    //videoConfig._heightScale = ver >> 14;
                    return true;
                }
            }
        }
    }
    return false;
}

bool RtpDepacketizerVpx::RtpAssembly::ParseVp9VideoConfig(const RtpPacket* packet,
                                                          RtpVideoFrameConfig& videoConfig)
{
    if (const auto pds = GetPayloadDescriptorSize(packet)) {
        if (const auto payload = packet->GetPayload()) {
            //const auto offset = pds.value();
        }
    }
    return false;
}

bool RtpDepacketizerVpx::RtpAssembly::AddVp8Payload(const RtpPacket* packet)
{
    if (const auto pds = GetPayloadDescriptorSize(packet)) {
        if (const auto payload = packet->GetPayload()) {
            const auto len = packet->GetPayloadLength();
            if (len > pds.value()) {
                const auto data = packet->GetPayload() + pds.value();
                return _payload.Append(data, len - pds.value());
            }
        }
    }
    return false;
}

bool RtpDepacketizerVpx::RtpAssembly::AddVp9Payload(const RtpPacket* packet)
{
    return false;
}

} // namespace RTC
