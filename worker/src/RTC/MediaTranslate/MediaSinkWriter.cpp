#include "RTC/MediaTranslate/MediaSinkWriter.hpp"
#include "RTC/MediaTranslate/RtpDepacketizer.hpp"
#include "RTC/MediaTranslate/MediaFrameWriter.hpp"

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

bool MediaSinkWriter::WriteRtpMedia(uint32_t ssrc, uint32_t rtpTimestamp,
                                    bool keyFrame, bool hasMarker,
                                    const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& pdh,
                                    const std::shared_ptr<Buffer>& payload)
{
    return Write(CreateFrame(ssrc, rtpTimestamp, keyFrame, hasMarker, pdh, payload));
}

bool MediaSinkWriter::Write(const MediaFrame& mediaFrame)
{
    if (mediaFrame) {
        const auto& timestamp = mediaFrame.GetTimestamp();
        if (IsAccepted(timestamp)) {
            return _frameWriter->Write(mediaFrame, Update(timestamp));
        }
    }
    return false;
}

MediaFrame MediaSinkWriter::CreateFrame(uint32_t ssrc, uint32_t rtpTimestamp,
                                        bool keyFrame, bool hasMarker,
                                        const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& pdh,
                                        const std::shared_ptr<Buffer>& payload)
{
    bool configChanged = false;
    auto frame = _depacketizer->FromRtpPacket(ssrc, rtpTimestamp, keyFrame, hasMarker,
                                              pdh, payload, &configChanged);
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

const webrtc::TimeDelta& MediaSinkWriter::Update(const Timestamp& timestamp)
{
    if (!_lastTimestamp) {
        _lastTimestamp = timestamp;
    }
    else if (timestamp > _lastTimestamp) {
        _offset += timestamp - _lastTimestamp;
        _lastTimestamp = timestamp;
    }
    return _offset;
}

bool MediaSinkWriter::IsAccepted(const Timestamp& timestamp) const
{
    return !_lastTimestamp || timestamp >= _lastTimestamp;
}

} // namespace RTC
