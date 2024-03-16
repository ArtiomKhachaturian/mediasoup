#define MS_CLASS "RTC::FileEndPoint"
#include "RTC/MediaTranslate/TranslatorEndPoint/FileEndPoint.hpp"
#include "RTC/MediaTranslate/Websocket/WebsocketState.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimer.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerCallback.hpp"
#include "RTC/MediaTranslate/FileReader.hpp"
#include "RTC/MediaTranslate/TranslatorDefines.hpp"
#include "RTC/Buffers/BufferAllocations.hpp"
#include "Logger.hpp"
#include <algorithm>

namespace {

enum class StartMode {
    IfStopped,
    Force
};

}

namespace RTC
{

class FileEndPoint::TimerCallback : public BufferAllocations<MediaTimerCallback>
{
public:
    TimerCallback(FileEndPoint* owner, const std::shared_ptr<BufferAllocator>& allocator);
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

FileEndPoint::FileEndPoint(std::string ownerId,
                           const std::shared_ptr<BufferAllocator>& allocator,
                           const std::shared_ptr<MediaTimer>& timer)
    : TranslatorEndPoint(std::move(ownerId), MOCK_WEBM_INPUT_FILE, allocator)
    , _fileIsValid(FileReader::IsValidForRead(GetName()))
    , _callback(_fileIsValid ? std::make_shared<TimerCallback>(this, allocator) : nullptr)
    , _timer(_fileIsValid ? (timer ? timer : std::make_shared<MediaTimer>(GetName())) : nullptr)
    , _timerId(_timer ? _timer->Register(_callback) : 0UL)
    , _intervalBetweenTranslationsMs(1000U + (MOCK_WEBM_INPUT_FILE_LEN_SECS * 1000U))
#ifdef MOCK_CONNECTION_DELAY_MS
    , _connectionDelaylMs(MOCK_CONNECTION_DELAY_MS)
#else
    , _connectionDelaylMs(0U)
#endif
{
    _instances.fetch_add(1U);
#ifdef MOCK_DISCONNECT_AFTER_MS
    if (_timer && _timerId) {
        const auto timeout = std::max<uint32_t>(_connectionDelaylMs, MOCK_DISCONNECT_AFTER_MS) + 10U;
        _disconnectedTimerId = _timer->Singleshot(timeout, std::bind(&FileEndPoint::Disconnect, this));
    }
#endif
}

FileEndPoint::~FileEndPoint()
{
    FileEndPoint::Disconnect();
    if (_timer) {
        _timer->Unregister(_timerId);
#ifdef MOCK_DISCONNECT_AFTER_MS
        _timer->Unregister(_disconnectedTimerId);
#endif
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
    if (_callback && buffer && !buffer->IsEmpty()) {
        _callback->SetHasWrittenInputMedia(true);
        return true;
    }
    return false;
}

bool FileEndPoint::SendText(const std::string& text) const
{
    return IsConnected() && !text.empty();
}

FileEndPoint::TimerCallback::TimerCallback(FileEndPoint* owner,
                                           const std::shared_ptr<BufferAllocator>& allocator)
    : BufferAllocations<MediaTimerCallback>(allocator)
    , _owner(owner)
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
        if (HasWrittenInputMedia() && !_owner->IsInputMediaSourcePaused()) {
            if (const auto media = ReadMediaFromFile()) {
                NotifyAboutReceivedMedia(media);
                StartTimer(StartMode::IfStopped);
            }
            else {
                StopTimer();
                MS_ERROR_STD("unable to read of [%s] media file", _owner->GetName().c_str());
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
    FileReader reader(GetAllocator());
    if (reader.Open(_owner->GetName())) {
        return reader.ReadAll();
    }
    return nullptr;
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
