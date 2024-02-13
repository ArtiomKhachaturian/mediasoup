#pragma once
#include "RTC/RtpDictionaries.hpp"
#include <memory>

namespace RTC
{

class MemoryBuffer;
class MediaTimer;
class MediaFrameDeserializer;
class RtpPacketsPlayerCallback;

class RtpPacketsPlayerMediaFragment
{
    class TimerCallback;
public:
    RtpPacketsPlayerMediaFragment(const std::shared_ptr<MediaTimer>& timer,
                                  const std::weak_ptr<RtpPacketsPlayerCallback>& playerCallbackRef,
                                  std::unique_ptr<MediaFrameDeserializer> deserializer,
                                  uint32_t ssrc, uint8_t payloadType, uint64_t mediaId,
                                  const void* userData = nullptr);
    ~RtpPacketsPlayerMediaFragment();
    bool Parse(const RtpCodecMimeType& mime, uint32_t clockRate,
               const std::shared_ptr<MemoryBuffer>& buffer);
    void PlayFrames();
    bool IsPlaying() const;
    uint64_t GetMediaId() const;
private:
    const std::shared_ptr<TimerCallback> _timerCallback;
    const std::shared_ptr<MediaTimer> _timer;
};

} // namespace RTC
