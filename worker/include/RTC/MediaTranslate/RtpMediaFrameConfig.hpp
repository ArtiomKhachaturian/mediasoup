#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace RTC
{


class MemoryBuffer;

class RtpMediaFrameConfig
{
public:
	virtual ~RtpMediaFrameConfig();
    virtual std::string ToString() const = 0;
	std::shared_ptr<const MemoryBuffer> GetCodecSpecificData() const;
	void SetCodecSpecificData(const std::shared_ptr<const MemoryBuffer>& codecSpecificData);
private:
	std::shared_ptr<const MemoryBuffer> _codecSpecificData;
};

} // namespace RTC
