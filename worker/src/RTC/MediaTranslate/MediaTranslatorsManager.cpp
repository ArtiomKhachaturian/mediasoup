#define MS_CLASS "RTC::MediaTranslatorsManager"
#include "RTC/MediaTranslate/MediaTranslatorsManager.hpp"
#include "RTC/RtpPacketsCollector.hpp"
#include "RTC/MediaTranslate/RtpDepacketizer.hpp"
#include "RTC/MediaTranslate/RtpMediaFrameSerializer.hpp"
#include "RTC/MediaTranslate/OutputDevice.hpp"
#include "RTC/MediaTranslate/ProducerTranslator.hpp"
#include "RTC/MediaTranslate/ConsumerTranslator.hpp"
#include "RTC/MediaTranslate/Details/MediaPacketsSink.hpp"
#include "RTC/MediaTranslate/Details/TranslatorEndPoint.hpp"
#include "RTC/RtpPacket.hpp"
#include "RTC/Producer.hpp"
#include "RTC/Consumer.hpp"
#include "RTC/RtpStream.hpp"
#include "Logger.hpp"
#include "Utils.hpp"


namespace {

template<class TObserver, class... TInterfaces>
class TranslatorUnitImpl : public TInterfaces...
{
public:
    // impl. of TInterface
    const std::string& GetId() const final { return _id; }
protected:
    TranslatorUnitImpl(const std::string& id, const std::weak_ptr<TObserver>& observerRef);
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

class MediaTranslatorsManager::ProducerTranslatorImpl : public TranslatorUnitImpl<ProducerObserver, ProducerTranslator, RtpPacketsCollector>
{
public:
    ProducerTranslatorImpl(const std::string& id, const std::weak_ptr<ProducerObserver>& observerRef);
    bool HasAudio(uint32_t audioSsrc) const;
    // impl. of RtpPacketsCollector
    void AddPacket(const RtpCodecMimeType& mimeType, const RtpPacket* packet) final;
    // impl. of ProducerTranslator
    bool AddOutputDevice(uint32_t audioSsrc, OutputDevice* outputDevice) final;
    bool RemoveOutputDevice(uint32_t audioSsrc, OutputDevice* outputDevice) final;
    void SetLanguage(const std::optional<MediaLanguage>& language = std::nullopt) final;
    std::optional<MediaLanguage> GetLanguage() const final;
    bool SetSerializer(uint32_t audioSsrc, std::unique_ptr<RtpMediaFrameSerializer> serializer) final;
    RtpPacketsCollector* AddAudio(uint32_t audioSsrc) final;
    RtpPacketsCollector* SetVideo(uint32_t videoSsrc, uint32_t associatedAudioSsrc) final;
    bool RemoveAudio(uint32_t audioSsrc) final;
    bool RemoveVideo(uint32_t videoSsrc) final;
protected:
    void OnPauseChanged(bool pause) final;
private:
    void AddAudioPacket(const RtpCodecMimeType& mimeType, const RtpPacket* packet);
    void AddVideoPacket(const RtpCodecMimeType& mimeType, const RtpPacket* packet);
private:
    absl::flat_hash_map<uint32_t, std::unique_ptr<MediaPacketsSink>> _audio;
    absl::flat_hash_map<uint32_t, MediaPacketsSink*> _video;
    // input language
    ProtectedOptional<MediaLanguage> _language = DefaultInputMediaLanguage();
};

class MediaTranslatorsManager::ConsumerTranslatorImpl : public TranslatorUnitImpl<ConsumerObserver, ConsumerTranslator>
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
protected:
    void OnPauseChanged(bool pause) final;
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
    virtual void OnProducerPauseChanged(const std::string& producerId, bool pause) = 0;
    virtual void OnProducerLanguageChanged(const std::string& producerId) = 0;
    virtual void OnProducerAudioRemoved(const std::string& producerId, uint32_t audioSsrc) = 0;
};

class MediaTranslatorsManager::ConsumerObserver
{
public:
    virtual ~ConsumerObserver() = default;
    virtual void OnConsumerPauseChanged(const std::string& consumerId, bool pause) = 0;
    virtual void OnConsumerLanguageChanged(const std::string& consumerId) = 0;
    virtual void OnConsumerVoiceChanged(const std::string& consumerId) = 0;
    virtual uint64_t BindToProducer(const std::string& consumerId,
                                    uint32_t producerAudioSsrc, const std::string& producerId,
                                    RtpPacketsCollector* output) = 0;
    virtual void UnBindFromProducer(uint64_t bindId) = 0;
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
    void OnProducerPauseChanged(const std::string& producerId, bool pause) final;
    void OnProducerLanguageChanged(const std::string& producerId) final;
    void OnProducerAudioRemoved(const std::string& producerId, uint32_t audioSsrc) final;
    // impl. of ConsumerObserver
    void OnConsumerPauseChanged(const std::string& consumerId, bool pause) final;
    void OnConsumerLanguageChanged(const std::string& consumerId) final;
    void OnConsumerVoiceChanged(const std::string& consumerId) final;
    uint64_t BindToProducer(const std::string& consumerId,
                            uint32_t producerAudioSsrc, const std::string& producerId,
                            RtpPacketsCollector* output) final;
    void UnBindFromProducer(uint64_t bindId) final;
private:
    std::list<std::shared_ptr<TranslatorEndPoint>> FindEndPointsByProducerId(const std::string& producerId) const;
    std::list<std::shared_ptr<TranslatorEndPoint>> FindEndPointsConsumerId(const std::string& consumerId) const;
private:
    const std::string _serviceUri;
    const std::string _serviceUser;
    const std::string _servicePassword;
    absl::flat_hash_map<std::string, std::shared_ptr<ProducerTranslatorImpl>> _producers;
    absl::flat_hash_map<std::string, std::shared_ptr<ConsumerTranslatorImpl>> _consumers;
    absl::flat_hash_map<uint64_t, std::shared_ptr<TranslatorEndPoint>> _endPoints;
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

MediaTranslatorsManager::ProducerTranslatorImpl::ProducerTranslatorImpl(const std::string& id,
                                                                        const std::weak_ptr<ProducerObserver>& observerRef)
    : TranslatorUnitImpl<ProducerObserver, ProducerTranslator, RtpPacketsCollector>(id, observerRef)
{
}

bool MediaTranslatorsManager::ProducerTranslatorImpl::HasAudio(uint32_t audioSsrc) const
{
    const auto ita = _audio.find(audioSsrc);
    return ita != _audio.end();
}

void MediaTranslatorsManager::ProducerTranslatorImpl::AddPacket(const RtpCodecMimeType& mimeType,
                                                                const RtpPacket* packet)
{
    if (packet && !IsPaused()) {
        switch (mimeType.type) {
            case RtpCodecMimeType::Type::AUDIO:
                AddAudioPacket(mimeType, packet);
                break;
            case RtpCodecMimeType::Type::VIDEO:
                AddVideoPacket(mimeType, packet);
                break;
            default:
                MS_ASSERT(false, "invalid packet media type");
                break;
        }
    }
}

bool MediaTranslatorsManager::ProducerTranslatorImpl::AddOutputDevice(uint32_t audioSsrc,
                                                                      OutputDevice* outputDevice)
{
    if (outputDevice) {
        const auto ita = _audio.find(audioSsrc);
        if (ita != _audio.end()) {
            return ita->second->AddOutputDevice(outputDevice);
        }
    }
    return false;
}

bool MediaTranslatorsManager::ProducerTranslatorImpl::RemoveOutputDevice(uint32_t audioSsrc,
                                                                         OutputDevice* outputDevice)
{
    if (outputDevice) {
        const auto ita = _audio.find(audioSsrc);
        if (ita != _audio.end()) {
            return ita->second->RemoveOutputDevice(outputDevice);
        }
    }
    return false;
}

void MediaTranslatorsManager::ProducerTranslatorImpl::SetLanguage(const std::optional<MediaLanguage>& language)
{
    bool changed = false;
    {
        LOCK_WRITE_PROTECTED_OBJ(_language);
        if (_language.constRef() != language) {
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

RtpPacketsCollector* MediaTranslatorsManager::ProducerTranslatorImpl::AddAudio(uint32_t audioSsrc)
{
    const auto ita = _audio.find(audioSsrc);
    if (ita == _audio.end()) {
        _audio[audioSsrc] = std::make_unique<MediaPacketsSink>();
    }
    return this;
}

RtpPacketsCollector* MediaTranslatorsManager::ProducerTranslatorImpl::
    SetVideo(uint32_t videoSsrc, uint32_t associatedAudioSsrc)
{
    MS_ASSERT(false, "no yet implemented");
    return nullptr;
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

std::optional<MediaLanguage> MediaTranslatorsManager::ProducerTranslatorImpl::GetLanguage() const
{
    LOCK_READ_PROTECTED_OBJ(_language);
    return _language;
}

void MediaTranslatorsManager::ProducerTranslatorImpl::AddAudioPacket(const RtpCodecMimeType& mimeType,
                                                                     const RtpPacket* packet)
{
    if (packet) {
        const auto ita = _audio.find(packet->GetSsrc());
        if (ita != _audio.end()) {
            ita->second->AddPacket(mimeType, packet);
        }
    }
}

void MediaTranslatorsManager::ProducerTranslatorImpl::OnPauseChanged(bool pause)
{
    if (const auto observer = _observerRef.lock()) {
        observer->OnProducerPauseChanged(GetId(), pause);
    }
}

void MediaTranslatorsManager::ProducerTranslatorImpl::AddVideoPacket(const RtpCodecMimeType& mimeType,
                                                                     const RtpPacket* packet)
{
    if (packet) {
        const auto itv = _video.find(packet->GetSsrc());
        if (itv != _video.end()) {
            itv->second->AddPacket(mimeType, packet);
        }
    }
}

MediaTranslatorsManager::ConsumerTranslatorImpl::ConsumerTranslatorImpl(const std::string& id,
                                                                        const std::weak_ptr<ConsumerObserver>& observerRef)
    : TranslatorUnitImpl<ConsumerObserver, ConsumerTranslator>(id, observerRef)
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

void MediaTranslatorsManager::ConsumerTranslatorImpl::OnPauseChanged(bool pause)
{
    if (const auto observer = _observerRef.lock()) {
        observer->OnConsumerPauseChanged(GetId(), pause);
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

bool MediaTranslatorsManager::Impl::UnRegisterProducer(const std::string& producerId)
{
    if (!producerId.empty()) {
        const auto it = _producers.find(producerId);
        if (it != _producers.end()) {
            for (auto its = _endPoints.begin(); its != _endPoints.end();) {
                if (its->second->GetProducerId() == producerId) {
                    _endPoints.erase(its++);
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
            for (auto its = _endPoints.begin(); its != _endPoints.end();) {
                if (its->second->GetConsumerId() == consumerId) {
                    _endPoints.erase(its++);
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

void MediaTranslatorsManager::Impl::OnProducerPauseChanged(const std::string& producerId,
                                                           bool pause)
{
}

void MediaTranslatorsManager::Impl::OnProducerLanguageChanged(const std::string& producerId)
{
    for (const auto& endPoint : FindEndPointsByProducerId(producerId)) {
        endPoint->SendTranslationChanges();
    }
}

void MediaTranslatorsManager::Impl::OnProducerAudioRemoved(const std::string& producerId,
                                                           uint32_t audioSsrc)
{
    if (audioSsrc && !producerId.empty()) {
        for (auto it = _endPoints.begin(); it != _endPoints.end();) {
            if (audioSsrc == it->second->GetAudioSsrc() && it->second->GetProducerId() == producerId) {
                _endPoints.erase(it++);
            }
            else {
                ++it;
            }
        }
    }
}

void MediaTranslatorsManager::Impl::OnConsumerPauseChanged(const std::string& consumerId,
                                                           bool pause)
{
}

void MediaTranslatorsManager::Impl::OnConsumerLanguageChanged(const std::string& consumerId)
{
    for (const auto& endPoint : FindEndPointsConsumerId(consumerId)) {
        endPoint->SendTranslationChanges();
    }
}

void MediaTranslatorsManager::Impl::OnConsumerVoiceChanged(const std::string& consumerId)
{
    for (const auto& endPoint : FindEndPointsConsumerId(consumerId)) {
        endPoint->SendTranslationChanges();
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
        const auto it = _endPoints.find(id);
        if (it == _endPoints.end()) {
            const auto producer = _producers.find(producerId);
            if (producer != _producers.end()) {
                const auto consumer = _consumers.find(consumerId);
                if (consumer != _consumers.end()) {
                    if (producer->second->HasAudio(producerAudioSsrc)) {
                        auto endPoint = std::make_shared<TranslatorEndPoint>(producerAudioSsrc,
                                                                             producer->second,
                                                                             consumer->second,
                                                                             _serviceUri,
                                                                             _serviceUser,
                                                                             _servicePassword);
                        endPoint->SetOutput(output);
                        if (endPoint->Open()) {
                            _endPoints[bindId = id] = std::move(endPoint);
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
        const auto it = _endPoints.find(bindId);
        if (it != _endPoints.end()) {
            _endPoints.erase(it);
        }
    }
}

std::list<std::shared_ptr<TranslatorEndPoint>> MediaTranslatorsManager::Impl::
    FindEndPointsByProducerId(const std::string& producerId) const
{
    std::list<std::shared_ptr<TranslatorEndPoint>> endPoints;
    if (!producerId.empty()) {
        for (auto it = _endPoints.begin(); it != _endPoints.end(); ++it) {
            if (it->second->GetProducerId() == producerId) {
                endPoints.push_back(it->second);
            }
        }
    }
    return endPoints;
}

std::list<std::shared_ptr<TranslatorEndPoint>>
    MediaTranslatorsManager::Impl::FindEndPointsConsumerId(const std::string& consumerId) const
{
    std::list<std::shared_ptr<TranslatorEndPoint>> endPoints;
    if (!consumerId.empty()) {
        for (auto it = _endPoints.begin(); it != _endPoints.end(); ++it) {
            if (it->second->GetConsumerId() == consumerId) {
                endPoints.push_back(it->second);
            }
        }
    }
    return endPoints;
}

void TranslatorUnit::Pause(bool pause)
{
    if (pause != _paused.exchange(pause)) {
        OnPauseChanged(pause);
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

template<class TObserver, class... TInterfaces>
TranslatorUnitImpl<TObserver, TInterfaces...>::TranslatorUnitImpl(const std::string& id,
                                                                  const std::weak_ptr<TObserver>& observerRef)
    : _id(id)
    , _observerRef(observerRef)
{
}

}
