#pragma once
#include <cstdint>

namespace RTC
{

class MediaObject
{
public:
	virtual ~MediaObject() = default;
	uint64_t GetId() const { return reinterpret_cast<uint64_t>(this); }
};

} // namespace RTC