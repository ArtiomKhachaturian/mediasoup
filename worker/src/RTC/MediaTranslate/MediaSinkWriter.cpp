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

bool MediaSinkWriter::WriteRtpMedia(const RtpPacketInfo& rtpMedia)
{
    return Write(CreateFrame(rtpMedia));
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

MediaFrame MediaSinkWriter::CreateFrame(const RtpPacketInfo& rtpMedia)
{
    bool configChanged = false;
    auto frame = _depacketizer->AddPacketInfo(rtpMedia, &configChanged);
    if (configChanged) {
        if (_depacketizer->IsAudio()) {
            _frameWriter->SetConfig(_depacketizer->GetAudioConfig());
        }
        else {
            _frameWriter->SetConfig(_depacketizer->GetVideoConfig());
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
