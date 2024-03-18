#define MS_CLASS "RTC::RtpDepacketizerVpx"
#include "RTC/MediaTranslate/RtpDepacketizerVpx.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/RtpDictionaries.hpp"
#include "RTC/Codecs/VP8.hpp"
#include "RTC/Codecs/VP9.hpp"
#include "Logger.hpp"

namespace RTC
{


class RtpDepacketizerVpx::ShiftedPayload : public Buffer
{
public:
    ShiftedPayload(const std::shared_ptr<Buffer>& payload, size_t offset);
    // impl. of Buffer
    size_t GetSize() const final { return _payload->GetSize() - _offset; }
    uint8_t* GetData() final { return _payload->GetData() + _offset; }
    const uint8_t* GetData() const final { return _payload->GetData() + _offset; }
private:
    const std::shared_ptr<Buffer> _payload;
    const size_t _offset;
};

RtpDepacketizerVpx::RtpDepacketizerVpx(uint32_t ssrc, uint32_t clockRate, bool vp8,
                                       const std::shared_ptr<BufferAllocator>& allocator)
    : RtpVideoDepacketizer(ssrc, clockRate, allocator)
    , _vp8(vp8)
{
}

RtpDepacketizerVpx::~RtpDepacketizerVpx()
{
}

MediaFrame RtpDepacketizerVpx::AddPacketInfo(uint32_t rtpTimestamp,
                                             bool keyFrame, bool hasMarker,
                                             const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& pdh,
                                             const std::shared_ptr<Buffer>& payload,
                                             bool* configWasChanged)
{
    return MergePacketInfo(rtpTimestamp, keyFrame, hasMarker, pdh, payload, configWasChanged);
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

std::string RtpDepacketizerVpx::GetDescription() const
{
    const auto subtype = _vp8 ? RtpCodecMimeType::Subtype::VP8 : RtpCodecMimeType::Subtype::VP9;
    const RtpCodecMimeType mime(RtpCodecMimeType::Type::VIDEO, subtype);
    return GetStreamInfoString(mime, GetSsrc());
}

MediaFrame RtpDepacketizerVpx::MergePacketInfo(uint32_t rtpTimestamp, bool keyFrame, bool hasMarker,
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
                MS_ERROR("failed to parse video config for stream [%s]", GetDescription().c_str());
            }
        }
        else {
            MS_ERROR("failed to add payload for stream [%s]", GetDescription().c_str());
        }
    }
    return MediaFrame();
}

bool RtpDepacketizerVpx::AddPayload(uint32_t rtpTimestamp, bool keyFrame,
                                    const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& pdh,
                                    const std::shared_ptr<Buffer>& payload)
{
    if (_frame && payload && (!keyFrame || pdh)) {
        const size_t offset = pdh ? pdh->GetPayloadDescriptorSize() : 0U;
        const auto len = payload->GetSize();
        if (len > offset) {
            auto buffer = payload;
            if (offset) {
                buffer = std::make_shared<ShiftedPayload>(payload, offset);
            }
            return AddPacketInfoToFrame(rtpTimestamp, keyFrame, buffer, _frame);
        }
    }
    return false;
}

bool RtpDepacketizerVpx::ParseVideoConfig(const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& pdh,
                                          const std::shared_ptr<Buffer>& payload,
                                          bool keyFrame, bool* configWasChanged)
{
    bool ok = false;
    if (keyFrame) {
        VideoFrameConfig config;
        if (_vp8) {
            ok = RtpDepacketizerVpx::ParseVp8VideoConfig(pdh, payload, config);
        }
        else {
            ok = RtpDepacketizerVpx::ParseVp9VideoConfig(pdh, payload, config);
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

MediaFrame RtpDepacketizerVpx::ResetFrame()
{
    MediaFrame frame(std::move(_frame));
    _frame = CreateFrame();
    return frame;
}

void RtpDepacketizerVpx::SetLastTimeStamp(uint32_t lastTimeStamp)
{
    if (_lastTimeStamp != lastTimeStamp) {
        _lastTimeStamp = lastTimeStamp;
        _frame = CreateFrame();
    }
}

RtpDepacketizerVpx::ShiftedPayload::ShiftedPayload(const std::shared_ptr<Buffer>& payload,
                                                   size_t offset)
    : _payload(payload)
    , _offset(offset)
{
}

} // namespace RTC
