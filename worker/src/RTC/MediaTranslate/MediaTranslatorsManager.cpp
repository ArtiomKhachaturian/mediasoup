#define MS_CLASS "RTC::MediaTranslatorsManager"
#include "RTC/MediaTranslate/MediaTranslatorsManager.hpp"
#include "RTC/RtpPacketsCollector.hpp"
#include "RTC/MediaTranslate/RtpDepacketizer.hpp"
#include "RTC/MediaTranslate/RtpMediaFrameSerializer.hpp"
#include "RTC/MediaTranslate/OutputDevice.hpp"
#include "RTC/MediaTranslate/ProducerTranslator.hpp"
#include "RTC/MediaTranslate/ConsumerTranslator.hpp"
#include "RTC/RtpPacket.hpp"
#include "Logger.hpp"
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
    // impl. of
    void BeginWriteMediaPayload(uint32_t ssrc, bool isKeyFrame,
                                const RtpCodecMimeType& codecMimeType,
                                uint16_t rtpSequenceNumber,
                                uint32_t rtpTimestamp,
                                uint32_t rtpAbsSendtime,
                                uint32_t duration) final;
    void EndWriteMediaPayload(uint32_t ssrc, bool ok) final;
    bool Write(const void* buf, uint32_t len) final;
    int64_t GetPosition() const final { return _binaryWritePosition; }
    bool SetPosition(int64_t /*position*/) final { return false; }
    bool IsSeekable() const final { return false; }
    bool IsFileDevice() const final { return false; }
private:
    absl::flat_hash_set<OutputDevice*> _outputDevices;
    absl::flat_hash_map<RTC::RtpCodecMimeType::Subtype, std::unique_ptr<RtpDepacketizer>> _depacketizers;
    std::unique_ptr<RtpMediaFrameSerializer> _serializer;
    int64_t _binaryWritePosition = 0LL;
};

class MediaTranslatorsManager::Producer : public TranslatorUnit<ProducerObserver, ProducerTranslator>
{
public:
    Producer(const std::string& id, const std::weak_ptr<ProducerObserver>& observerRef);
    void AddOutputDevice(uint32_t audioSsrc, OutputDevice* outputDevice);
    void RemoveOutputDevice(uint32_t audioSsrc, OutputDevice* outputDevice);
    // impl. of ProducerTranslator
    std::shared_ptr<RtpPacketsCollector> AddAudio(uint32_t audioSsrc) final;
    std::weak_ptr<RtpPacketsCollector> SetVideo(uint32_t videoSsrc, uint32_t associatedAudioSsrc) final;
    void RemoveAudio(uint32_t audioSsrc) final;
    void RemoveVideo(uint32_t videoSsrc) final;
    void SetLanguage(const std::optional<MediaLanguage>& language) final;
    std::optional<MediaLanguage> GetLanguage() const final;
    void SetSerializer(uint32_t audioSsrc, std::unique_ptr<RtpMediaFrameSerializer> serializer) final;
private:
    absl::flat_hash_map<uint32_t, std::shared_ptr<MediaPacketsCollector>> _audio;
    absl::flat_hash_map<uint32_t, std::weak_ptr<MediaPacketsCollector>> _video;
    // input language
    std::optional<MediaLanguage> _language = DefaultInputMediaLanguage();
    mutable std::mutex _languageMutex;
};

class MediaTranslatorsManager::Consumer : public TranslatorUnit<ConsumerObserver, ConsumerTranslator>
{
public:
    Consumer(const std::string& id, const std::weak_ptr<ConsumerObserver>& observerRef);
    // impl. of ConsumerObserver
    void SetLanguage(MediaLanguage language) final;
    MediaLanguage GetLanguage() const final { return _language.load(std::memory_order_relaxed); }
    void SetVoice(MediaVoice voice) final;
    MediaVoice GetVoice() const final { return _voice.load(std::memory_order_relaxed); }
    uint64_t Bind(uint32_t producerAudioSsrc, const std::string& producerId,
                  const std::shared_ptr<RtpPacketsCollector>& output) final;
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
};

class MediaTranslatorsManager::ConsumerObserver
{
public:
    virtual ~ConsumerObserver() = default;
    virtual void OnConsumerLanguageChanged(const std::string& consumerId) = 0;
    virtual void OnConsumerVoiceChanged(const std::string& consumerId) = 0;
    virtual uint64_t BindToProducer(const std::string& consumerId,
                                    uint32_t producerAudioSsrc, const std::string& producerId,
                                    const std::shared_ptr<RtpPacketsCollector>& output) = 0;
    virtual void UnBindFromProducer(const std::string& consumerId, uint64_t bindId) = 0;
};

