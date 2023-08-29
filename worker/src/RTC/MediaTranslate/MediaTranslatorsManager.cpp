#define MS_CLASS "RTC::MediaTranslatorsManager"
#include "RTC/MediaTranslate/MediaTranslatorsManager.hpp"
#include "RTC/RtpPacketsCollector.hpp"
#include "RTC/MediaTranslate/RtpDepacketizer.hpp"
#include "RTC/MediaTranslate/RtpMediaFrameSerializer.hpp"
#include "RTC/MediaTranslate/OutputDevice.hpp"
#include "RTC/MediaTranslate/ProducerTranslator.hpp"
#include "RTC/MediaTranslate/ConsumerTranslator.hpp"
#include "RTC/MediaTranslate/Websocket.hpp"
#include "RTC/RtpPacket.hpp"
#include "RTC/Producer.hpp"
#include "RTC/Consumer.hpp"
#include "RTC/RtpStream.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include <absl/container/flat_hash_set.h>


namespace {

template<class TObserver, class TInterface>
class TranslatorUnit : public TInterface
{
public:
    // impl. of TInterface
    const std::string& GetId() const final { return _id; }
protected:
    TranslatorUnit(const std::string& id, const std::weak_ptr<TObserver>& observerRef);
protected:
    const std::string _id;
    const std::weak_ptr<TObserver> _observerRef;
};

inline bool IsAudioStream(const RTC::RtpStream* stream) {
    return stream && RTC::RtpCodecMimeType::Type::AUDIO == stream->GetMimeType().type;
}

inline bool IsVideoStream(const RTC::RtpStream* stream) {
    return stream && RTC::RtpCodecMimeType::Type::VIDEO == stream->GetMimeType().type;
}

}

