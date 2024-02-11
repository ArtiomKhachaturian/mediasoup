#pragma once
#include <memory>

namespace RTC
{

class MemoryBuffer;

class TranslatorEndPointListener
{
public:
    // mediaSeqNum is monotonic zero-based counter of received medias for this end-point
    virtual void OnTranslatedMediaReceived(uint64_t endPointId, uint64_t mediaSeqNum,
                                           const std::shared_ptr<MemoryBuffer>& media) = 0;
protected:
    virtual ~TranslatorEndPointListener() = default;
};

}