class MediaTranslatorsManager::Impl : public std::enable_shared_from_this<Impl>,
                                      public ProducerObserver, public ConsumerObserver
{
public:
    Impl(const std::string& serviceUri, const std::string& serviceUser, const std::string& servicePassword);
    std::shared_ptr<ProducerTranslator> RegisterProducer(const std::string& producerId);
    std::shared_ptr<ProducerTranslator> GetRegisteredProducer(const std::string& producerId) const;
    void UnegisterProducer(const std::string& producerId);
    std::shared_ptr<ConsumerTranslator> RegisterConsumer(const std::string& consumerId);
    std::shared_ptr<ConsumerTranslator> GetRegisteredConsumer(const std::string& consumerId) const;
    void UnegisterConsumer(const std::string& consumerId);
    // impl. of ProducerObserver
    void OnProducerLanguageChanged(const std::string& producerId) final;
    // impl. of ConsumerObserver
    void OnConsumerLanguageChanged(const std::string& consumerId) final;
    void OnConsumerVoiceChanged(const std::string& consumerId) final;
    uint64_t BindToProducer(const std::string& consumerId,
                            uint32_t producerAudioSsrc, const std::string& producerId,
                            const std::shared_ptr<RtpPacketsCollector>& output) final;
    void UnBindFromProducer(const std::string& consumerId, uint64_t bindId) final;
private:
    const std::string _serviceUri;
    const std::string _serviceUser;
    const std::string _servicePassword;
    absl::flat_hash_map<std::string, std::shared_ptr<Producer>> _producers;
    absl::flat_hash_map<std::string, std::shared_ptr<Consumer>> _consumers;
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

std::shared_ptr<ProducerTranslator> MediaTranslatorsManager::RegisterProducer(const std::string& producerId)
{
    return _impl->RegisterProducer(producerId);
}

std::shared_ptr<ProducerTranslator> MediaTranslatorsManager::GetRegisteredProducer(const std::string& producerId) const
{
    return _impl->GetRegisteredProducer(producerId);
}

void MediaTranslatorsManager::UnegisterProducer(const std::string& producerId)
{
    _impl->UnegisterProducer(producerId);
}

std::shared_ptr<ConsumerTranslator> MediaTranslatorsManager::RegisterConsumer(const std::string& consumerId)
{
    return _impl->RegisterConsumer(consumerId);
}

std::shared_ptr<ConsumerTranslator> MediaTranslatorsManager::GetRegisteredConsumer(const std::string& consumerId) const
{
    return _impl->GetRegisteredConsumer(consumerId);
}

void MediaTranslatorsManager::UnegisterConsumer(const std::string& consumerId)
{
    _impl->UnegisterConsumer(consumerId);
}

MediaTranslatorsManager::Impl::Impl(const std::string& serviceUri,
                                    const std::string& serviceUser,
                                    const std::string& servicePassword)
    : _serviceUri(serviceUri)
    , _serviceUser(serviceUser)
    , _servicePassword(servicePassword)
{
    MS_ASSERT(!_serviceUri.empty(), "service URI must be not empty");
}

std::shared_ptr<ProducerTranslator> MediaTranslatorsManager::Impl::RegisterProducer(const std::string& producerId)
{
    if (!producerId.empty()) {
        const auto it = _producers.find(producerId);
        if (it == _producers.end()) {
            auto producer = std::make_shared<Producer>(producerId, weak_from_this());
            _producers[producerId] = producer;
            return producer;
        }
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<ProducerTranslator> MediaTranslatorsManager::Impl::GetRegisteredProducer(const std::string& producerId) const
{
    if (!producerId.empty()) {
        const auto it = _producers.find(producerId);
        if (it != _producers.end()) {
            return it->second;
        }
    }
    return nullptr;
}

void MediaTranslatorsManager::Impl::UnegisterProducer(const std::string& producerId)
{
    if (!producerId.empty()) {
        const auto it = _producers.find(producerId);
        if (it != _producers.end()) {
            _producers.erase(it);
        }
    }
}

std::shared_ptr<ConsumerTranslator> MediaTranslatorsManager::Impl::RegisterConsumer(const std::string& consumerId)
{
    if (!consumerId.empty()) {
        const auto it = _consumers.find(consumerId);
        if (it == _consumers.end()) {
            auto consumer = std::make_shared<Consumer>(consumerId, weak_from_this());
            _consumers[consumerId] = consumer;
            return consumer;
        }
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<ConsumerTranslator> MediaTranslatorsManager::Impl::GetRegisteredConsumer(const std::string& consumerId) const
{
    if (!consumerId.empty()) {
        const auto it = _consumers.find(consumerId);
        if (it != _consumers.end()) {
            return it->second;
        }
    }
    return nullptr;
}

void MediaTranslatorsManager::Impl::UnegisterConsumer(const std::string& consumerId)
{
    if (!consumerId.empty()) {
        const auto it = _consumers.find(consumerId);
        if (it != _consumers.end()) {
            _consumers.erase(it);
        }
    }
}

void MediaTranslatorsManager::Impl::OnProducerLanguageChanged(const std::string& producerId)
{
    
}

void MediaTranslatorsManager::Impl::OnConsumerLanguageChanged(const std::string& consumerId)
{
    
}

void MediaTranslatorsManager::Impl::OnConsumerVoiceChanged(const std::string& consumerId)
{
    
}

uint64_t MediaTranslatorsManager::Impl::BindToProducer(const std::string& consumerId,
                                                       uint32_t producerAudioSsrc,
                                                       const std::string& producerId,
                                                       const std::shared_ptr<RtpPacketsCollector>& output)
{
    if (output && !consumerId.empty() && !producerId.empty()) {
        
    }
    return 0ULL;
}

void MediaTranslatorsManager::Impl::UnBindFromProducer(const std::string& consumerId, uint64_t bindId)
{
    if (bindId && !consumerId.empty()) {
        
    }
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

MediaTranslatorsManager::Producer::Producer(const std::string& id,
                                            const std::weak_ptr<ProducerObserver>& observerRef)
    : TranslatorUnit<ProducerObserver, ProducerTranslator>(id, observerRef)
{
}

void MediaTranslatorsManager::Producer::AddOutputDevice(uint32_t audioSsrc, OutputDevice* outputDevice)
{
    if (outputDevice) {
        const auto ita = _audio.find(audioSsrc);
        if (ita != _audio.end()) {
            ita->second->AddOutputDevice(outputDevice);
        }
    }
}

void MediaTranslatorsManager::Producer::RemoveOutputDevice(uint32_t audioSsrc, OutputDevice* outputDevice)
{
    if (outputDevice) {
        const auto ita = _audio.find(audioSsrc);
        if (ita != _audio.end()) {
            ita->second->RemoveOutputDevice(outputDevice);
        }
    }
}

std::shared_ptr<RtpPacketsCollector> MediaTranslatorsManager::Producer::AddAudio(uint32_t audioSsrc)
{
    const auto ita = _audio.find(audioSsrc);
    if (ita == _audio.end()) {
        auto collector = std::make_shared<MediaPacketsCollector>();
        _audio[audioSsrc] = collector;
        return collector;
    }
    return ita->second;
}

std::weak_ptr<RtpPacketsCollector> MediaTranslatorsManager::Producer::SetVideo(uint32_t videoSsrc,
                                                                               uint32_t associatedAudioSsrc)
{
    
}

void MediaTranslatorsManager::Producer::RemoveAudio(uint32_t audioSsrc)
{
    auto ita = _audio.find(audioSsrc);
    if (ita != _audio.end()) {
        for (auto itv = _video.begin(); itv != _video.end();) {
            if (itv->second.lock() == ita->second) {
                _video.erase(itv++);
            }
            else {
                ++itv;
            }
        }
        _audio.erase(ita);
    }
}

void MediaTranslatorsManager::Producer::RemoveVideo(uint32_t videoSsrc)
{
    auto itv = _video.find(videoSsrc);
    if (itv != _video.end()) {
        _video.erase(itv);
    }
}

void MediaTranslatorsManager::Producer::SetLanguage(const std::optional<MediaLanguage>& language)
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

std::optional<MediaLanguage> MediaTranslatorsManager::Producer::GetLanguage() const
{
    const std::lock_guard<std::mutex> lock(_languageMutex);
    return _language;
}

void MediaTranslatorsManager::Producer::SetSerializer(uint32_t audioSsrc,
                                                      std::unique_ptr<RtpMediaFrameSerializer> serializer)
{
    if (audioSsrc) {
        const auto ita = _audio.find(audioSsrc);
        if (ita != _audio.end()) {
            ita->second->SetSerializer(std::move(serializer));
        }
    }
}

MediaTranslatorsManager::Consumer::Consumer(const std::string& id,
                                            const std::weak_ptr<ConsumerObserver>& observerRef)
    : TranslatorUnit<ConsumerObserver, ConsumerTranslator>(id, observerRef)
{
}

void MediaTranslatorsManager::Consumer::SetLanguage(MediaLanguage language)
{
    if (language != _language.exchange(language)) {
        if (const auto observer = _observerRef.lock()) {
            observer->OnConsumerLanguageChanged(GetId());
        }
    }
}

void MediaTranslatorsManager::Consumer::SetVoice(MediaVoice voice)
{
    if (voice != _voice.exchange(voice)) {
        if (const auto observer = _observerRef.lock()) {
            observer->OnConsumerVoiceChanged(GetId());
        }
    }
}

uint64_t MediaTranslatorsManager::Consumer::Bind(uint32_t producerAudioSsrc,
                                                 const std::string& producerId,
                                                 const std::shared_ptr<RtpPacketsCollector>& output)
{
    if (output && !producerId.empty()) {
        if (const auto observer = _observerRef.lock()) {
            return observer->BindToProducer(GetId(), producerAudioSsrc, producerId, output);
        }
    }
    return 0ULL;
}

void MediaTranslatorsManager::Consumer::UnBind(uint64_t bindId)
{
    if (bindId) {
        if (const auto observer = _observerRef.lock()) {
            observer->UnBindFromProducer(GetId(), bindId);
        }
    }
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
