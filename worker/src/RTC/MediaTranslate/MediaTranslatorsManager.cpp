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

using ConsumerTranslatorsList = std::list<std::shared_ptr<ConsumerTranslator>>;

class MediaTranslatorsManager::Impl : public ProducerObserver
{
public:
    Impl(const std::string& serviceUri, const std::string& serviceUser, const std::string& servicePassword);
    // producers API
    bool Register(Producer* producer);
    std::shared_ptr<ProducerTranslator> GetRegistered(const Producer* producer) const;
    std::shared_ptr<ProducerTranslator> GetRegisteredProducer(const std::string& id) const;
    bool UnRegister(const Producer* producer);
    void RegisterProducerStream(const std::string& id, const RtpStream* stream, uint32_t mappedSsrc);
    // consumers API
    bool Register(Consumer* consumer, const std::string& producerId);
    std::shared_ptr<ConsumerTranslator> GetRegistered(const Consumer* consumer) const;
    std::shared_ptr<ConsumerTranslator> GetRegisteredConsumer(const std::string& id) const;
    ConsumerTranslatorsList GetAssociated(const std::shared_ptr<ProducerTranslator>& producer) const;
    ConsumerTranslatorsList GetAssociated(const std::string& producerId) const;
    bool UnRegister(const Consumer* consumer);
    // impl. of ProducerObserver
    void onStreamAdded(const std::string& producerId, uint32_t mappedSsrc,
                       const RtpCodecMimeType& mime, uint32_t clockRate) final;
    void onStreamRemoved(const std::string& producerId, uint32_t mappedSsrc,
                         const RtpCodecMimeType& mime) final;
    void OnLanguageChanged(const std::string& producerId,
                           const std::optional<MediaLanguage>& from,
                           const std::optional<MediaLanguage>& to) final;
    void OnMediaFrameProduced(const std::string& producerId,
                              uint32_t mappedSsrc,
                              const std::shared_ptr<RtpMediaFrame>& mediaFrame) final;
private:
    static void AddProducerStream(const std::shared_ptr<ProducerTranslator>& producerTranslator,
                                  const RtpStream* stream, uint32_t mappedSsrc);
    std::shared_ptr<RtpMediaFrameSerializer> FindSerializer(const std::string& producerId) const;
    void SetTranslatorMediaInput(const std::string& producerId, bool set);
    void SetTranslatorMediaInput(const std::shared_ptr<ConsumerTranslator>& consumerTranslator,
                                 bool set);
#ifdef DEBUG_UNITED_PRODUCER_MEDIA
    std::shared_ptr<RtpMediaFrameSerializer> FindSerializer(const std::string& producerId,
                                                            const RtpCodecMimeType& mime,
                                                            uint32_t mappedSsrc) const;
#endif
private:
    const std::string _serviceUri;
    const std::string _serviceUser;
    const std::string _servicePassword;
    // key is audio producer ID
    absl::flat_hash_map<std::string, std::shared_ptr<RtpMediaFrameSerializer>> _serializers;
#ifdef DEBUG_UNITED_PRODUCER_MEDIA
    // key is mapped SSRC of video stream
    // TODO: this is a workaround, needs to be changed into PeerClient abstraction (producers + serializer, maybe also link to consumers)
    absl::flat_hash_map<uint32_t, std::string> _mapFromVideoSsrcProducerToAudio;
#endif
    // key is producer ID
    absl::flat_hash_map<std::string, std::shared_ptr<ProducerTranslator>> _producerTranslators;
    // key is consumer ID
    absl::flat_hash_map<std::string, std::shared_ptr<ConsumerTranslator>> _consumerTranslators;
#ifdef WRITE_PRODUCER_RECV_TO_FILE
    absl::flat_hash_map<std::string, std::unique_ptr<FileWriter>> _fileWriters;
#endif
#ifdef SINGLE_TRANSLATION_POINT_CONNECTION
    bool _alreadyHasTranslationPointConnection = false;
#endif
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
    _impl->UnRegister(consumer);
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

bool MediaTranslatorsManager::Impl::Register(Producer* producer)
{
    if (producer && !producer->id.empty()) {
        const auto it = _producerTranslators.find(producer->id);
        if (it == _producerTranslators.end()) {
            const auto producerTranslator = std::make_shared<ProducerTranslator>(producer);
            const auto& streams = producer->GetRtpStreams();
            for (auto its = streams.begin(); its != streams.end(); ++its) {
                AddProducerStream(producerTranslator, its->first, its->second);
            }
            _producerTranslators[producer->id] = producerTranslator;
            producerTranslator->AddObserver(this);
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
            it->second->RemoveObserver(this);
            if (it->second->IsAudio()) {
                SetTranslatorMediaInput(producer->id, false);
                _serializers.erase(it->second->GetId());
            }
#ifdef DEBUG_UNITED_PRODUCER_MEDIA
            {
                if (it->second->IsAudio()) {
                    for (auto itl = _mapFromVideoSsrcProducerToAudio.begin();
                         itl != _mapFromVideoSsrcProducerToAudio.end();) {
                        if (itl->second == it->second->GetId()) {
                            _mapFromVideoSsrcProducerToAudio.erase(itl++);
                        }
                        else {
                            ++itl;
                        }
                    }
                }
                else {
                    for (auto mappedSsrc : it->second->GetAddedStreams()) {
                        if (_mapFromVideoSsrcProducerToAudio.erase(mappedSsrc)) {
                            break;
                        }
                    }
                }
            }
#endif
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
        AddProducerStream(GetRegisteredProducer(id), stream, mappedSsrc);
    }
}

bool MediaTranslatorsManager::Impl::Register(Consumer* consumer, const std::string& producerId)
{
    if (consumer && consumer->IsTranslationRequired() && !consumer->id.empty()) {
        if (const auto producerTranslator = GetRegisteredProducer(producerId)) {
            const auto it = _consumerTranslators.find(consumer->id);
            std::shared_ptr<ConsumerTranslator> consumerTranslator;
            if (it == _consumerTranslators.end()) {
                consumerTranslator = std::make_shared<ConsumerTranslator>(consumer,
                                                                          producerId,
                                                                          _serviceUri,
                                                                          _serviceUser,
                                                                          _servicePassword);
                _consumerTranslators[consumer->id] = consumerTranslator;
            }
            else {
                consumerTranslator = it->second;
            }
            if (producerTranslator->IsAudio()) {
                consumerTranslator->SetProducerLanguage(producerTranslator->GetLanguage());
                SetTranslatorMediaInput(consumerTranslator, true);
            }
            return true;
        }
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
            SetTranslatorMediaInput(it->second, false);
            _consumerTranslators.erase(it);
            return true;
        }
    }
    return false;
}

void MediaTranslatorsManager::Impl::onStreamAdded(const std::string& producerId,
                                                  uint32_t mappedSsrc,
                                                  const RtpCodecMimeType& mime,
                                                  uint32_t clockRate)
{
#ifdef DEBUG_UNITED_PRODUCER_MEDIA
    static_assert(_maxVideosInAudioSerializer > 0UL);
    if (mime.IsVideoCodec() && _mapFromVideoSsrcProducerToAudio.size() < _maxVideosInAudioSerializer) {
        for (auto it = _producerTranslators.begin(); it != _producerTranslators.end(); ++it) {
            if (it->second->IsAudio()) {
                _mapFromVideoSsrcProducerToAudio[mappedSsrc] = it->second->GetId();
                break;
            }
        }
    }
    auto serializer = FindSerializer(producerId, mime, mappedSsrc);
#else
    auto serializer = FindSerializer(producerId);
#endif
    if (!serializer && mime.IsAudioCodec()) {
        serializer = RtpMediaFrameSerializer::create(mime);
        if (serializer) {
            _serializers[producerId] = serializer;
        }
    }
    if (serializer) {
        bool trackAdded = false;
        switch (mime.GetType()) {
            case RtpCodecMimeType::Type::AUDIO:
                trackAdded = serializer->AddAudio(mappedSsrc, clockRate, mime.GetSubtype());
                if (trackAdded) {
                    SetTranslatorMediaInput(producerId, true);
                }
                break;
            case RtpCodecMimeType::Type::VIDEO:
                trackAdded = serializer->AddVideo(mappedSsrc, clockRate, mime.GetSubtype());
                break;
            default:
                break;
        }
        if (!trackAdded) {
            const auto error = GetStreamInfoString(mime, mappedSsrc);
            MS_ERROR("failed to add media track for serialization, stream [%s]", error.c_str());
        }
#ifdef WRITE_PRODUCER_RECV_TO_FILE
        else if (mime.IsAudioCodec()) {
            const auto it = _fileWriters.find(producerId);
            if (it == _fileWriters.end()) {
                const auto depacketizerPath = std::getenv("MEDIASOUP_DEPACKETIZER_PATH");
                if (depacketizerPath && std::strlen(depacketizerPath)) {
                    const auto extension = serializer->GetFileExtension(mime);
                    if (!extension.empty()) {
                        std::string fileName = producerId + "." + std::string(extension);
                        fileName = std::string(depacketizerPath) + "/" + fileName;
                        auto fileWriter = std::make_unique<FileWriter>(fileName);
                        if (fileWriter->IsOpen()) {
                            serializer->AddOutputDevice(fileWriter.get());
                            serializer->SetLiveMode(false);
                            _fileWriters[producerId] = std::move(fileWriter);
                        }
                    }
                }
            }
        }
#endif
    }
}

void MediaTranslatorsManager::Impl::onStreamRemoved(const std::string& producerId,
                                                    uint32_t mappedSsrc,
                                                    const RtpCodecMimeType& mime)
{
#ifdef DEBUG_UNITED_PRODUCER_MEDIA
    const auto serializer = FindSerializer(producerId, mime, mappedSsrc);
#else
    const auto serializer = FindSerializer(producerId);
#endif
    if (serializer) {
#ifndef WRITE_PRODUCER_RECV_TO_FILE
        serializer->RemoveMedia(mappedSsrc);
#endif
        if (mime.IsAudioCodec()) {
#ifdef WRITE_PRODUCER_RECV_TO_FILE
            const auto it = _fileWriters.find(producerId);
            if (it != _fileWriters.end()) {
                serializer->RemoveOutputDevice(it->second.get());
                _fileWriters.erase(it);
            }
#endif
            _serializers.erase(producerId);
            SetTranslatorMediaInput(producerId, false);
        }
    }
}


void MediaTranslatorsManager::Impl::OnLanguageChanged(const std::string& producerId,
                                                      const std::optional<MediaLanguage>& /*from*/,
                                                      const std::optional<MediaLanguage>& to)
{
    if (!producerId.empty()) {
        for (auto it = _consumerTranslators.begin(); it != _consumerTranslators.end(); ++it) {
            if (it->second->GetProducerId() == producerId) {
                it->second->SetProducerLanguage(to);
            }
        }
    }
}

void MediaTranslatorsManager::Impl::OnMediaFrameProduced(const std::string& producerId,
                                                         uint32_t mappedSsrc,
                                                         const std::shared_ptr<RtpMediaFrame>& mediaFrame)
{
    if (mediaFrame) {
#ifdef DEBUG_UNITED_PRODUCER_MEDIA
        const auto serializer = FindSerializer(producerId, mediaFrame->GetMimeType(),
                                               mappedSsrc);
#else
        const auto serializer = FindSerializer(producerId);
#endif
        if (serializer) {
            serializer->Push(mediaFrame);
        }
    }
}

void MediaTranslatorsManager::Impl::
    AddProducerStream(const std::shared_ptr<ProducerTranslator>& producerTranslator,
                      const RtpStream* stream, uint32_t mappedSsrc)
{
    if (stream && producerTranslator && !producerTranslator->AddStream(stream,
                                                                       mappedSsrc)) {
        const auto desc = GetStreamInfoString(mappedSsrc, stream);
        MS_ERROR("failed to register stream [%s] for producer %s", desc.c_str(),
                 producerTranslator->GetId().c_str());
    }
}

std::shared_ptr<RtpMediaFrameSerializer> MediaTranslatorsManager::Impl::
    FindSerializer(const std::string& producerId) const
{
    if (!producerId.empty()) {
        const auto it = _serializers.find(producerId);
        if (it != _serializers.end()) {
            return it->second;
        }
    }
    return nullptr;
}

void MediaTranslatorsManager::Impl::SetTranslatorMediaInput(const std::string& producerId,
                                                            bool set)
{
    const auto producerTranslator = GetRegisteredProducer(producerId);
    if (producerTranslator && producerTranslator->IsAudio()) {
        const auto associated = GetAssociated(producerId);
        if (!associated.empty()) {
            std::shared_ptr<RtpMediaFrameSerializer> serializer;
            if (set) {
                serializer = FindSerializer(producerId);
            }
            for (const auto& consumerTranslator : GetAssociated(producerId)) {
                if (set) {
                    if (serializer) {
#ifdef SINGLE_TRANSLATION_POINT_CONNECTION
                        if (!_alreadyHasTranslationPointConnection) {
                            consumerTranslator->SetProducerInput(serializer);
                            _alreadyHasTranslationPointConnection = consumerTranslator->HasProducerInput();
                        }
                        else {
                            break;
                        }
#else
                        consumerTranslator->SetProducerInput(serializer);
#endif
                    }
                }
                else {
#ifdef SINGLE_TRANSLATION_POINT_CONNECTION
                    if (_alreadyHasTranslationPointConnection) {
                        consumerTranslator->SetProducerInput(nullptr);
                        _alreadyHasTranslationPointConnection = consumerTranslator->HasProducerInput();
                    }
                    else {
                        break;
                    }
#else
                    consumerTranslator->SetProducerInput(nullptr);
#endif
                }
            }
        }
    }
}

void MediaTranslatorsManager::Impl::SetTranslatorMediaInput(const std::shared_ptr<ConsumerTranslator>& consumerTranslator,
                                                            bool set)
{
    if (consumerTranslator) {
        const auto producerTranslator = GetRegisteredProducer(consumerTranslator->GetProducerId());
        if (producerTranslator && producerTranslator->IsAudio()) {
            std::shared_ptr<RtpMediaFrameSerializer> serializer;
            if (set) {
                const auto serializer = FindSerializer(consumerTranslator->GetProducerId());
                if (serializer) {
#ifdef SINGLE_TRANSLATION_POINT_CONNECTION
                    if (!_alreadyHasTranslationPointConnection) {
                        consumerTranslator->SetProducerInput(serializer);
                        _alreadyHasTranslationPointConnection = consumerTranslator->HasProducerInput();
                    }
#else
                    consumerTranslator->SetProducerInput(serializer);
#endif
                }
            }
            else {
#ifdef SINGLE_TRANSLATION_POINT_CONNECTION
                if (_alreadyHasTranslationPointConnection) {
                    consumerTranslator->SetProducerInput(nullptr);
                    _alreadyHasTranslationPointConnection = consumerTranslator->HasProducerInput();
                }
#else
                consumerTranslator->SetProducerInput(nullptr);
#endif
            }
        }
    }
}

#ifdef DEBUG_UNITED_PRODUCER_MEDIA
std::shared_ptr<RtpMediaFrameSerializer> MediaTranslatorsManager::Impl::
    FindSerializer(const std::string& producerId, const RtpCodecMimeType& mime,
                   uint32_t mappedSsrc) const
{
    std::shared_ptr<RtpMediaFrameSerializer> serializer;
    if (!producerId.empty()) {
        if (mime.IsVideoCodec()) {
            const auto itl = _mapFromVideoSsrcProducerToAudio.find(mappedSsrc);
            if (itl != _mapFromVideoSsrcProducerToAudio.end()) {
                serializer = FindSerializer(itl->second);
                if (serializer && !serializer->IsCompatible(mime)) {
                    serializer = nullptr;
                }
            }
        }
        else {
            serializer = FindSerializer(producerId);
        }
    }
    return serializer;
}
#endif

void TranslatorUnit::Pause(bool pause)
{
    if (pause != _paused.exchange(pause)) {
        OnPauseChanged(pause);
    }
}

} // namespace RTC
