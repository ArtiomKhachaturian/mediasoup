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
class RtpPacketsPlayerMediaFragmentQueue;
class RtpPacketizer;
class RtpPacket;

class RtpPacketsPlayerMediaFragment
{
private:
    RtpPacketsPlayerMediaFragment(std::shared_ptr<RtpPacketsPlayerMediaFragmentQueue> queue);
public:
    static std::unique_ptr<RtpPacketsPlayerMediaFragment> Parse(const std::shared_ptr<Buffer>& buffer,
                                                                const std::shared_ptr<MediaTimer> playerTimer,
                                                                RtpPacketsPlayerCallback* callback,
                                                                const std::shared_ptr<BufferAllocator>& allocator = nullptr);
    ~RtpPacketsPlayerMediaFragment();
    size_t GetTracksCount() const;
    std::optional<RtpCodecMimeType> GetTrackMimeType(size_t trackIndex) const;
    void Start(size_t trackIndex, uint32_t ssrc, uint32_t clockRate,
               uint64_t mediaId, uint64_t mediaSourceId);
private:
    const std::shared_ptr<RtpPacketsPlayerMediaFragmentQueue> _queue;
};

} // namespace RTC
