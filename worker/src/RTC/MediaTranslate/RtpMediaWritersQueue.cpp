#include "RTC/MediaTranslate/RtpMediaWritersQueue.hpp"
#include "RTC/MediaTranslate/RtpMediaWriter.hpp"
#include "RTC/MediaTranslate/RtpPacketInfo.hpp"
#include "RTC/Buffers/BufferAllocator.hpp"
#include "RTC/RtpPacket.hpp"
#include <array>

namespace RTC
{

struct RtpMediaWritersQueue::Packet
{
    Packet(uint64_t writerId, RtpPacketInfo packetInfo);
    const uint64_t _writerId;
    const RtpPacketInfo _packetInfo;
};

RtpMediaWritersQueue::RtpMediaWritersQueue()
    : ThreadExecution("RtpMediaWritersQueue", ThreadPriority::Highest)
    , _packets(std::make_unique<PacketsQueue>())
{
}

RtpMediaWritersQueue::~RtpMediaWritersQueue()
{
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
    if (packet && !IsCancelled()) {
        auto packeInfo = RtpPacketInfo::FromRtpPacket(packet, allocator);
        {
            const std::lock_guard guard(_packetsMutex);
            _packets->emplace(writerId, std::move(packeInfo));
        }
        _packetsCondition.notify_one();
    }
}

void RtpMediaWritersQueue::Write(const ObjectId* writer, const RtpPacket* packet,
                                 const std::shared_ptr<BufferAllocator>& allocator)
{
    if (writer && packet) {
        Write(writer->GetId(), packet, allocator);
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
    {
        const std::lock_guard guard(_packetsMutex);
        while (!_packets->empty()) {
            _packets->pop();
        }
    }
    _packetsCondition.notify_one();
}

void RtpMediaWritersQueue::WritePacket(const Packet& packet) const
{
    LOCK_READ_PROTECTED_OBJ(_writers);
    const auto it = _writers->find(packet._writerId);
    if (it != _writers->end()) {
        it->second->WriteRtpMedia(packet._packetInfo);
    }
}

RtpMediaWritersQueue::Packet::Packet(uint64_t writerId, RtpPacketInfo packetInfo)
    : _writerId(writerId)
    , _packetInfo(std::move(packetInfo))
{
}

RtpPacketInfo RtpPacketInfo::FromRtpPacket(const RtpPacket* packet,
                                           const std::shared_ptr<BufferAllocator>& allocator)
{
    if (packet) {
        auto payload = RTC::AllocateBuffer(packet->GetPayloadLength(),
                                           packet->GetPayload(),
                                           allocator);
        return RtpPacketInfo(packet->GetTimestamp(), packet->IsKeyFrame(),
                             packet->HasMarker(), packet->GetPayloadDescriptorHandler(),
                             std::move(payload));
    }
    return RtpPacketInfo();
}

} // namespace RTC
