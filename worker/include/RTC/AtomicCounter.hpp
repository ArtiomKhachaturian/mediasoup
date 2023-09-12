#pragma once

#include <atomic>

namespace RTC
{

class AtomicCounter
{
public:
	AtomicCounter() = default;
	void IncRef();
    bool DecRef();
    // Returns true if has no more references
    bool HasNoMoreRef() const;
private:
	std::atomic<uint64_t> _refCounter = 1UL;
};

} // namespace RTC
