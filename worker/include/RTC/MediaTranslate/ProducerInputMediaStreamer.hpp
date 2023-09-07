#pragma once

#include <cstdint>

namespace RTC
{

class OutputDevice;

class ProducerInputMediaStreamer
{
public:
    virtual uint32_t GetSsrc() const = 0;
	virtual bool AddOutputDevice(OutputDevice* outputDevice) = 0;
    virtual bool RemoveOutputDevice(OutputDevice* outputDevice) = 0;
};

} // namespace RTC
