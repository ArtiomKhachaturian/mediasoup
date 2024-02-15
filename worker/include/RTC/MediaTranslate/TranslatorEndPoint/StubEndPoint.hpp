#pragma once
#include "RTC/MediaTranslate/TranslatorEndPoint/TranslatorEndPoint.hpp"
#include <atomic>

namespace RTC
{

class StubEndPoint : public TranslatorEndPoint
{
public:
    StubEndPoint();
    ~StubEndPoint() final;
    // impl. of TranslatorEndPoint
    bool IsConnected() const final { return _connected.load(); }
protected:
    void Connect() final;
    void Disconnect() final;
    bool SendBinary(const MemoryBuffer& buffer) const final;
    bool SendText(const std::string& text) const final;
private:
    static inline std::atomic<uint64_t> _instances = 0ULL;
    const uint64_t _index;
    std::atomic_bool _connected = false;
};

} // namespace RTC
