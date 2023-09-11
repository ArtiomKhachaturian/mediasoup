#pragma once

namespace RTC {

template <class TOwner, class... TListeners>
class OwnerAwareListener : public TListeners...
{
public:
    ~OwnerAwareListener() = default;
protected:
    OwnerAwareListener(const TOwner& owner) : _owner(owner) {}
    TOwner const _owner;
};

} // namespace RTC
