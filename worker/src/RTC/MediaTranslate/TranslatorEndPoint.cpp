#define MS_CLASS "RTC::TranslatorEndPoint"
#include "RTC/MediaTranslate/TranslatorEndPoint.hpp"
#include "RTC/MediaTranslate/Websocket.hpp"
#include "RTC/MediaTranslate/MediaSource.hpp"
#ifdef WRITE_TRANSLATION_TO_FILE
#include "RTC/MediaTranslate/FileWriter.hpp"
#endif
#include "Logger.hpp"

namespace RTC
{

TranslatorEndPoint::TranslatorEndPoint(const std::string& serviceUri,
                                       const std::string& serviceUser,
                                       const std::string& servicePassword,
                                       const std::string& userAgent)
    : _userAgent(userAgent)
    , _socket(std::make_unique<Websocket>(serviceUri, serviceUser, servicePassword))
    , _serviceUri(serviceUri)
{
    _socket->AddListener(this);
}

TranslatorEndPoint::~TranslatorEndPoint()
{
    SetInput(nullptr);
    _socket->RemoveListener(this);
    SetConnected(false);
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

void TranslatorEndPoint::SetInput(MediaSource* input)
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
            OpenSocket();
        }
        else {
            _socket->Close();
        }
    }
}

bool TranslatorEndPoint::HasInput() const
{
    LOCK_READ_PROTECTED_OBJ(_input);
    return nullptr != _input.ConstRef();
}

void TranslatorEndPoint::SetOutput(MediaSink* output)
{
    LOCK_WRITE_PROTECTED_OBJ(_output);
    _output = output;
}

bool TranslatorEndPoint::IsConnected() const
{
    return WebsocketState::Connected == _socket->GetState();
}

void TranslatorEndPoint::ProcessTranslation(uint32_t ssrc, MediaSink* output,
                                            const std::shared_ptr<MemoryBuffer>& message)
{
    if (output && message) {
        MS_ERROR_STD("Received translation for %u SSRC", ssrc);
        output->StartMediaWriting(false);
        output->WriteMediaPayload(ssrc, message);
        output->EndMediaWriting();
#ifdef WRITE_TRANSLATION_TO_FILE
        const auto depacketizerPath = std::getenv("MEDIASOUP_DEPACKETIZER_PATH");
        if (depacketizerPath && std::strlen(depacketizerPath)) {
            std::string fileName = "received_translation_" + std::to_string(ssrc) + "_"
                + std::to_string(++_translationsCounter) + ".webm";
            FileWriter file(std::string(depacketizerPath) + "/" + fileName);
            if (file.IsOpen()) {
                file.WriteMediaPayload(ssrc, message);
            }
        }
#endif
    }
}

std::string_view TranslatorEndPoint::LanguageToId(const std::optional<FBS::TranslationPack::Language>& language)
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

std::string_view TranslatorEndPoint::VoiceToId(FBS::TranslationPack::Voice voice)
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
        case FBS::TranslationPack::Voice::Male:
            return "Male";
        case FBS::TranslationPack::Voice::Female:
            return "Female";
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
    // language settings
    nlohmann::json languageSettings;
    languageSettings["from"] = LanguageToId(languageFrom);
    languageSettings["to"] = LanguageToId(languageTo);
    languageSettings["voiceID"] = VoiceToId(voice);
    // command
    nlohmann::json command;
    command["type"] = "set_target_language";
    command["cmd"] = languageSettings;
    return command;
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
    if (connected) {
        if (SendTranslationChanges()) {
            ConnectToMediaInput(true);
        }
    }
    else {
        ConnectToMediaInput(false);
    }
}

void TranslatorEndPoint::ConnectToMediaInput(bool connect)
{
    LOCK_READ_PROTECTED_OBJ(_input);
    ConnectToMediaInput(_input.ConstRef(), connect);
}

void TranslatorEndPoint::ConnectToMediaInput(MediaSource* input, bool connect)
{
    if (input) {
        if (connect) {
            input->AddSink(this);
        }
        else {
            input->RemoveSink(this);
        }
        MS_ERROR_STD("Translation service connected to media source: %s", connect ? "YES" : "NO");
    }
}

void TranslatorEndPoint::UpdateTranslationChanges()
{
    if (HasValidTranslationSettings()) {
        if (IsConnected()) {
            SendTranslationChanges();
        }
        else {
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
    const auto jsonAsText = nlohmann::to_string(data);
    const auto ok = _socket->WriteText(jsonAsText);
    if (!ok) {
        MS_ERROR_STD("failed write JSON command '%s' into translation service", jsonAsText.c_str());
    }
    return ok;
}

void TranslatorEndPoint::OpenSocket()
{
    if (HasInput() && HasValidTranslationSettings()) {
        switch (_socket->GetState()) {
            case WebsocketState::Disconnected:
                if (!_socket->Open(_userAgent)) {
                    // TODO: log error
                }
                break;
            default:
                break;
        }
    }
}

void TranslatorEndPoint::WriteMediaPayload(uint32_t ssrc,
                                           const std::shared_ptr<const MemoryBuffer>& buffer)
{
    MS_ERROR_STD("dataavailable, size = %d", int(buffer->GetSize()));
    if (buffer && IsConnected() && !_socket->WriteBinary(buffer)) {
        MS_ERROR_STD("failed write binary packet (%ld bytes)' into translation service", buffer->GetSize());
    }
    else {
        _latestSsrc = ssrc;
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

void TranslatorEndPoint::OnBinaryMessageReceved(uint64_t /*socketId*/,
                                                const std::shared_ptr<MemoryBuffer>& message)
{
    if (message) {
        LOCK_READ_PROTECTED_OBJ(_output);
        ProcessTranslation(_latestSsrc.load(), _output.ConstRef(), message);
    }
}

} // namespace RTC
