#define MS_CLASS "RTC::FileEndPoint"
#include "RTC/MediaTranslate/TranslatorEndPoint/FileEndPoint.hpp"
#include "RTC/MediaTranslate/Websocket/WebsocketState.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimer.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerCallback.hpp"
#include "RTC/MediaTranslate/Buffers/Buffer.hpp"
#include "RTC/MediaTranslate/FileReader.hpp"
#include "Logger.hpp"

namespace {

enum class StartMode {
    IfStopped,
    Force
};

}

namespace RTC
{

class FileEndPoint::TimerCallback : public MediaTimerCallback
{
public:
    TimerCallback(FileEndPoint* owner);
    bool IsConnected() const { return WebsocketState::Connected == GetState(); }
    void SetHasWrittenInputMedia(bool has);
    bool HasWrittenInputMedia() const { return _hasWrittenInputMedia.load(); }
    bool SetConnectingState();
    bool SetDisconnectedState();
    // impl. of MediaTimerCallback
    void OnEvent(uint64_t timerId) final;
private:
    WebsocketState GetState() const { return _state.load(); }
    bool SetConnectedState();
    bool SetStrongState(WebsocketState actual, WebsocketState expected);
    std::shared_ptr<Buffer> ReadMediaFromFile() const;
    void NotifyAboutReceivedMedia(const std::shared_ptr<Buffer>& media);
    void NotifyThatConnected(bool connected = true);
    void StartTimer(uint32_t interval, StartMode mode);
    void StartTimer(StartMode mode);
    void StopTimer();
private:
    FileEndPoint* const _owner;
    std::atomic<WebsocketState> _state = WebsocketState::Disconnected;
    std::atomic_bool _hasWrittenInputMedia = false;
};

FileEndPoint::FileEndPoint(std::string fileName,
                           std::string ownerId,
                           uint32_t intervalBetweenTranslationsMs,
                           uint32_t connectionDelaylMs,
                           const std::optional<uint32_t>& disconnectAfterMs)
    : TranslatorEndPoint(std::move(ownerId), std::move(fileName))
    , _fileIsValid(FileReader::IsValidForRead(GetName()))
    , _callback(_fileIsValid ? std::make_shared<TimerCallback>(this) : nullptr)
    , _timer(_fileIsValid ? std::make_shared<MediaTimer>(GetName()) : nullptr)
    , _timerId(_timer ? _timer->Register(_callback) : 0UL)
    , _intervalBetweenTranslationsMs(intervalBetweenTranslationsMs)
    , _connectionDelaylMs(connectionDelaylMs)
{
    _instances.fetch_add(1U);
    if (_timer && _timerId && disconnectAfterMs.has_value()) {
        // gap +10ms
        const auto timeout = std::max<uint32_t>(connectionDelaylMs, disconnectAfterMs.value()) + 10U;
        _disconnectedTimerId = _timer->Singleshot(timeout, [this](uint64_t) { Disconnect(); });
    }
}

FileEndPoint::~FileEndPoint()
{
    FileEndPoint::Disconnect();
    if (_timer) {
        if (_timerId) {
            _timer->Unregister(_timerId);
        }
        if (_disconnectedTimerId) {
            _timer->Unregister(_disconnectedTimerId);
        }
    }
    _instances.fetch_sub(1U);
}

bool FileEndPoint::IsConnected() const
{
    return _callback && _callback->IsConnected();
}

void FileEndPoint::Connect()
{
    if (_callback && _timer && _callback->SetConnectingState()) {
        // for emulation of connection establishing
        _timer->SetTimeout(_timerId, _connectionDelaylMs);
        _timer->Start(_timerId, true);
    }
}

void FileEndPoint::Disconnect()
{
    if (_callback && _timer && _callback->SetDisconnectedState()) {
        _timer->Stop(_timerId);
        NotifyThatConnectionEstablished(false);
    }
}

bool FileEndPoint::SendBinary(const std::shared_ptr<Buffer>& buffer) const
{
    if (_callback && buffer && IsConnected() && !buffer->IsEmpty()) {
        _callback->SetHasWrittenInputMedia(true);
        return true;
    }
    return false;
}

bool FileEndPoint::SendText(const std::string& text) const
{
    return IsConnected() && !text.empty();
}

FileEndPoint::TimerCallback::TimerCallback(FileEndPoint* owner)
    : _owner(owner)
{
}

bool FileEndPoint::TimerCallback::SetConnectingState()
{
    return SetStrongState(WebsocketState::Connecting, WebsocketState::Disconnected);
}

bool FileEndPoint::TimerCallback::SetDisconnectedState()
{
    return WebsocketState::Disconnected != _state.exchange(WebsocketState::Disconnected);
}

void FileEndPoint::TimerCallback::OnEvent(uint64_t /*timerId*/)
{
    if (SetConnectedState()) {
        NotifyThatConnected();
    }
    if (WebsocketState::Connected == GetState()) {
        if (HasWrittenInputMedia()) {
            if (const auto media = ReadMediaFromFile()) {
                NotifyAboutReceivedMedia(media);
                StartTimer(StartMode::IfStopped);
            }
            else {
                StopTimer();
                MS_ERROR_STD("unable to read of %s media file", _owner->GetName().c_str());
            }
        }
    }
}

bool FileEndPoint::TimerCallback::SetConnectedState()
{
    return SetStrongState(WebsocketState::Connected, WebsocketState::Connecting);
}

bool FileEndPoint::TimerCallback::SetStrongState(WebsocketState actual, WebsocketState expected)
{
    return _state.compare_exchange_strong(expected, actual);
}

std::shared_ptr<Buffer> FileEndPoint::TimerCallback::ReadMediaFromFile() const
{
    return FileReader::ReadAllAsBuffer(_owner->GetName());
}

void FileEndPoint::TimerCallback::NotifyAboutReceivedMedia(const std::shared_ptr<Buffer>& media)
{
    if (media) {
        _owner->NotifyThatTranslationReceived(media);
    }
}

void FileEndPoint::TimerCallback::NotifyThatConnected(bool connected)
{
    _owner->NotifyThatConnectionEstablished(connected);
}

void FileEndPoint::TimerCallback::StartTimer(uint32_t interval, StartMode mode)
{
    bool start = true;
    if (StartMode::IfStopped == mode) {
        start = !_owner->_timer->IsStarted(_owner->_timerId);
    }
    _owner->_timer->SetTimeout(_owner->_timerId, interval);
    if (start) {
        _owner->_timer->Start(_owner->_timerId, false);
    }
}

void FileEndPoint::TimerCallback::StartTimer(StartMode mode)
{
    StartTimer(_owner->_intervalBetweenTranslationsMs, mode);
}

void FileEndPoint::TimerCallback::SetHasWrittenInputMedia(bool has)
{
    if (has != _hasWrittenInputMedia.exchange(has)) {
        if (has) {
            StartTimer(0U, StartMode::Force);
        }
        else {
            StopTimer();
        }
    }
}

void FileEndPoint::TimerCallback::StopTimer()
{
    _owner->_timer->Stop(_owner->_timerId);
}

} // namespace RTC
