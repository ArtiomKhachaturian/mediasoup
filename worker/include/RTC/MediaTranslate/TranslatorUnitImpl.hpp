#pragma once

#include "RTC/MediaTranslate/TranslatorUnit.hpp"

namespace RTC
{

template<class TObserver, class TInterface>
class TranslatorUnitImpl : public TInterface
{
    static_assert(std::is_base_of<RTC::TranslatorUnit, TInterface>::value);
public:
    // impl. of TInterface
    const std::string& GetId() const final { return _id; }
protected:
    TranslatorUnitImpl(const std::string& id, const std::weak_ptr<TObserver>& observerRef);
protected:
    const std::string _id;
    const std::weak_ptr<TObserver> _observerRef;
};

template<class TObserver, class TInterface>
TranslatorUnitImpl<TObserver, TInterface>::TranslatorUnitImpl(const std::string& id,
                                                              const std::weak_ptr<TObserver>& observerRef)
    : _id(id)
    , _observerRef(observerRef)
{
}

} // namespace RTC