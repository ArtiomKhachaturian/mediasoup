#define MS_CLASS "RTC::FileEndPoint"
#include "RTC/MediaTranslate/TranslatorEndPoint/FileEndPoint.hpp"
#include "RTC/MediaTranslate/TranslatorEndPoint/WebsocketState.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimer.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerCallback.hpp"
#include "RTC/MediaTranslate/FileReader.hpp"
#include "MemoryBuffer.hpp"
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
private:
    FileEndPoint* const _owner;
    std::atomic<WebsocketState> _state = WebsocketState::Disconnected;
    std::atomic_bool _hasWrittenInputMedia = false;
};

FileEndPoint::FileEndPoint(uint32_t ssrc, std::string fileName)
    : TranslatorEndPoint(ssrc)
    , _fileIsValid(FileReader::IsValidForRead(fileName))
    , _fileName(std::move(fileName))
    , _callback(_fileIsValid ? std::make_shared<TimerCallback>(this) : nullptr)
    , _timer(_fileIsValid ? std::make_unique<MediaTimer>(fileName) : nullptr)
    , _timerId(_timer ? _timer->RegisterTimer(_callback) : 0UL)
{
}

FileEndPoint::~FileEndPoint()
{
    FileEndPoint::Disconnect();
    if (_timer && _timerId) {
        _timer->UnregisterTimer(_timerId);
    }
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
        MS_ERROR_STD("Disconnected from %s", GetFileName().c_str());
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
        MS_ERROR_STD("Connected to %s", _owner->GetFileName().c_str());
        _owner->NotifyThatConnectionEstablished(true);
        _owner->_timer->SetTimeout(_owner->_timerId, _owner->GetIntervalBetweenTranslationsMs());
        _owner->_timer->Start(_owner->_timerId, false);
    }
    else if (WebsocketState::Connected == GetState()) {
        if (HasWrittenInputMedia()) {
            if (const auto media = FileReader::ReadAllAsBuffer(_owner->GetFileName())) {
                _owner->NotifyThatTranslatedMediaReceived(media);
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

} // namespace RTC
