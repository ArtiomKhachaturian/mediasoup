#define MS_CLASS "RTC::TranslatorEndPoint"
#include "RTC/MediaTranslate/TranslatorEndPoint.hpp"
#include "RTC/MediaTranslate/Websocket.hpp"
#include "RTC/MediaTranslate/ProducerInputMediaStreamer.hpp"
#include "Logger.hpp"

namespace RTC
{

TranslatorEndPoint::TranslatorEndPoint(const std::string& serviceUri,
                                       const std::string& serviceUser,
                                       const std::string& servicePassword,
                                       const std::string& userAgent)
    : _userAgent(userAgent)
    , _socket(std::make_unique<Websocket>(serviceUri, serviceUser, servicePassword))
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

void TranslatorEndPoint::SetProducerLanguage(const std::optional<FBS::TranslationPack::Language>& language)
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
        UpdateTranslationChanges();
    }
}

void TranslatorEndPoint::SetConsumerLanguageAndVoice(const std::optional<FBS::TranslationPack::Language>& language,
                                                     const std::optional<FBS::TranslationPack::Voice>& voice)
{
    bool changed = false;
    {
        LOCK_WRITE_PROTECTED_OBJ(_consumerLanguage);
        if (language != _consumerLanguage.ConstRef()) {
            _consumerLanguage = language;
            changed = true;
        }
    }
    {
        LOCK_WRITE_PROTECTED_OBJ(_consumerVoice);
        if (voice != _consumerVoice.ConstRef()) {
            _consumerVoice = voice;
            changed = true;
        }
    }
    if (changed) {
        UpdateTranslationChanges();
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

std::string_view TranslatorEndPoint::LanguageToString(const std::optional<FBS::TranslationPack::Language>& language)
{
    if (language.has_value()) {
        switch (language.value()) {
            case FBS::TranslationPack::Language::English:
                return "en";
            case FBS::TranslationPack::Language::Italian:
                return "it";
            case FBS::TranslationPack::Language::Spain:
                return "es";
            case FBS::TranslationPack::Language::Thai:
                return "th";
            case FBS::TranslationPack::Language::French:
                return "fr";
            case FBS::TranslationPack::Language::German:
                return "de";
            case FBS::TranslationPack::Language::Russian:
                return "ru";
            case FBS::TranslationPack::Language::Arabic:
                return "ar";
            case FBS::TranslationPack::Language::Farsi:
                return "fa";
            default:
                MS_ASSERT(false, "unhandled language type");
                break;
        }
        return "";
    }
    return "auto";
}

std::string_view TranslatorEndPoint::VoiceToString(FBS::TranslationPack::Voice voice)
{
    switch (voice) {
        case FBS::TranslationPack::Voice::Abdul:
            return "YkxA6GRXs4A6i5cwfm1E";
        case FBS::TranslationPack::Voice::JesusRodriguez:
            return "ovxyZ1ldY23QpYBvkKx5";
        case FBS::TranslationPack::Voice::TestIrina:
            return "ovxyZ1ldY23QpYBvkKx5";
        case FBS::TranslationPack::Voice::Serena:
            return "pMsXgVXv3BLzUgSXRplE";
        case FBS::TranslationPack::Voice::Ryan:
            return "wViXBPUzp2ZZixB1xQuM";
        default:
            MS_ASSERT(false, "unhandled voice type");
            break;
    }
    return "";
}

nlohmann::json TranslatorEndPoint::TargetLanguageCmd(FBS::TranslationPack::Language languageTo,
                                                     FBS::TranslationPack::Voice voice,
                                                     const std::optional<FBS::TranslationPack::Language>& languageFrom)
{
    nlohmann::json cmd = {{
        "from",    LanguageToString(languageFrom),
        "to",      LanguageToString(languageTo),
        "voiceID", VoiceToString(voice)
    }};
    nlohmann::json data = {
        "type", "set_target_language",
        "cmd",  cmd
    };
    return data;
}

bool TranslatorEndPoint::HasValidTranslationSettings() const
{
    return GetConsumerVoice().has_value() && GetConsumerLanguage().has_value();
}

std::optional<FBS::TranslationPack::Voice> TranslatorEndPoint::GetConsumerVoice() const
{
    LOCK_READ_PROTECTED_OBJ(_consumerVoice);
    return _consumerVoice.ConstRef();
}

std::optional<FBS::TranslationPack::Language> TranslatorEndPoint::GetConsumerLanguage() const
{
    LOCK_READ_PROTECTED_OBJ(_consumerLanguage);
    return _consumerLanguage.ConstRef();
}

std::optional<FBS::TranslationPack::Language> TranslatorEndPoint::GetProducerLanguage() const
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


void TranslatorEndPoint::UpdateTranslationChanges()
{
    if (HasValidTranslationSettings()) {
        if (IsConnected()) {
            SendTranslationChanges();
        }
        else if(IsWantsToOpen()) {
            OpenSocket();
        }
    }
    else {
        _socket->Close();
    }
}

bool TranslatorEndPoint::SendTranslationChanges()
{
    if (IsConnected()) {
        if (const auto languageTo = GetConsumerLanguage()) {
            if (const auto voice = GetConsumerVoice()) {
                const auto jsonData = TargetLanguageCmd(languageTo.value(), voice.value(),
                                                        GetProducerLanguage());
                return WriteJson(jsonData);
            }
        }
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
    if (HasInput() && !IsConnected() && HasValidTranslationSettings()) {
        _socket->Open(_userAgent);
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
    
