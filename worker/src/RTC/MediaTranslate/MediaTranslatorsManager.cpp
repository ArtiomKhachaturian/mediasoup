#define MS_CLASS "RTC::MediaTranslatorsManager"
#include "RTC/MediaTranslate/MediaTranslatorsManager.hpp"
#include "RTC/RtpPacketsCollector.hpp"
#include "RTC/MediaTranslate/ProducerTranslator.hpp"
#include "RTC/MediaTranslate/ConsumerTranslator.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/MediaTranslate/RtpMediaFrameSerializer.hpp"
#include "RTC/MediaTranslate/RtpMediaFrame.hpp"
#ifdef WRITE_PRODUCER_RECV_TO_FILE
#include "RTC/MediaTranslate/FileWriter.hpp"
#endif
#include "RTC/RtpPacket.hpp"
#include "RTC/Producer.hpp"
#include "RTC/Consumer.hpp"
#include "RTC/RtpStream.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include <absl/container/flat_hash_set.h>


namespace RTC
{

MediaTranslatorsManager::MediaTranslatorsManager(TransportListener* router,
                                                 const std::string& serviceUri,
                                                 const std::string& serviceUser,
                                                 const std::string& servicePassword)
    : _router(router)
    , _serviceUri(serviceUri)
    , _serviceUser(serviceUser)
    , _servicePassword(servicePassword)
{
    MS_ASSERT(nullptr != _router, "router must be non-null");
}

MediaTranslatorsManager::~MediaTranslatorsManager()
{
}

std::shared_ptr<ProducerTranslatorSettings> MediaTranslatorsManager::GetTranslatorSettings(const Producer* producer) const
{
    return GetRegistered(producer);
}

std::shared_ptr<ConsumerTranslatorSettings> MediaTranslatorsManager::GetTranslatorSettings(const Consumer* consumer) const
{
    return GetRegistered(consumer);
}

void MediaTranslatorsManager::OnTransportNewProducer(Transport* transport, Producer* producer)
{
    _router->OnTransportNewProducer(transport, producer);
    Register(producer);
}

void MediaTranslatorsManager::OnTransportProducerClosed(Transport* transport, Producer* producer)
{
    _router->OnTransportProducerClosed(transport, producer);
    UnRegister(producer);
}

void MediaTranslatorsManager::OnTransportProducerPaused(Transport* transport, Producer* producer)
{
    _router->OnTransportProducerPaused(transport, producer);
    if (const auto translator = GetRegistered(producer)) {
        translator->Pause();
    }
}

void MediaTranslatorsManager::OnTransportProducerResumed(Transport* transport,
                                                         Producer* producer)
{
    _router->OnTransportProducerResumed(transport, producer);
    if (const auto translator = GetRegistered(producer)) {
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
       AddProducerStream(producer->id, rtpStream, mappedSsrc);
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
    std::shared_ptr<ProducerTranslator> audioTranslator;
    if (packet) {
        audioTranslator = GetRegistered(producer);
    }
    _router->OnTransportProducerRtpPacketReceived(transport, producer, packet);
    if (audioTranslator) {
        audioTranslator->AddPacket(packet);
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
    Register(consumer, producerId);
}

void MediaTranslatorsManager::OnTransportConsumerClosed(Transport* transport,
                                                        Consumer* consumer)
{
    _router->OnTransportConsumerClosed(transport, consumer);
    UnRegister(consumer);
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

bool MediaTranslatorsManager::Register(Producer* producer)
{
    if (producer && Media::Kind::AUDIO == producer->GetKind() && !producer->id.empty()) {
        const auto it = _audioProducers.find(producer->id);
        if (it == _audioProducers.end()) {
            const auto audioProducer = std::make_shared<ProducerTranslator>(producer);
            _audioProducers[producer->id] = audioProducer;
            audioProducer->AddObserver(this);
            // add streams
            const auto& streams = producer->GetRtpStreams();
            for (auto its = streams.begin(); its != streams.end(); ++its) {
                AddProducerStream(audioProducer, its->first, its->second);
            }
        }
        return true;
    }
    return false;
}

std::shared_ptr<ProducerTranslator> MediaTranslatorsManager::GetRegistered(const Producer* producer) const
{
    return producer ? GetRegisteredProducer(producer->id) : nullptr;
}

std::shared_ptr<ProducerTranslator> MediaTranslatorsManager::GetRegisteredProducer(const std::string& id) const
{
    if (!id.empty()) {
        const auto it = _audioProducers.find(id);
        if (it != _audioProducers.end()) {
            return it->second;
        }
    }
    return nullptr;
}

bool MediaTranslatorsManager::UnRegister(const Producer* producer)
{
    if (producer && !producer->id.empty()) {
        const auto it = _audioProducers.find(producer->id);
        if (it != _audioProducers.end()) {
            it->second->RemoveObserver(this);
            for (const auto& associated : GetAssociated(it->second)) {
                associated->SetProducerInput(nullptr);
            }
#ifdef WRITE_PRODUCER_RECV_TO_FILE
            const auto itf = _fileWriters.find(it->second->GetId());
            if (itf != _fileWriters.end()) {
                it->second->RemoveOutputDevice(itf->second.get());
                _fileWriters.erase(itf);
            }
#endif
            _audioProducers.erase(it);
            return true;
        }
    }
    return false;
}

void MediaTranslatorsManager::AddProducerStream(const std::string& id,
                                                const RtpStream* stream,
                                                uint32_t mappedSsrc) const
{
    if (stream && mappedSsrc) {
        AddProducerStream(GetRegisteredProducer(id), stream, mappedSsrc);
    }
}

void MediaTranslatorsManager::AddProducerStream(const std::shared_ptr<ProducerTranslator>& audioProducer,
                                                const RtpStream* stream, uint32_t mappedSsrc) const
{
    if (audioProducer && stream && mappedSsrc) {
        if (!audioProducer->AddStream(stream, mappedSsrc)) {
            const auto desc = GetStreamInfoString(mappedSsrc, stream);
            MS_ERROR("failed to register stream [%s] for producer %s", desc.c_str(),
                     audioProducer->GetId().c_str());
        }
    }
}

bool MediaTranslatorsManager::Register(Consumer* consumer, const std::string& producerId)
{
    if (consumer && consumer->IsTranslationRequired() && !consumer->id.empty()) {
        if (const auto audioProducer = GetRegisteredProducer(producerId)) {
            const auto it = _audioConsumers.find(consumer->id);
            std::shared_ptr<ConsumerTranslator> audioConsumer;
            if (it == _audioConsumers.end()) {
                audioConsumer = std::make_shared<ConsumerTranslator>(consumer, producerId,
                                                                     _serviceUri, _serviceUser,
                                                                     _servicePassword);
                _audioConsumers[consumer->id] = audioConsumer;
            }
            else {
                audioConsumer = it->second;
            }
            audioConsumer->SetProducerLanguage(audioProducer->GetLanguage());
#ifdef SINGLE_TRANSLATION_POINT_CONNECTION
            if (!_alreadyHasTranslationPointConnection) {
                audioConsumer->SetProducerInput(audioProducer);
                _alreadyHasTranslationPointConnection = audioConsumer->HasProducerInput();
            }
#else
            audioConsumer->SetProducerInput(audioProducer);
#endif
            return true;
        }
    }
    return false;
}

std::shared_ptr<ConsumerTranslator> MediaTranslatorsManager::GetRegistered(const Consumer* consumer) const
{
    return consumer ? GetRegisteredConsumer(consumer->id) : nullptr;
}

std::shared_ptr<ConsumerTranslator> MediaTranslatorsManager::GetRegisteredConsumer(const std::string& id) const
{
    if (!id.empty()) {
        const auto it = _audioConsumers.find(id);
        if (it != _audioConsumers.end()) {
            return it->second;
        }
    }
    return nullptr;
}

MediaTranslatorsManager::ConsumerTranslatorsList MediaTranslatorsManager::
    GetAssociated(const std::shared_ptr<ProducerTranslator>& producer) const
{
    if (producer) {
        return GetAssociated(producer->GetId());
    }
    return {};
}

MediaTranslatorsManager::ConsumerTranslatorsList MediaTranslatorsManager::
    GetAssociated(const std::string& producerId) const
{
    ConsumerTranslatorsList consumers;
    if (!producerId.empty()) {
        for (auto it = _audioConsumers.begin(); it != _audioConsumers.end(); ++it) {
            if (it->second->GetProducerId() == producerId) {
                consumers.push_back(it->second);
            }
        }
    }
    return consumers;
}

bool MediaTranslatorsManager::UnRegister(const Consumer* consumer)
{
    if (consumer && !consumer->id.empty()) {
        const auto it = _audioConsumers.find(consumer->id);
        if (it != _audioConsumers.end()) {
#ifdef SINGLE_TRANSLATION_POINT_CONNECTION
            if (it->second->HasProducerInput()) {
                it->second->SetProducerInput(nullptr);
                _alreadyHasTranslationPointConnection = false;
            }
#else
            it->second->SetProducerInput(nullptr);
#endif
            _audioConsumers.erase(it);
            return true;
        }
    }
    return false;
}

#ifdef WRITE_PRODUCER_RECV_TO_FILE
void MediaTranslatorsManager::onStreamAdded(const std::string& producerId,
                                            uint32_t mappedSsrc,
                                            const RtpCodecMimeType& /*mime*/,
                                            uint32_t /*clockRate*/)
{
    if (const auto audioProducer = GetRegisteredProducer(producerId)) {
        const auto it = _fileWriters.find(producerId);
        if (it == _fileWriters.end()) {
            const auto depacketizerPath = std::getenv("MEDIASOUP_DEPACKETIZER_PATH");
            if (depacketizerPath && std::strlen(depacketizerPath)) {
                const auto extension = audioProducer->GetFileExtension(mappedSsrc);
                if (!extension.empty()) {
                    std::string fileName = producerId + "." + std::string(extension);
                    fileName = std::string(depacketizerPath) + "/" + fileName;
                    auto fileWriter = std::make_unique<FileWriter>(fileName);
                    if (fileWriter->IsOpen()) {
                        audioProducer->AddOutputDevice(fileWriter.get());
                            _fileWriters[audioProducer->GetId()] = std::move(fileWriter);
                    }
                }
            }
        }
    }
}
#endif

void MediaTranslatorsManager::OnLanguageChanged(const std::string& producerId,
                                                const std::optional<MediaLanguage>& /*from*/,
                                                const std::optional<MediaLanguage>& to)
{
    for (const auto& associated : GetAssociated(producerId)) {
        associated->SetProducerLanguage(to);
    }
}

void TranslatorUnit::Pause(bool pause)
{
    if (pause != _paused.exchange(pause)) {
        OnPauseChanged(pause);
    }
}

} // namespace RTC
