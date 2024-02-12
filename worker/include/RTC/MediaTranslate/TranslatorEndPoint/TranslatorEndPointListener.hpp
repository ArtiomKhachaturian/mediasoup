#pragma once
#include <memory>
#include <set>

namespace RTC
{

class MemoryBuffer;
class TranslatorEndPoint;

class TranslatorEndPointListener
{
public:
    // mediaSeqNum is monotonic zero-based counter of received medias for this end-point
    virtual void OnTranslatedMediaReceived(const TranslatorEndPoint* endPoint, uint64_t mediaSeqNum,
                                           const std::shared_ptr<MemoryBuffer>& media) = 0;
protected:
    virtual ~TranslatorEndPointListener() = default;
};

}
