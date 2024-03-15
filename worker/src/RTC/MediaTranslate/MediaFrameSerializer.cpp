#define MS_CLASS "RTC::MediaFrameSerializer"
#include "RTC/MediaTranslate/MediaFrameSerializer.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "RTC/MediaTranslate/MediaFrameWriter.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/MediaTranslate/RtpDepacketizer.hpp"
#include "RTC/MediaTranslate/MediaSink.hpp"
#include "RTC/MediaTranslate/ThreadExecution.hpp"
#include "RTC/RtpDictionaries.hpp"
#include "RTC/RtpPacket.hpp"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "Logger.hpp"
#include <condition_variable>
#include <optional>
#include <queue>

namespace {

struct PacketInfo
{
    PacketInfo() = default;
    PacketInfo(uint64_t serializerId, uint32_t ssrc, uint32_t rtpTimestamp,
               bool keyFrame, bool hasMarker,
               std::shared_ptr<const RTC::Codecs::PayloadDescriptorHandler> pdh,
               std::shared_ptr<RTC::Buffer> payload);
    static PacketInfo FromRtpPacket(const RTC::ObjectId* serializer, const RTC::RtpPacket* packet,
                                    const std::shared_ptr<RTC::BufferAllocator>& allocator = nullptr);
    uint64_t _serializerId = 0U;
    uint32_t _ssrc = 0U;
    uint32_t _rtpTimestamp = 0U;
    bool _keyFrame = false;
    bool _hasMarker = false;
    std::shared_ptr<const RTC::Codecs::PayloadDescriptorHandler> _pdh;
    std::shared_ptr<RTC::Buffer> _payload;
};

}

namespace RTC
{

class MediaFrameSerializer::SinkWriter
{
public:
    SinkWriter(std::unique_ptr<RtpDepacketizer> depacketizer,
               std::unique_ptr<MediaFrameWriter> impl);
    ~SinkWriter();
    bool Write(uint32_t ssrc, uint32_t rtpTimestamp,
               bool keyFrame, bool hasMarker,
               const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& pdh,
               const std::shared_ptr<Buffer>& payload);
private:
    bool Write(const MediaFrame& mediaFrame);
    std::optional<MediaFrame> CreateFrame(uint32_t ssrc, uint32_t rtpTimestamp,
                                          bool keyFrame, bool hasMarker,
                                          const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& pdh,
                                          const std::shared_ptr<Buffer>& payload);
    const webrtc::TimeDelta& Update(const Timestamp& timestamp);
    bool IsAccepted(const Timestamp& timestamp) const;
private:
    const std::unique_ptr<RtpDepacketizer> _depacketizer;
    const std::unique_ptr<MediaFrameWriter> _impl;
    std::optional<Timestamp> _lastTimestamp;
    webrtc::TimeDelta _offset = webrtc::TimeDelta::Zero();
};

class MediaFrameSerializer::Queue : public ThreadExecution
{
    using PacketsQueue = std::queue<PacketInfo>;
public:
    Queue();
    ~Queue();
    void RegisterSerializer(MediaFrameSerializer* serializer);
    void UnregisterSerializer(const MediaFrameSerializer* serializer);
    void Write(const ObjectId* serializer, const RtpPacket* packet,
               const std::shared_ptr<BufferAllocator>& allocator = nullptr);
protected:
    // impl. of ThreadExecution
    void DoExecuteInThread() final;
    void DoStopThread() final;
private:
    void DropPendingPackets();
    void WritePacket(const PacketInfo& packetInfo) const;
private:
    // key is serializer ID
    ProtectedMap<uint64_t, MediaFrameSerializer*> _serializers;
    std::mutex _packetsMutex;
    std::condition_variable _packetsCondition;
    PacketsQueue _packets;
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
        GetQueue().Write(this, packet);
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
                    added = true;
                    firstSink = 1U == _writers->size();
                }
            }
        }
        if (added && firstSink) {
            GetQueue().RegisterSerializer(this);
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
                lastSink = _writers->empty();
            }
        }
        if (removed && lastSink) {
            GetQueue().UnregisterSerializer(this);
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
        }
    }
    if (removed) {
        GetQueue().UnregisterSerializer(this);
    }
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

void MediaFrameSerializer::WriteToSinks(uint32_t ssrc, uint32_t rtpTimestamp,
                                        bool keyFrame, bool hasMarker,
                                        const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& pdh,
                                        const std::shared_ptr<Buffer>& payload) const
{
    LOCK_READ_PROTECTED_OBJ(_writers);
    for (auto it = _writers->begin(); it != _writers->end(); ++it) {
        if (!it->second->Write(ssrc, rtpTimestamp, keyFrame, hasMarker, pdh, payload)) {
            MS_ERROR("unable to write media frame [%s]", GetMimeText().c_str());
        }
    }
}

