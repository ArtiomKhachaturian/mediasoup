#pragma once
#include "RTC/MediaTranslate/TranslatorEndPoint/TranslatorEndPoint.hpp"
#include <atomic>

namespace RTC
{

class StubEndPoint : public TranslatorEndPoint
{
public:
    StubEndPoint(std::string ownerId = std::string());
    ~StubEndPoint() final;
    // impl. of TranslatorEndPoint
    bool IsConnected() const final { return _connected.load(); }
protected:
    void Connect() final;
    void Disconnect() final;
    bool SendBinary(const std::shared_ptr<MemoryBuffer>& buffer) const final;
    bool SendText(const std::string& text) const final;
private:
    static inline std::atomic<uint64_t> _instances = 0ULL;
    std::atomic_bool _connected = false;
};

} // namespace RTC
