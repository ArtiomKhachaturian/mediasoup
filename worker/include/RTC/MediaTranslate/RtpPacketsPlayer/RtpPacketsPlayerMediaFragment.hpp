#pragma once
#include "RTC/MediaTranslate/MediaTimer/MediaTimerCallback.hpp"
#include "absl/container/flat_hash_map.h"
#include <atomic>
#include <memory>

namespace RTC
{

class BufferAllocator;
class Buffer;
class MediaFrame;
class MediaTimer;
class MediaFrameDeserializer;
class RtpCodecMimeType;
class RtpPacketsPlayerCallback;
class RtpPacketizer;
class RtpPacket;

class RtpPacketsPlayerMediaFragment : public MediaTimerCallback
{
    // key is track number, value - packetizer instance
    using Packetizers = absl::flat_hash_map<size_t, std::unique_ptr<RtpPacketizer>>;
    class TasksQueue;
private:
    RtpPacketsPlayerMediaFragment(std::shared_ptr<TasksQueue> queue);
public:
    static std::shared_ptr<RtpPacketsPlayerMediaFragment> Parse(const RtpCodecMimeType& mime,
                                                                const std::shared_ptr<Buffer>& buffer,
                                                                const std::shared_ptr<MediaTimer> timer,
                                                                uint32_t ssrc,
                                                                uint32_t clockRate,
                                                                uint8_t payloadType,
                                                                uint64_t mediaId,
                                                                uint64_t mediaSourceId,
                                                                RtpPacketsPlayerCallback* callback,
                                                                const std::weak_ptr<BufferAllocator>& allocator);
    ~RtpPacketsPlayerMediaFragment() final;
    // impl. of MediaTimerCallback
    void OnEvent() final;
private:
    const std::shared_ptr<TasksQueue> _queue;
};

} // namespace RTC
