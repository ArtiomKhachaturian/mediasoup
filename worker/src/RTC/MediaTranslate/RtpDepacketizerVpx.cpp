#define MS_CLASS "RTC::RtpDepacketizerVpx"
#include "RTC/MediaTranslate/RtpDepacketizerVpx.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/Codecs/VP8.hpp"
#include "RTC/Codecs/VP9.hpp"
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
    RtpAssembly(uint32_t ssrc, const RtpDepacketizerVpx* depacketizer);
    std::optional<MediaFrame> AddPacket(uint32_t rtpTimestamp,
                                        bool keyFrame, bool hasMarker,
                                        const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& pdh,
                                        const std::shared_ptr<Buffer>& payload,
                                        bool* configWasChanged);
    const VideoFrameConfig& GetConfig() const { return _config; }
private:
    bool AddPayload(uint32_t rtpTimestamp, bool keyFrame,
                    const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& pdh,
                    const std::shared_ptr<Buffer>& payload);
    bool ParseVideoConfig(const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& pdh,
                          const std::shared_ptr<Buffer>& payload,
                          bool keyFrame, bool* configWasChanged);
    std::optional<MediaFrame> ResetFrame();
    void SetLastTimeStamp(uint32_t lastTimeStamp);
private:
    const uint32_t _ssrc;
    const RtpDepacketizerVpx* const _depacketizer;
    uint32_t _lastTimeStamp = 0U;
    std::optional<MediaFrame> _frame;
    VideoFrameConfig _config;
};

class RtpDepacketizerVpx::ShiftedPayloadBuffer : public Buffer
{
public:
    ShiftedPayloadBuffer(const std::shared_ptr<Buffer>& payload, size_t offset);
    // impl. of Buffer
    size_t GetSize() const final { return _payload->GetSize() - _offset; }
    uint8_t* GetData() final { return _payload->GetData() + _offset; }
    const uint8_t* GetData() const final { return _payload->GetData() + _offset; }
private:
    const std::shared_ptr<Buffer> _payload;
    const size_t _offset;
};

RtpDepacketizerVpx::RtpDepacketizerVpx(uint32_t clockRate, bool vp8,
                                       const std::shared_ptr<BufferAllocator>& allocator)
    : RtpVideoDepacketizer(GetType(vp8), clockRate, allocator)
{
}

RtpDepacketizerVpx::~RtpDepacketizerVpx()
{
}

std::optional<MediaFrame> RtpDepacketizerVpx::FromRtpPacket(uint32_t ssrc, uint32_t rtpTimestamp,
                                                            bool keyFrame, bool hasMarker,
                                                            const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& pdh,
                                                            const std::shared_ptr<Buffer>& payload,
                                                            bool* configWasChanged)
{
    if (payload) {
        auto it = _assemblies.find(ssrc);
        if (it == _assemblies.end()) {
            auto assembly = std::make_unique<RtpAssembly>(ssrc, this);
            it = _assemblies.insert({ssrc, std::move(assembly)}).first;
        }
        return it->second->AddPacket(rtpTimestamp, keyFrame, hasMarker, pdh,
                                     payload, configWasChanged);
    }
    return std::nullopt;
}

VideoFrameConfig RtpDepacketizerVpx::GetVideoConfig(uint32_t ssrc) const
{
    const auto it = _assemblies.find(ssrc);
    if (it != _assemblies.end()) {
        return it->second->GetConfig();
    }
    return RtpDepacketizer::GetVideoConfig(ssrc);
}

bool RtpDepacketizerVpx::ParseVp8VideoConfig(const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& pdh,
                                             const std::shared_ptr<Buffer>& payload,
                                             VideoFrameConfig& applyTo)
{
    if (pdh && payload && payload->GetData()) {
        const auto offset = pdh->GetPayloadDescriptorSize();
        const auto len = payload->GetSize();
        if (len >= offset + 10U) {
            const auto data = payload->GetData();
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
            if (data[offset + 3U] == 0x9d &&
                data[offset + 4U] == 0x01 &&
                data[offset + 5U] == 0x2a) {
                const uint16_t hor = data[offset + 7U] << 8 | data[offset + 6U];
                const uint16_t ver = data[offset + 9U] << 8 | data[offset + 8U];
                applyTo.SetWidth(hor & 0x3fff);
                //_videoConfig._widthScale = hor >> 14;
                applyTo.SetHeight(ver & 0x3fff);
                //_videoConfig._heightScale = ver >> 14;
                return true;
            }
        }
    }
    return false;
}

