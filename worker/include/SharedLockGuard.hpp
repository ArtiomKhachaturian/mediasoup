#pragma once

template<class TSharedMutex>
class SharedLockGuard
{
public:
    SharedLockGuard(TSharedMutex& mutex);
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
