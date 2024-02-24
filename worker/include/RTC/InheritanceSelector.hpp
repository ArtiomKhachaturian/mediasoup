#pragma once
#include <utility>

namespace RTC
{

template<class TBase>
class InheritanceSelector : public TBase
{
protected:
    template <class... Args>
    InheritanceSelector(Args&&... args) : TBase(std::forward<Args>(args)...) {}
};


template<>
class InheritanceSelector<void>
{
protected:
    InheritanceSelector<void>() = default;
};

} // namespace RTC