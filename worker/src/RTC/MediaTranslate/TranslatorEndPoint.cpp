#define MS_CLASS "RTC::TranslatorEndPoint"
#include "RTC/MediaTranslate/TranslatorEndPoint.hpp"
#include "RTC/MediaTranslate/SimpleMemoryBuffer.hpp"
#include "RTC/MediaTranslate/Websocket.hpp"
#include "RTC/MediaTranslate/MediaSource.hpp"
#ifdef PLAY_MOCK_FILE_AFTER_CONNECTION
#include "RTC/MediaTranslate/FileReader.hpp"
#endif
#ifdef WRITE_TRANSLATION_TO_FILE
#include "RTC/MediaTranslate/FileWriter.hpp"
#endif
#include "DepLibUV.hpp"
#include "Logger.hpp"

namespace RTC
{

class TranslatorEndPoint::InputSliceBuffer
{
public:
    InputSliceBuffer(uint32_t timeSliceMs);
    void Add(const std::shared_ptr<MemoryBuffer>& buffer, TranslatorEndPoint* endPoint);
    void Reset(bool start);
    static std::unique_ptr<InputSliceBuffer> Create(uint32_t timeSliceMs);
private:
    const uint32_t _timeSliceMs;
    uint64_t _sliceOriginTimestamp = 0ULL;
    ProtectedObj<SimpleMemoryBuffer> _impl;
};

TranslatorEndPoint::TranslatorEndPoint(const std::string& serviceUri,
                                       const std::string& serviceUser,
                                       const std::string& servicePassword,
                                       const std::string& userAgent,
                                       uint32_t timeSliceMs)
    : _userAgent(userAgent)
    , _socket(std::make_unique<Websocket>(serviceUri, serviceUser, servicePassword))
    , _serviceUri(serviceUri)
    , _inputSlice(InputSliceBuffer::Create(timeSliceMs))
{
    _socket->AddListener(this);
}

TranslatorEndPoint::~TranslatorEndPoint()   
{
    SetInput(nullptr);
    _socket->RemoveListener(this);
    SetConnected(false);
#ifdef PLAY_MOCK_FILE_AFTER_CONNECTION
    LOCK_WRITE_PROTECTED_OBJ(_mockInputFile);
    _mockInputFile->reset();
#endif
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
            CloseSocket();
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

void TranslatorEndPoint::SendDataToMediaSink(uint32_t ssrc, const std::shared_ptr<MemoryBuffer>& data,
                                             MediaSink* sink)
{
    if (data && sink) {
        sink->StartMediaWriting(ssrc);
        sink->WriteMediaPayload(data);
        sink->EndMediaWriting();
    }
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

std::optional<nlohmann::json> TranslatorEndPoint::TargetLanguageCmd() const
{
    if (const auto languageTo = GetConsumerLanguage()) {
        if (const auto voice = GetConsumerVoice()) {
            return TargetLanguageCmd(languageTo.value(), voice.value(), GetProducerLanguage());
        }
    }
    return std::nullopt;
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
        if (_inputSlice) {
            _inputSlice->Reset(false);
        }
    }
    MS_DEBUG_DEV_STD("%s translation service %s",
                     connected ? "connected to" : "disconnected from",
                     _serviceUri.c_str());
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
        CloseSocket();
    }
}

bool TranslatorEndPoint::SendTranslationChanges()
{
    if (IsConnected()) {
        if (const auto cmd = TargetLanguageCmd()) {
            return WriteJson(cmd.value());
        }
    }
    return false;
}

bool TranslatorEndPoint::WriteJson(const nlohmann::json& data) const
{
    bool ok = false;
    if (IsConnected()) {
        const auto jsonAsText = nlohmann::to_string(data);
        ok = _socket->WriteText(jsonAsText);
        if (!ok) {
            MS_ERROR_STD("failed write JSON '%s' into translation service %s",
                         jsonAsText.c_str(), _serviceUri.c_str());
        }
    }
    return ok;
}

bool TranslatorEndPoint::WriteBinary(const MemoryBuffer& buffer) const
{
    bool ok = false;
    if (IsConnected()) {
        ok = _socket->WriteBinary(buffer);
        if (!ok) {
            MS_ERROR_STD("failed write binary (%llu bytes)' into translation service %s",
                         buffer.GetSize(), _serviceUri.c_str());
        }
    }
    return ok;
}

void TranslatorEndPoint::OpenSocket()
{
    if (HasInput() && HasValidTranslationSettings()) {
        switch (_socket->GetState()) {
            case WebsocketState::Disconnected:
                if (!_socket->Open(_userAgent)) {
                    MS_ERROR_STD("failed open websocket connection with %s", _serviceUri.c_str());
                }
                break;
            default:
                break;
        }
    }
}

void TranslatorEndPoint::CloseSocket()
{
    if (IsConnected()) {
        SimpleMemoryBuffer bye;
        _socket->WriteBinary(bye);
    }
    _socket->Close();
}

void TranslatorEndPoint::StartMediaWriting(uint32_t ssrc)
{
    MediaSink::StartMediaWriting(ssrc);
    if (_inputSlice) {
        _inputSlice->Reset(true);
    }
    if (0U == _ssrc.exchange(ssrc)) {
#ifdef PLAY_MOCK_FILE_AFTER_CONNECTION
        if (const auto ssrc = _ssrc.load()) {
            LOCK_READ_PROTECTED_OBJ(_output);
            if (const auto output = _output.ConstRef()) {
                auto mockInputFile = std::make_unique<FileReader>(_testFileName, false);
                if (mockInputFile->IsOpen()) {
                    mockInputFile->SetSsrc(ssrc);
                    if (mockInputFile->AddSink(output)) {
                        LOCK_WRITE_PROTECTED_OBJ(_mockInputFile);
                        _mockInputFile = std::move(mockInputFile);
                    }
                }
            }
        }
#endif
    }
}

void TranslatorEndPoint::WriteMediaPayload(const std::shared_ptr<MemoryBuffer>& buffer)
{
    if (buffer && !buffer->IsEmpty() && IsConnected()) {
        if (_inputSlice) {
            _inputSlice->Add(buffer, this);
        }
        else {
            WriteBinary(*buffer);
        }
    }
}

void TranslatorEndPoint::EndMediaWriting()
{
    MediaSink::EndMediaWriting();
    if (_inputSlice) {
        _inputSlice->Reset(false);
    }
    if (_ssrc.exchange(0U)) {
#ifdef PLAY_MOCK_FILE_AFTER_CONNECTION
        LOCK_WRITE_PROTECTED_OBJ(_mockInputFile);
        _mockInputFile->reset();
#endif
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

void TranslatorEndPoint::OnBinaryMessageReceved(uint64_t, const std::shared_ptr<MemoryBuffer>& message)
{
    if (message) {
        if (const auto ssrc = _ssrc.load()) {
            MS_ERROR_STD("Received translation for %u SSRC", _ssrc.load()); // TODO: remove it for production
            LOCK_READ_PROTECTED_OBJ(_output);
            if (const auto output = _output.ConstRef()) {
                SendDataToMediaSink(ssrc, message, output);
#ifdef WRITE_TRANSLATION_TO_FILE
                const auto depacketizerPath = std::getenv("MEDIASOUP_DEPACKETIZER_PATH");
                if (depacketizerPath && std::strlen(depacketizerPath)) {
                    const auto ssrc = _ssrc.load();
                    std::string fileName = "received_translation_" + std::to_string(ssrc) + "_"
                    + std::to_string(++_translationsCounter) + ".webm";
                    FileWriter file(std::string(depacketizerPath) + "/" + fileName);
                    if (file.IsOpen()) {
                        SendDataToMediaSink(ssrc, message, &file);
                    }
                }
#endif
            }
        }
    }
}

TranslatorEndPoint::InputSliceBuffer::InputSliceBuffer(uint32_t timeSliceMs)
    : _timeSliceMs(timeSliceMs)
{
}

void TranslatorEndPoint::InputSliceBuffer::Add(const std::shared_ptr<MemoryBuffer>& buffer,
                                               TranslatorEndPoint* endPoint)
{
    if (buffer && endPoint) {
        LOCK_WRITE_PROTECTED_OBJ(_impl);
        if (_impl->Append(*buffer)) {
            const auto now = DepLibUV::GetTimeMs();
            if (now > _sliceOriginTimestamp + _timeSliceMs) {
                _sliceOriginTimestamp = now;
                endPoint->WriteBinary(_impl.ConstRef());
                _impl->Clear();
            }
        }
        else {
            MS_ERROR_STD("unable to add memory buffer (%llu bytes) to input slice", buffer->GetSize());
        }
    }
}

void TranslatorEndPoint::InputSliceBuffer::Reset(bool start)
{
    LOCK_WRITE_PROTECTED_OBJ(_impl);
    _impl->Clear();
    if (start) {
        _sliceOriginTimestamp = DepLibUV::GetTimeMs();
    }
    else {
        _sliceOriginTimestamp = 0ULL;
    }
}

std::unique_ptr<TranslatorEndPoint::InputSliceBuffer> TranslatorEndPoint::InputSliceBuffer::
    Create(uint32_t timeSliceMs)
{
    if (timeSliceMs) {
        return std::make_unique<InputSliceBuffer>(timeSliceMs);
    }
    return nullptr;
}

} // namespace RTC
