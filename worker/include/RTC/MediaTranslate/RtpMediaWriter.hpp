#pragma once
#include "RTC/ObjectId.hpp"

namespace RTC
{

struct RtpPacketInfo;

class RtpMediaWriter : public ObjectId
{
public:
	virtual ~RtpMediaWriter() = default;
 	virtual bool WriteRtpMedia(const RtpPacketInfo& rtpMedia) = 0;
};

} // namespace RTC
