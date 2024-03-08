#define MS_CLASS "RTC::MediaFrameSerializer"
#include "RTC/MediaTranslate/MediaFrameSerializer.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "RTC/MediaTranslate/MediaFrameWriter.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/RtpDictionaries.hpp"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "Logger.hpp"
#include <optional>

namespace RTC
{

class MediaFrameSerializer::SinkWriter
{
public:
    SinkWriter(std::unique_ptr<MediaFrameWriter> impl);
    bool Write(const MediaFrame& mediaFrame);
    void SetConfig(const AudioFrameConfig& config);
    void SetConfig(const VideoFrameConfig& config);
private:
    const webrtc::TimeDelta& Update(const Timestamp& timestamp);
    bool IsAccepted(const Timestamp& timestamp) const;
private:
    const std::unique_ptr<MediaFrameWriter> _impl;
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

bool MediaFrameSerializer::Write(const MediaFrame& mediaFrame)
{
    bool ok = false;
    if (!IsPaused() && mediaFrame.GetMimeType() == GetMimeType()) {
        WriteToTestSink(mediaFrame);
        LOCK_READ_PROTECTED_OBJ(_writers);
        if (!_writers->empty()) {
            for (auto it = _writers->begin(); it != _writers->end(); ++it) {
                if (it->second->Write(mediaFrame)) {
                    ok = true;
                }
            }
            if (!ok) {
                MS_ERROR_STD("unable to write media frame [%s]", GetMimeType().ToString().c_str());
            }
        }
    }
    return ok;
}

void MediaFrameSerializer::SetConfig(const AudioFrameConfig& config)
{
    MS_ASSERT(GetMimeType().IsAudioCodec(), "audio config is not suitable");
    SetMediaConfig(config);
}

void MediaFrameSerializer::SetConfig(const VideoFrameConfig& config)
{
    MS_ASSERT(GetMimeType().IsVideoCodec(), "video config is not suitable");
    SetMediaConfig(config);
}

bool MediaFrameSerializer::AddTestSink(MediaSink* sink)
{
    if (sink) {
        if (auto writer = CreateSinkWriter(sink)) {
            LOCK_WRITE_PROTECTED_OBJ(_testWriter);
            _testWriter = std::move(writer);
            return true;
        }
    }
    return false;
}

void MediaFrameSerializer::RemoveTestSink()
{
    LOCK_WRITE_PROTECTED_OBJ(_testWriter);
    _testWriter->reset();
}

bool MediaFrameSerializer::HasTestSink() const
{
    LOCK_READ_PROTECTED_OBJ(_testWriter);
    return nullptr != _testWriter->get();
}

bool MediaFrameSerializer::AddSink(MediaSink* sink)
{
    bool added = false;
    if (sink) {
        LOCK_WRITE_PROTECTED_OBJ(_writers);
        added = _writers->count(sink) > 0U;
        if (!added) {
            if (auto writer = CreateSinkWriter(sink)) {
                _writers->insert(std::make_pair(sink, std::move(writer)));
                added = true;
            }
        }
    }
    return added;
}

bool MediaFrameSerializer::RemoveSink(MediaSink* sink)
{
    if (sink) {
        LOCK_WRITE_PROTECTED_OBJ(_writers);
        return _writers->erase(sink) > 0U;
    }
    return false;
}

void MediaFrameSerializer::RemoveAllSinks()
{
    LOCK_WRITE_PROTECTED_OBJ(_writers);
    _writers->clear();
}

bool MediaFrameSerializer::HasSinks() const
{
    LOCK_READ_PROTECTED_OBJ(_writers);
    return !_writers->empty();
}

std::string_view MediaFrameSerializer::GetFileExtension() const
{
    return MimeSubTypeToString(GetMimeType().GetSubtype());
}

std::unique_ptr<MediaFrameSerializer::SinkWriter> MediaFrameSerializer::CreateSinkWriter(MediaSink* sink)
{
    if (auto impl = CreateWriter(sink)) {
        return std::make_unique<SinkWriter>(std::move(impl));
    }
    return nullptr;
}

void MediaFrameSerializer::WriteToTestSink(const MediaFrame& mediaFrame) const
{
    LOCK_READ_PROTECTED_OBJ(_testWriter);
    if (const auto& testWriter = _testWriter.ConstRef()) {
        if (!testWriter->Write(mediaFrame)) {
            // TODO: log warning, maybe at debug level
        }
    }
}

template<class TConfig>
void MediaFrameSerializer::SetMediaConfig(const TConfig& config)
{
    {
        LOCK_READ_PROTECTED_OBJ(_writers);
        for (auto it = _writers->begin(); it != _writers->end(); ++it) {
            it->second->SetConfig(config);
        }
    }
    LOCK_READ_PROTECTED_OBJ(_testWriter);
    if (const auto& testWriter = _testWriter.ConstRef()) {
        testWriter->SetConfig(config);
    }
}

MediaFrameSerializer::SinkWriter::SinkWriter(std::unique_ptr<MediaFrameWriter> impl)
    : _impl(std::move(impl))
{
}

bool MediaFrameSerializer::SinkWriter::Write(const MediaFrame& mediaFrame)
{
    const auto& timestamp = mediaFrame.GetTimestamp();
    if (IsAccepted(timestamp)) {
        return _impl->Write(mediaFrame, Update(timestamp));
    }
    return false;
}

void MediaFrameSerializer::SinkWriter::SetConfig(const AudioFrameConfig& config)
{
    _impl->SetConfig(config);
}

void MediaFrameSerializer::SinkWriter::SetConfig(const VideoFrameConfig& config)
{
    _impl->SetConfig(config);
}

const webrtc::TimeDelta& MediaFrameSerializer::SinkWriter::Update(const Timestamp& timestamp)
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

bool MediaFrameSerializer::SinkWriter::IsAccepted(const Timestamp& timestamp) const
{
    return !_lastTimestamp.has_value() || timestamp >= _lastTimestamp.value();
}

} // namespace RTC
