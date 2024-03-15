#include "RTC/MediaTranslate/MediaSinkWriter.hpp"
#include "RTC/MediaTranslate/RtpDepacketizer.hpp"
#include "RTC/MediaTranslate/MediaFrameWriter.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"

namespace RTC
{

MediaSinkWriter::MediaSinkWriter(std::unique_ptr<RtpDepacketizer> depacketizer,
                                 std::unique_ptr<MediaFrameWriter> frameWriter)
    : _depacketizer(std::move(depacketizer))
    , _frameWriter(std::move(frameWriter))
{
}

MediaSinkWriter::~MediaSinkWriter()
{
}

bool MediaSinkWriter::Write(uint32_t ssrc, uint32_t rtpTimestamp,
                            bool keyFrame, bool hasMarker,
                            const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& pdh,
                            const std::shared_ptr<Buffer>& payload)
{
    if (auto frame = CreateFrame(ssrc, rtpTimestamp, keyFrame, hasMarker, pdh, payload)) {
        return Write(frame.value());
    }
    return false;
}

bool MediaSinkWriter::Write(const MediaFrame& mediaFrame)
{
    const auto& timestamp = mediaFrame.GetTimestamp();
    if (IsAccepted(timestamp)) {
        return _frameWriter->Write(mediaFrame, Update(timestamp));
    }
    return false;
}

std::optional<MediaFrame> MediaSinkWriter::CreateFrame(uint32_t ssrc, uint32_t rtpTimestamp,
                                                       bool keyFrame, bool hasMarker,
                                                       const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& pdh,
                                                       const std::shared_ptr<Buffer>& payload)
{
    bool configChanged = false;
    if (auto frame = _depacketizer->FromRtpPacket(ssrc, rtpTimestamp, keyFrame,
                                                  hasMarker, pdh, payload,
                                                  &configChanged)) {
        if (configChanged) {
            switch (_depacketizer->GetMime().GetType()) {
                case RtpCodecMimeType::Type::AUDIO:
                    _frameWriter->SetConfig(_depacketizer->GetAudioConfig(ssrc));
                    break;
                case RtpCodecMimeType::Type::VIDEO:
                    _frameWriter->SetConfig(_depacketizer->GetVideoConfig(ssrc));
                    break;
            }
        }
        return frame;
    }
    return std::nullopt;
}

const webrtc::TimeDelta& MediaSinkWriter::Update(const Timestamp& timestamp)
{
    if (!_lastTimestamp) {
        _lastTimestamp = timestamp;
    }
    else if (timestamp > _lastTimestamp.value()) {
        _offset += timestamp - _lastTimestamp.value();
        _lastTimestamp = timestamp;
    }
    return _offset;
}

bool MediaSinkWriter::IsAccepted(const Timestamp& timestamp) const
{
    return !_lastTimestamp.has_value() || timestamp >= _lastTimestamp.value();
}

} // namespace RTC
