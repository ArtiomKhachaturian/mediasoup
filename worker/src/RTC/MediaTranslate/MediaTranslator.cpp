#include "RTC/MediaTranslate/MediaTranslator.hpp"
#include "RTC/MediaTranslate/RtpMediaFrameSerializer.hpp"
#include "RTC/MediaTranslate/RtpDepacketizer.hpp"
#include "RTC/MediaTranslate/Websocket.hpp"
#include "RTC/MediaTranslate/WebsocketListener.hpp"
#include "RTC/MediaTranslate/OutputDevice.hpp"
#include "RTC/MediaTranslate/MediaVoice.hpp"
#include "RTC/MediaTranslate/MediaLanguage.hpp"
#include "ProtectedObj.hpp"
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
    // impl. of WebsocketListener
    void OnStateChanged(uint64_t, WebsocketState state) final;
    // impl. of OutputDevice
    void Write(const std::shared_ptr<const MemoryBuffer>& buffer) final;
private:
    static bool WriteJson(Websocket* socket, const json& data);
    void ApplyFromLanguage(const std::optional<MediaLanguage>& language);
    bool WriteLanguageChanges();
    nlohmann::json GetLanguageData() const;
    std::optional<MediaLanguage> GetLanguageFrom() const;
    MediaLanguage GetLanguageTo() const { return _languageTo.load(std::memory_order_relaxed); }
    MediaVoice GetVoice() const { return _voice.load(std::memory_order_relaxed); }
private:
    // websocket ref
    ProtectedObj<Websocket*> _socket;
    // input language
    ProtectedOptional<MediaLanguage> _languageFrom = DefaultInputMediaLanguage();
    // output language
    std::atomic<MediaLanguage> _languageTo = DefaultOutputMediaLanguage();
    // voice
    std::atomic<MediaVoice> _voice = DefaultMediaVoice();
};

MediaTranslator::MediaTranslator(const std::string& serviceUri,
                                 std::unique_ptr<RtpMediaFrameSerializer> serializer,
                                 std::shared_ptr<RtpDepacketizer> depacketizer,
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

void MediaTranslator::AddPacket(const RtpCodecMimeType& mimeType, const RtpPacket* packet)
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
    LOCK_WRITE_PROTECTED_OBJ(_socket);
    if (const auto socket = _socket.constRef()) {
        socket->Close();
        if (andReset) {
            _socket = nullptr;
        }
    }
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

void MediaTranslator::Impl::Write(const std::shared_ptr<const MemoryBuffer>& buffer)
{
    if (buffer) {
        LOCK_READ_PROTECTED_OBJ(_socket);
        const auto socket = _socket.constRef();
        if (socket && WebsocketState::Connected == socket->GetState()) {
            if (!socket->WriteBinary(buffer)) {
                // TODO: log error
            }
        }
    }
}

void MediaTranslator::Impl::OnStateChanged(uint64_t socketId, WebsocketState state)
{
    WebsocketListener::OnStateChanged(socketId, state);
    switch (state) {
        case WebsocketState::Connected:
            WriteLanguageChanges();
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
        LOCK_WRITE_PROTECTED_OBJ(_languageFrom);
        if (_languageFrom.constRef() != language) {
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
    LOCK_READ_PROTECTED_OBJ(_socket);
    const auto socket = _socket.constRef();
    if (socket && WebsocketState::Connected == socket->GetState()) {
        if (!WriteJson(socket, GetLanguageData())) {
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
    LOCK_READ_PROTECTED_OBJ(_languageFrom);
    return _languageFrom;
}

} // namespace RTC
