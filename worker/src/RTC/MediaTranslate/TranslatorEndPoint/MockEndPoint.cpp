#include "RTC/MediaTranslate/TranslatorEndPoint/MockEndPoint.hpp"
#include "RTC/MediaTranslate/TranslatorEndPoint/WebsocketState.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimer.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerCallback.hpp"
#include "RTC/MediaTranslate/FileReader.hpp"
#include "MemoryBuffer.hpp"
#include "ProtectedObj.hpp"

namespace RTC
{

class MockEndPoint::Impl
{
public:
    virtual ~Impl() = default;
    virtual bool IsConnected() const = 0;
    virtual void Connect() = 0;
    virtual bool Disconnect() = 0;
    void SetOwner(MockEndPoint* owner);
    void SetHasWrittenInputMedia(bool has) { _hasWrittenInputMedia = has; }
    bool HasWrittenInputMedia() const { return _hasWrittenInputMedia.load(); }
protected:
    Impl() = default;
    void NotifyThatConnectionEstablished(bool connected);
    void NotifyThatTranslatedMediaReceived(const std::shared_ptr<MemoryBuffer>& media);
private:
    ProtectedObj<MockEndPoint*> _owner = nullptr;
    std::atomic_bool _hasWrittenInputMedia = false;
};

class MockEndPoint::TrivialImpl : public Impl
{
public:
    TrivialImpl() = default;
    // impl. of Impl
    bool IsConnected() const final { return _connected.load(); }
    void Connect() final;
    bool Disconnect() final;
private:
    std::atomic_bool _connected = false;
};

class MockEndPoint::FileImpl : public Impl, public MediaTimerCallback,
                               public std::enable_shared_from_this<FileImpl>
{
public:
    FileImpl(std::shared_ptr<MemoryBuffer> outputMedia, uint32_t intervalBetweenTranslationsMs);
    static std::shared_ptr<Impl> Create(const std::string_view& fileNameUtf8,
                                        uint32_t intervalBetweenTranslationsMs);
    // impl. of Impl
    bool IsConnected() const final { return WebsocketState::Connected == GetState(); }
    void Connect() final;
    bool Disconnect() final;
    // impl. of MediaTimerCallback
    void OnEvent() final;
private:
    WebsocketState GetState() const { return _state.load(); }
private:
    const std::shared_ptr<MemoryBuffer> _outputMedia;
    const uint32_t _intervalBetweenTranslationsMs;
    MediaTimer _timer;
    std::atomic<uint64_t> _timerId = 0ULL;
    std::atomic<WebsocketState> _state = WebsocketState::Disconnected;
};

MockEndPoint::MockEndPoint(uint32_t ssrc)
    : TranslatorEndPoint(ssrc, 0U)
    , _impl(std::make_shared<TrivialImpl>())
{
    _impl->SetOwner(this);
}

MockEndPoint::MockEndPoint(uint32_t ssrc, const std::string_view& fileNameUtf8,
                           uint32_t intervalBetweenTranslationsMs)
    : TranslatorEndPoint(ssrc, 0U)
    , _impl(FileImpl::Create(fileNameUtf8, intervalBetweenTranslationsMs))
{
    _impl->SetOwner(this);
}

MockEndPoint::~MockEndPoint()
{
    MockEndPoint::Disconnect();
    _impl->SetOwner(nullptr);
}

bool MockEndPoint::IsConnected() const
{
    return _impl->IsConnected();
}

void MockEndPoint::Connect()
{
    _impl->Connect();
}

void MockEndPoint::Disconnect()
{
    if (_impl->Disconnect()) {
        _impl->SetHasWrittenInputMedia(false);
    }
}

bool MockEndPoint::SendBinary(const MemoryBuffer& buffer) const
{
    if (IsConnected() && !buffer.IsEmpty()) {
        _impl->SetHasWrittenInputMedia(true);
        return true;
    }
    return false;
}

bool MockEndPoint::SendText(const std::string& text) const
{
    return IsConnected() && !text.empty();
}

void MockEndPoint::Impl::SetOwner(MockEndPoint* owner)
{
    LOCK_WRITE_PROTECTED_OBJ(_owner);
    _owner = owner;
}

void MockEndPoint::Impl::NotifyThatConnectionEstablished(bool connected)
{
    LOCK_READ_PROTECTED_OBJ(_owner);
    if (const auto owner = _owner.ConstRef()) {
        owner->NotifyThatConnectionEstablished(connected);
    }
}

void MockEndPoint::Impl::NotifyThatTranslatedMediaReceived(const std::shared_ptr<MemoryBuffer>& media)
{
    if (media) {
        LOCK_READ_PROTECTED_OBJ(_owner);
        if (const auto owner = _owner.ConstRef()) {
            owner->NotifyThatTranslatedMediaReceived(media);
        }
    }
}

void MockEndPoint::TrivialImpl::Connect()
{
    if (!_connected.exchange(true)) {
        NotifyThatConnectionEstablished(true);
    }
}

bool MockEndPoint::TrivialImpl::Disconnect()
{
    if (_connected.exchange(false)) {
        NotifyThatConnectionEstablished(false);
        return true;
    }
    return false;
}

MockEndPoint::FileImpl::FileImpl(std::shared_ptr<MemoryBuffer> outputMedia,
                                 uint32_t intervalBetweenTranslationsMs)
    : _outputMedia(std::move(outputMedia))
    , _intervalBetweenTranslationsMs(intervalBetweenTranslationsMs)
{
}

std::shared_ptr<MockEndPoint::Impl> MockEndPoint::FileImpl::
    Create(const std::string_view& fileNameUtf8, uint32_t intervalBetweenTranslationsMs)
{
    std::shared_ptr<Impl> impl;
    if (auto media = FileReader::ReadAllAsBuffer(fileNameUtf8)) {
        impl = std::make_shared<FileImpl>(std::move(media), intervalBetweenTranslationsMs);
    }
    else {
        // TODO: log warning
        impl = std::make_shared<TrivialImpl>();
    }
    return impl;
}

void MockEndPoint::FileImpl::OnEvent()
{
    WebsocketState expected = WebsocketState::Connecting;
    if (_state.compare_exchange_strong(expected, WebsocketState::Connected)) {
        NotifyThatConnectionEstablished(true);
        if (const auto timerId = _timerId.load()) {
            _timer.SetTimeout(timerId, _intervalBetweenTranslationsMs);
            _timer.Start(timerId, false);
        }
    }
    else if (WebsocketState::Connected == GetState()) {
        if (HasWrittenInputMedia()) {
            NotifyThatTranslatedMediaReceived(_outputMedia);
        }
    }
    else if (const auto timerId = _timerId.load()) {
        _timer.Stop(timerId);
    }
}

void MockEndPoint::FileImpl::Connect()
{
    WebsocketState expected = WebsocketState::Disconnected;
    if (_state.compare_exchange_strong(expected, WebsocketState::Connecting)) {
        if (const auto timerId = _timer.RegisterTimer(weak_from_this())) {
            // 500ms for emulation of connection establishing
            _timer.SetTimeout(timerId, 500U);
            _timer.Start(timerId, true);
            _timerId = timerId;
        }
    }
}

bool MockEndPoint::FileImpl::Disconnect()
{
    if (WebsocketState::Disconnected != _state.exchange(WebsocketState::Disconnected)) {
        if (const auto timerId = _timerId.exchange(0ULL)) {
            _timer.UnregisterTimer(timerId);
        }
        NotifyThatConnectionEstablished(false);
        return true;
    }
    return false;
}

}