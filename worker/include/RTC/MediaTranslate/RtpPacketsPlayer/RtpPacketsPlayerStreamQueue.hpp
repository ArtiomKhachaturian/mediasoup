#pragma once
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerCallback.hpp"
#include "ProtectedObj.hpp"
#include <absl/container/flat_hash_map.h>

namespace RTC
{

class RtpPacketsPlayerMediaFragment;

class RtpPacketsPlayerStreamQueue : public RtpPacketsPlayerCallback
{
  using FragmentsMap = absl::flat_hash_map<uint64_t, std::unique_ptr<RtpPacketsPlayerMediaFragment>>;
public:
    RtpPacketsPlayerStreamQueue(RtpPacketsPlayerCallback* callback);
    ~RtpPacketsPlayerStreamQueue() final;
    void ResetCallback();
    void PushFragment(std::unique_ptr<RtpPacketsPlayerMediaFragment> fragment);
    bool HasFragments() const;
    // impl. of RtpPacketsPlayerCallback
    void OnPlayStarted(uint32_t ssrc, uint64_t mediaId, uint64_t mediaSourceId) final;
    void OnPlay(const Timestamp& timestampOffset, RtpPacket* packet, uint64_t mediaId,
                uint64_t mediaSourceId) final;
    void OnPlayFinished(uint32_t ssrc, uint64_t mediaId, uint64_t mediaSourceId) final;
private:
    void RemoveFinishedFragment(uint64_t mediaId);
    template <class Method, typename... Args>
    bool InvokeCallbackMethod(const Method& method, Args&&... args) const;
private:
    ProtectedObj<RtpPacketsPlayerCallback*> _callback;
    ProtectedObj<FragmentsMap> _fragments;
};

} // namespace RTC
