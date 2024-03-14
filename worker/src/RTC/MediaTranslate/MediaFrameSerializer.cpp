#define MS_CLASS "RTC::MediaFrameSerializer"
#include "RTC/MediaTranslate/MediaFrameSerializer.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "RTC/MediaTranslate/MediaFrameWriter.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/MediaTranslate/RtpDepacketizer.hpp"
#include "RTC/MediaTranslate/MediaSink.hpp"
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
    SinkWriter(std::unique_ptr<RtpDepacketizer> depacketizer,
               std::unique_ptr<MediaFrameWriter> impl);
    ~SinkWriter();
    bool Write(const RtpPacket* packet);
private:
    bool Write(const MediaFrame& mediaFrame);
    std::optional<MediaFrame> CreateFrame(const RtpPacket* packet);
    const webrtc::TimeDelta& Update(const Timestamp& timestamp);
    bool IsAccepted(const Timestamp& timestamp) const;
private:
    const std::unique_ptr<RtpDepacketizer> _depacketizer;
    const std::unique_ptr<MediaFrameWriter> _impl;
    std::optional<Timestamp> _lastTimestamp;
    webrtc::TimeDelta _offset = webrtc::TimeDelta::Zero();
};

MediaFrameSerializer::MediaFrameSerializer(const RtpCodecMimeType& mime,
                                           uint32_t clockRate,
                                           const std::shared_ptr<BufferAllocator>& allocator)
    :  BufferAllocations<MediaSource>(allocator)
    , _mime(mime)
    , _clockRate(clockRate)
{
    _writers->reserve(1U);
}

MediaFrameSerializer::~MediaFrameSerializer()
{
    MediaFrameSerializer::RemoveAllSinks();
}

void MediaFrameSerializer::Write(const RtpPacket* packet)
{
    if (packet && !IsPaused()) {
        WriteToTestSink(packet);
        LOCK_READ_PROTECTED_OBJ(_writers);
        for (auto it = _writers->begin(); it != _writers->end(); ++it) {
            if (!it->second->Write(packet)) {
                MS_ERROR("unable to write media frame [%s]", GetMimeText().c_str());
            }
        }
    }
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
    return MimeSubTypeToString(GetMime().GetSubtype());
}

std::unique_ptr<MediaFrameSerializer::SinkWriter> MediaFrameSerializer::
    CreateSinkWriter(MediaSink* sink)
{
    std::unique_ptr<SinkWriter> writer;
    if (auto impl = CreateWriter(GetId(), sink)) {
        if (auto depacketizer = RtpDepacketizer::Create(GetMime(),
                                                        GetClockRate(),
                                                        GetAllocator())) {
            writer = std::make_unique<SinkWriter>(std::move(depacketizer),
                                                  std::move(impl));
        }
        else {
            MS_ERROR("failed create of RTP depacketizer [%s], clock rate %u Hz",
                     GetMimeText().c_str(), GetClockRate());
        }
    }
    else {
        MS_ERROR("failed create of media sink writer [%s]", GetMimeText().c_str());
    }
    return writer;
}

void MediaFrameSerializer::WriteToTestSink(const RtpPacket* packet) const
{
    LOCK_READ_PROTECTED_OBJ(_testWriter);
    if (const auto& testWriter = _testWriter.ConstRef()) {
        if (!testWriter->Write(packet)) {
            MS_ERROR("unable write media frame [%s] to test sink", GetMimeText().c_str());
        }
    }
}

MediaFrameSerializer::SinkWriter::SinkWriter(std::unique_ptr<RtpDepacketizer> depacketizer,
                                             std::unique_ptr<MediaFrameWriter> impl)
    : _depacketizer(std::move(depacketizer))
    , _impl(std::move(impl))
{
}

MediaFrameSerializer::SinkWriter::~SinkWriter()
{
}

bool MediaFrameSerializer::SinkWriter::Write(const RtpPacket* packet)
{
    if (auto frame = CreateFrame(packet)) {
        return Write(frame.value());
    }
    return false;
}

bool MediaFrameSerializer::SinkWriter::Write(const MediaFrame& mediaFrame)
{
    const auto& timestamp = mediaFrame.GetTimestamp();
    if (IsAccepted(timestamp)) {
        return _impl->Write(mediaFrame, Update(timestamp));
    }
    return false;
}

std::optional<MediaFrame> MediaFrameSerializer::SinkWriter::CreateFrame(const RtpPacket* packet)
{
    if (packet) {
        bool configChanged = false;
        if (auto frame = _depacketizer->FromRtpPacket(packet, &configChanged)) {
            if (configChanged) {
                switch (_depacketizer->GetMime().GetType()) {
                    case RtpCodecMimeType::Type::AUDIO:
                        _impl->SetConfig(_depacketizer->GetAudioConfig(packet));
                        break;
                    case RtpCodecMimeType::Type::VIDEO:
                        _impl->SetConfig(_depacketizer->GetVideoConfig(packet));
                        break;
                }
            }
            return frame;
        }
    }
    return std::nullopt;
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
