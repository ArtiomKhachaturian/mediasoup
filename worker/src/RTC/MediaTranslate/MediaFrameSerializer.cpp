#define MS_CLASS "RTC::MediaFrameSerializer"
#include "RTC/MediaTranslate/MediaFrameSerializer.hpp"
#include "RTC/MediaTranslate/MediaSinkWriter.hpp"
#include "RTC/MediaTranslate/MediaFrameWriter.hpp"
#include "RTC/MediaTranslate/RtpMediaWritersQueue.hpp"
#include "RTC/MediaTranslate/RtpDepacketizer.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "Logger.hpp"

namespace RTC
{

MediaFrameSerializer::MediaFrameSerializer(const RtpCodecMimeType& mime, 
                                           uint32_t ssrc, uint32_t clockRate,
                                           const std::shared_ptr<BufferAllocator>& allocator)
    :  BufferAllocations<MediaSource>(allocator)
    , _mime(mime)
    , _ssrc(ssrc)
    , _clockRate(clockRate)
{
    _writers->reserve(2U); // test + end-point
}

MediaFrameSerializer::~MediaFrameSerializer()
{
    MediaFrameSerializer::RemoveAllSinks();
}

void MediaFrameSerializer::Write(const RtpPacket* packet)
{
    if (packet && IsReadyToWrite()) {
        GetQueue().Write(this, packet, GetAllocator());
    }
}

bool MediaFrameSerializer::AddSink(MediaSink* sink)
{
    bool added = false;
    if (sink) {
        bool firstSink = false;
        {
            LOCK_WRITE_PROTECTED_OBJ(_writers);
            added = _writers->count(sink) > 0U;
            if (!added) {
                if (auto writer = CreateSinkWriter(sink)) {
                    _writers->insert(std::make_pair(sink, std::move(writer)));
                    const auto size = _writers->size();
                    added = true;
                    _writersCount = size;
                    firstSink = 1U == size;
                }
            }
        }
        if (added && firstSink) {
            GetQueue().RegisterWriter(this);
        }
    }
    return added;
}

bool MediaFrameSerializer::RemoveSink(MediaSink* sink)
{
    bool removed = false;
    if (sink) {
        bool lastSink = false;
        {
            LOCK_WRITE_PROTECTED_OBJ(_writers);
            removed = _writers->erase(sink) > 0U;
            if (removed) {
                _writersCount = _writers->size();
                lastSink = _writers->empty();
            }
        }
        if (removed && lastSink) {
            GetQueue().UnregisterWriter(this);
        }
    }
    return removed;
}

void MediaFrameSerializer::RemoveAllSinks()
{
    bool removed = false;
    {
        LOCK_WRITE_PROTECTED_OBJ(_writers);
        if (!_writers->empty()) {
            _writers->clear();
            removed = true;
            _writersCount = 0U;
        }
    }
    if (removed) {
        GetQueue().UnregisterWriter(this);
    }
}

bool MediaFrameSerializer::HasSinks() const
{
    return _writersCount.load() > 0U;
}

std::string_view MediaFrameSerializer::GetFileExtension() const
{
    return MimeSubTypeToString(GetMime().GetSubtype());
}

RtpMediaWritersQueue& MediaFrameSerializer::GetQueue()
{
    static RtpMediaWritersQueue queue;
    return queue;
}

std::unique_ptr<MediaSinkWriter> MediaFrameSerializer::CreateSinkWriter(MediaSink* sink)
{
    std::unique_ptr<MediaSinkWriter> writer;
    if (auto impl = CreateWriter(GetId(), sink)) {
        if (auto depacketizer = RtpDepacketizer::Create(GetMime(), GetSsrc(), 
                                                        GetClockRate(), GetAllocator())) {
            writer = std::make_unique<MediaSinkWriter>(std::move(depacketizer),
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

bool MediaFrameSerializer::WriteRtpMedia(const RtpPacketInfo& rtpMedia)
{
    LOCK_READ_PROTECTED_OBJ(_writers);
    for (auto it = _writers->begin(); it != _writers->end(); ++it) {
        if (!it->second->WriteRtpMedia(rtpMedia)) {
            MS_ERROR("unable to write media frame [%s]", GetMimeText().c_str());
        }
    }
    return true;
}

} // namespace RTC