MediaFrameSerializer::Queue& MediaFrameSerializer::GetQueue()
{
    static Queue queue;
    return queue;
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

bool MediaFrameSerializer::SinkWriter::Write(uint32_t ssrc, uint32_t rtpTimestamp,
                                             bool keyFrame, bool hasMarker,
                                             const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& pdh,
                                             const std::shared_ptr<Buffer>& payload)
{
    if (auto frame = CreateFrame(ssrc, rtpTimestamp, keyFrame, hasMarker, pdh, payload)) {
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

std::optional<MediaFrame> MediaFrameSerializer::SinkWriter::CreateFrame(uint32_t ssrc, uint32_t rtpTimestamp,
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
                    _impl->SetConfig(_depacketizer->GetAudioConfig(ssrc));
                    break;
                case RtpCodecMimeType::Type::VIDEO:
                    _impl->SetConfig(_depacketizer->GetVideoConfig(ssrc));
                    break;
            }
        }
        return frame;
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

MediaFrameSerializer::Queue::Queue()
    : ThreadExecution("MediaFrameSerializerQueue")
{
}

MediaFrameSerializer::Queue::~Queue()
{
    DropPendingPackets();
    StopExecution();
    LOCK_WRITE_PROTECTED_OBJ(_serializers);
    _serializers->clear();
}

void MediaFrameSerializer::Queue::RegisterSerializer(MediaFrameSerializer* serializer)
{
    if (serializer) {
        bool firstSerializer = false;
        const auto id = serializer->GetId();
        {
            LOCK_WRITE_PROTECTED_OBJ(_serializers);
            if (!_serializers->count(id)) {
                _serializers->insert(std::make_pair(id, serializer));
                firstSerializer = 1U == _serializers->size();
            }
        }
        if (firstSerializer) {
            StartExecution();
        }
    }
}

void MediaFrameSerializer::Queue::UnregisterSerializer(const MediaFrameSerializer* serializer)
{
    if (serializer) {
        bool lastSerializer = false;
        {
            LOCK_WRITE_PROTECTED_OBJ(_serializers);
            if (_serializers->erase(serializer->GetId())) {
                lastSerializer = _serializers->empty();
            }
        }
        if (lastSerializer) {
            StopExecution();
        }
    }
}

void MediaFrameSerializer::Queue::Write(const ObjectId* serializer,
                                        const RtpPacket* packet,
                                        const std::shared_ptr<BufferAllocator>& allocator)
{
    if (serializer && packet) {
        auto packetInfo = PacketInfo::FromRtpPacket(serializer, packet, allocator);
        {
            const std::lock_guard guard(_packetsMutex);
            _packets.push(std::move(packetInfo));
        }
        _packetsCondition.notify_one();
    }
}

void MediaFrameSerializer::Queue::DoExecuteInThread()
{
    while (!IsCancelled()) {
        std::unique_lock lock(_packetsMutex);
        _packetsCondition.wait(lock);
        while (!_packets.empty()) {
            WritePacket(_packets.front());
            _packets.pop();
        }
    }
}

void MediaFrameSerializer::Queue::DoStopThread()
{
    ThreadExecution::DoStopThread();
    _packetsCondition.notify_one();
}

void MediaFrameSerializer::Queue::DropPendingPackets()
{
    const std::lock_guard guard(_packetsMutex);
    while (!_packets.empty()) {
        _packets.pop();
    }
}

void MediaFrameSerializer::Queue::WritePacket(const PacketInfo& packetInfo) const
{
    LOCK_READ_PROTECTED_OBJ(_serializers);
    const auto it = _serializers->find(packetInfo._serializerId);
    if (it != _serializers->end()) {
        it->second->WriteToSinks(packetInfo._ssrc, packetInfo._rtpTimestamp,
                                 packetInfo._keyFrame, packetInfo._hasMarker,
                                 packetInfo._pdh, packetInfo._payload);
    }
}

} // namespace RTC

namespace {

PacketInfo::PacketInfo(uint64_t serializerId, uint32_t ssrc, uint32_t rtpTimestamp,
                       bool keyFrame, bool hasMarker,
                       std::shared_ptr<const RTC::Codecs::PayloadDescriptorHandler> pdh,
                       std::shared_ptr<RTC::Buffer> payload)
    : _serializerId(serializerId)
    , _ssrc(ssrc)
    , _rtpTimestamp(rtpTimestamp)
    , _keyFrame(keyFrame)
    , _hasMarker(hasMarker)
    , _pdh(std::move(pdh))
    , _payload(std::move(payload))
{
}

PacketInfo PacketInfo::FromRtpPacket(const RTC::ObjectId* serializer,
                                     const RTC::RtpPacket* packet,
                                     const std::shared_ptr<RTC::BufferAllocator>& allocator)
{
    auto payload = RTC::AllocateBuffer(packet->GetPayloadLength(), packet->GetPayload(), allocator);
    return PacketInfo(serializer->GetId(),
                      packet->GetSsrc(),
                      packet->GetTimestamp(),
                      packet->IsKeyFrame(),
                      packet->HasMarker(),
                      packet->GetPayloadDescriptorHandler(),
                      std::move(payload));
}

}
