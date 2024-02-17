#pragma once

#include <memory>

namespace RTC
{

class MediaTimer;
class MemoryBuffer;

class RtpPacketsPlayerStream
{
public:
	virtual ~RtpPacketsPlayerStream() = default;
	virtual void Play(uint64_t mediaSourceId, 
					  const std::shared_ptr<MemoryBuffer>& media,
              		  const std::shared_ptr<MediaTimer>& timer) = 0;
    virtual bool IsPlaying() const = 0;
};

} // namespace RTC
