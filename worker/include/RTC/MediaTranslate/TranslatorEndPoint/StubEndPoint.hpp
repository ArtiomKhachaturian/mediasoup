#pragma once
#include "RTC/MediaTranslate/TranslatorEndPoint/TranslatorEndPoint.hpp"
#include "RTC/MediaTranslate/TranslatorDefines.hpp"
#include <atomic>

namespace RTC
{

class MediaTimer;

class StubEndPoint : public TranslatorEndPoint
{
public:
    StubEndPoint(std::string ownerId = std::string(),
                 const std::shared_ptr<MediaTimer>& timer = nullptr);
    ~StubEndPoint() final;
    // impl. of TranslatorEndPoint
    bool IsConnected() const final { return _connected.load(); }
protected:
    void Connect() final;
    void Disconnect() final;
    bool SendBinary(const std::shared_ptr<Buffer>& buffer) const final;
    bool SendText(const std::string& text) const final;
private:
    void MakeConnect();
private:
    static inline std::atomic<uint64_t> _instances = 0ULL;
#if defined(MOCK_CONNECTION_DELAY_MS) || defined(MOCK_DISCONNECT_AFTER_MS)
    const std::shared_ptr<MediaTimer> _timer;
#endif
    std::atomic_bool _connected = false;
#ifdef MOCK_CONNECTION_DELAY_MS
    uint64_t _connectedTimerId = 0U;
#endif
#if defined(MOCK_DISCONNECT_AFTER_MS) && defined(MOCK_DISCONNECT_STUB_END_POINTS)
    uint64_t _disconnectedTimerId = 0U;
#endif
};

} // namespace RTC
