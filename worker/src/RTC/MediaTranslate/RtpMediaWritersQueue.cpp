#include "RTC/MediaTranslate/RtpMediaWritersQueue.hpp"
#include "RTC/MediaTranslate/RtpMediaWriter.hpp"
#include "RTC/Buffers/BufferAllocator.hpp"
#include "RTC/RtpPacket.hpp"
#include <array>

namespace RTC
{

struct RtpMediaWritersQueue::Packet
{
    Packet(uint64_t writerId, uint32_t ssrc, uint32_t rtpTimestamp,
           bool keyFrame, bool hasMarker,
           std::shared_ptr<const Codecs::PayloadDescriptorHandler> pdh,
           std::shared_ptr<Buffer> payload);
    const uint64_t _writerId;
    const uint32_t _ssrc;
    const uint32_t _rtpTimestamp;
    const bool _keyFrame;
    const bool _hasMarker;
    const std::shared_ptr<const Codecs::PayloadDescriptorHandler> _pdh;
    const std::shared_ptr<Buffer> _payload;
};

RtpMediaWritersQueue::RtpMediaWritersQueue()
    : ThreadExecution("RtpMediaWritersQueue", ThreadPriority::Highest)
    , _packets(std::make_unique<PacketsQueue>())
{
}

RtpMediaWritersQueue::~RtpMediaWritersQueue()
{
    DropPendingPackets();
    StopExecution();
    LOCK_WRITE_PROTECTED_OBJ(_writers);
    _writers->clear();
}

void RtpMediaWritersQueue::RegisterWriter(RtpMediaWriter* writer)
{
    if (writer) {
        bool first = false;
        const auto id = writer->GetId();
        {
            LOCK_WRITE_PROTECTED_OBJ(_writers);
            if (!_writers->count(id)) {
                _writers->insert(std::make_pair(id, writer));
                first = 1U == _writers->size();
            }
        }
        if (first) {
            StartExecution();
        }
    }
}

void RtpMediaWritersQueue::UnregisterWriter(uint64_t writerId)
{
    bool last = false;
    {
        LOCK_WRITE_PROTECTED_OBJ(_writers);
        if (_writers->erase(writerId)) {
            last = _writers->empty();
        }
    }
    if (last) {
        StopExecution();
    }
}

void RtpMediaWritersQueue::UnregisterWriter(const ObjectId* writer)
{
    if (writer) {
        UnregisterWriter(writer->GetId());
    }
}

void RtpMediaWritersQueue::Write(uint64_t writerId, const RtpPacket* packet,
                                 const std::shared_ptr<BufferAllocator>& allocator)
{
    if (packet) {
        auto payload = RTC::AllocateBuffer(packet->GetPayloadLength(),
                                           packet->GetPayload(),
                                           allocator);
        {
            const std::lock_guard guard(_packetsMutex);
            _packets->emplace(writerId, packet->GetSsrc(), packet->GetTimestamp(),
                              packet->IsKeyFrame(), packet->HasMarker(),
                              packet->GetPayloadDescriptorHandler(),
                              std::move(payload));
        }
        _packetsCondition.notify_one();
    }
}

void RtpMediaWritersQueue::Write(const ObjectId* writer, const RtpPacket* packet,
                                 const std::shared_ptr<BufferAllocator>& allocator)
{
    if (writer && packet) {
        Write(writer->GetId(), packet);
    }
}

void RtpMediaWritersQueue::DoExecuteInThread()
{
    while (!IsCancelled()) {
        std::unique_lock lock(_packetsMutex);
        _packetsCondition.wait(lock);
        while (!_packets->empty()) {
            WritePacket(_packets->front());
            _packets->pop();
        }
    }
}

void RtpMediaWritersQueue::DoStopThread()
{
    ThreadExecution::DoStopThread();
    _packetsCondition.notify_one();
}

void RtpMediaWritersQueue::DropPendingPackets()
{
    const std::lock_guard guard(_packetsMutex);
    while (!_packets->empty()) {
        _packets->pop();
    }
}

void RtpMediaWritersQueue::WritePacket(const Packet& packet) const
{
    LOCK_READ_PROTECTED_OBJ(_writers);
    const auto it = _writers->find(packet._writerId);
    if (it != _writers->end()) {
        it->second->WriteRtpMedia(packet._ssrc, packet._rtpTimestamp,
                                  packet._keyFrame, packet._hasMarker,
                                  packet._pdh, packet._payload);
    }
}

RtpMediaWritersQueue::Packet::Packet(uint64_t writerId, uint32_t ssrc,
                                     uint32_t rtpTimestamp,
                                     bool keyFrame, bool hasMarker,
                                     std::shared_ptr<const Codecs::PayloadDescriptorHandler> pdh,
                                     std::shared_ptr<Buffer> payload)
    : _writerId(writerId)
    , _ssrc(ssrc)
    , _rtpTimestamp(rtpTimestamp)
    , _keyFrame(keyFrame)
    , _hasMarker(hasMarker)
    , _pdh(std::move(pdh))
    , _payload(std::move(payload))
{
}

} // namespace RTC
