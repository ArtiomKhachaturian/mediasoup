#define MS_CLASS "RTC::TranslatorEndPoint"
#include "RTC/MediaTranslate/TranslatorEndPoint.hpp"
#include "RTC/MediaTranslate/Websocket.hpp"
#include "RTC/MediaTranslate/ProducerInputMediaStreamer.hpp"
#include "RTC/MediaTranslate/MediaLanguage.hpp"
#include "RTC/MediaTranslate/MediaVoice.hpp"

#include "Logger.hpp"

namespace RTC
{

TranslatorEndPoint::TranslatorEndPoint(const std::string& serviceUri,
                                       const std::string& serviceUser,
                                       const std::string& servicePassword,
                                       const std::string& userAgent)
    : _userAgent(userAgent)
    , _socket(std::make_unique<Websocket>(serviceUri, serviceUser, servicePassword))
    , _consumerLanguage(DefaultOutputMediaLanguage())
    , _consumerVoice(DefaultMediaVoice())
    , _producerLanguage(DefaultInputMediaLanguage())
{
    _socket->AddListener(this);
}

TranslatorEndPoint::~TranslatorEndPoint()
{
    Close();
    SetInput(nullptr);
    _socket->RemoveListener(this);
}

void TranslatorEndPoint::Open()
{
    if (!_wantsToOpen.exchange(true)) {
        OpenSocket();
    }
}

void TranslatorEndPoint::Close()
{
    if (_wantsToOpen.exchange(false)) {
        _socket->Close();
    }
}

void TranslatorEndPoint::SetProducerLanguage(const std::optional<MediaLanguage>& language)
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

void TranslatorEndPoint::SetConsumerLanguage(MediaLanguage language)
{
    if (language != _consumerLanguage.exchange(language)) {
        SendTranslationChanges();
    }
}

void TranslatorEndPoint::SetConsumerVoice(MediaVoice voice)
{
    if (voice != _consumerVoice.exchange(voice)) {
        SendTranslationChanges();
    }
}

void TranslatorEndPoint::SetInput(const std::shared_ptr<ProducerInputMediaStreamer>& input)
{
    bool changed = false;
    {
        LOCK_WRITE_PROTECTED_OBJ(_input);
        if (input != _input.ConstRef()) {
            ConnectToMediaInput(_input.ConstRef(), false);
            _input = input;
            if (IsConnected()) {
                ConnectToMediaInput(_input.ConstRef(), true);
            }
            changed = true;
        }
    }
    if (changed) {
        if (input) {
            const auto maybeOpen = IsWantsToOpen() != IsConnected();
            if (maybeOpen) {
                OpenSocket();
            }
        }
        else {
            _socket->Close();
        }
    }
}

bool TranslatorEndPoint::HasInput() const
{
    LOCK_READ_PROTECTED_OBJ(_input);
    return nullptr != _input->get();
}

void TranslatorEndPoint::SetOutput(const std::weak_ptr<RtpPacketsCollector>& outputRef)
{
    LOCK_WRITE_PROTECTED_OBJ(_outputRef);
    _outputRef = outputRef;
}

std::optional<MediaLanguage> TranslatorEndPoint::GetProducerLanguage() const
{
    LOCK_READ_PROTECTED_OBJ(_producerLanguage);
    return _producerLanguage.ConstRef();
}

void TranslatorEndPoint::SetConnected(bool connected)
{
    if (connected != _connected.exchange(connected)) {
        if (connected) {
            if (SendTranslationChanges()) {
                ConnectToMediaInput(true);
            }
        }
        else {
            ConnectToMediaInput(false);
        }
    }
}

void TranslatorEndPoint::ConnectToMediaInput(bool connect)
{
    LOCK_READ_PROTECTED_OBJ(_input);
    ConnectToMediaInput(_input.ConstRef(), connect);
}

void TranslatorEndPoint::ConnectToMediaInput(const std::shared_ptr<ProducerInputMediaStreamer>& input,
                                             bool connect)
{
    if (input) {
        if (connect) {
            input->AddOutputDevice(this);
        }
        else {
            input->RemoveOutputDevice(this);
        }
    }
}

bool TranslatorEndPoint::SendTranslationChanges()
{
    if (IsConnected()) {
        const TranslationPack tp(GetConsumerLanguage(), GetConsumerVoice(), GetProducerLanguage());
        const auto jsonData = GetTargetLanguageCmd(tp);
        return WriteJson(jsonData);
    }
    return false;
}

bool TranslatorEndPoint::WriteJson(const nlohmann::json& data) const
{
    const auto jsonAsText = to_string(data);
    const auto ok = _socket->WriteText(jsonAsText);
    if (!ok) {
        MS_ERROR("failed write JSON command '%s' into translation service", jsonAsText.c_str());
    }
    return ok;
}

void TranslatorEndPoint::OpenSocket()
{
    if (HasInput() && !IsConnected()) {
        _socket->Open();
    }
}

void TranslatorEndPoint::StartStream(bool restart) noexcept
{
    OutputDevice::StartStream(restart);
    if (IsConnected()) {
        // TODO: send JSON command
    }
}

void TranslatorEndPoint::BeginWriteMediaPayload(uint32_t ssrc,
                                                const std::vector<RtpMediaPacketInfo>& packets) noexcept
{
    OutputDevice::BeginWriteMediaPayload(ssrc, packets);
    if (IsConnected()) {
        // TODO: send JSON command
    }
}

void TranslatorEndPoint::EndWriteMediaPayload(uint32_t ssrc,
                                              const std::vector<RtpMediaPacketInfo>& packets,
                                              bool ok) noexcept
{
    OutputDevice::EndWriteMediaPayload(ssrc, packets, ok);
    if (IsConnected()) {
        // TODO: send JSON command
    }
}

void TranslatorEndPoint::Write(const std::shared_ptr<const MemoryBuffer>& buffer) noexcept
{
    if (buffer && IsConnected()) {
        _socket->WriteBinary(buffer);
    }
}

void TranslatorEndPoint::EndStream(bool failure) noexcept
{
    OutputDevice::EndStream(failure);
    if (IsConnected()) {
        // TODO: send JSON command
    }
}

void TranslatorEndPoint::OnStateChanged(uint64_t socketId, WebsocketState state)
{
    WebsocketListener::OnStateChanged(socketId, state);
    switch (state) {
        case WebsocketState::Connected:
            SetConnected(true);
            break;
        case WebsocketState::Disconnected:
            SetConnected(false);
            break;
        default:
            break;
    }
}

void TranslatorEndPoint::OnTextMessageReceived(uint64_t socketId, std::string message)
{
}

void TranslatorEndPoint::OnBinaryMessageReceved(uint64_t socketId, const std::shared_ptr<MemoryBuffer>& message)
{
}

} // namespace RTC
