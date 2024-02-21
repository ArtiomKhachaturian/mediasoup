#define MS_CLASS "RTC::MediaFrameSerializer"
#include "RTC/MediaTranslate/MediaFrameSerializer.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "RTC/MediaTranslate/MediaFrameWriter.hpp"
#include "RTC/RtpDictionaries.hpp"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "Logger.hpp"
#include <optional>

namespace RTC
{

class MediaFrameSerializer::OffsetEstimator
{
public:
    OffsetEstimator() = default;
    const webrtc::TimeDelta& Update(const Timestamp& timestamp);
    bool IsAccepted(const Timestamp& timestamp) const;
private:
    // TODO: think about thread-safety for these members
    std::optional<Timestamp> _lastTimestamp;
    webrtc::TimeDelta _offset = webrtc::TimeDelta::Zero();
};

MediaFrameSerializer::MediaFrameSerializer(const RtpCodecMimeType& mime)
    : _mime(mime)
{
}

MediaFrameSerializer::~MediaFrameSerializer()
{
    MediaFrameSerializer::RemoveAllSinks();
}

bool MediaFrameSerializer::Push(const std::shared_ptr<const MediaFrame>& mediaFrame)
{
    bool ok = false;
    if (mediaFrame && mediaFrame->GetMimeType() == GetMimeType()) {
        PushToTestSink(mediaFrame);
        LOCK_READ_PROTECTED_OBJ(_sinks);
        if (!_sinks->empty()) {
            for (auto it = _sinks->begin(); it != _sinks->end(); ++it) {
                if (Push(mediaFrame, it->second)) {
                    ok = true;
                }
            }
            if (!ok) {
                const auto frameInfo = GetMediaFrameInfoString(mediaFrame);
                MS_ERROR_STD("unable to write media frame [%s]", frameInfo.c_str());
            }
        }
    }
    return ok;
}

bool MediaFrameSerializer::AddTestSink(MediaSink* sink)
{
    if (sink) {
        if (auto writer = CreateWriter(sink)) {
            LOCK_WRITE_PROTECTED_OBJ(_testSink);
            _testSink->first = std::move(writer);
            _testSink->second = std::make_unique<OffsetEstimator>();
            return true;
        }
    }
    return false;
}

void MediaFrameSerializer::RemoveTestSink()
{
    LOCK_WRITE_PROTECTED_OBJ(_testSink);
    _testSink->first.reset();
    _testSink->second.reset();
}

bool MediaFrameSerializer::AddSink(MediaSink* sink)
{
    bool added = false;
    if (sink) {
        LOCK_WRITE_PROTECTED_OBJ(_sinks);
        added = _sinks->count(sink) > 0U;
        if (!added) {
            if (auto writer = CreateWriter(sink)) {
                auto sinkData = std::make_pair(std::move(writer), std::make_unique<OffsetEstimator>());
                _sinks->insert(std::make_pair(sink, std::move(sinkData)));
                added = true;
            }
        }
    }
    return added;
}

bool MediaFrameSerializer::RemoveSink(MediaSink* sink)
{
    if (sink) {
        LOCK_WRITE_PROTECTED_OBJ(_sinks);
        return _sinks->erase(sink) > 0U;
    }
    return false;
}

void MediaFrameSerializer::RemoveAllSinks()
{
    LOCK_WRITE_PROTECTED_OBJ(_sinks);
    _sinks->clear();
}

bool MediaFrameSerializer::HasSinks() const
{
    LOCK_READ_PROTECTED_OBJ(_sinks);
    return !_sinks->empty();
}

std::string_view MediaFrameSerializer::GetFileExtension() const
{
    return MimeSubTypeToString(GetMimeType().GetSubtype());
}

void MediaFrameSerializer::PushToTestSink(const std::shared_ptr<const MediaFrame>& mediaFrame) const
{
    if (mediaFrame) {
        LOCK_READ_PROTECTED_OBJ(_testSink);
        if (!Push(mediaFrame, _testSink.ConstRef())) {
            // TODO: log warning, maybe at debug level
        }
    }
}

bool MediaFrameSerializer::Push(const std::shared_ptr<const MediaFrame>& mediaFrame,
                                const SinkData& sink)
{
    if (mediaFrame) {
        const auto& writer = sink.first;
        const auto& offset = sink.second;
        if (writer && offset) {
            const auto& timestamp = mediaFrame->GetTimestamp();
            if (offset->IsAccepted(timestamp)) {
                switch (mediaFrame->GetMimeType().GetType()) {
                    case RtpCodecMimeType::Type::AUDIO:
                        writer->SetConfig(mediaFrame->GetAudioConfig());
                        break;
                    case RtpCodecMimeType::Type::VIDEO:
                        writer->SetConfig(mediaFrame->GetVideoConfig());
                        break;
                }
                return writer->Write(mediaFrame, offset->Update(timestamp));
            }
        }
    }
    return false;
}

const webrtc::TimeDelta& MediaFrameSerializer::OffsetEstimator::Update(const Timestamp& timestamp)
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

bool MediaFrameSerializer::OffsetEstimator::IsAccepted(const Timestamp& timestamp) const
{
    return !_lastTimestamp.has_value() || timestamp >= _lastTimestamp.value();
}

} // namespace RTC
