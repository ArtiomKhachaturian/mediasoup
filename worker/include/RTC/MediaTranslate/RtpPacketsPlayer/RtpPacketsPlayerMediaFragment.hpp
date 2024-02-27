#pragma once
#include <memory>
#include <optional>

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

class RtpPacketsPlayerMediaFragment
{
    class TasksQueue;
private:
    RtpPacketsPlayerMediaFragment(std::shared_ptr<TasksQueue> queue);
public:
    static std::unique_ptr<RtpPacketsPlayerMediaFragment> Parse(const std::shared_ptr<Buffer>& buffer,
                                                                const std::shared_ptr<MediaTimer> playerTimer,
                                                                RtpPacketsPlayerCallback* callback,
                                                                const std::weak_ptr<BufferAllocator>& allocator);
    ~RtpPacketsPlayerMediaFragment();
    size_t GetTracksCount() const;
    std::optional<RtpCodecMimeType> GetTrackMimeType(size_t trackIndex) const;
    void Start(size_t trackIndex, uint32_t ssrc, uint32_t clockRate,
               uint64_t mediaId, uint64_t mediaSourceId);
private:
    const std::shared_ptr<TasksQueue> _queue;
};

} // namespace RTC
