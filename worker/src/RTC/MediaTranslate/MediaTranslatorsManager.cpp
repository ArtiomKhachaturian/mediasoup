#define MS_CLASS "RTC::MediaTranslatorsManager"
#include "RTC/MediaTranslate/MediaTranslatorsManager.hpp"
#include "RTC/RtpPacketsCollector.hpp"
#include "RTC/MediaTranslate/ProducerTranslator.hpp"
#include "RTC/MediaTranslate/ConsumerTranslator.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/MediaTranslate/RtpMediaFrame.hpp"
#include "RTC/MediaTranslate/TranslatorEndPoint.hpp"
#include "RTC/MediaTranslate/ProducerObserver.hpp"
#include "RTC/MediaTranslate/WebM/WebMMediaFrameSerializationFactory.hpp"
#include "RTC/RtpPacket.hpp"
#include "RTC/Producer.hpp"
#include "RTC/Consumer.hpp"
#include "RTC/RtpStream.hpp"
#include "RTC/RtpPacket.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include <absl/container/flat_hash_set.h>


namespace RTC
{

class MediaTranslatorsManager::Translator : private RtpPacketsCollector
{
    using ConsumersMap = absl::flat_hash_map<Consumer*, std::unique_ptr<ConsumerTranslator>>;
    using TranslationEndPointsMap = absl::flat_hash_map<Consumer*, std::shared_ptr<TranslatorEndPoint>>;
public:
    Translator(MediaTranslatorsManager* manager,
               std::unique_ptr<ProducerTranslator> producer,
               const std::string& serviceUri,
               const std::string& serviceUser,
               const std::string& servicePassword);
    ~Translator() final;
    void Pause(bool pause);
    void AddConsumer(Consumer* consumer, const std::shared_ptr<MediaFrameSerializationFactory>& serializationFactory);
    bool RemoveConsumer(Consumer* consumer);
    void UpdateConsumerLanguageAndVoice(Consumer* consumer);
    void UpdateProducerLanguage();
    void AddNewRtpStream(RtpStreamRecv* rtpStream, uint32_t mappedSsrc);
    bool DispatchProducerPacket(RtpPacket* packet);
private:
    // impl. of RtpPacketsCollector
    bool AddPacket(RtpPacket* packet) final;
private:
    MediaTranslatorsManager* const _manager;
    const std::unique_ptr<ProducerTranslator> _producer;
    const std::string& _serviceUri;
    const std::string& _serviceUser;
    const std::string& _servicePassword;
    ProtectedObj<ConsumersMap> _consumers;
    // key is consumer instance, simple model - each consumer has own channer for translation
    // TODO: revise this logic for better resources consumption if more than 1 consumers has the same language and voice
    ProtectedObj<TranslationEndPointsMap> _endPoints;
};

MediaTranslatorsManager::MediaTranslatorsManager(TransportListener* router,
                                                 const std::string& serviceUri,
                                                 const std::string& serviceUser,
                                                 const std::string& servicePassword)
    : _router(router)
    , _serviceUri(serviceUri)
    , _serviceUser(serviceUser)
    , _servicePassword(servicePassword)
    , _serializationFactory(std::make_shared<WebMMediaFrameSerializationFactory>())
    , _connectedTransport(nullptr)
{
    MS_ASSERT(nullptr != _router, "router must be non-null");
}

MediaTranslatorsManager::~MediaTranslatorsManager()
{
}

void MediaTranslatorsManager::OnTransportConnected(RTC::Transport* transport)
{
    _router->OnTransportConnected(transport);
    _connectedTransport.store(transport);
}

void MediaTranslatorsManager::OnTransportDisconnected(RTC::Transport* transport)
{
    _connectedTransport.compare_exchange_strong(transport, nullptr);
    _router->OnTransportDisconnected(transport);
}

void MediaTranslatorsManager::OnTransportNewProducer(Transport* transport, Producer* producer)
{
    _router->OnTransportNewProducer(transport, producer);
#ifdef SINGLE_TRANSLATION_POINT_CONNECTION
    if (!_translators.empty()) {
        return;
    }
#endif
    if (producer && Media::Kind::AUDIO == producer->GetKind() && !producer->id.empty()) {
        const auto it = _translators.find(producer->id);
        if (it == _translators.end()) {
            if (auto producerTranslator = ProducerTranslator::Create(producer, _serializationFactory)) {
                auto translator = std::make_unique<Translator>(this,
                                                               std::move(producerTranslator),
                                                               _serviceUri,
                                                               _serviceUser,
                                                               _servicePassword);
                // add streams
                const auto& streams = producer->GetRtpStreams();
                for (auto its = streams.begin(); its != streams.end(); ++its) {
                    translator->AddNewRtpStream(its->first, its->second);
                }
                // enqueue
                _translators[producer->id] = std::move(translator);
            }
        }
    }
}

void MediaTranslatorsManager::OnTransportProducerLanguageChanged(RTC::Transport* transport,
                                                                 RTC::Producer* producer)
{
    _router->OnTransportProducerLanguageChanged(transport, producer);
    if (producer) {
        const auto it = _translators.find(producer->id);
        if (it != _translators.end()) {
            it->second->UpdateProducerLanguage();
        }
    }
}

void MediaTranslatorsManager::OnTransportProducerClosed(Transport* transport, Producer* producer)
{
    _router->OnTransportProducerClosed(transport, producer);
    if (producer) {
        const auto it = _translators.find(producer->id);
        if (it != _translators.end()) {
            _translators.erase(it);
        }
    }
}

void MediaTranslatorsManager::OnTransportProducerPaused(Transport* transport, Producer* producer)
{
    _router->OnTransportProducerPaused(transport, producer);
    if (producer) {
        const auto it = _translators.find(producer->id);
        if (it != _translators.end()) {
            it->second->Pause(true);
        }
    }
}

void MediaTranslatorsManager::OnTransportProducerResumed(Transport* transport,
                                                         Producer* producer)
{
    _router->OnTransportProducerResumed(transport, producer);
    if (producer) {
        const auto it = _translators.find(producer->id);
        if (it != _translators.end()) {
            it->second->Pause(false);
        }
    }
}

void MediaTranslatorsManager::OnTransportProducerNewRtpStream(Transport* transport,
                                                              Producer* producer,
                                                              RtpStreamRecv* rtpStream,
                                                              uint32_t mappedSsrc)
{
    _router->OnTransportProducerNewRtpStream(transport, producer, rtpStream, mappedSsrc);
    if (producer && rtpStream) {
        const auto it = _translators.find(producer->id);
        if (it != _translators.end()) {
            it->second->AddNewRtpStream(rtpStream, mappedSsrc);
        }
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
    bool dispatched = false;
    if (producer && packet) {
        const auto it = _translators.find(producer->id);
        if (it != _translators.end()) {
            dispatched = it->second->DispatchProducerPacket(packet);
        }
    }
#ifdef SINGLE_TRANSLATION_POINT_CONNECTION
    if (!dispatched && !_translators.empty()) {
        return;
    }
#endif
    if (!dispatched) {
        _router->OnTransportProducerRtpPacketReceived(transport, producer, packet);
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
    if (consumer) {
        const auto it = _translators.find(producerId);
        if (it != _translators.end()) {
            it->second->AddConsumer(consumer, _serializationFactory);
        }
    }
}

void MediaTranslatorsManager::OnTransportConsumerLanguageChanged(RTC::Transport* transport,
                                                                 RTC::Consumer* consumer)
{
    _router->OnTransportConsumerLanguageChanged(transport, consumer);
    for (auto it = _translators.begin(); it != _translators.end(); ++it) {
        it->second->UpdateConsumerLanguageAndVoice(consumer);
    }
}

void MediaTranslatorsManager::OnTransportConsumerVoiceChanged(RTC::Transport* transport,
                                                              RTC::Consumer* consumer)
{
    _router->OnTransportConsumerVoiceChanged(transport, consumer);
    for (auto it = _translators.begin(); it != _translators.end(); ++it) {
        it->second->UpdateConsumerLanguageAndVoice(consumer);
    }
}

void MediaTranslatorsManager::OnTransportConsumerClosed(Transport* transport,
                                                        Consumer* consumer)
{
    _router->OnTransportConsumerClosed(transport, consumer);
    for (auto it = _translators.begin(); it != _translators.end(); ++it) {
        it->second->RemoveConsumer(consumer);
    }
}

void MediaTranslatorsManager::OnTransportConsumerProducerClosed(Transport* transport,
                                                                Consumer* consumer)
{
    _router->OnTransportConsumerProducerClosed(transport, consumer);
}

void MediaTranslatorsManager::OnTransportDataProducerPaused(Transport* transport,
                                                            DataProducer* dataProducer)
{
    _router->OnTransportDataProducerPaused(transport, dataProducer);
}

void MediaTranslatorsManager::OnTransportDataProducerResumed(Transport* transport,
                                                             DataProducer* dataProducer)
{
    _router->OnTransportDataProducerResumed(transport, dataProducer);
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
                                                                     const uint8_t* msg,
                                                                     size_t len,
                                                                     uint32_t ppid,
                                                                     const std::vector<uint16_t>& subchannels,
                                                                     const std::optional<uint16_t>& requiredSubchannel)
{
    _router->OnTransportDataProducerMessageReceived(transport, dataProducer, msg, len, ppid, subchannels, requiredSubchannel);
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

bool MediaTranslatorsManager::SendRtpPacket(RTC::Producer* producer, RtpPacket* packet)
{
    if (producer && packet) {
        if (const auto transport = _connectedTransport.load()) {
            _router->OnTransportProducerRtpPacketReceived(transport, producer, packet);
            return true;
        }
    }
    return false;
}

void TranslatorUnit::Pause(bool pause)
{
    if (pause != _paused.exchange(pause)) {
        OnPauseChanged(pause);
    }
}

MediaTranslatorsManager::Translator::Translator(MediaTranslatorsManager* manager,
                                                std::unique_ptr<ProducerTranslator> producer,
                                                const std::string& serviceUri,
                                                const std::string& serviceUser,
                                                const std::string& servicePassword)
    : _manager(manager)
    , _producer(std::move(producer))
    , _serviceUri(serviceUri)
    , _serviceUser(serviceUser)
    , _servicePassword(servicePassword)
{
}

MediaTranslatorsManager::Translator::~Translator()
{
#ifdef NO_TRANSLATION_SERVICE
    LOCK_READ_PROTECTED_OBJ(_consumers);
    for (auto it = _consumers.ConstRef().begin(); it != _consumers.ConstRef().end(); ++it) {
        _producer->RemoveSink(it->second.get());
    }
#else
    LOCK_READ_PROTECTED_OBJ(_endPoints);
    for (auto it = _endPoints.ConstRef().begin(); it != _endPoints.ConstRef().end(); ++it) {
        it->second->SetInput(nullptr);
    }
#endif
}

bool MediaTranslatorsManager::Translator::AddPacket(RtpPacket* packet)
{
    return packet && _manager->SendRtpPacket(_producer->GetProducer(), packet);
}

void MediaTranslatorsManager::Translator::Pause(bool pause)
{
    if (pause) {
        _producer->Pause();
    }
    else {
        _producer->Resume();
    }
}

void MediaTranslatorsManager::Translator::AddConsumer(Consumer* consumer,
                                                      const std::shared_ptr<MediaFrameSerializationFactory>& serializationFactory)
{
    if (consumer && serializationFactory) {
        {
            LOCK_WRITE_PROTECTED_OBJ(_consumers);
            auto& consumers = _consumers.Ref();
            const auto it = consumers.find(consumer);
            if (it == consumers.end()) {
                const auto packetsCollector = static_cast<RtpPacketsCollector*>(this);
                auto consumerTranslator = std::make_unique<ConsumerTranslator>(consumer, packetsCollector,
                                                                               serializationFactory);
#ifdef NO_TRANSLATION_SERVICE
                _producer->AddSink(consumerTranslator.get());
#endif
                consumers[consumer] = std::move(consumerTranslator);
            }
#ifndef NO_TRANSLATION_SERVICE
            UpdateConsumerLanguageAndVoice(consumer);
#endif
        }
    }
}

bool MediaTranslatorsManager::Translator::RemoveConsumer(Consumer* consumer)
{
    if (consumer) {
        LOCK_WRITE_PROTECTED_OBJ(_consumers);
        auto& consumers = _consumers.Ref();
        const auto it = consumers.find(consumer);
        if (it != consumers.end()) {
#ifdef NO_TRANSLATION_SERVICE
            _producer->RemoveSink(it->second.get());
#else
            {
                LOCK_WRITE_PROTECTED_OBJ(_endPoints);
                const auto ite = _endPoints.Ref().find(consumer);
                if (ite != _endPoints.Ref().end()) {
                    ite->second->SetInput(nullptr);
                    ite->second->SetOutput(nullptr);
                    _endPoints.Ref().erase(ite);
                }
            }
#endif
            consumers.erase(it);
            return true;
        }
    }
    return false;
}

void MediaTranslatorsManager::Translator::UpdateConsumerLanguageAndVoice(Consumer* consumer)
{
    if (consumer) {
        LOCK_READ_PROTECTED_OBJ(_consumers);
        const auto it = _consumers.ConstRef().find(consumer);
        if (it != _consumers.ConstRef().end()) {
            LOCK_WRITE_PROTECTED_OBJ(_endPoints);
            auto ite = _endPoints.Ref().find(consumer);
            if (ite == _endPoints.Ref().end()) {
                const auto endPoint = std::make_shared<TranslatorEndPoint>(_serviceUri,
                                                                           _serviceUser,
                                                                           _servicePassword);
                endPoint->SetProducerLanguage(_producer->GetLanguage());
                endPoint->SetInput(_producer.get());
                endPoint->SetOutput(it->second.get());
                ite = _endPoints.Ref().insert({consumer, endPoint}).first;
            }
            ite->second->SetConsumerLanguageAndVoice(it->second->GetLanguage(), it->second->GetVoice());
        }
    }
}

void MediaTranslatorsManager::Translator::UpdateProducerLanguage()
{
    LOCK_READ_PROTECTED_OBJ(_endPoints);
    for (auto it = _endPoints.ConstRef().begin(); it != _endPoints.ConstRef().end(); ++it) {
        it->second->SetProducerLanguage(_producer->GetLanguage());
    }
}

void MediaTranslatorsManager::Translator::AddNewRtpStream(RtpStreamRecv* rtpStream,
                                                          uint32_t mappedSsrc)
{
    if (!_producer->AddStream(rtpStream, mappedSsrc)) {
        const auto desc = GetStreamInfoString(mappedSsrc, rtpStream);
        MS_ERROR("failed to register stream [%s] for producer %s", desc.c_str(),
                 _producer->GetId().c_str());
    }
}

bool MediaTranslatorsManager::Translator::DispatchProducerPacket(RtpPacket* packet)
{
    LOCK_READ_PROTECTED_OBJ(_consumers);
    for (auto it = _consumers.ConstRef().begin(); it != _consumers.ConstRef().end(); ++it) {
        it->second->ProcessProducerRtpPacket(packet);
    }
    if (_producer->AddPacket(packet)) {
        for (auto it = _consumers.ConstRef().begin(); it != _consumers.ConstRef().end(); ++it) {
            if (it->second->HadIncomingMedia()) {
                // drop packet's dispatching if connection with translation service was established
                packet->AddRejectedConsumer(it->first);
            }
        }
        return true;
    }
    return false;
}

} // namespace RTC
