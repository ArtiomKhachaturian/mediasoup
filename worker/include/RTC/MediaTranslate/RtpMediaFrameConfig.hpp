#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <optional>

namespace RTC
{

class RtpPacket;
class MemoryBuffer;

class RtpMediaFrameConfig
{
public:
	virtual ~RtpMediaFrameConfig();
    virtual std::string ToString() const = 0;
	std::shared_ptr<const MemoryBuffer> GetCodecSpecificData() const;
	void SetCodecSpecificData(const std::shared_ptr<const MemoryBuffer>& codecSpecificData);
    // null_opt if no descriptor for the packet
    static std::optional<size_t> GetPayloadDescriptorSize(const RtpPacket* packet);
private:
	std::shared_ptr<const MemoryBuffer> _codecSpecificData;
};

} // namespace RTC
