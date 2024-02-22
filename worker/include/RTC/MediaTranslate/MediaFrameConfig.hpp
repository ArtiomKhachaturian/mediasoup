#pragma once

#include <memory>
#include <string>

namespace RTC
{

class Buffer;
class BufferAllocator;

class MediaFrameConfig
{
public:
	virtual ~MediaFrameConfig();
    virtual std::string ToString() const = 0;
	const std::shared_ptr<const Buffer>& GetCodecSpecificData() const;
	void SetCodecSpecificData(const std::shared_ptr<const Buffer>& data);
    // makes a deep copy of input data
    void SetCodecSpecificData(const uint8_t* data, size_t len,
                              const std::weak_ptr<BufferAllocator>& allocator);
private:
	std::shared_ptr<const Buffer> _codecSpecificData;
};

} // namespace RTC
