#define MS_CLASS "RTC::WebMDeserializedTrack"
#include "RTC/MediaTranslate/WebM/WebMDeserializedTrack.hpp"
#include "RTC/MediaTranslate/WebM/MkvReadResult.hpp"
#include "RTC/MediaTranslate/WebM/WebMCodecs.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "Logger.hpp"

namespace RTC
{

WebMDeserializedTrack::WebMDeserializedTrack(const RtpCodecMimeType& mime,
                                             const mkvparser::Track* track,
                                             const std::shared_ptr<BufferAllocator>& allocator)
    : BufferAllocations<MediaFrameDeserializedTrack>(allocator)
    , _mime(mime)
    , _track(track)
{
    if (_mime.IsAudioCodec()) {
        const auto audioTrack = static_cast<const mkvparser::AudioTrack*>(_track);
        SetClockRate(audioTrack->GetSamplingRate());
    }
}

std::optional<MediaFrame> WebMDeserializedTrack::NextFrame(size_t payloadOffset,
                                                           bool skipPayload,
                                                           size_t payloadExtraSize)
{
    std::optional<MediaFrame> mediaFrame;
    MkvReadResult mkvResult = MkvReadResult::Success;
    if (_currentEntry) {
        if (_currentEntry.IsEndOfStream()) {
            mkvResult = MkvReadResult::NoMoreClusters;
        }
        else if (_currentEntry.IsEndOfEntry()) {
            mkvResult = AdvanceToNextEntry();
        }
    }
    else {
        mkvResult = AdvanceToNextEntry();
    }
    if (MaybeOk(mkvResult) && !_currentEntry.IsEndOfEntry()) {
        const auto frame = _currentEntry.NextFrame();
        const auto frameLen = skipPayload ? 0U : static_cast<size_t>(frame.len);
        std::shared_ptr<Buffer> buffer;
        if (!skipPayload) {
            buffer = AllocateBuffer(payloadOffset + frameLen + payloadExtraSize);
            if (buffer) {
                const auto payloadAddr = buffer->GetData() + payloadOffset;
                mkvResult = ToMkvReadResult(frame.Read(GetReader(), payloadAddr));
            }
            else {
                mkvResult = MkvReadResult::OutOfMemory;
            }
        }
        if (MaybeOk(mkvResult)) {
            mediaFrame = std::make_optional<MediaFrame>(GetClockRate(), GetAllocator());
            mediaFrame->SetKeyFrame(_currentEntry.IsKey());
            mediaFrame->AddPayload(std::move(buffer));
            mediaFrame->SetTimestamp(_currentEntry.GetTime());
            SetLastPayloadSize(frameLen);
        }
    }
    if (!MaybeOk(mkvResult)) {
        MS_ERROR_STD("read frame error: %s", MkvReadResultToString(mkvResult));
    }
    SetLastResult(FromMkvReadResult(mkvResult));
    return mediaFrame;
}

std::unique_ptr<WebMDeserializedTrack> WebMDeserializedTrack::
    Create(const mkvparser::Tracks* tracks, unsigned long trackIndex,
           const std::shared_ptr<BufferAllocator>& allocator)
{
    std::unique_ptr<WebMDeserializedTrack> trackInfo;
    if (tracks) {
        const auto track = tracks->GetTrackByIndex(trackIndex);
        if (const auto mime = GetMime(track)) {
            trackInfo.reset(new WebMDeserializedTrack(mime.value(), track, allocator));
        }
    }
    return trackInfo;
}

mkvparser::IMkvReader* WebMDeserializedTrack::GetReader() const
{
    return _track->m_pSegment->m_pReader;
}

MkvReadResult WebMDeserializedTrack::AdvanceToNextEntry()
{
    if (!_currentEntry) {
        return _currentEntry.ReadFirst(_track);
    }
    return _currentEntry.ReadNext(_track);
}

std::optional<RtpCodecMimeType> WebMDeserializedTrack::GetMime(const mkvparser::Track* track)
{
    if (track) {
        std::optional<RtpCodecMimeType::Type> type;
        switch (track->GetType()) {
            case mkvparser::Track::Type::kVideo:
                type = RtpCodecMimeType::Type::VIDEO;
                break;
            case mkvparser::Track::Type::kAudio:
                type = RtpCodecMimeType::Type::AUDIO;
                break;
            default:
                break;
        }
        if (type.has_value()) {
            std::optional<RtpCodecMimeType::Subtype> subtype;
            for (auto it = RtpCodecMimeType::subtype2String.begin();
                 it != RtpCodecMimeType::subtype2String.end(); ++it) {
                const auto codecId = WebMCodecs::GetCodecId(it->first);
                if (codecId && 0 == std::strcmp(track->GetCodecId(), codecId)) {
                    subtype = it->first;
                    break;
                }
            }
            if (subtype.has_value()) {
                return std::make_optional<RtpCodecMimeType>(type.value(), subtype.value());
            }
        }
    }
    return std::nullopt;
}

} // namespace RTC
