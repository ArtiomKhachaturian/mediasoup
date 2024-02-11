#pragma once
#include <memory>
#include <set>

namespace RTC
{

class MemoryBuffer;

class TranslatorEndPointListener
{
public:
    // mediaSeqNum is monotonic zero-based counter of received medias for this end-point
    virtual void OnTranslatedMediaReceived(uint64_t endPointId, uint64_t mediaSeqNum,
                                           const std::set<uint32_t>& ssrcs,
                                           const std::shared_ptr<MemoryBuffer>& media) = 0;
protected:
    virtual ~TranslatorEndPointListener() = default;
};

}
