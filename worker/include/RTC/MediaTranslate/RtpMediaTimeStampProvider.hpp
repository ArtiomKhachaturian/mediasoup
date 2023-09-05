#pragma once

#include <memory>

namespace RTC
{

class RtpMediaFrame;

class RtpMediaTimeStampProvider
{
public:
	virtual ~RtpMediaTimeStampProvider() = default;
    //  Timestamp of the frame in nanoseconds from 0.
	virtual uint64_t GetTimeStampNano(const std::shared_ptr<const RtpMediaFrame>& frame) = 0;
};

} // namespace RTC
