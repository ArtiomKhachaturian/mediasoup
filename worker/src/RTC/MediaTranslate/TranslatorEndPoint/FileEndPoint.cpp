#define MS_CLASS "RTC::FileEndPoint"
#include "RTC/MediaTranslate/TranslatorEndPoint/FileEndPoint.hpp"
#include "RTC/MediaTranslate/TranslatorEndPoint/WebsocketState.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimer.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerCallback.hpp"
#include "RTC/MediaTranslate/FileReader.hpp"
#include "RTC/MediaTranslate/MemoryBuffer.hpp"
#include "Logger.hpp"

namespace RTC
{

class FileEndPoint::TimerCallback : public MediaTimerCallback
{
public:
    TimerCallback(FileEndPoint* owner);
    bool IsConnected() const { return WebsocketState::Connected == GetState(); }
    void SetHasWrittenInputMedia(bool has) { _hasWrittenInputMedia = has; }
    bool HasWrittenInputMedia() const { return _hasWrittenInputMedia.load(); }
    bool SetConnectingState();
    bool SetDisconnectedState();
    // impl. of MediaTimerCallback
    void OnEvent() final;
private:
    WebsocketState GetState() const { return _state.load(); }
    bool SetConnectedState();
    bool SetStrongState(WebsocketState actual, WebsocketState expected);
    std::shared_ptr<MemoryBuffer> ReadMediaFromFile() const;
    void NotifyAboutReceivedMedia(const std::shared_ptr<MemoryBuffer>& media);
    void NotifyThatConnected(bool connected = true);
    void AdjustIntervalTimeout(bool restart = false);
private:
    FileEndPoint* const _owner;
    std::atomic<WebsocketState> _state = WebsocketState::Disconnected;
    std::atomic_bool _hasWrittenInputMedia = false;
};

FileEndPoint::FileEndPoint(std::string fileName)
    : _fileIsValid(FileReader::IsValidForRead(fileName))
    , _fileName(std::move(fileName))
    , _callback(_fileIsValid ? std::make_shared<TimerCallback>(this) : nullptr)
    , _timer(_fileIsValid ? std::make_unique<MediaTimer>(fileName) : nullptr)
    , _timerId(_timer ? _timer->RegisterTimer(_callback) : 0UL)
{
    _instances.fetch_add(1U);
    SetName(_fileName);
}

FileEndPoint::~FileEndPoint()
{
    FileEndPoint::Disconnect();
    if (_timer && _timerId) {
        _timer->UnregisterTimer(_timerId);
    }
    _instances.fetch_sub(1U);
}

uint32_t FileEndPoint::GetIntervalBetweenTranslationsMs() const
{
    return _intervalBetweenTranslationsMs.load();
}

void FileEndPoint::SetIntervalBetweenTranslationsMs(uint32_t intervalMs)
{
    _intervalBetweenTranslationsMs = intervalMs;
}

uint32_t FileEndPoint::GetConnectionDelay() const
{
    return _connectionDelaylMs.load();
}

void FileEndPoint::SetConnectionDelay(uint32_t delaylMs)
{
    _connectionDelaylMs = delaylMs;
}

bool FileEndPoint::IsConnected() const
{
    return _callback && _callback->IsConnected();
}

void FileEndPoint::Connect()
{
    if (_callback && _timer && _callback->SetConnectingState()) {
        // for emulation of connection establishing
        _timer->SetTimeout(_timerId, GetConnectionDelay());
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

bool FileEndPoint::SendBinary(const MemoryBuffer& buffer) const
{
    if (_callback && IsConnected() && !buffer.IsEmpty()) {
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

void FileEndPoint::TimerCallback::OnEvent()
{
    if (SetConnectedState()) {
        NotifyThatConnected();
        AdjustIntervalTimeout(true);
    }
    else if (WebsocketState::Connected == GetState()) {
        if (HasWrittenInputMedia()) {
            if (const auto media = ReadMediaFromFile()) {
                NotifyAboutReceivedMedia(media);
                AdjustIntervalTimeout();
            }
            else {
                // TODO: log warning
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

std::shared_ptr<MemoryBuffer> FileEndPoint::TimerCallback::ReadMediaFromFile() const
{
    return FileReader::ReadAllAsBuffer(_owner->GetFileName());
}

void FileEndPoint::TimerCallback::NotifyAboutReceivedMedia(const std::shared_ptr<MemoryBuffer>& media)
{
    if (media) {
        _owner->NotifyThatTranslatedMediaReceived(media);
    }
}

void FileEndPoint::TimerCallback::NotifyThatConnected(bool connected)
{
    _owner->NotifyThatConnectionEstablished(connected);
}

void FileEndPoint::TimerCallback::AdjustIntervalTimeout(bool restart)
{
    _owner->_timer->SetTimeout(_owner->_timerId, _owner->GetIntervalBetweenTranslationsMs());
    if (restart) {
        _owner->_timer->Start(_owner->_timerId, false);
    }
}

} // namespace RTC
