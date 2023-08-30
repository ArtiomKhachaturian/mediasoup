#define MS_CLASS "RTC::TranslatorEndPoint"
#include "RTC/MediaTranslate/Details/TranslatorEndPoint.hpp"
#include "RTC/MediaTranslate/Websocket.hpp"
#include "RTC/MediaTranslate/OutputDevice.hpp"
#include "RTC/MediaTranslate/WebsocketListener.hpp"
#include "RTC/MediaTranslate/ProducerTranslator.hpp"
#include "RTC/MediaTranslate/ConsumerTranslator.hpp"
#include "ProtectedObj.hpp"
#include "Logger.hpp"

namespace RTC
{

class TranslatorEndPoint::Impl : public WebsocketListener, private OutputDevice
{
public:
    Impl(uint32_t audioSsrc,
         const std::weak_ptr<Websocket>& websocketRef,
         const std::weak_ptr<ProducerTranslator>& producerRef,
         const std::weak_ptr<ConsumerTranslator>& consumerRef);
    ~Impl() final { FinalizeMediaInput(); }
    void FinalizeMediaInput();
    void SetOutput(RtpPacketsCollector* output);
    uint32_t GetAudioSsrc() const { return _audioSsrc; }
    const std::string& GetProducerId() const { return _producerId; }
    const std::string& GetConsumerId() const { return _consumerId; }
    bool IsConnected() const { return _connected.load(std::memory_order_relaxed); }
    bool SendTranslationChanges();
    // impl. of WebsocketListener
    void OnStateChanged(uint64_t socketId, WebsocketState state) final;
    void OnTextMessageReceived(uint64_t socketId, std::string message) final;
    void OnBinaryMessageReceved(uint64_t socketId, const std::shared_ptr<MemoryBuffer>& message) final;
private:
    template<class TTranslatorUnit>
    static std::string GetId(const std::weak_ptr<TTranslatorUnit>& unit);
    void InitializeMediaInput();
    bool WriteJson(const nlohmann::json& data);
    std::optional<TranslationPack> GetTranslationPack() const;
    bool IsProducerMediaAllowed() const;
    bool IsConsumerMediaAllowed() const;
    // impl. of OutputDevice
    void BeginWriteMediaPayload(uint32_t ssrc, bool isKeyFrame,
                                const RtpCodecMimeType& codecMimeType,
                                uint16_t rtpSequenceNumber,
                                uint32_t rtpTimestamp,
                                uint32_t rtpAbsSendtime,
                                uint32_t duration) final;
    void EndWriteMediaPayload(uint32_t ssrc, bool ok) final;
    bool Write(const void* buf, uint32_t len) final;
    int64_t GetPosition() const final { return 0LL; }
private:
    const uint32_t _audioSsrc;
    const std::weak_ptr<Websocket> _websocketRef;
    const std::weak_ptr<ProducerTranslator> _producerRef;
    const std::weak_ptr<ConsumerTranslator> _consumerRef;
    const std::string _producerId;
    const std::string _consumerId;
    std::atomic_bool _connected = false;
    ProtectedObj<RtpPacketsCollector*> _output;
};

TranslatorEndPoint::TranslatorEndPoint(uint32_t audioSsrc,
                                       const std::weak_ptr<ProducerTranslator>& producerRef,
                                       const std::weak_ptr<ConsumerTranslator>& consumerRef,
                                       const std::string& serviceUri,
                                       const std::string& serviceUser,
                                       const std::string& servicePassword)
    : _websocket(std::make_shared<Websocket>(serviceUri, serviceUser, servicePassword))
    , _impl(std::make_shared<Impl>(audioSsrc, _websocket, producerRef, consumerRef))
{
    _websocket->SetListener(_impl);
}

TranslatorEndPoint::~TranslatorEndPoint()
{
    SetOutput(nullptr);
    Close();
}

uint32_t TranslatorEndPoint::GetAudioSsrc() const
{
    return _impl->GetAudioSsrc();
}

const std::string& TranslatorEndPoint::GetProducerId() const
{
    return _impl->GetProducerId();
}

const std::string& TranslatorEndPoint::GetConsumerId() const
{
    return _impl->GetConsumerId();
}

bool TranslatorEndPoint::Open(const std::string& userAgent)
{
    return _websocket->Open(userAgent);
}

void TranslatorEndPoint::Close()
{
    _websocket->Close();
    _impl->FinalizeMediaInput();
}

void TranslatorEndPoint::SetOutput(RtpPacketsCollector* output)
{
    _impl->SetOutput(output);
}

void TranslatorEndPoint::SendTranslationChanges()
{
    _impl->SendTranslationChanges();
}

TranslatorEndPoint::Impl::Impl(uint32_t audioSsrc,
                               const std::weak_ptr<Websocket>& websocketRef,
                               const std::weak_ptr<ProducerTranslator>& producerRef,
                               const std::weak_ptr<ConsumerTranslator>& consumerRef)
    : _audioSsrc(audioSsrc)
    , _websocketRef(websocketRef)
    , _producerRef(producerRef)
    , _consumerRef(consumerRef)
    , _producerId(GetId(_producerRef))
    , _consumerId(GetId(_consumerRef))
{
    MS_ASSERT(_audioSsrc > 0U, "invalid audio SSRC");
    MS_ASSERT(!_producerId.empty(), "empty producer ID");
    MS_ASSERT(!_consumerId.empty(), "empty consumer ID");
}

void TranslatorEndPoint::Impl::FinalizeMediaInput()
{
    if (const auto producer = _producerRef.lock()) {
        producer->RemoveOutputDevice(GetAudioSsrc(), this);
    }
}

void TranslatorEndPoint::Impl::SetOutput(RtpPacketsCollector* output)
{
    LOCK_WRITE_PROTECTED_OBJ(_output);
    _output = output;
}

bool TranslatorEndPoint::Impl::SendTranslationChanges()
{
    bool ok = false;
    if (IsConnected()) {
        const auto tp = GetTranslationPack();
        if (tp.has_value()) {
            const auto jsonData = GetTargetLanguageCmd(tp.value());
            ok = WriteJson(jsonData);
            if (!ok) {
                MS_ERROR("Failed write language settings to translation service");
            }
        }
    }
    return ok;
}

void TranslatorEndPoint::Impl::OnStateChanged(uint64_t socketId, WebsocketState state)
{
    WebsocketListener::OnStateChanged(socketId, state);
    switch (state) {
        case WebsocketState::Connected:
            _connected = true;
            InitializeMediaInput();
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
    if (IsConsumerMediaAllowed()) {
        
    }
}

void TranslatorEndPoint::Impl::OnBinaryMessageReceved(uint64_t /*socketId*/,
                                                      const std::shared_ptr<MemoryBuffer>& /*message*/)
{
    if (IsConsumerMediaAllowed()) {
        
    }
}

template<class TTranslatorUnit>
std::string TranslatorEndPoint::Impl::GetId(const std::weak_ptr<TTranslatorUnit>& unitRef)
{
    if (const auto unit = unitRef.lock()) {
        return unit->GetId();
    }
    return std::string();
}

void TranslatorEndPoint::Impl::InitializeMediaInput()
{
    if (const auto producer = _producerRef.lock()) {
        if (SendTranslationChanges()) {
            if (!producer->AddOutputDevice(GetAudioSsrc(), this)) {
                // TODO: log error
            }
        }
    }
}

bool TranslatorEndPoint::Impl::WriteJson(const nlohmann::json& data)
{
    if (const auto websocket = _websocketRef.lock()) {
        return websocket->WriteText(to_string(data));
    }
    return false;
}

std::optional<TranslationPack> TranslatorEndPoint::Impl::GetTranslationPack() const
{
    if (const auto producer = _producerRef.lock()) {
        if (const auto consumer = _consumerRef.lock()) {
            return std::make_optional<TranslationPack>(consumer->GetLanguage(),
                                                       consumer->GetVoice(),
                                                       producer->GetLanguage());
        }
    }
    return std::nullopt;
}

bool TranslatorEndPoint::Impl::IsProducerMediaAllowed() const
{
    if (const auto producer = _producerRef.lock()) {
        return !producer->IsPaused();
    }
    return false;
}

bool TranslatorEndPoint::Impl::IsConsumerMediaAllowed() const
{
    if (const auto consumer = _consumerRef.lock()) {
        return !consumer->IsPaused();
    }
    return false;
}

void TranslatorEndPoint::Impl::BeginWriteMediaPayload(uint32_t ssrc, bool isKeyFrame,
                                                      const RtpCodecMimeType& codecMimeType,
                                                      uint16_t rtpSequenceNumber,
                                                      uint32_t rtpTimestamp,
                                                      uint32_t rtpAbsSendtime,
                                                      uint32_t duration)
{
    if (IsConnected()) {
        // TODO: send JSON command
    }
}

void TranslatorEndPoint::Impl::EndWriteMediaPayload(uint32_t ssrc, bool ok)
{
    if (IsConnected()) {
        // TODO: send JSON command
    }
}

bool TranslatorEndPoint::Impl::Write(const void* buf, uint32_t len)
{
    bool ok = false;
    if (buf && len && IsConnected()) {
        if (IsProducerMediaAllowed()) {
            if (const auto websocket = _websocketRef.lock()) {
                ok = websocket->Write(buf, len);
            }
        }
        else {
            ok = true; // ignore input media traffic
        }
    }
    return ok;
}

} // namespace RTC
