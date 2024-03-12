#include "RTC/MediaTranslate/TranslatorEndPoint/StubEndPoint.hpp"
#if defined(MOCK_CONNECTION_DELAY_MS) || defined(MOCK_DISCONNECT_AFTER_MS)
#include "RTC/MediaTranslate/MediaTimer/MediaTimer.hpp"
#endif
#include "RTC/Buffers/Buffer.hpp"
#include <algorithm>

namespace RTC
{

StubEndPoint::StubEndPoint(std::string ownerId,
                           const std::shared_ptr<BufferAllocator>& allocator,
                           const std::shared_ptr<MediaTimer>& timer)
    : TranslatorEndPoint(std::move(ownerId),
                         "StubEndPoint #" + std::to_string(_instances.fetch_add(1U) + 1U),
                         allocator)
#if defined(MOCK_CONNECTION_DELAY_MS) || defined(MOCK_DISCONNECT_AFTER_MS)
    , _timer(timer)
#endif
{
    _instances.fetch_add(1U);
#if defined(MOCK_DISCONNECT_AFTER_MS) && defined(MOCK_DISCONNECT_STUB_END_POINTS)
    if (_timer) {
#ifdef MOCK_CONNECTION_DELAY_MS
        const uint32_t connectionDelaylMs = MOCK_CONNECTION_DELAY_MS;
#else
        const uint32_t connectionDelaylMs = 0U;
#endif
        const auto timeout = std::max<uint32_t>(connectionDelaylMs, MOCK_DISCONNECT_AFTER_MS) + 10U;
        _disconnectedTimerId = _timer->Singleshot(timeout, std::bind(&StubEndPoint::Disconnect, this));
    }
#endif
}

StubEndPoint::~StubEndPoint()
{
    StubEndPoint::Disconnect();
    _instances.fetch_sub(1U);
}

void StubEndPoint::Connect()
{
#ifdef MOCK_CONNECTION_DELAY_MS
    if (!_connectedTimerId && _timer) {
        _connectedTimerId = _timer->Singleshot(MOCK_CONNECTION_DELAY_MS,
                                               std::bind(&StubEndPoint::MakeConnect, this));
    }
#else
    MakeConnect();
#endif
}

void StubEndPoint::Disconnect()
{
    if (_connected.exchange(false)) {
#ifdef MOCK_CONNECTION_DELAY_MS
        if (_timer) {
            _timer->Unregister(_connectedTimerId);
            _connectedTimerId = 0ULL;
        }
#endif
#if defined(MOCK_DISCONNECT_AFTER_MS) && defined(MOCK_DISCONNECT_STUB_END_POINTS)
        if (_timer) {
            _timer->Unregister(_disconnectedTimerId);
            _disconnectedTimerId = 0ULL;
        }
#endif
        NotifyThatConnectionEstablished(false);
    }
}

bool StubEndPoint::SendBinary(const std::shared_ptr<Buffer>& buffer) const
{
    return buffer && IsConnected() && !buffer->IsEmpty();
}

bool StubEndPoint::SendText(const std::string& text) const
{
    return IsConnected() && !text.empty();
}

void StubEndPoint::MakeConnect()
{
    if (!_connected.exchange(true)) {
        NotifyThatConnectionEstablished(true);
    }
}

} // namespace RTC
