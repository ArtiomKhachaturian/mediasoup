#include "RTC/MediaTranslate/MediaFrameSerializer.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "RTC/RtpDictionaries.hpp"

namespace RTC
{

MediaFrameSerializer::MediaFrameSerializer(const RtpCodecMimeType& mime)
    : _mime(mime)
{
}

std::string_view MediaFrameSerializer::GetFileExtension() const
{
    return MimeSubTypeToString(GetMimeType().GetSubtype());
}

bool MediaFrameSerializer::IsAccepted(const std::shared_ptr<const MediaFrame>& mediaFrame) const
{
    if (mediaFrame && mediaFrame->GetMimeType() == GetMimeType()) {
        const auto timestamp = mediaFrame->GetTimestamp();
        // special case if both timestamps are zero, for 1st initial frame
        return (timestamp.IsZero() && _lastTimestamp.IsZero()) || timestamp > _lastTimestamp;
    }
    return false;
}

const webrtc::TimeDelta& MediaFrameSerializer::UpdateTimeOffset(const webrtc::Timestamp& timestamp)
{
    if (timestamp > _lastTimestamp) {
        if (!_lastTimestamp.IsZero()) {
            _offset += timestamp - _lastTimestamp;
        }
        _lastTimestamp = timestamp;
    }
    return _offset;
}

} // namespace RTC
