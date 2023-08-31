#pragma once
#include <memory>
#include <mutex>
#include <optional>

// Do magic! Creates a unique name using the line number
#define PROTECTED_OBJ_NAME(prefix) PROTECTED_OBJ_JOIN(prefix, __LINE__)
#define PROTECTED_OBJ_JOIN(symbol1, symbol2) DO_PROTECTED_OBJ_JOIN(symbol1, symbol2)
#define DO_PROTECTED_OBJ_JOIN(symbol1, symbol2) symbol1##symbol2
#define LOCK_READ_PROTECTED_OBJ(object) const RTC::MutexReadGuard PROTECTED_OBJ_NAME(rl)(object)
#define LOCK_WRITE_PROTECTED_OBJ(object) const RTC::MutexWriteGuard PROTECTED_OBJ_NAME(wl)(object)

namespace RTC
{

using Mutex = std::recursive_mutex;
using MutexWriteGuard = std::lock_guard<Mutex>;
using MutexReadGuard = MutexWriteGuard;

template <typename T>
class ProtectedObj
{
public:
    ProtectedObj() = default;
    explicit ProtectedObj(T val);
    explicit ProtectedObj(ProtectedObj&& tmp) noexcept;
    template <class... Types>
    ProtectedObj(Types&&... args);
    ProtectedObj(const ProtectedObj&) = delete;
    ProtectedObj& operator=(const ProtectedObj&) = delete;
    ProtectedObj& operator=(ProtectedObj&& tmp);
    template <typename U = T>
    ProtectedObj& operator=(U src);
    operator Mutex&() const { return _mtx; }
    operator const T&() const { return ConstRef(); }
    operator T&() { return Ref(); }
    const T& ConstRef() const { return _obj; }
    T& Ref() { return _obj; }
    T Take() { return std::move(_obj); }
    T* operator -> () noexcept { return &_obj; }
    const T* operator -> () const noexcept { return &_obj; }

private:
    mutable Mutex _mtx;
    T _obj;
};

// smart pointers typedef

template <typename T>
using ProtectedSharedPtr = ProtectedObj<std::shared_ptr<T>>;

template <typename T>
using ProtectedWeakPtr = ProtectedObj<std::weak_ptr<T>>;

template <typename T>
using ProtectedUniquePtr = ProtectedObj<std::unique_ptr<T>>;

template <typename T>
using ProtectedOptional = ProtectedObj<std::optional<T>>;

// impl. of ProtectedObj

template <typename T>
ProtectedObj<T>::ProtectedObj(T val)
    : _obj(std::move(val))
{
}

template <typename T>
ProtectedObj<T>::ProtectedObj(ProtectedObj&& tmp) noexcept
    : _obj(std::move(tmp._obj))
{
}

template <typename T>
template <class... Types>
ProtectedObj<T>::ProtectedObj(Types&&... args)
    : _obj(std::forward<Types>(args)...)
{
}

template <typename T>
ProtectedObj<T>& ProtectedObj<T>::operator=(ProtectedObj&& tmp)
{
    _obj = std::move(tmp._obj);
    return *this;
}

template <typename T>
template <typename U>
ProtectedObj<T>& ProtectedObj<T>::operator=(U src)
{
    _obj = std::move(src);
    return *this;
}

} // namespace darkmatter::rtc
