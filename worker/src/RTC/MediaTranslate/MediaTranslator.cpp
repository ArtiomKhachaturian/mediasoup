#include "RTC/MediaTranslate/MediaTranslator.hpp"
#include "RTC/MediaTranslate/RtpMediaFrameSerializer.hpp"
#include "RTC/MediaTranslate/RtpDepacketizer.hpp"
#include "RTC/MediaTranslate/Websocket.hpp"
#include "RTC/MediaTranslate/WebsocketListener.hpp"
#include "RTC/MediaTranslate/OutputDevice.hpp"
#include "RTC/MediaTranslate/MediaVoice.hpp"
#include "RTC/MediaTranslate/MediaLanguage.hpp"
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
    void SetFromLanguage(MediaLanguage language) { ApplyFromLanguage(language); }
    void SetFromLanguageAuto() { ApplyFromLanguage(std::nullopt); }
    void SetToLanguage(MediaLanguage language);
    void SetVoice(MediaVoice voice);
    int64_t GetMediaPosition() const { return _mediaPosition.load(std::memory_order_relaxed); }
    // impl. of WebsocketListener
    void OnStateChanged(uint64_t, WebsocketState state) final;
    // impl. of OutputDevice
    bool Write(const void* buf, uint32_t len) final;
    int64_t GetPosition() const final { return _mediaPosition.load(std::memory_order_relaxed); }
private:
    static bool WriteJson(Websocket* socket, const json& data);
    void ApplyFromLanguage(const std::optional<MediaLanguage>& language);
    bool WriteLanguageChanges();
    nlohmann::json GetLanguageData() const;
    std::optional<MediaLanguage> GetLanguageFrom() const;
    MediaLanguage GetLanguageTo() const { return _languageTo.load(std::memory_order_relaxed); }
    MediaVoice GetVoice() const { return _voice.load(std::memory_order_relaxed); }
    void ResetMedia() { _mediaPosition = 0LL; }
private:
    // websocket ref
    Websocket* _socket;
    std::mutex _socketMutex;
    // input language
    std::optional<MediaLanguage> _languageFrom = DefaultInputMediaLanguage();
    mutable std::mutex _languageFromMutex;
    // output language
    std::atomic<MediaLanguage> _languageTo = DefaultOutputMediaLanguage();
    // voice
    std::atomic<MediaVoice> _voice = DefaultMediaVoice();
    // media
    std::atomic<int64_t> _mediaPosition = 0LL;
};

MediaTranslator::MediaTranslator(const std::string& serviceUri,
                                 std::unique_ptr<RtpMediaFrameSerializer> serializer,
                                 std::unique_ptr<RtpDepacketizer> depacketizer,
                                 const std::string& user,
                                 const std::string& password)
    : _serializer(std::move(serializer))
    , _depacketizer(std::move(depacketizer))
    , _websocket(CreateWebsocket(serviceUri, user, password))
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
                                                         const RtpCodecMimeType& mimeType,
                                                         const std::string& user,
                                                         const std::string& password)
{
    if (!serviceUri.empty()) {
        if (auto serializer = RtpMediaFrameSerializer::create(mimeType)) {
            if (auto depacketizer = RtpDepacketizer::create(mimeType)) {
                auto translator = std::make_unique<MediaTranslator>(std::move(serviceUri),
                                                                    std::move(serializer),
                                                                    std::move(depacketizer),
                                                                    user,
                                                                    password);
                if (translator->_websocket) {
                    return translator;
                }
            }
        }
    }
    return nullptr;
}

bool MediaTranslator::OpenService()
{
    return _depacketizer && _serializer && _websocket && _websocket->Open();
}

void MediaTranslator::CloseService()
{
    if (_impl) {
        _impl->Close(false);
    }
}

void MediaTranslator::SetFromLanguage(MediaLanguage language)
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

void MediaTranslator::SetToLanguage(MediaLanguage language)
{
    if (_impl) {
        _impl->SetToLanguage(language);
    }
}

void MediaTranslator::SetVoice(MediaVoice voice)
{
    if (_impl) {
        _impl->SetVoice(voice);
    }
}

void MediaTranslator::AddPacket(const RTC::RtpCodecMimeType& mimeType, const RtpPacket* packet)
{
    if (packet && _depacketizer && _serializer && _websocket &&
        mimeType == _depacketizer->GetCodecMimeType() &&
        WebsocketState::Connected == _websocket->GetState()) {
        _serializer->Push(_depacketizer->AddPacket(packet));
    }
}

std::unique_ptr<Websocket> MediaTranslator::CreateWebsocket(const std::string& serviceUri,
                                                            const std::string& user,
                                                            const std::string& password)
{
    if (!serviceUri.empty()) {
        auto websocket = std::make_unique<Websocket>(serviceUri, user, password);
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

void MediaTranslator::Impl::SetToLanguage(MediaLanguage language)
{
    if (language != _languageTo.exchange(language)) {
        WriteLanguageChanges();
    }
}

void MediaTranslator::Impl::SetVoice(MediaVoice voice)
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

void MediaTranslator::Impl::OnStateChanged(uint64_t, WebsocketState state)
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

bool MediaTranslator::Impl::WriteJson(Websocket* socket, const json& data)
{
    return socket && socket->WriteText(to_string(data));
}

void MediaTranslator::Impl::ApplyFromLanguage(const std::optional<MediaLanguage>& language)
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

nlohmann::json MediaTranslator::Impl::GetLanguageData() const
{
    return GetTargetLanguageCmd(GetLanguageTo(), GetVoice(), GetLanguageFrom());
}

std::optional<MediaLanguage> MediaTranslator::Impl::GetLanguageFrom() const
{
    const std::lock_guard<std::mutex> lock(_languageFromMutex);
    return _languageFrom;
}

} // namespace RTC