namespace RTC
{

class MediaTranslatorsManager::MediaPacketsCollector : public RtpPacketsCollector,
                                                       private OutputDevice
{
public:
    MediaPacketsCollector() = default;
    void AddOutputDevice(OutputDevice* outputDevice);
    void RemoveOutputDevice(OutputDevice* outputDevice);
    void SetSerializer(std::unique_ptr<RtpMediaFrameSerializer> serializer);
    // impl. of RtpPacketsCollector
    void AddPacket(const RTC::RtpCodecMimeType& mimeType, const RtpPacket* packet) final;
private:
    RtpDepacketizer* GetDepackizer(const RTC::RtpCodecMimeType& mimeType);
    // impl. of OutputDevice
    void BeginWriteMediaPayload(uint32_t ssrc, bool isKeyFrame,
                                const RtpCodecMimeType& codecMimeType,
                                uint16_t rtpSequenceNumber,
                                uint32_t rtpTimestamp,
                                uint32_t rtpAbsSendtime,
                                uint32_t duration) final;
    void EndWriteMediaPayload(uint32_t ssrc, bool ok) final;
    bool Write(const void* buf, uint32_t len) final;
    int64_t GetPosition() const final { return _binaryWritePosition; }
private:
    absl::flat_hash_set<OutputDevice*> _outputDevices;
    absl::flat_hash_map<RTC::RtpCodecMimeType::Subtype, std::unique_ptr<RtpDepacketizer>> _depacketizers;
    std::unique_ptr<RtpMediaFrameSerializer> _serializer;
    int64_t _binaryWritePosition = 0LL;
};

class MediaTranslatorsManager::ProducerTranslatorImpl : public TranslatorUnit<ProducerObserver, ProducerTranslator>
{
public:
    ProducerTranslatorImpl(const std::string& id, const std::weak_ptr<ProducerObserver>& observerRef);
    void AddOutputDevice(uint32_t audioSsrc, OutputDevice* outputDevice);
    void RemoveOutputDevice(uint32_t audioSsrc, OutputDevice* outputDevice);
    bool HasAudio(uint32_t audioSsrc) const;
    // impl. of ProducerTranslator
    RtpPacketsCollector* AddAudio(uint32_t audioSsrc) final;
    RtpPacketsCollector* SetVideo(uint32_t videoSsrc, uint32_t associatedAudioSsrc) final;
    bool RemoveAudio(uint32_t audioSsrc) final;
    bool RemoveVideo(uint32_t videoSsrc) final;
    void SetLanguage(const std::optional<MediaLanguage>& language) final;
    std::optional<MediaLanguage> GetLanguage() const final;
    bool SetSerializer(uint32_t audioSsrc, std::unique_ptr<RtpMediaFrameSerializer> serializer) final;
private:
    absl::flat_hash_map<uint32_t, std::unique_ptr<MediaPacketsCollector>> _audio;
    absl::flat_hash_map<uint32_t, MediaPacketsCollector*> _video;
    // input language
    std::optional<MediaLanguage> _language = DefaultInputMediaLanguage();
    mutable std::mutex _languageMutex;
};

class MediaTranslatorsManager::ConsumerTranslatorImpl : public TranslatorUnit<ConsumerObserver, ConsumerTranslator>
{
public:
    ConsumerTranslatorImpl(const std::string& id, const std::weak_ptr<ConsumerObserver>& observerRef);
    // impl. of ConsumerObserver
    void SetLanguage(MediaLanguage language) final;
    MediaLanguage GetLanguage() const final { return _language.load(std::memory_order_relaxed); }
    void SetVoice(MediaVoice voice) final;
    MediaVoice GetVoice() const final { return _voice.load(std::memory_order_relaxed); }
    uint64_t Bind(uint32_t producerAudioSsrc, const std::string& producerId,
                  RtpPacketsCollector* output) final;
    void UnBind(uint64_t bindId) final;
private:
    // output language
    std::atomic<MediaLanguage> _language = DefaultOutputMediaLanguage();
    // voice
    std::atomic<MediaVoice> _voice = DefaultMediaVoice();
};

class MediaTranslatorsManager::ProducerObserver
{
public:
    virtual ~ProducerObserver() = default;
    virtual void OnProducerLanguageChanged(const std::string& producerId) = 0;
    virtual void OnProducerAudioRemoved(const std::string& producerId, uint32_t audioSsrc) = 0;
};

class MediaTranslatorsManager::ConsumerObserver
{
public:
    virtual ~ConsumerObserver() = default;
    virtual void OnConsumerLanguageChanged(const std::string& consumerId) = 0;
    virtual void OnConsumerVoiceChanged(const std::string& consumerId) = 0;
    virtual uint64_t BindToProducer(const std::string& consumerId,
                                    uint32_t producerAudioSsrc, const std::string& producerId,
                                    RtpPacketsCollector* output) = 0;
    virtual void UnBindFromProducer(uint64_t bindId) = 0;
};

class MediaTranslatorsManager::TranslatorService : public WebsocketListener,
                                                   private OutputDevice
{
public:
    TranslatorService(uint32_t audioSsrc,
                      const std::weak_ptr<Websocket>& websocketRef,
                      const std::weak_ptr<ProducerTranslatorImpl>& producerRef,
                      const std::weak_ptr<ConsumerTranslatorImpl>& customerRef);
    ~TranslatorService() override;
    bool Open();
    void Close();
    void SetOutput(RtpPacketsCollector* output);
    uint32_t GetAudioSsrc() const { return _audioSsrc; }
    bool HasReferenceToProducer(const std::string& id) const;
    bool HasReferenceToConsumer(const std::string& id) const;
    void SendTranslationChanges();
    // impl. of WebsocketListener
    void OnStateChanged(uint64_t, WebsocketState state) final;
private:
    static bool WriteJson(const std::shared_ptr<Websocket>& socket, const nlohmann::json& data);
    std::optional<TranslationPack> GetTranslationPack() const;
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
    const std::weak_ptr<ProducerTranslatorImpl> _producerRef;
    const std::weak_ptr<ConsumerTranslatorImpl> _customerRef;
    RtpPacketsCollector* _output;
    std::mutex _outputMutex;
};

class MediaTranslatorsManager::Impl : public std::enable_shared_from_this<Impl>,
                                      public ProducerObserver,
                                      public ConsumerObserver
{
public:
    Impl(const std::string& serviceUri, const std::string& serviceUser, const std::string& servicePassword);
    std::weak_ptr<ProducerTranslator> RegisterProducer(const std::string& producerId);
    std::weak_ptr<ProducerTranslator> GetRegisteredProducer(const std::string& producerId) const;
    bool UnRegisterProducer(const std::string& producerId);
    std::weak_ptr<ConsumerTranslator> RegisterConsumer(const std::string& consumerId);
    std::weak_ptr<ConsumerTranslator> GetRegisteredConsumer(const std::string& consumerId) const;
    bool UnRegisterConsumer(const std::string& consumerId);
    // impl. of ProducerObserver
    void OnProducerLanguageChanged(const std::string& producerId) final;
    void OnProducerAudioRemoved(const std::string& producerId, uint32_t audioSsrc) final;
    // impl. of ConsumerObserver
    void OnConsumerLanguageChanged(const std::string& consumerId) final;
    void OnConsumerVoiceChanged(const std::string& consumerId) final;
    uint64_t BindToProducer(const std::string& consumerId,
                            uint32_t producerAudioSsrc, const std::string& producerId,
                            RtpPacketsCollector* output) final;
    void UnBindFromProducer(uint64_t bindId) final;
private:
    std::list<std::shared_ptr<TranslatorService>> FindServicesByProducerId(const std::string& producerId) const;
    std::list<std::shared_ptr<TranslatorService>> FindServicesByConsumerId(const std::string& consumerId) const;
    void CloseService(uint64_t bindId, const std::shared_ptr<TranslatorService>& service);
private:
    const std::string _serviceUri;
    const std::string _serviceUser;
    const std::string _servicePassword;
    absl::flat_hash_map<std::string, std::shared_ptr<ProducerTranslatorImpl>> _producers;
    absl::flat_hash_map<std::string, std::shared_ptr<ConsumerTranslatorImpl>> _consumers;
    absl::flat_hash_map<uint64_t, std::shared_ptr<Websocket>> _sockets;
    absl::flat_hash_map<uint64_t, std::shared_ptr<TranslatorService>> _services;
};

MediaTranslatorsManager::MediaTranslatorsManager(const std::string& serviceUri,
                                                 const std::string& serviceUser,
                                                 const std::string& servicePassword)
    : _impl(std::make_shared<Impl>(serviceUri, serviceUser, servicePassword))
{
}

MediaTranslatorsManager::~MediaTranslatorsManager()
{
}

std::weak_ptr<ProducerTranslator> MediaTranslatorsManager::RegisterProducer(const std::string& producerId)
{
    return _impl->RegisterProducer(producerId);
}

std::weak_ptr<ProducerTranslator> MediaTranslatorsManager::RegisterProducer(const Producer* producer)
{
    if (producer) {
        return RegisterProducer(producer->id);
    }
    return {};
}

std::weak_ptr<ProducerTranslator> MediaTranslatorsManager::GetRegisteredProducer(const std::string& producerId) const
{
    return _impl->GetRegisteredProducer(producerId);
}

bool MediaTranslatorsManager::UnRegisterProducer(const std::string& producerId)
{
    return _impl->UnRegisterProducer(producerId);
}

bool MediaTranslatorsManager::UnRegisterProducer(const Producer* producer)
{
    return producer && UnRegisterProducer(producer->id);
}

std::weak_ptr<ConsumerTranslator> MediaTranslatorsManager::RegisterConsumer(const std::string& consumerId)
{
    return _impl->RegisterConsumer(consumerId);
}

std::weak_ptr<ConsumerTranslator> MediaTranslatorsManager::RegisterConsumer(const Consumer* consumer)
{
    if (consumer) {
        return RegisterConsumer(consumer->id);
    }
    return {};
}

std::weak_ptr<ConsumerTranslator> MediaTranslatorsManager::GetRegisteredConsumer(const std::string& consumerId) const
{
    return _impl->GetRegisteredConsumer(consumerId);
}

bool MediaTranslatorsManager::UnRegisterConsumer(const std::string& consumerId)
{
    return _impl->UnRegisterConsumer(consumerId);
}

bool MediaTranslatorsManager::UnRegisterConsumer(const Consumer* consumer)
{
    return consumer && UnRegisterConsumer(consumer->id);
}

void MediaTranslatorsManager::MediaPacketsCollector::AddOutputDevice(OutputDevice* outputDevice)
{
    if (outputDevice && !_outputDevices.count(outputDevice)) {
        _outputDevices.insert(outputDevice);
        if (_serializer && 1UL == _outputDevices.size()) {
            _serializer->SetOutputDevice(this);
        }
    }
}

void MediaTranslatorsManager::MediaPacketsCollector::RemoveOutputDevice(OutputDevice* outputDevice)
{
    if (outputDevice) {
        const auto it = _outputDevices.find(outputDevice);
        if (it != _outputDevices.end()) {
            _outputDevices.erase(it);
            if (_serializer && _outputDevices.empty()) {
                _serializer->SetOutputDevice(nullptr);
            }
        }
    }
}

void MediaTranslatorsManager::MediaPacketsCollector::SetSerializer(std::unique_ptr<RtpMediaFrameSerializer> serializer)
{
    if (serializer != _serializer) {
        if (_serializer) {
            _serializer->SetOutputDevice(nullptr);
        }
        _serializer = std::move(serializer);
        if (_serializer && !_outputDevices.empty()) {
            _serializer->SetOutputDevice(this);
        }
    }
}

void MediaTranslatorsManager::MediaPacketsCollector::AddPacket(const RTC::RtpCodecMimeType& mimeType,
                                                               const RtpPacket* packet)
{
    if (packet && _serializer && _serializer->GetOutputDevice()) {
        if (const auto depacketizer = GetDepackizer(mimeType)) {
            _serializer->Push(depacketizer->AddPacket(packet));
        }
    }
}

RtpDepacketizer* MediaTranslatorsManager::MediaPacketsCollector::
    GetDepackizer(const RTC::RtpCodecMimeType& mimeType)
{
    RtpDepacketizer* depacketizer = nullptr;
    auto it = _depacketizers.find(mimeType.subtype);
    if (it == _depacketizers.end()) {
        auto newDepacketizer = RtpDepacketizer::create(mimeType);
        depacketizer = newDepacketizer.get();
        _depacketizers[mimeType.subtype] = std::move(newDepacketizer);
        if (!depacketizer) {
            // TODO: log error
        }
    }
    else {
        depacketizer = it->second.get();
    }
    return depacketizer;
}

void MediaTranslatorsManager::MediaPacketsCollector::BeginWriteMediaPayload(uint32_t ssrc, bool isKeyFrame,
                                                                            const RtpCodecMimeType& codecMimeType,
                                                                            uint16_t rtpSequenceNumber,
                                                                            uint32_t rtpTimestamp,
                                                                            uint32_t rtpAbsSendtime,
                                                                            uint32_t duration)
{
    for (const auto outputDevice : _outputDevices) {
        outputDevice->BeginWriteMediaPayload(ssrc, isKeyFrame, codecMimeType,
                                             rtpSequenceNumber, rtpTimestamp,
                                             rtpAbsSendtime, duration);
    }
}

void MediaTranslatorsManager::MediaPacketsCollector::EndWriteMediaPayload(uint32_t ssrc, bool ok)
{
    for (const auto outputDevice : _outputDevices) {
        outputDevice->EndWriteMediaPayload(ssrc, ok);
    }
}

bool MediaTranslatorsManager::MediaPacketsCollector::Write(const void* buf, uint32_t len)
{
    if (buf && len && !_outputDevices.empty()) {
        size_t failures = 0UL;
        for (const auto outputDevice : _outputDevices) {
            if (!outputDevice->Write(buf, len)) {
                ++failures;
            }
        }
        return failures < _outputDevices.size();
    }
    return false;
}

MediaTranslatorsManager::ProducerTranslatorImpl::ProducerTranslatorImpl(const std::string& id,
                                                                        const std::weak_ptr<ProducerObserver>& observerRef)
    : TranslatorUnit<ProducerObserver, ProducerTranslator>(id, observerRef)
{
}

void MediaTranslatorsManager::ProducerTranslatorImpl::AddOutputDevice(uint32_t audioSsrc,
                                                                      OutputDevice* outputDevice)
{
    if (outputDevice) {
        const auto ita = _audio.find(audioSsrc);
        if (ita != _audio.end()) {
            ita->second->AddOutputDevice(outputDevice);
        }
    }
}

void MediaTranslatorsManager::ProducerTranslatorImpl::RemoveOutputDevice(uint32_t audioSsrc,
                                                                         OutputDevice* outputDevice)
{
    if (outputDevice) {
        const auto ita = _audio.find(audioSsrc);
        if (ita != _audio.end()) {
            ita->second->RemoveOutputDevice(outputDevice);
        }
    }
}

bool MediaTranslatorsManager::ProducerTranslatorImpl::HasAudio(uint32_t audioSsrc) const
{
    const auto ita = _audio.find(audioSsrc);
    return ita != _audio.end();
}

RtpPacketsCollector* MediaTranslatorsManager::ProducerTranslatorImpl::AddAudio(uint32_t audioSsrc)
{
    RtpPacketsCollector* collector = nullptr;
    const auto ita = _audio.find(audioSsrc);
    if (ita == _audio.end()) {
        auto newCollector = std::make_unique<MediaPacketsCollector>();
        collector = newCollector.get();
        _audio[audioSsrc] = std::move(newCollector);
    }
    else {
        collector = ita->second.get();
    }
    return collector;
}

RtpPacketsCollector* MediaTranslatorsManager::ProducerTranslatorImpl::
    SetVideo(uint32_t videoSsrc, uint32_t associatedAudioSsrc)
{
    return {};
}

bool MediaTranslatorsManager::ProducerTranslatorImpl::RemoveAudio(uint32_t audioSsrc)
{
    auto ita = _audio.find(audioSsrc);
    if (ita != _audio.end()) {
        for (auto itv = _video.begin(); itv != _video.end();) {
            if (itv->second == ita->second.get()) {
                _video.erase(itv++);
            }
            else {
                ++itv;
            }
        }
        _audio.erase(ita);
        if (const auto observer = _observerRef.lock()) {
            observer->OnProducerAudioRemoved(GetId(), audioSsrc);
        }
        return true;
    }
    return false;
}

bool MediaTranslatorsManager::ProducerTranslatorImpl::RemoveVideo(uint32_t videoSsrc)
{
    auto itv = _video.find(videoSsrc);
    if (itv != _video.end()) {
        _video.erase(itv);
        return true;
    }
    return false;
}

void MediaTranslatorsManager::ProducerTranslatorImpl::SetLanguage(const std::optional<MediaLanguage>& language)
{
    bool changed = false;
    {
        const std::lock_guard<std::mutex> lock(_languageMutex);
        if (_language != language) {
            _language = language;
            changed = true;
        }
    }
    if (changed) {
        if (const auto observer = _observerRef.lock()) {
            observer->OnProducerLanguageChanged(GetId());
        }
    }
}

std::optional<MediaLanguage> MediaTranslatorsManager::ProducerTranslatorImpl::GetLanguage() const
{
    const std::lock_guard<std::mutex> lock(_languageMutex);
    return _language;
}

bool MediaTranslatorsManager::ProducerTranslatorImpl::
    SetSerializer(uint32_t audioSsrc, std::unique_ptr<RtpMediaFrameSerializer> serializer)
{
    if (audioSsrc) {
        const auto ita = _audio.find(audioSsrc);
        if (ita != _audio.end()) {
            ita->second->SetSerializer(std::move(serializer));
        }
    }
    return false;
}

MediaTranslatorsManager::ConsumerTranslatorImpl::ConsumerTranslatorImpl(const std::string& id,
                                                                        const std::weak_ptr<ConsumerObserver>& observerRef)
    : TranslatorUnit<ConsumerObserver, ConsumerTranslator>(id, observerRef)
{
}

void MediaTranslatorsManager::ConsumerTranslatorImpl::SetLanguage(MediaLanguage language)
{
    if (language != _language.exchange(language)) {
        if (const auto observer = _observerRef.lock()) {
            observer->OnConsumerLanguageChanged(GetId());
        }
    }
}

void MediaTranslatorsManager::ConsumerTranslatorImpl::SetVoice(MediaVoice voice)
{
    if (voice != _voice.exchange(voice)) {
        if (const auto observer = _observerRef.lock()) {
            observer->OnConsumerVoiceChanged(GetId());
        }
    }
}

uint64_t MediaTranslatorsManager::ConsumerTranslatorImpl::Bind(uint32_t producerAudioSsrc,
                                                               const std::string& producerId,
                                                               RtpPacketsCollector* output)
{
    if (output && !producerId.empty()) {
        if (const auto observer = _observerRef.lock()) {
            return observer->BindToProducer(GetId(), producerAudioSsrc, producerId, output);
        }
    }
    return 0ULL;
}

void MediaTranslatorsManager::ConsumerTranslatorImpl::UnBind(uint64_t bindId)
{
    if (bindId) {
        if (const auto observer = _observerRef.lock()) {
            observer->UnBindFromProducer(bindId);
        }
    }
}

MediaTranslatorsManager::Impl::Impl(const std::string& serviceUri,
                                    const std::string& serviceUser,
                                    const std::string& servicePassword)
    : _serviceUri(serviceUri)
    , _serviceUser(serviceUser)
    , _servicePassword(servicePassword)
{
}

std::weak_ptr<ProducerTranslator> MediaTranslatorsManager::Impl::RegisterProducer(const std::string& producerId)
{
    if (!producerId.empty()) {
        const auto it = _producers.find(producerId);
        if (it == _producers.end()) {
            auto producer = std::make_shared<ProducerTranslatorImpl>(producerId, weak_from_this());
            _producers[producerId] = producer;
            return producer;
        }
        return it->second;
    }
    return {};
}

std::weak_ptr<ProducerTranslator> MediaTranslatorsManager::Impl::GetRegisteredProducer(const std::string& producerId) const
{
    if (!producerId.empty()) {
        const auto it = _producers.find(producerId);
        if (it != _producers.end()) {
            return it->second;
        }
    }
    return {};
}

MediaTranslatorsManager::TranslatorService::TranslatorService(uint32_t audioSsrc,
                                                              const std::weak_ptr<Websocket>& websocketRef,
                                                              const std::weak_ptr<ProducerTranslatorImpl>& producerRef,
                                                              const std::weak_ptr<ConsumerTranslatorImpl>& customerRef)
    : _audioSsrc(audioSsrc)
    , _websocketRef(websocketRef)
    , _producerRef(producerRef)
    , _customerRef(customerRef)
{
}

MediaTranslatorsManager::TranslatorService::~TranslatorService()
{
    Close();
    if (const auto producer = _producerRef.lock()) {
        producer->RemoveOutputDevice(GetAudioSsrc(), this);
    }
}

bool MediaTranslatorsManager::TranslatorService::Open()
{
    if (const auto websocket = _websocketRef.lock()) {
        return websocket->Open();
    }
    return false;
}

void MediaTranslatorsManager::TranslatorService::Close()
{
    if (const auto websocket = _websocketRef.lock()) {
        websocket->Close();
    }
}

void MediaTranslatorsManager::TranslatorService::SetOutput(RtpPacketsCollector* output)
{
    const std::lock_guard<std::mutex> lock(_outputMutex);
    _output = output;
}

bool MediaTranslatorsManager::TranslatorService::HasReferenceToProducer(const std::string& id) const
{
    if (!id.empty()) {
        const auto producer = _producerRef.lock();
        return producer && id == producer->GetId();
    }
    return false;
}

bool MediaTranslatorsManager::TranslatorService::HasReferenceToConsumer(const std::string& id) const
{
    if (!id.empty()) {
        const auto customer = _customerRef.lock();
        return customer && id == customer->GetId();
    }
    return false;
}

void MediaTranslatorsManager::TranslatorService::SendTranslationChanges()
{
    const auto websocket = _websocketRef.lock();
    if (websocket &&  WebsocketState::Connected == websocket->GetState()) {
        const auto tp = GetTranslationPack();
        if (tp.has_value()) {
            const auto jsonData = GetTargetLanguageCmd(tp.value());
            if (!WriteJson(websocket, jsonData)) {
                // TODO: log error
            }
        }
    }
}

void MediaTranslatorsManager::TranslatorService::OnStateChanged(uint64_t, WebsocketState state)
{
    switch (state) {
        case WebsocketState::Connected:
            if (const auto producer = _producerRef.lock()) {
                producer->AddOutputDevice(GetAudioSsrc(), this);
                SendTranslationChanges();
            }
            break;
        case WebsocketState::Disconnected:
            if (const auto producer = _producerRef.lock()) {
                producer->RemoveOutputDevice(GetAudioSsrc(), this);
            }
            break;
        default:
            break;
    }
}

bool MediaTranslatorsManager::TranslatorService::WriteJson(const std::shared_ptr<Websocket>& socket,
                                                           const nlohmann::json& data)
{
    return socket && socket->WriteText(to_string(data));
}

std::optional<TranslationPack> MediaTranslatorsManager::TranslatorService::GetTranslationPack() const
{
    if (const auto producer = _producerRef.lock()) {
        if (const auto customer = _customerRef.lock()) {
            return std::make_optional<TranslationPack>(customer->GetLanguage(),
                                                       customer->GetVoice(),
                                                       producer->GetLanguage());
        }
    }
    return std::nullopt;
}

void MediaTranslatorsManager::TranslatorService::BeginWriteMediaPayload(uint32_t ssrc, bool isKeyFrame,
                                                                        const RtpCodecMimeType& codecMimeType,
                                                                        uint16_t rtpSequenceNumber,
                                                                        uint32_t rtpTimestamp,
                                                                        uint32_t rtpAbsSendtime,
                                                                        uint32_t duration)
{
    // TODO: send JSON command
}

void MediaTranslatorsManager::TranslatorService::EndWriteMediaPayload(uint32_t ssrc, bool ok)
{
    // TODO: send JSON command
}

bool MediaTranslatorsManager::TranslatorService::Write(const void* buf, uint32_t len)
{
    if (buf && len) {
        const auto websocket = _websocketRef.lock();
        if (websocket &&  WebsocketState::Connected == websocket->GetState()) {
            return websocket->Write(buf, len);
        }
    }
    return false;
}

bool MediaTranslatorsManager::Impl::UnRegisterProducer(const std::string& producerId)
{
    if (!producerId.empty()) {
        const auto it = _producers.find(producerId);
        if (it != _producers.end()) {
            for (auto its = _services.begin(); its != _services.end();) {
                if (its->second->HasReferenceToProducer(producerId)) {
                    CloseService(its->first, its->second);
                    _services.erase(its++);
                }
                else {
                    ++its;
                }
            }
            _producers.erase(it);
            return true;
        }
    }
    return false;
}

std::weak_ptr<ConsumerTranslator> MediaTranslatorsManager::Impl::RegisterConsumer(const std::string& consumerId)
{
    if (!consumerId.empty()) {
        const auto it = _consumers.find(consumerId);
        if (it == _consumers.end()) {
            auto consumer = std::make_shared<ConsumerTranslatorImpl>(consumerId, weak_from_this());
            _consumers[consumerId] = consumer;
            return consumer;
        }
        return it->second;
    }
    return {};
}

std::weak_ptr<ConsumerTranslator> MediaTranslatorsManager::Impl::GetRegisteredConsumer(const std::string& consumerId) const
{
    if (!consumerId.empty()) {
        const auto it = _consumers.find(consumerId);
        if (it != _consumers.end()) {
            return it->second;
        }
    }
    return {};
}

bool MediaTranslatorsManager::Impl::UnRegisterConsumer(const std::string& consumerId)
{
    if (!consumerId.empty()) {
        const auto it = _consumers.find(consumerId);
        if (it != _consumers.end()) {
            for (auto its = _services.begin(); its != _services.end();) {
                if (its->second->HasReferenceToConsumer(consumerId)) {
                    CloseService(its->first, its->second);
                    _services.erase(its++);
                }
                else {
                    ++its;
                }
            }
            _consumers.erase(it);
            return true;
        }
    }
    return false;
}

void MediaTranslatorsManager::Impl::OnProducerLanguageChanged(const std::string& producerId)
{
    for (const auto& service : FindServicesByProducerId(producerId)) {
        service->SendTranslationChanges();
    }
}

void MediaTranslatorsManager::Impl::OnProducerAudioRemoved(const std::string& producerId,
                                                           uint32_t audioSsrc)
{
    if (audioSsrc && !producerId.empty()) {
        for (auto it = _services.begin(); it != _services.end();) {
            if (audioSsrc == it->second->GetAudioSsrc() && it->second->HasReferenceToProducer(producerId)) {
                CloseService(it->first, it->second);
                _services.erase(it++);
            }
            else {
                ++it;
            }
        }
    }
}

void MediaTranslatorsManager::Impl::OnConsumerLanguageChanged(const std::string& consumerId)
{
    for (const auto& service : FindServicesByConsumerId(consumerId)) {
        service->SendTranslationChanges();
    }
}

void MediaTranslatorsManager::Impl::OnConsumerVoiceChanged(const std::string& consumerId)
{
    for (const auto& service : FindServicesByConsumerId(consumerId)) {
        service->SendTranslationChanges();
    }
}

uint64_t MediaTranslatorsManager::Impl::BindToProducer(const std::string& consumerId,
                                                       uint32_t producerAudioSsrc,
                                                       const std::string& producerId,
                                                       RtpPacketsCollector* output)
{
    uint64_t bindId = 0LL;
    if (output && !consumerId.empty() && !producerId.empty()) {
        const auto id = Utils::HashCombine(consumerId, producerAudioSsrc, producerId);
        const auto it = _services.find(id);
        if (it == _services.end()) {
            const auto producer = _producers.find(producerId);
            if (producer != _producers.end()) {
                const auto consumer = _consumers.find(consumerId);
                if (consumer != _consumers.end()) {
                    if (producer->second->HasAudio(producerAudioSsrc)) {
                        auto websocket = std::make_shared<Websocket>(_serviceUri, _serviceUser, _servicePassword);
                        if (WebsocketState::Invalid != websocket->GetState()) {
                            auto service = std::make_shared<TranslatorService>(producerAudioSsrc,
                                                                               websocket,
                                                                               producer->second,
                                                                               consumer->second);
                            websocket->SetListener(service);
                            service->SetOutput(output);
                            if (service->Open()) {
                                bindId = id;
                                _sockets[id] = std::move(websocket);
                                _services[id] = std::move(service);
                            }
                            else {
                                service->SetOutput(nullptr);
                                websocket->SetListener(nullptr);
                                // TODO: report error
                            }
                        }
                        else {
                            // TODO: report error
                        }
                    }
                    else {
                        // TODO: report error
                    }
                }
                else {
                    // TODO: report error
                }
            }
            else {
                // TODO: report error
            }
        }
        else {
            bindId = id;
            it->second->SetOutput(output);
        }
    }
    return bindId;
}

void MediaTranslatorsManager::Impl::UnBindFromProducer(uint64_t bindId)
{
    if (bindId) {
        const auto it = _services.find(bindId);
        if (it != _services.end()) {
            CloseService(bindId, it->second);
            _services.erase(it);
        }
    }
}

std::list<std::shared_ptr<MediaTranslatorsManager::TranslatorService>>
    MediaTranslatorsManager::Impl::FindServicesByProducerId(const std::string& producerId) const
{
    if (!producerId.empty()) {
        std::list<std::shared_ptr<TranslatorService>> services;
        for (auto it = _services.begin(); it != _services.end(); ++it) {
            if (it->second->HasReferenceToProducer(producerId)) {
                services.push_back(it->second);
            }
        }
        return services;
    }
    return {};
}

std::list<std::shared_ptr<MediaTranslatorsManager::TranslatorService>>
    MediaTranslatorsManager::Impl::FindServicesByConsumerId(const std::string& consumerId) const
{
    if (!consumerId.empty()) {
        std::list<std::shared_ptr<TranslatorService>> services;
        for (auto it = _services.begin(); it != _services.end(); ++it) {
            if (it->second->HasReferenceToConsumer(consumerId)) {
                services.push_back(it->second);
            }
        }
        return services;
    }
    return {};
}

void MediaTranslatorsManager::Impl::CloseService(uint64_t bindId, const std::shared_ptr<TranslatorService>& service)
{
    if (service) {
        service->SetOutput(nullptr);
        service->Close();
    }
    const auto it = _sockets.find(bindId);
    if (it != _sockets.end()) {
        it->second->SetListener(nullptr);
        _sockets.erase(it);
    }
}

uint64_t ConsumerTranslator::Bind(const RtpStream* producerAudioStream, const Producer* producerId,
                                  RtpPacketsCollector* output)
{
    if (producerId && output && IsAudioStream(producerAudioStream)) {
        return Bind(producerAudioStream->GetSsrc(), producerId->id, output);
    }
    return 0ULL;
}

RtpPacketsCollector* ProducerTranslator::SetVideo(const RtpStream* videoStream,
                                                  const RtpStream* associatedAudioStream)
{
    if (IsVideoStream(videoStream) && IsAudioStream(associatedAudioStream)) {
        return SetVideo(videoStream->GetSsrc(), associatedAudioStream->GetSsrc());
    }
    return nullptr;
}

RtpPacketsCollector* ProducerTranslator::AddAudio(const RtpStream* audioStream)
{
    if (IsAudioStream(audioStream)) {
        return AddAudio(audioStream->GetSsrc());
    }
    return nullptr;
}

bool ProducerTranslator::RemoveAudio(const RtpStream* audioStream)
{
    return IsAudioStream(audioStream) && RemoveAudio(audioStream->GetSsrc());
}

bool ProducerTranslator::RemoveVideo(const RtpStream* videoStream)
{
    return IsVideoStream(videoStream) && RemoveVideo(videoStream->GetSsrc());
}

bool ProducerTranslator::SetSerializer(const RtpStream* audioStream,
                                       std::unique_ptr<RtpMediaFrameSerializer> serializer)
{
    return IsAudioStream(audioStream) && SetSerializer(audioStream->GetSsrc(), std::move(serializer));
}

} // namespace RTC

namespace {

template<class TObserver, class TInterface>
TranslatorUnit<TObserver, TInterface>::TranslatorUnit(const std::string& id,
                                                      const std::weak_ptr<TObserver>& observerRef)
    : _id(id)
    , _observerRef(observerRef)
{
}

}
