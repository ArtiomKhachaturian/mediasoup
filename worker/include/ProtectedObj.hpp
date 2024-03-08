#pragma once
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <optional>

// Do magic! Creates a unique name using the line number
#define PROTECTED_OBJ_NAME(prefix) PROTECTED_OBJ_JOIN(prefix, __LINE__)
#define PROTECTED_OBJ_JOIN(symbol1, symbol2) DO_PROTECTED_OBJ_JOIN(symbol1, symbol2)
#define DO_PROTECTED_OBJ_JOIN(symbol1, symbol2) symbol1##symbol2
#define LOCK_READ_PROTECTED_OBJ(object) const auto PROTECTED_OBJ_NAME(rl)(object.GetReadGuard())
#define LOCK_WRITE_PROTECTED_OBJ(object) const auto PROTECTED_OBJ_NAME(wl)(object.GetWriteGuard())

namespace RTC
{

template<class TMutexType> struct MutextTraits {
    using MutexWriteGuard = std::lock_guard<TMutexType>;
    using MutexReadGuard = MutexWriteGuard;
};

template<> struct MutextTraits<std::shared_mutex> {
    using MutexWriteGuard = std::lock_guard<std::shared_mutex>;
    using MutexReadGuard = std::shared_lock<std::shared_mutex>;
};

template <typename T, class TMutexType = std::mutex>
class ProtectedObj
{
public:
    using ObjectMutexType = TMutexType;
    using GuardTraits = MutextTraits<ObjectMutexType>;
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
    auto GetWriteGuard() const { return typename GuardTraits::MutexWriteGuard(_mtx); }
    auto GetReadGuard() const { return typename GuardTraits::MutexReadGuard(_mtx); }
    operator const T&() const { return ConstRef(); }
    operator T&() { return Ref(); }
    const T& ConstRef() const { return _obj; }
    T& Ref() { return _obj; }
    T Take() { return std::move(_obj); }
    T Exchange(T obj) {
        std::swap(_obj, obj);
        return obj; // old
    }
    T* operator -> () noexcept { return &_obj; }
    const T* operator -> () const noexcept { return &_obj; }

private:
    mutable ObjectMutexType _mtx;
    T _obj;
};

// smart pointers typedef

template <typename T, class TMutexType = std::recursive_mutex>
using ProtectedSharedPtr = ProtectedObj<std::shared_ptr<T>, TMutexType>;

template <typename T, class TMutexType = std::recursive_mutex>
using ProtectedWeakPtr = ProtectedObj<std::weak_ptr<T>, TMutexType>;

template <typename T, class TMutexType = std::recursive_mutex>
using ProtectedUniquePtr = ProtectedObj<std::unique_ptr<T>, TMutexType>;

template <typename T, class TMutexType = std::recursive_mutex>
using ProtectedOptional = ProtectedObj<std::optional<T>, TMutexType>;

// impl. of ProtectedObj

template <typename T, class TMutexType>
ProtectedObj<T, TMutexType>::ProtectedObj(T val)
    : _obj(std::move(val))
{
}

template <typename T, class TMutexType>
ProtectedObj<T, TMutexType>::ProtectedObj(ProtectedObj&& tmp) noexcept
    : _obj(std::move(tmp._obj))
{
}

template <typename T, class TMutexType>
template <class... Types>
ProtectedObj<T, TMutexType>::ProtectedObj(Types&&... args)
    : _obj(std::forward<Types>(args)...)
{
}

template <typename T, class TMutexType>
ProtectedObj<T, TMutexType>& ProtectedObj<T, TMutexType>::operator=(ProtectedObj&& tmp)
{
    _obj = std::move(tmp._obj);
    return *this;
}

template <typename T, class TMutexType>
template <typename U>
ProtectedObj<T, TMutexType>& ProtectedObj<T, TMutexType>::operator=(U src)
{
    _obj = std::move(src);
    return *this;
}

} // RTC
