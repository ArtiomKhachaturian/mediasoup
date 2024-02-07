#pragma once

#include "RTC/AtomicCounter.hpp"
#include "ProtectedObj.hpp"
#include <atomic>
#include <vector>
#include <memory>

namespace RTC
{

template<class TListener>
class Listeners
{
    class Impl;
private: // assertions
    template<class T>
    struct IsSharedPointer : std::false_type {};
    template<class T>
    struct IsSharedPointer<std::shared_ptr<T>> : std::true_type {};
    template<class T>
    using IsRawPointer = std::is_pointer<T>;
    static_assert(IsRawPointer<TListener>::value || IsSharedPointer<TListener>::value);
public:
    Listeners();
    Listeners(Listeners&& tmp) = delete;
    Listeners(const Listeners&) = delete;
    ~Listeners();
    bool Add(const TListener& listener);
    bool Remove(const TListener& listener);
    void Clear();
    void BlockInvokes(bool block);
    bool IsEmpty() const;
    size_t GetSize() const;
    template <class Method, typename... Args>
    void InvokeMethod(const Method& method, Args&&... args) const;
    Listeners& operator = (const Listeners&) = delete;
    Listeners& operator = (Listeners&&) = delete;
private:
    AtomicCounter _blocksCounter;
    std::shared_ptr<Impl> _impl;
};


template <typename TListener>
class Listeners<TListener>::Impl
{
    template<class T>
    using ProtectedContainer = ProtectedObj<T, std::recursive_mutex>;
public:
    Impl();
    bool Add(const TListener& listener);
    bool Remove(const TListener& listener);
    void Clear();
    bool IsEmpty() const;
    size_t GetSize() const;
    template <class Method, typename... Args>
    void InvokeMethod(const Method& method, Args&&... args) const;
private:
    ProtectedContainer<std::vector<TListener>> _listeners;
};

template<class TListener>
Listeners<TListener>::Listeners()
    :  _impl(std::make_shared<Impl>())
{
}

template<class TListener>
Listeners<TListener>::~Listeners()
{
    std::atomic_store(&_impl, std::shared_ptr<Impl>());
}

template<class TListener>
bool Listeners<TListener>::Add(const TListener& listener)
{
    if (listener) {
        if (const auto impl = std::atomic_load(&_impl)) {
            return impl->Add(listener);
        }
    }
    return false;
}

template<class TListener>
bool Listeners<TListener>::Remove(const TListener& listener)
{
    if (listener) {
        if (const auto impl = std::atomic_load(&_impl)) {
            return impl->Remove(listener);
        }
    }
    return false;
}

template<class TListener>
void Listeners<TListener>::Clear()
{
    if (const auto impl = std::atomic_load(&_impl)) {
        impl->Clear();
    }
}

template<class TListener>
void Listeners<TListener>::BlockInvokes(bool block)
{
    if (block) {
        _blocksCounter.IncRef();
    }
    else {
        _blocksCounter.DecRef();
    }
}

template<class TListener>
bool Listeners<TListener>::IsEmpty() const
{
    if (const auto impl = std::atomic_load(&_impl)) {
        return impl->IsEmpty();
    }
    return true;
}

template<class TListener>
size_t Listeners<TListener>::GetSize() const
{
    if (const auto impl = std::atomic_load(&_impl)) {
        return impl->GetSize();
    }
    return 0UL;
}

template<class TListener>
template <class Method, typename... Args>
void Listeners<TListener>::InvokeMethod(const Method& method, Args&&... args) const
{
    if (_blocksCounter.HasNoMoreRef()) {
        if (const auto impl = std::atomic_load(&_impl)) {
            impl->InvokeMethod(method, std::forward<Args>(args)...);
        }
    }
}

template<class TListener>
Listeners<TListener>::Impl::Impl()
{
    _listeners->reserve(1UL);
}

template<class TListener>
bool Listeners<TListener>::Impl::Add(const TListener& listener)
{
    LOCK_WRITE_PROTECTED_OBJ(_listeners);
    const auto it = std::find(_listeners->begin(), _listeners->end(), listener);
    if (it == _listeners->end()) {
        _listeners->push_back(listener);
        return true;
    }
    return false;
}

template<class TListener>
bool Listeners<TListener>::Impl::Remove(const TListener& listener)
{
    LOCK_WRITE_PROTECTED_OBJ(_listeners);
    const auto it = std::find(_listeners->begin(), _listeners->end(), listener);
    if (it != _listeners->end()) {
        _listeners->erase(it);
        return true;
    }
    return false;
}

template<class TListener>
void Listeners<TListener>::Impl::Clear()
{
    LOCK_WRITE_PROTECTED_OBJ(_listeners);
    _listeners->clear();
}

template<class TListener>
bool Listeners<TListener>::Impl::IsEmpty() const
{
    LOCK_READ_PROTECTED_OBJ(_listeners);
    return _listeners->empty();
}

template<class TListener>
size_t Listeners<TListener>::Impl::GetSize() const
{
    LOCK_READ_PROTECTED_OBJ(_listeners);
    return _listeners->size();
}

template<class TListener>
template <class Method, typename... Args>
void Listeners<TListener>::Impl::InvokeMethod(const Method& method, Args&&... args) const
{
    LOCK_READ_PROTECTED_OBJ(_listeners);
    if (!_listeners->empty()) {
        size_t i = 0UL;
        do {
            const size_t size = _listeners->size();
            if (i < size) {
                (_listeners->at(i)->*method)(std::forward<Args>(args)...);
                if (_listeners->size() >= size) {
                    ++i;
                }
            }
            else {
                break;
            }
        }
        while(true);
    }
}


} // namespace RTC
