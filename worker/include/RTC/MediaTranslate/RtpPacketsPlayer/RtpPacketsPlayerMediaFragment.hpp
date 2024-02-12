#pragma once
#include <memory>

namespace RTC
{

class MemoryBuffer;
class MediaTimer;
class MediaFrameDeserializer;
class MediaFrameDeserializer;
class RtpCodecMimeType;
class RtpPacketsPlayerCallback;
class RtpPacketsInfoProvider;

class RtpPacketsPlayerMediaFragment
{
    class TimerCallback;
public:
    RtpPacketsPlayerMediaFragment(const std::shared_ptr<MediaTimer>& timer,
                                  std::unique_ptr<MediaFrameDeserializer> deserializer,
                                  uint32_t ssrc, uint64_t mediaId, const void* userData);
    ~RtpPacketsPlayerMediaFragment();
    void SetPlayerCallback(const std::shared_ptr<RtpPacketsPlayerCallback>& playerCallback);
    bool Parse(const RtpCodecMimeType& mime,
               const RtpPacketsInfoProvider* packetsInfoProvider,
               const std::shared_ptr<MemoryBuffer>& buffer);
    void PlayFrames();
    bool IsPlaying() const;
private:
    const std::shared_ptr<TimerCallback> _timerCallback;
    const std::shared_ptr<MediaTimer> _timer;
};

} // namespace RTC
