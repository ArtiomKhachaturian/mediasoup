#define MS_CLASS "RTC::MediaTranslatorsManager"
#include "RTC/MediaTranslate/MediaTranslatorsManager.hpp"
#include "RTC/RtpPacketsCollector.hpp"
#include "RTC/MediaTranslate/RtpMediaFrameSerializer.hpp"
#include "RTC/MediaTranslate/ProducerTranslator.hpp"
#include "RTC/MediaTranslate/ConsumerTranslator.hpp"
#include "RTC/MediaTranslate/TranslatorEndPoint.hpp"
#include "RTC/RtpPacket.hpp"
#include "RTC/Producer.hpp"
#include "RTC/Consumer.hpp"
#include "RTC/RtpStream.hpp"
#include "Logger.hpp"
#include "Utils.hpp"


namespace RTC
{

using ConsumerTranslatorsList = std::list<std::shared_ptr<ConsumerTranslator>>;
using TranslatorEndPointsList = std::list<std::shared_ptr<TranslatorEndPoint>>;

class MediaTranslatorsManager::Impl : public std::enable_shared_from_this<Impl>,
                                      public ProducerObserver,
                                      public ConsumerObserver
{
public:
    Impl(const std::string& serviceUri, const std::string& serviceUser, const std::string& servicePassword);
    // producers API
    bool Register(const Producer* producer);
    std::shared_ptr<ProducerTranslator> GetRegistered(const Producer* producer) const;
    std::shared_ptr<ProducerTranslator> GetRegisteredProducer(const std::string& id) const;
    bool UnRegister(const Producer* producer);
    void RegisterProducerStream(const std::string& id, const RtpStream* stream, uint32_t mappedSsrc);
    // consumers API
    bool Register(const Consumer* consumer, const std::string& producerId);
    std::shared_ptr<ConsumerTranslator> GetRegistered(const Consumer* consumer) const;
    std::shared_ptr<ConsumerTranslator> GetRegisteredConsumer(const std::string& id) const;
    ConsumerTranslatorsList GetAssociated(const std::shared_ptr<ProducerTranslator>& producer) const;
    ConsumerTranslatorsList GetAssociated(const std::string& producerId) const;
    bool UnRegister(const Consumer* consumer);
    // impl. of ProducerObserver
    void onProducerMediaRegistered(const std::string& producerId, bool audio,
                                   uint32_t ssrc, uint32_t mappedSsrc,
                                   bool registered) final;
    void OnProducerPauseChanged(const std::string& producerId, bool pause) final;
    void OnProducerLanguageChanged(const std::string& producerId) final;
    // impl. of ConsumerObserver
    void OnConsumerPauseChanged(const std::string& consumerId, bool pause) final;
    void OnConsumerLanguageChanged(const std::string& consumerId) final;
    void OnConsumerVoiceChanged(const std::string& consumerId) final;
    void OnConsumerEnabledChanged(const std::string& consumerId, bool enabled) final;
private:
    TranslatorEndPointsList FindEndPointsByProducerId(const std::string& producerId) const;
    TranslatorEndPointsList FindEndPointsConsumerId(const std::string& consumerId) const;
    void FetchEndPoint(uint32_t audioSsrc, const std::string& producerId);
    static void OpenEndPont(const std::shared_ptr<TranslatorEndPoint>& endPoint);
    static void RegisterStream(const std::shared_ptr<ProducerTranslator>& producerTranslator,
                               const RtpStream* stream, uint32_t mappedSsrc);
private:
    const std::string _serviceUri;
    const std::string _serviceUser;
    const std::string _servicePassword;
    absl::flat_hash_map<std::string, std::shared_ptr<ProducerTranslator>> _producerTranslators;
    absl::flat_hash_map<std::string, std::shared_ptr<ConsumerTranslator>> _consumerTranslators;
    absl::flat_hash_set<std::shared_ptr<TranslatorEndPoint>> _endPoints;
};

MediaTranslatorsManager::MediaTranslatorsManager(TransportListener* router,
                                                 const std::string& serviceUri,
                                                 const std::string& serviceUser,
                                                 const std::string& servicePassword)
    : _router(router)
    , _impl(std::make_shared<Impl>(serviceUri, serviceUser, servicePassword))
{
    MS_ASSERT(nullptr != _router, "router must be non-null");
}

MediaTranslatorsManager::~MediaTranslatorsManager()
{
}

std::weak_ptr<ProducerTranslatorSettings> MediaTranslatorsManager::GetTranslatorSettings(const Producer* producer) const
{
    return _impl->GetRegistered(producer);
}

std::weak_ptr<ConsumerTranslatorSettings> MediaTranslatorsManager::GetTranslatorSettings(const Consumer* consumer) const
{
    return _impl->GetRegistered(consumer);
}


void MediaTranslatorsManager::OnTransportNewProducer(Transport* transport, Producer* producer)
{
    _router->OnTransportNewProducer(transport, producer);
    _impl->Register(producer);
}

void MediaTranslatorsManager::OnTransportProducerClosed(Transport* transport, Producer* producer)
{
    _router->OnTransportProducerClosed(transport, producer);
    _impl->UnRegister(producer);
}

void MediaTranslatorsManager::OnTransportProducerPaused(Transport* transport, Producer* producer)
{
    _router->OnTransportProducerPaused(transport, producer);
    if (const auto translator = _impl->GetRegistered(producer)) {
        translator->Pause();
    }
}

void MediaTranslatorsManager::OnTransportProducerResumed(Transport* transport,
                                                         Producer* producer)
{
    _router->OnTransportProducerResumed(transport, producer);
    if (const auto translator = _impl->GetRegistered(producer)) {
        translator->Resume();
    }
}

void MediaTranslatorsManager::OnTransportProducerNewRtpStream(Transport* transport,
                                                              Producer* producer,
                                                              RtpStreamRecv* rtpStream,
                                                              uint32_t mappedSsrc)
{
    _router->OnTransportProducerNewRtpStream(transport, producer, rtpStream, mappedSsrc);
    if (producer && rtpStream) {
        _impl->RegisterProducerStream(producer->id, rtpStream, mappedSsrc);
    }
}

void MediaTranslatorsManager::OnTransportProducerRtpStreamScore(Transport* transport,
                                                                Producer* producer,
                                                                RtpStreamRecv* rtpStream,
                                                                uint8_t score, uint8_t previousScore)
{
    _router->OnTransportProducerRtpStreamScore(transport, producer, rtpStream, score, previousScore);
}

void MediaTranslatorsManager::OnTransportProducerRtcpSenderReport(Transport* transport,
                                                                  Producer* producer,
                                                                  RtpStreamRecv* rtpStream,
                                                                  bool first)
{
    _router->OnTransportProducerRtcpSenderReport(transport, producer, rtpStream, first);
}

void MediaTranslatorsManager::OnTransportProducerRtpPacketReceived(Transport* transport,
                                                                   Producer* producer,
                                                                   RtpPacket* packet)
{
    _router->OnTransportProducerRtpPacketReceived(transport, producer, packet);
    if (packet) {
        if (const auto producerTranslator = _impl->GetRegistered(producer)) {
            producerTranslator->AddPacket(packet);
        }
    }
}

void MediaTranslatorsManager::OnTransportNeedWorstRemoteFractionLost(Transport* transport,
                                                                     Producer* producer,
                                                                     uint32_t mappedSsrc,
                                                                     uint8_t& worstRemoteFractionLost)
{
    _router->OnTransportNeedWorstRemoteFractionLost(transport, producer,
                                                    mappedSsrc, worstRemoteFractionLost);
}

void MediaTranslatorsManager::OnTransportNewConsumer(Transport* transport, Consumer* consumer,
                                                     const std::string& producerId)
{
    _router->OnTransportNewConsumer(transport, consumer, producerId);
    _impl->Register(consumer, producerId);
}

void MediaTranslatorsManager::OnTransportConsumerClosed(Transport* transport,
                                                        Consumer* consumer)
{
    _router->OnTransportConsumerClosed(transport, consumer);
    if (_impl->UnRegister(consumer)) {
        
    }
}

void MediaTranslatorsManager::OnTransportConsumerProducerClosed(Transport* transport,
                                                                Consumer* consumer)
{
    _router->OnTransportConsumerProducerClosed(transport, consumer);
}

void MediaTranslatorsManager::OnTransportConsumerKeyFrameRequested(Transport* transport,
                                                                   Consumer* consumer,
                                                                   uint32_t mappedSsrc)
{
    _router->OnTransportConsumerKeyFrameRequested(transport, consumer, mappedSsrc);
}

void MediaTranslatorsManager::OnTransportNewDataProducer(Transport* transport,
                                                         DataProducer* dataProducer)
{
    _router->OnTransportNewDataProducer(transport, dataProducer);
}

void MediaTranslatorsManager::OnTransportDataProducerClosed(Transport* transport,
                                                            DataProducer* dataProducer)
{
    _router->OnTransportDataProducerClosed(transport, dataProducer);
}

void MediaTranslatorsManager::OnTransportDataProducerMessageReceived(Transport* transport,
                                                                     DataProducer* dataProducer,
                                                                     uint32_t ppid,
                                                                     const uint8_t* msg,
                                                                     size_t len)
{
    _router->OnTransportDataProducerMessageReceived(transport, dataProducer, ppid, msg, len);
}

void MediaTranslatorsManager::OnTransportNewDataConsumer(Transport* transport,
                                                         DataConsumer* dataConsumer,
                                                         const std::string& dataProducerId)
{
    _router->OnTransportNewDataConsumer(transport, dataConsumer, dataProducerId);
}

void MediaTranslatorsManager::OnTransportDataConsumerClosed(Transport* transport, DataConsumer* dataConsumer)
{
    _router->OnTransportDataConsumerClosed(transport, dataConsumer);
}

void MediaTranslatorsManager::OnTransportDataConsumerDataProducerClosed(Transport* transport,
                                                                        DataConsumer* dataConsumer)
{
    _router->OnTransportDataConsumerDataProducerClosed(transport, dataConsumer);
}

void MediaTranslatorsManager::OnTransportListenServerClosed(Transport* transport)
{
    _router->OnTransportListenServerClosed(transport);
}

MediaTranslatorsManager::Impl::Impl(const std::string& serviceUri,
                                    const std::string& serviceUser,
                                    const std::string& servicePassword)
    : _serviceUri(serviceUri)
    , _serviceUser(serviceUser)
    , _servicePassword(servicePassword)
{
}

bool MediaTranslatorsManager::Impl::Register(const Producer* producer)
{
    if (producer && !producer->id.empty()) {
        const auto it = _producerTranslators.find(producer->id);
        if (it == _producerTranslators.end()) {
            const auto producerTranslator = std::make_shared<ProducerTranslator>(producer->id, weak_from_this());
            _producerTranslators[producer->id] = producerTranslator;
            const auto& streams = producer->GetRtpStreams();
            for (auto it = streams.begin(); it != streams.end(); ++it) {
                RegisterStream(producerTranslator, it->first, it->second);
            }
        }
        return true;
    }
    return false;
}

std::shared_ptr<ProducerTranslator> MediaTranslatorsManager::Impl::GetRegistered(const Producer* producer) const
{
    return producer ? GetRegisteredProducer(producer->id) : nullptr;
}

std::shared_ptr<ProducerTranslator> MediaTranslatorsManager::Impl::GetRegisteredProducer(const std::string& id) const
{
    if (!id.empty()) {
        const auto it = _producerTranslators.find(id);
        if (it != _producerTranslators.end()) {
            return it->second;
        }
    }
    return nullptr;
}

bool MediaTranslatorsManager::Impl::UnRegister(const Producer* producer)
{
    if (producer && !producer->id.empty()) {
        const auto it = _producerTranslators.find(producer->id);
        if (it != _producerTranslators.end()) {
            _producerTranslators.erase(it);
            return true;
        }
    }
    return false;
}

void MediaTranslatorsManager::Impl::RegisterProducerStream(const std::string& id,
                                                           const RtpStream* stream,
                                                           uint32_t mappedSsrc)
{
    if (stream && mappedSsrc) {
        RegisterStream(GetRegisteredProducer(id), stream, mappedSsrc);
    }
}

bool MediaTranslatorsManager::Impl::Register(const Consumer* consumer, const std::string& producerId)
{
    if (consumer && consumer->IsTranslationRequired() && !consumer->id.empty()) {
        const auto it = _consumerTranslators.find(consumer->id);
        if (it == _consumerTranslators.end()) {
            _consumerTranslators[consumer->id] = std::make_shared<ConsumerTranslator>(consumer->id,
                                                                                      weak_from_this(),
                                                                                      producerId);
            if (const auto producerTranslator = GetRegisteredProducer(producerId)) {
                for (auto audioSsrc : producerTranslator->GetRegisteredAudio()) {
                    FetchEndPoint(audioSsrc, producerId);
                }
            }
        }
        return true;
    }
    return false;
}

std::shared_ptr<ConsumerTranslator> MediaTranslatorsManager::Impl::GetRegistered(const Consumer* consumer) const
{
    return consumer ? GetRegisteredConsumer(consumer->id) : nullptr;
}

std::shared_ptr<ConsumerTranslator> MediaTranslatorsManager::Impl::GetRegisteredConsumer(const std::string& id) const
{
    if (id.empty()) {
        const auto it = _consumerTranslators.find(id);
        if (it != _consumerTranslators.end()) {
            return it->second;
        }
    }
    return nullptr;
}

ConsumerTranslatorsList MediaTranslatorsManager::Impl::GetAssociated(const std::shared_ptr<ProducerTranslator>& producer) const
{
    if (producer) {
        return GetAssociated(producer->GetId());
    }
    return {};
}

ConsumerTranslatorsList MediaTranslatorsManager::Impl::GetAssociated(const std::string& producerId) const
{
    ConsumerTranslatorsList consumers;
    if (!producerId.empty()) {
        for (auto it = _consumerTranslators.begin(); it != _consumerTranslators.end(); ++it) {
            if (it->second->GetProducerId() == producerId) {
                consumers.push_back(it->second);
            }
        }
    }
    return consumers;
}

bool MediaTranslatorsManager::Impl::UnRegister(const Consumer* consumer)
{
    if (consumer && !consumer->id.empty()) {
        const auto it = _consumerTranslators.find(consumer->id);
        if (it != _consumerTranslators.end()) {
            for (auto its = _endPoints.begin(); its != _endPoints.end();) {
                if ((*its)->GetConsumerId() == consumer->id) {
                    _endPoints.erase(its++);
                }
                else {
                    ++its;
                }
            }
            _consumerTranslators.erase(it);
            return true;
        }
    }
    return false;
}

void MediaTranslatorsManager::Impl::onProducerMediaRegistered(const std::string& producerId,
                                                              bool audio, uint32_t ssrc,
                                                              uint32_t /*mappedSsrc*/,
                                                              bool registered)
{
    if (ssrc && !producerId.empty()) {
        if (audio) {
            if (!registered) {
                for (auto it = _endPoints.begin(); it != _endPoints.end(); ++it) {
                    const auto& endPoint = *it;
                    if (ssrc == endPoint->GetAudioSsrc() && producerId == endPoint->GetProducerId()) {
                        endPoint->Close();
                    }
                }
            }
            else {
                FetchEndPoint(ssrc, producerId);
            }
        }
    }
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

void MediaTranslatorsManager::Impl::OnConsumerEnabledChanged(const std::string& consumerId,
                                                             bool enabled)
{
    for (const auto& endPoint : FindEndPointsConsumerId(consumerId)) {
        if (enabled) {
            OpenEndPont(endPoint);
        }
        else {
            endPoint->Close();
        }
    }
}

TranslatorEndPointsList MediaTranslatorsManager::Impl::FindEndPointsByProducerId(const std::string& producerId) const
{
    std::list<std::shared_ptr<TranslatorEndPoint>> endPoints;
    if (!producerId.empty()) {
        for (auto it = _endPoints.begin(); it != _endPoints.end(); ++it) {
            const auto& endPoint = *it;
            if (producerId == endPoint->GetProducerId()) {
                endPoints.push_back(endPoint);
            }
        }
    }
    return endPoints;
}

TranslatorEndPointsList MediaTranslatorsManager::Impl::FindEndPointsConsumerId(const std::string& consumerId) const
{
    std::list<std::shared_ptr<TranslatorEndPoint>> endPoints;
    if (!consumerId.empty()) {
        for (auto it = _endPoints.begin(); it != _endPoints.end(); ++it) {
            const auto& endPoint = *it;
            if (consumerId == endPoint->GetConsumerId()) {
                endPoints.push_back(endPoint);
            }
        }
    }
    return endPoints;
}

void MediaTranslatorsManager::Impl::FetchEndPoint(uint32_t audioSsrc, const std::string& producerId)
{
    if (audioSsrc) {
        if (const auto producerTranslator = GetRegisteredProducer(producerId)) {
            for (const auto& consumerTranslator : GetAssociated(producerTranslator)) {
                bool endPointExists = false;
                for (auto it = _endPoints.begin(); it != _endPoints.end(); ++it) {
                    const auto& endPoint = *it;
                    if (audioSsrc == endPoint->GetAudioSsrc() &&
                        producerId == endPoint->GetProducerId() &&
                        consumerTranslator->GetId() == endPoint->GetConsumerId()) {
                        endPointExists = true;
                        break;
                    }
                }
                if (!endPointExists) {
                    auto endPoint = std::make_shared<TranslatorEndPoint>(audioSsrc,
                                                                         producerTranslator,
                                                                         consumerTranslator,
                                                                         _serviceUri,
                                                                         _serviceUser,
                                                                         _servicePassword);
                    _endPoints.insert(endPoint);
                    if (consumerTranslator->IsEnabled()) {
                        OpenEndPont(endPoint);
                    }
                }
            }
        }
    }
}

void MediaTranslatorsManager::Impl::OpenEndPont(const std::shared_ptr<TranslatorEndPoint>& endPoint)
{
    if (endPoint) {
        if (!endPoint->Open()) {
            // TODO: log error
        }
    }
}

void MediaTranslatorsManager::Impl::RegisterStream(const std::shared_ptr<ProducerTranslator>& producerTranslator,
                                                   const RtpStream* stream, uint32_t mappedSsrc)
{
    if (producerTranslator && stream) {
        switch (stream->GetMimeType().type) {
            case RtpCodecMimeType::Type::AUDIO:
                if (!producerTranslator->RegisterAudio(stream, mappedSsrc)) {
                    // TODO: log error
                }
                break;
            case RtpCodecMimeType::Type::VIDEO:
                if (!producerTranslator->RegisterVideo(stream, mappedSsrc)) {
                    // TODO: log error
                }
                break;
            default:
                break;
        }
    }
}

void TranslatorUnit::Pause(bool pause)
{
    if (pause != _paused.exchange(pause)) {
        OnPauseChanged(pause);
    }
}

} // namespace RTC
