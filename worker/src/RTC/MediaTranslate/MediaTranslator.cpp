#include "RTC/MediaTranslate/MediaTranslator.hpp"
#include "RTC/MediaTranslate/RtpMediaFrameSerializer.hpp"
#include "RTC/MediaTranslate/RtpDepacketizer.hpp"
#include "RTC/MediaTranslate/Websocket.hpp"
#include "RTC/MediaTranslate/OutputDevice.hpp"
#include <nlohmann/json.hpp>
#include <mutex>
#include <optional>

namespace RTC
{

using json = nlohmann::json;

class MediaTranslator::Impl : public WebsocketListener, public OutputDevice
{
public:
    Impl(Websocket* socket);
    void Close(bool andReset);
    void SetFromLanguage(Language language) { ApplyFromLanguage(language); }
    void SetFromLanguageAuto() { ApplyFromLanguage(std::nullopt); }
    void SetToLanguage(Language language);
    void SetVoice(Voice voice);
    int64_t GetMediaPosition() const { return _mediaPosition.load(std::memory_order_relaxed); }
    // impl. of WebsocketListener
    void OnStateChanged(WebsocketState state);
    // impl. of OutputDevice
    bool Write(const void* buf, uint32_t len) final;
    int64_t GetPosition() const final { return _mediaPosition.load(std::memory_order_relaxed); }
private:
    static std::string_view OptionalLanguageString(const std::optional<Language>& language);
    static std::string_view LanguagetoString(Language language);
    static std::string_view VoicetoString(Voice voice);
    static bool WriteJson(Websocket* socket, const json& data);
    void ApplyFromLanguage(const std::optional<Language>& language);
    bool WriteLanguageChanges();
    json GetLanguageData() const;
    std::optional<Language> GetLanguageFrom() const;
    Language GetLanguageTo() const { return _languageTo.load(std::memory_order_relaxed); }
    Voice GetVoice() const { return _voice.load(std::memory_order_relaxed); }
    void ResetMedia() { _mediaPosition = 0LL; }
private:
    // websocket ref
    Websocket* _socket;
    std::mutex _socketMutex;
    // input language
    std::optional<Language> _languageFrom;
    mutable std::mutex _languageFromMutex;
    // output language
    std::atomic<Language> _languageTo = Language::Spain;
    // voice
    std::atomic<Voice> _voice = Voice::Abdul;
    // media
    std::atomic<int64_t> _mediaPosition = 0LL;
};

MediaTranslator::MediaTranslator(const std::string& serviceUri,
                                 std::unique_ptr<RtpMediaFrameSerializer> serializer,
                                 std::unique_ptr<RtpDepacketizer> depacketizer)
    : _serializer(std::move(serializer))
    , _depacketizer(std::move(depacketizer))
    , _websocket(CreateWebsocket(serviceUri))
    , _impl(CreateImpl(_websocket.get()))
{
    if (_websocket && _impl) {
        _websocket->SetListener(_impl);
        if (_serializer) {
            _serializer->SetOutputDevice(_impl.get());
        }
    }
}

MediaTranslator::~MediaTranslator()
{
    if (_websocket && _impl) {
        if (_serializer) {
            _serializer->SetOutputDevice(nullptr);
        }
        _impl->Close(true);
        _websocket->SetListener(nullptr);
    }
}

std::unique_ptr<MediaTranslator> MediaTranslator::Create(std::string serviceUri,
                                                         const RtpCodecMimeType& mimeType)
{
    if (!serviceUri.empty()) {
        if (auto serializer = RtpMediaFrameSerializer::create(mimeType)) {
            if (auto depacketizer = RtpDepacketizer::create(mimeType)) {
                auto translator = std::make_unique<MediaTranslator>(std::move(serviceUri),
                                                                    std::move(serializer),
                                                                    std::move(depacketizer));
                if (translator->_websocket) {
                    return translator;
                }
            }
        }
    }
    return nullptr;
}

bool MediaTranslator::OpenService(const std::string& user, const std::string& password)
{
    return _depacketizer && _serializer && _websocket && _websocket->Open(user, password);
}

void MediaTranslator::CloseService()
{
    if (_impl) {
        _impl->Close(false);
    }
}

void MediaTranslator::SetFromLanguage(Language language)
{
    if (_impl) {
        _impl->SetFromLanguage(language);
    }
}

void MediaTranslator::SetFromLanguageAuto()
{
    if (_impl) {
        _impl->SetFromLanguageAuto();
    }
}

void MediaTranslator::SetToLanguage(Language language)
{
    if (_impl) {
        _impl->SetToLanguage(language);
    }
}

void MediaTranslator::SetVoice(Voice voice)
{
    if (_impl) {
        _impl->SetVoice(voice);
    }
}

void MediaTranslator::AddPacket(const RtpPacket* packet)
{
    if (packet && _depacketizer && _serializer && _websocket) {
        if (WebsocketState::Connected == _websocket->GetState()) {
            _serializer->Push(_depacketizer->AddPacket(packet));
        }
    }
}

std::unique_ptr<Websocket> MediaTranslator::CreateWebsocket(const std::string& serviceUri)
{
    if (!serviceUri.empty()) {
        auto websocket = std::make_unique<Websocket>(serviceUri);
        if (WebsocketState::Invalid != websocket->GetState()) {
            return websocket;
        }
    }
    return nullptr;
}

std::shared_ptr<MediaTranslator::Impl> MediaTranslator::CreateImpl(Websocket* websocket)
{
    if (websocket) {
        return std::make_shared<Impl>(websocket);
    }
    return nullptr;
}

MediaTranslator::Impl::Impl(Websocket* socket)
    : _socket(socket)
{
}

void MediaTranslator::Impl::Close(bool andReset)
{
    const std::lock_guard<std::mutex> lock(_socketMutex);
    if (_socket) {
        _socket->Close();
        if (andReset) {
            _socket = nullptr;
        }
    }
    ResetMedia();
}

void MediaTranslator::Impl::SetToLanguage(Language language)
{
    if (language != _languageTo.exchange(language)) {
        WriteLanguageChanges();
    }
}

void MediaTranslator::Impl::SetVoice(Voice voice)
{
    if (voice != _voice.exchange(voice)) {
        WriteLanguageChanges();
    }
}

bool MediaTranslator::Impl::Write(const void* buf, uint32_t len)
{
    bool ok = false;
    if (buf && len) {
        const std::lock_guard<std::mutex> lock(_socketMutex);
        if (_socket && WebsocketState::Connected == _socket->GetState()) {
            ok = _socket->Write(buf, len);
        }
    }
    if (ok) {
        _mediaPosition.fetch_add(len);
    }
    return ok;
}

void MediaTranslator::Impl::OnStateChanged(WebsocketState state)
{
    switch (state) {
        case WebsocketState::Connected:
            WriteLanguageChanges();
            break;
        case WebsocketState::Disconnected:
            ResetMedia();
            break;
        default:
            break;
    }
}

std::string_view MediaTranslator::Impl::OptionalLanguageString(const std::optional<Language>& language)
{
    if (language.has_value()) {
        return LanguagetoString(language.value());
    }
    return "auto";
}

std::string_view MediaTranslator::Impl::LanguagetoString(Language language)
{
    switch (language) {
        case Language::English:
            return "en";
        case Language::Italian:
            return "it";
        case Language::Spain:
            return "es";
        case Language::Thai:
            return "th";
        case Language::French:
            return "fr";
        case Language::German:
            return "de";
        case Language::Russian:
            return "ru";
        case Language::Arabic:
            return "ar";
        case Language::Farsi:
            return "fa";
        default:
            // assert
            break;
    }
    return "";
}

std::string_view MediaTranslator::Impl::VoicetoString(Voice voice)
{
    switch (voice) {
        case Voice::Abdul:
            return "YkxA6GRXs4A6i5cwfm1E";
        case Voice::Jesus_Rodriguez:
            return "ovxyZ1ldY23QpYBvkKx5";
        case Voice::Test_Irina:
            return "ovxyZ1ldY23QpYBvkKx5";
        case Voice::Serena:
            return "pMsXgVXv3BLzUgSXRplE";
            break;
        case Voice::Ryan:
            return "wViXBPUzp2ZZixB1xQuM";
        default:
            // assert
            break;
    }
    return "";
}

bool MediaTranslator::Impl::WriteJson(Websocket* socket, const json& data)
{
    return socket && socket->WriteText(to_string(data));
}

void MediaTranslator::Impl::ApplyFromLanguage(const std::optional<Language>& language)
{
    bool changed = false;
    {
        const std::lock_guard<std::mutex> lock(_languageFromMutex);
        if (_languageFrom != language) {
            _languageFrom = language;
            changed = true;
        }
    }
    if (changed) {
        WriteLanguageChanges();
    }
}

bool MediaTranslator::Impl::WriteLanguageChanges()
{
    const std::lock_guard<std::mutex> lock(_socketMutex);
    if (_socket && WebsocketState::Connected == _socket->GetState()) {
        if (!WriteJson(_socket, GetLanguageData())) {
            // log error
            return false;
        }
    }
    return true;
}

json MediaTranslator::Impl::GetLanguageData() const
{
    json cmd = {{
        "from", OptionalLanguageString(GetLanguageFrom()),
        "to", LanguagetoString(GetLanguageTo()),
        "voiceID", VoicetoString(GetVoice())
    }};
    json data = {
        "type", "set_target_language",
        "cmd", cmd
    };
    return data;
}

std::optional<MediaTranslator::Language> MediaTranslator::Impl::GetLanguageFrom() const
{
    const std::lock_guard<std::mutex> lock(_languageFromMutex);
    return _languageFrom;
}

} // namespace RTC
