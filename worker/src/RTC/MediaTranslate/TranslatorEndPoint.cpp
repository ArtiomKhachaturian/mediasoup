#define MS_CLASS "RTC::TranslatorEndPoint"
#include "RTC/MediaTranslate/TranslatorEndPoint.hpp"
#include "RTC/MediaTranslate/Websocket.hpp"
#include "RTC/MediaTranslate/OutputDevice.hpp"
#include "RTC/MediaTranslate/WebsocketListener.hpp"
#include "RTC/MediaTranslate/ProducerInputMediaStreamer.hpp"
#include "RTC/MediaTranslate/MediaLanguage.hpp"
#include "RTC/MediaTranslate/MediaVoice.hpp"
#include "ProtectedObj.hpp"
#include "Logger.hpp"

namespace RTC
{

class TranslatorEndPoint::Impl : public WebsocketListener, private OutputDevice
{
public:
    Impl(const std::weak_ptr<Websocket>& websocketRef, const std::string& userAgent);
    ~Impl() final;
    void FinalizeMedia();
    void Open();
    void Close();
    void SetProducerLanguage(const std::optional<MediaLanguage>& language);
    void SetConsumerLanguage(MediaLanguage language);
    void SetConsumerVoice(MediaVoice voice);
    void SetInput(const std::shared_ptr<ProducerInputMediaStreamer>& input);
    bool HasInput() const;
    void SetOutput(const std::weak_ptr<RtpPacketsCollector>& outputRef);
    bool IsConnected() const { return _connected.load(std::memory_order_relaxed); }
    // impl. of WebsocketListener
    void OnStateChanged(uint64_t socketId, WebsocketState state) final;
    void OnTextMessageReceived(uint64_t socketId, std::string message) final;
    void OnBinaryMessageReceved(uint64_t socketId, const std::shared_ptr<MemoryBuffer>& message) final;
private:
    bool IsWantsToOpen() const { return _wantsToOpen.load(std::memory_order_relaxed); }
    void OpenWebsocket();
    void CloseWebsocket();
    void InitializeMediaInput();
    void InitializeMediaInput(const std::shared_ptr<ProducerInputMediaStreamer>& input);
    void FinalizeMediaInput();
    void FinalizeMediaInput(const std::shared_ptr<ProducerInputMediaStreamer>& input);
    bool SendTranslationChanges();
    bool WriteJson(const nlohmann::json& data);
    MediaLanguage GetConsumerLanguage() const { return _consumerLanguage.load(std::memory_order_relaxed); }
    MediaVoice GetConsumerVoice() const { return _consumerVoice.load(std::memory_order_relaxed); }
    std::optional<MediaLanguage> GetProducerLanguage() const;
    // impl. of OutputDevice
    void StartStream(bool restart) noexcept final;
    void BeginWriteMediaPayload(uint32_t ssrc, bool isKeyFrame,
                                const RtpCodecMimeType& mimeType,
                                uint16_t rtpSequenceNumber,
                                uint32_t rtpTimestamp,
                                uint32_t rtpAbsSendtime) noexcept final;
    void EndWriteMediaPayload(uint32_t ssrc, bool ok) noexcept final;
    void Write(const std::shared_ptr<const MemoryBuffer>& buffer) noexcept final;
    void EndStream(bool failure) noexcept final;
private:
    const std::weak_ptr<Websocket> _websocketRef;
    const std::string _userAgent;
    std::atomic_bool _connected = false;
    std::atomic<MediaLanguage> _consumerLanguage = DefaultOutputMediaLanguage();
    std::atomic<MediaVoice> _consumerVoice = DefaultMediaVoice();
    ProtectedOptional<MediaLanguage> _producerLanguage = DefaultInputMediaLanguage();
    ProtectedSharedPtr<ProducerInputMediaStreamer> _input;
    ProtectedWeakPtr<RtpPacketsCollector> _outputRef;
    std::atomic_bool _wantsToOpen = false;
};

TranslatorEndPoint::TranslatorEndPoint(const std::string& serviceUri,
                                       const std::string& serviceUser,
                                       const std::string& servicePassword,
                                       const std::string& userAgent)
    : _websocket(std::make_shared<Websocket>(serviceUri, serviceUser, servicePassword))
    , _impl(std::make_shared<Impl>(_websocket, userAgent))
{
    _websocket->SetListener(_impl);
}

TranslatorEndPoint::~TranslatorEndPoint()
{
    Close();
    _impl->FinalizeMedia();
}

void TranslatorEndPoint::Open()
{
    _impl->Open();
}

void TranslatorEndPoint::Close()
{
    _impl->Close();
}

void TranslatorEndPoint::SetProducerLanguage(const std::optional<MediaLanguage>& language)
{
    _impl->SetProducerLanguage(language);
}

void TranslatorEndPoint::SetConsumerLanguage(MediaLanguage language)
{
    _impl->SetConsumerLanguage(language);
}

void TranslatorEndPoint::SetConsumerVoice(MediaVoice voice)
{
    _impl->SetConsumerVoice(voice);
}

void TranslatorEndPoint::SetInput(const std::shared_ptr<ProducerInputMediaStreamer>& input)
{
    _impl->SetInput(input);
}

bool TranslatorEndPoint::HasInput() const
{
    return _impl->HasInput();
}

void TranslatorEndPoint::SetOutput(const std::weak_ptr<RtpPacketsCollector>& outputRef)
{
    _impl->SetOutput(outputRef);
}

TranslatorEndPoint::Impl::Impl(const std::weak_ptr<Websocket>& websocketRef, const std::string& userAgent)
    : _websocketRef(websocketRef)
    , _userAgent(userAgent)
{
}

TranslatorEndPoint::Impl::~Impl()
{
    FinalizeMedia();
}

void TranslatorEndPoint::Impl::FinalizeMedia()
{
    FinalizeMediaInput();
    SetInput(nullptr);
    SetOutput(std::weak_ptr<RtpPacketsCollector>());
}

void TranslatorEndPoint::Impl::Open()
{
    if (!_wantsToOpen.exchange(true)) {
        LOCK_READ_PROTECTED_OBJ(_input);
        if (_input.ConstRef()) {
            OpenWebsocket();
        }
    }
}

void TranslatorEndPoint::Impl::Close()
{
    if (_wantsToOpen.exchange(false)) {
        CloseWebsocket();
    }
}

void TranslatorEndPoint::Impl::SetProducerLanguage(const std::optional<MediaLanguage>& language)
{
    bool changed = false;
    {
        LOCK_WRITE_PROTECTED_OBJ(_producerLanguage);
        if (language != _producerLanguage.ConstRef()) {
            _producerLanguage = language;
            changed = true;
        }
    }
    if (changed) {
        SendTranslationChanges();
    }
}

void TranslatorEndPoint::Impl::SetConsumerLanguage(MediaLanguage language)
{
    if (language != _consumerLanguage.exchange(language)) {
        SendTranslationChanges();
    }
}

void TranslatorEndPoint::Impl::SetConsumerVoice(MediaVoice voice)
{
    if (voice != _consumerVoice.exchange(voice)) {
        SendTranslationChanges();
    }
}

void TranslatorEndPoint::Impl::SetInput(const std::shared_ptr<ProducerInputMediaStreamer>& input)
{
    bool changed = false;
    {
        LOCK_WRITE_PROTECTED_OBJ(_input);
        if (input != _input.ConstRef()) {
            FinalizeMediaInput(_input.ConstRef());
            _input = input;
            changed = true;
            if (input && IsConnected()) {
                InitializeMediaInput(input);
            }
        }
    }
    if (changed) {
        const auto maybeOpen = nullptr != input;
        if (maybeOpen == IsWantsToOpen()) {
            if (maybeOpen) {
                OpenWebsocket();
            }
            else {
                CloseWebsocket();
            }
        }
    }
}

bool TranslatorEndPoint::Impl::HasInput() const
{
    LOCK_READ_PROTECTED_OBJ(_input);
    return nullptr != _input.ConstRef();
}

void TranslatorEndPoint::Impl::SetOutput(const std::weak_ptr<RtpPacketsCollector>& outputRef)
{
    LOCK_WRITE_PROTECTED_OBJ(_outputRef);
    _outputRef = outputRef;
}

void TranslatorEndPoint::Impl::OnStateChanged(uint64_t socketId, WebsocketState state)
{
    WebsocketListener::OnStateChanged(socketId, state);
    switch (state) {
        case WebsocketState::Connected:
            _connected = true;
            if (SendTranslationChanges()) {
                InitializeMediaInput();
            }
            break;
        case WebsocketState::Disconnected:
            _connected = false;
            FinalizeMediaInput();
            break;
        default:
            break;
    }
}

void TranslatorEndPoint::Impl::OnTextMessageReceived(uint64_t /*socketId*/, std::string /*message*/)
{
}

void TranslatorEndPoint::Impl::OnBinaryMessageReceved(uint64_t /*socketId*/,
                                                      const std::shared_ptr<MemoryBuffer>& /*message*/)
{
}

void TranslatorEndPoint::Impl::OpenWebsocket()
{
    if (const auto websocket = _websocketRef.lock()) {
        if (!websocket->Open(_userAgent)) {
            MS_ERROR("failed to open websocket");
        }
    }
}

void TranslatorEndPoint::Impl::CloseWebsocket()
{
    if (const auto websocket = _websocketRef.lock()) {
        websocket->Close();
    }
}

void TranslatorEndPoint::Impl::InitializeMediaInput()
{
    LOCK_READ_PROTECTED_OBJ(_input);
    InitializeMediaInput(_input.ConstRef());
}

void TranslatorEndPoint::Impl::InitializeMediaInput(const std::shared_ptr<ProducerInputMediaStreamer>& input)
{
    if (input) {
        input->AddOutputDevice(this);
    }
}

void TranslatorEndPoint::Impl::FinalizeMediaInput()
{
    LOCK_READ_PROTECTED_OBJ(_input);
    FinalizeMediaInput(_input.ConstRef());
}

void TranslatorEndPoint::Impl::FinalizeMediaInput(const std::shared_ptr<ProducerInputMediaStreamer>& input)
{
    if (input) {
        input->RemoveOutputDevice(this);
    }
}

bool TranslatorEndPoint::Impl::SendTranslationChanges()
{
    if (IsConnected()) {
        const TranslationPack tp(GetConsumerLanguage(), GetConsumerVoice(), GetProducerLanguage());
        const auto jsonData = GetTargetLanguageCmd(tp);
        return WriteJson(jsonData);
    }
    return false;
}

bool TranslatorEndPoint::Impl::WriteJson(const nlohmann::json& data)
{
    bool ok = false;
    if (const auto websocket = _websocketRef.lock()) {
        const auto jsonAsText = to_string(data);
        ok = websocket->WriteText(jsonAsText);
        if (!ok) {
            MS_ERROR("failed write JSON command '%s' into translation service", jsonAsText.c_str());
        }
    }
    return ok;
}

std::optional<MediaLanguage> TranslatorEndPoint::Impl::GetProducerLanguage() const
{
    LOCK_READ_PROTECTED_OBJ(_producerLanguage);
    return _producerLanguage.ConstRef();
}

void TranslatorEndPoint::Impl::StartStream(bool restart) noexcept
{
    OutputDevice::StartStream(restart);
    if (IsConnected()) {
        // TODO: send JSON command
    }
}

void TranslatorEndPoint::Impl::BeginWriteMediaPayload(uint32_t ssrc, bool isKeyFrame,
                                                      const RtpCodecMimeType& mimeType,
                                                      uint16_t rtpSequenceNumber,
                                                      uint32_t rtpTimestamp,
                                                      uint32_t rtpAbsSendtime) noexcept
{
    OutputDevice::BeginWriteMediaPayload(ssrc, isKeyFrame, mimeType,
                                         rtpSequenceNumber, rtpTimestamp, rtpAbsSendtime);
    if (IsConnected()) {
        // TODO: send JSON command
    }
}

void TranslatorEndPoint::Impl::EndWriteMediaPayload(uint32_t ssrc, bool ok) noexcept
{
    if (IsConnected()) {
        // TODO: send JSON command
    }
}

void TranslatorEndPoint::Impl::Write(const std::shared_ptr<const MemoryBuffer>& buffer) noexcept
{
    if (buffer && IsConnected()) {
        if (const auto websocket = _websocketRef.lock()) {
            if (!websocket->WriteBinary(buffer)) {
                MS_ERROR("failed write binary buffer into into translation service");
            }
        }
    }
}

void TranslatorEndPoint::Impl::EndStream(bool failure) noexcept
{
    OutputDevice::EndStream(failure);
    if (IsConnected()) {
        // TODO: send JSON command
    }
}

} // namespace RTC
