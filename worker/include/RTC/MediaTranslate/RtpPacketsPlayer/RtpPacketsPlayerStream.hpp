#pragma once

#include <memory>

namespace RTC
{

class Buffer;
class MediaTimer;

class RtpPacketsPlayerStream
{
public:
	virtual ~RtpPacketsPlayerStream() = default;
	virtual void Play(uint64_t mediaSourceId, const std::shared_ptr<Buffer>& media,
                      const std::shared_ptr<MediaTimer> timer) = 0;
    virtual void Stop(uint64_t mediaSourceId, uint64_t mediaId = 0ULL) = 0;
    virtual bool IsPlaying() const = 0;
    virtual void Pause(bool pause) = 0;
};

} // namespace RTC
