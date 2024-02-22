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

class MediaFrameSerializer::SinkWriter
{
public:
    SinkWriter(std::unique_ptr<MediaFrameWriter> impl);
    bool Write(const std::shared_ptr<const MediaFrame>& mediaFrame);
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

bool MediaFrameSerializer::Push(const std::shared_ptr<const MediaFrame>& mediaFrame)
{
    bool ok = false;
    if (mediaFrame && mediaFrame->GetMimeType() == GetMimeType()) {
        WriteToTestSink(mediaFrame);
        LOCK_READ_PROTECTED_OBJ(_writers);
        if (!_writers->empty()) {
            for (auto it = _writers->begin(); it != _writers->end(); ++it) {
                if (it->second->Write(mediaFrame)) {
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

void MediaFrameSerializer::WriteToTestSink(const std::shared_ptr<const MediaFrame>& mediaFrame) const
{
    if (mediaFrame) {
        LOCK_READ_PROTECTED_OBJ(_testWriter);
        if (const auto& testWriter = _testWriter.ConstRef()) {
            if (!testWriter->Write(mediaFrame)) {
                // TODO: log warning, maybe at debug level
            }
        }
    }
}

MediaFrameSerializer::SinkWriter::SinkWriter(std::unique_ptr<MediaFrameWriter> impl)
    : _impl(std::move(impl))
{
}

bool MediaFrameSerializer::SinkWriter::Write(const std::shared_ptr<const MediaFrame>& mediaFrame)
{
    if (mediaFrame) {
        const auto& timestamp = mediaFrame->GetTimestamp();
        if (IsAccepted(timestamp)) {
            switch (mediaFrame->GetMimeType().GetType()) {
                case RtpCodecMimeType::Type::AUDIO:
                    _impl->SetConfig(mediaFrame->GetAudioConfig());
                    break;
                case RtpCodecMimeType::Type::VIDEO:
                    _impl->SetConfig(mediaFrame->GetVideoConfig());
                    break;
            }
            return _impl->Write(mediaFrame, Update(timestamp));
        }
    }
    return false;
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
