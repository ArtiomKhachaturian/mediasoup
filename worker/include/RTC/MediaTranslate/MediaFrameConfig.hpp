#pragma once

#include <memory>
#include <string>

namespace RTC
{

class MemoryBuffer;

class MediaFrameConfig
{
public:
	virtual ~MediaFrameConfig();
    virtual std::string ToString() const = 0;
	const std::shared_ptr<const MemoryBuffer>& GetCodecSpecificData() const;
	void SetCodecSpecificData(const std::shared_ptr<const MemoryBuffer>& data);
    // makes a deep copy of input data
    void SetCodecSpecificData(const uint8_t* data, size_t len);
private:
	std::shared_ptr<const MemoryBuffer> _codecSpecificData;
};

} // namespace RTC
