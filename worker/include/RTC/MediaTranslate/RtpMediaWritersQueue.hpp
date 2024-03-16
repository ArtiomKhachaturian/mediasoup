#pragma once
#include "RTC/MediaTranslate/ThreadExecution.hpp"
#include <condition_variable>
#include <memory>
#include <unordered_map>
#include <queue>

namespace RTC
{

class BufferAllocator;
class RtpMediaWriter;
class ObjectId;
class RtpPacket;

class RtpMediaWritersQueue : public ThreadExecution
{
	struct Packet;
	using PacketsQueue = std::queue<Packet>;
	// key is serializer ID
	using WritersMap = std::unordered_map<uint64_t, RtpMediaWriter*>;
public:
    RtpMediaWritersQueue();
    ~RtpMediaWritersQueue();
    void RegisterWriter(RtpMediaWriter* writer);
    void UnregisterWriter(uint64_t writerId);
    void UnregisterWriter(const ObjectId* writer);
    void Write(uint64_t writerId, const RtpPacket* packet,
               const std::shared_ptr<BufferAllocator>& allocator = nullptr);
    void Write(const ObjectId* writer, const RtpPacket* packet,
               const std::shared_ptr<BufferAllocator>& allocator = nullptr);
protected:
    // impl. of ThreadExecution
    void DoExecuteInThread() final;
    void DoStopThread() final;
private:
    void DropPendingPackets();
    void WritePacket(const Packet& packet) const;
private:
    const std::unique_ptr<PacketsQueue> _packets;
    ProtectedObj<WritersMap, std::mutex> _writers;
    std::mutex _packetsMutex;
    std::condition_variable _packetsCondition;
};

} // namespace RTC