bool RtpDepacketizerVpx::ParseVp9VideoConfig(const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& /*pdh*/,
                                             const std::shared_ptr<Buffer>& /*payload*/,
                                             VideoFrameConfig& /*applyTo*/)
{
    MS_ASSERT(false, "VP9 config parser not yet implemented");
    return false;
}

RtpDepacketizerVpx::RtpAssembly::RtpAssembly(uint32_t ssrc, const RtpDepacketizerVpx* depacketizer)
    : _ssrc(ssrc)
    , _depacketizer(depacketizer)
{
}

std::optional<MediaFrame> RtpDepacketizerVpx::RtpAssembly::AddPacket(uint32_t rtpTimestamp,
                                                                     bool keyFrame, bool hasMarker,
                                                                     const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& pdh,
                                                                     const std::shared_ptr<Buffer>& payload,
                                                                     bool* configWasChanged)
{
    if (payload) {
        SetLastTimeStamp(rtpTimestamp);
        // Add payload
        if (AddPayload(rtpTimestamp, keyFrame, pdh, payload)) {
            if (ParseVideoConfig(pdh, payload, keyFrame, configWasChanged)) {
                if (hasMarker) {
                    return ResetFrame();
                }
            }
            else {
                const auto error = GetStreamInfoString(_depacketizer->GetMime(), _ssrc);
                MS_ERROR("failed to parse video config for stream [%s]", error.c_str());
            }
        }
        else {
            const auto error = GetStreamInfoString(_depacketizer->GetMime(), _ssrc);
            MS_ERROR("failed to add payload for stream [%s]", error.c_str());
        }
    }
    return std::nullopt;
}

bool RtpDepacketizerVpx::RtpAssembly::AddPayload(uint32_t rtpTimestamp, bool keyFrame,
                                                 const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& pdh,
                                                 const std::shared_ptr<Buffer>& payload)
{
    if (_frame && payload && (!keyFrame || pdh)) {
        const size_t offset = pdh ? pdh->GetPayloadDescriptorSize() : 0U;
        const auto len = payload->GetSize();
        if (len > offset) {
            auto buffer = payload;
            if (offset) {
                buffer = std::make_shared<ShiftedPayloadBuffer>(payload, offset);
            }
            RtpDepacketizer::AddPacketToFrame(_ssrc, rtpTimestamp, keyFrame, buffer, _frame.value());
            return true;
        }
    }
    return false;
}

bool RtpDepacketizerVpx::RtpAssembly::ParseVideoConfig(const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& pdh,
                                                       const std::shared_ptr<Buffer>& payload,
                                                       bool keyFrame, bool* configWasChanged)
{
    bool ok = false;
    if (keyFrame) {
        VideoFrameConfig config;
        switch (_depacketizer->GetMime().GetSubtype()) {
            case RtpCodecMimeType::Subtype::VP8:
                ok = RtpDepacketizerVpx::ParseVp8VideoConfig(pdh, payload, config);
                break;
            case RtpCodecMimeType::Subtype::VP9:
                ok = RtpDepacketizerVpx::ParseVp9VideoConfig(pdh, payload, config);
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
    return ok;
}

std::optional<MediaFrame> RtpDepacketizerVpx::RtpAssembly::ResetFrame()
{
    std::optional<MediaFrame> frame(std::move(_frame));
    _frame = _depacketizer->CreateFrame();
    return frame;
}

void RtpDepacketizerVpx::RtpAssembly::SetLastTimeStamp(uint32_t lastTimeStamp)
{
    if (_lastTimeStamp != lastTimeStamp) {
        _lastTimeStamp = lastTimeStamp;
        _frame = _depacketizer->CreateFrame();
    }
}

RtpDepacketizerVpx::ShiftedPayloadBuffer::ShiftedPayloadBuffer(const std::shared_ptr<Buffer>& payload,
                                                               size_t offset)
    : _payload(payload)
    , _offset(offset)
{
}

} // namespace RTC
