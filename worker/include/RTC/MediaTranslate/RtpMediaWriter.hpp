#pragma once
#include "RTC/ObjectId.hpp"
#include <memory>

namespace RTC
{

namespace Codecs {
	class PayloadDescriptorHandler;
}
class Buffer;

class RtpMediaWriter : public ObjectId
{
public:
	virtual ~RtpMediaWriter() = default;
 	virtual bool WriteRtpMedia(uint32_t ssrc, uint32_t rtpTimestamp,
                               bool keyFrame, bool hasMarker,
                               const std::shared_ptr<const Codecs::PayloadDescriptorHandler>& pdh,
                               const std::shared_ptr<Buffer>& payload) = 0;
};

} // namespace RTC
