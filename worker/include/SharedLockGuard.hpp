#pragma once
#include <mutex>

template<class TSharedMutex>
class SharedLockGuard
{
public:
    SharedLockGuard(TSharedMutex& mutex);
    SharedLockGuard(TSharedMutex& mutex, std::adopt_lock_t);
    ~SharedLockGuard() { _mutex.unlock_shared(); }
private:
    TSharedMutex& _mutex;
};

template<class TSharedMutex>
SharedLockGuard<TSharedMutex>::SharedLockGuard(TSharedMutex& mutex)
    : _mutex(mutex)
{
    _mutex.lock_shared();
}

template<class TSharedMutex>
SharedLockGuard<TSharedMutex>::SharedLockGuard(TSharedMutex& mutex, std::adopt_lock_t)
    : _mutex(mutex)
{
}
