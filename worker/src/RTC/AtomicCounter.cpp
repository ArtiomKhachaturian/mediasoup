#include "RTC/AtomicCounter.hpp"

namespace RTC
{

void AtomicCounter::IncRef()
{
    // Relaxed memory order: The current thread is allowed to act on the
    // resource protected by the reference counter both before and after the
    // atomic op, so this function doesn't prevent memory access reordering.
    _refCounter.fetch_add(1U, std::memory_order_relaxed);
}

bool AtomicCounter::DecRef()
{
    // Use release-acquire barrier to ensure all actions on the protected
    // resource are finished before the resource can be freed.
    // When refCountAfterSubtract > 0, this function require
    // std::memory_order_release part of the barrier.
    // When refCountAfterSubtract == 0, this function require
    // std::memory_order_acquire part of the barrier.
    // In addition std::memory_order_release is used for synchronization with
    // the HasOneRef function to make sure all actions on the protected resource
    // are finished before the resource is assumed to have exclusive access.
    const auto refCountAfterSubtract = _refCounter.fetch_sub(1U, std::memory_order_acq_rel) - 1U;
    return 0U == refCountAfterSubtract;
}

bool AtomicCounter::HasNoMoreRef() const
{
    // To ensure resource protected by the reference counter has exclusive
    // access, all changes to the resource before it was released by other
    // threads must be visible by current thread. That is provided by release
    // (in DecRef) and acquire (in this function) ordering.
    return _refCounter.load(std::memory_order_acquire) == 1ULL;
}

} // namespace RTC
