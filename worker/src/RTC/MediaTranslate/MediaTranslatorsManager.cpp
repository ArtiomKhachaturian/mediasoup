#define MS_CLASS "RTC::MediaTranslatorsManager"
#include "RTC/MediaTranslate/MediaTranslatorsManager.hpp"
#include "RTC/RtpPacketsCollector.hpp"
#include "RTC/MediaTranslate/ProducerTranslator.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/MediaTranslate/RtpMediaFrame.hpp"
#include "RTC/MediaTranslate/TranslatorEndPoint.hpp"
#include "RTC/RtpPacket.hpp"
#include "RTC/Producer.hpp"
#include "RTC/Consumer.hpp"
#include "RTC/RtpStream.hpp"
#include "RTC/RtpPacket.hpp"
#include "RTC/Transport.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include <absl/container/flat_hash_set.h>


namespace RTC
{

class MediaTranslatorsManager::Translator : private RtpPacketsCollector
{
    using TranslationEndPointsMap = absl::flat_hash_map<Consumer*, std::shared_ptr<TranslatorEndPoint>>;
public:
    Translator(MediaTranslatorsManager* manager,
               std::unique_ptr<ProducerTranslator> producer,
               const std::string& serviceUri,
               const std::string& serviceUser,
               const std::string& servicePassword);
    ~Translator() final;
    void AddConsumer(Consumer* consumer);
    bool RemoveConsumer(Consumer* consumer);
    void UpdateProducerLanguage();
    void UpdateConsumerLanguage(Consumer* consumer);
    void UpdateConsumerVoice(Consumer* consumer);
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
    // key is consumer instance, simple model - each consumer has own channer for translation
    // TODO: revise this logic for better resources consumption if more than 1 consumers has the same language and voice
    ProtectedObj<TranslationEndPointsMap> _endPoints;
};


#ifdef USE_MAIN_THREAD_FOR_PACKETS_RETRANSMISSION
class MediaTranslatorsManager::UVAsyncHandle
{
public:
    UVAsyncHandle(uv_async_t* handle);
    ~UVAsyncHandle();
    bool InLoop() const { return DepLibUV::GetLoop() == _handle->loop; }
    void Invoke() const { uv_async_send(_handle); }
    static std::unique_ptr<UVAsyncHandle> Create(uv_loop_t* loop, uv_async_cb asyncCb, void* data);
    static std::unique_ptr<UVAsyncHandle> Create(uv_async_cb asyncCb, void* data);
private:
    static void OnClosed(uv_handle_t* handle);
private:
    uv_async_t* _handle;
};
#endif

MediaTranslatorsManager::MediaTranslatorsManager(TransportListener* router,
                                                 const std::string& serviceUri,
                                                 const std::string& serviceUser,
                                                 const std::string& servicePassword)
    : _router(router)
    , _serviceUri(serviceUri)
    , _serviceUser(serviceUser)
    , _servicePassword(servicePassword)
#ifdef USE_MAIN_THREAD_FOR_PACKETS_RETRANSMISSION
    , _async(UVAsyncHandle::Create(PlaybackDefferedRtpPackets, this))
#endif
{
    MS_ASSERT(nullptr != _router, "router must be non-null");
#ifdef USE_MAIN_THREAD_FOR_PACKETS_RETRANSMISSION
    if (!_async) {
        MS_WARN_DEV_STD("failed to create asyn packets dispatcher");
    }
#endif
}

MediaTranslatorsManager::~MediaTranslatorsManager()
{
}

void MediaTranslatorsManager::OnTransportConnected(Transport* transport)
{
    _router->OnTransportConnected(transport);
    LOCK_WRITE_PROTECTED_OBJ(_connectedTransport);
    _connectedTransport = transport;
}

void MediaTranslatorsManager::OnTransportDisconnected(Transport* transport)
{
    {
        LOCK_WRITE_PROTECTED_OBJ(_connectedTransport);
        if (_connectedTransport.ConstRef() == transport) {
            _connectedTransport = nullptr;
#ifdef USE_MAIN_THREAD_FOR_PACKETS_RETRANSMISSION
            LOCK_WRITE_PROTECTED_OBJ(_defferedPackets);
            _defferedPackets.Ref().clear();
#endif
        }
    }
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
            if (auto producerTranslator = ProducerTranslator::Create(producer)) {
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
#ifdef USE_MAIN_THREAD_FOR_PACKETS_RETRANSMISSION
                LOCK_WRITE_PROTECTED_OBJ(_defferedPackets);
                _defferedPackets.Ref()[producer] = PacketsList();
#endif
            }
        }
    }
}

void MediaTranslatorsManager::OnTransportProducerLanguageChanged(Transport* transport,
                                                                 Producer* producer)
{
    _router->OnTransportProducerLanguageChanged(transport, producer);
#ifndef NO_TRANSLATION_SERVICE
    if (producer) {
        const auto it = _translators.find(producer->id);
        if (it != _translators.end()) {
            it->second->UpdateProducerLanguage();
        }
    }
#endif
}

void MediaTranslatorsManager::OnTransportProducerClosed(Transport* transport, Producer* producer)
{
#ifdef USE_MAIN_THREAD_FOR_PACKETS_RETRANSMISSION
    LOCK_WRITE_PROTECTED_OBJ(_defferedPackets);
    const auto itd = _defferedPackets.Ref().find(producer);
    if (itd != _defferedPackets.Ref().end()) {
        PlaybackDefferedRtpPackets(producer, std::move(itd->second));
        _defferedPackets.Ref().erase(itd);
    }
#endif
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
}

void MediaTranslatorsManager::OnTransportProducerResumed(Transport* transport,
                                                         Producer* producer)
{
    _router->OnTransportProducerResumed(transport, producer);
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
    if (producer && packet && !packet->IsSynthenized()) {
        const auto it = _translators.find(producer->id);
        if (it != _translators.end()) {
            dispatched = it->second->DispatchProducerPacket(packet);
        }
        else {
            dispatched = true; // drop packet if producer is not registered
        }
    }
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
            it->second->AddConsumer(consumer);
        }
    }
}

void MediaTranslatorsManager::OnTransportConsumerLanguageChanged(Transport* transport,
                                                                 Consumer* consumer)
{
    _router->OnTransportConsumerLanguageChanged(transport, consumer);
#ifndef NO_TRANSLATION_SERVICE
    for (auto it = _translators.begin(); it != _translators.end(); ++it) {
        it->second->UpdateConsumerLanguage(consumer);
    }
#endif
}

void MediaTranslatorsManager::OnTransportConsumerVoiceChanged(Transport* transport,
                                                              Consumer* consumer)
{
    _router->OnTransportConsumerVoiceChanged(transport, consumer);
#ifndef NO_TRANSLATION_SERVICE
    for (auto it = _translators.begin(); it != _translators.end(); ++it) {
        it->second->UpdateConsumerVoice(consumer);
    }
#endif
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

#ifdef USE_MAIN_THREAD_FOR_PACKETS_RETRANSMISSION
void MediaTranslatorsManager::PlaybackDefferedRtpPackets(uv_async_t* handle)
{
    if (handle) {
        const auto self = (MediaTranslatorsManager*)handle->data;
        self->PlaybackDefferedRtpPackets();
    }
}

void MediaTranslatorsManager::PlaybackDefferedRtpPackets()
{
    LOCK_WRITE_PROTECTED_OBJ(_defferedPackets);
    for (auto it = _defferedPackets.Ref().begin(); it != _defferedPackets.Ref().end(); ++it) {
        if (it->second.size() >= _defferedPacketsBatchSize) {
            PlaybackDefferedRtpPackets(it->first, std::move(it->second));
        }
    }
}

void MediaTranslatorsManager::PlaybackDefferedRtpPackets(Producer* producer, PacketsList packets)
{
    if (producer && !packets.empty()) {
        for (const auto packet : packets) {
            if (!PlaybackRtpPacket(producer, packet)) {
                delete packet;
            }
        }
    }
}

bool MediaTranslatorsManager::HasConnectedTransport() const
{
    LOCK_READ_PROTECTED_OBJ(_connectedTransport);
    return nullptr != _connectedTransport.ConstRef();
}
#endif

bool MediaTranslatorsManager::PlaybackRtpPacket(Producer* producer, RtpPacket* packet)
{
    if (packet && producer) {
        LOCK_READ_PROTECTED_OBJ(_connectedTransport);
        if (const auto transport = _connectedTransport.ConstRef()) {
            if (transport->ReceiveRtpPacket(packet, producer)) {
                ++_sendPackets;
            }
            return true;
        }
    }
    return false;
}

bool MediaTranslatorsManager::SendRtpPacket(Producer* producer, RtpPacket* packet)
{
    bool ok = false;
#ifdef USE_MAIN_THREAD_FOR_PACKETS_RETRANSMISSION
    if (packet) {
        if (_async) {
            if (_async->InLoop()) {
                ok = PlaybackRtpPacket(producer, packet);
            }
            else {
                LOCK_WRITE_PROTECTED_OBJ(_defferedPackets);
                const auto it = _defferedPackets.Ref().find(producer);
                if (it != _defferedPackets.Ref().end()) {
                    it->second.push_back(packet);
                    if (it->second.size() == _defferedPacketsBatchSize) {
                        _async->Invoke();
                    }
                    ok = true;
                }
            }
        }
        else {
            ok = PlaybackRtpPacket(producer, packet);
        }
    }
#else
    ok = PlaybackRtpPacket(producer, packet);
#endif
    if (!ok) {
        delete packet;
    }
    return ok;
}

#ifdef USE_MAIN_THREAD_FOR_PACKETS_RETRANSMISSION
MediaTranslatorsManager::UVAsyncHandle::UVAsyncHandle(uv_async_t* handle)
    : _handle(handle)
{
}

MediaTranslatorsManager::UVAsyncHandle::~UVAsyncHandle()
{
    uv_close(reinterpret_cast<uv_handle_t*>(_handle), OnClosed);
}

std::unique_ptr<MediaTranslatorsManager::UVAsyncHandle> MediaTranslatorsManager::
    UVAsyncHandle::Create(uv_loop_t* loop, uv_async_cb asyncCb, void* data)
{
    if (loop) {
        auto handle = std::make_unique<uv_async_t>();
        if (0 == uv_async_init(loop, handle.get(), asyncCb)) {
            handle->data = data;
            return std::make_unique<UVAsyncHandle>(handle.release());
        }
    }   
    return nullptr;
}

std::unique_ptr<MediaTranslatorsManager::UVAsyncHandle> MediaTranslatorsManager::
    UVAsyncHandle::Create(uv_async_cb asyncCb, void* data)
{
    return Create(DepLibUV::GetLoop(), asyncCb, data);
}

void MediaTranslatorsManager::UVAsyncHandle::OnClosed(uv_handle_t* handle)
{
    delete reinterpret_cast<uv_async_t*>(handle);
}
#endif

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
    MS_ERROR_STD("Translator for producer: %s", _producer->GetId().c_str());
}

MediaTranslatorsManager::Translator::~Translator()
{
    for(auto ssrc : _producer->GetSsrcs(false)) {
        _manager->_packetsPlayer.RemoveStream(ssrc);
    }
#ifdef NO_TRANSLATION_SERVICE
    _producer->RemoveSink(&_manager->_packetsPlayer);
    LOCK_WRITE_PROTECTED_OBJ(_endPoints);
    _endPoints->clear();
#else
    LOCK_WRITE_PROTECTED_OBJ(_endPoints);
    for (auto it = _endPoints->begin(); it != _endPoints->end(); ++it) {
        it->second->SetInput(nullptr);
        it->second->SetOutput(nullptr);
    }
    _endPoints->clear();
#endif
    _endPoints->clear();
}

bool MediaTranslatorsManager::Translator::AddPacket(RtpPacket* packet)
{
    if (packet) {
        packet->SetPayloadType(_producer->GetPayloadType(packet->GetSsrc()));
        return _manager->SendRtpPacket(_producer->GetProducer(), packet);
    }
    return false;
}

void MediaTranslatorsManager::Translator::AddConsumer(Consumer* consumer)
{
    if (consumer) {
        LOCK_WRITE_PROTECTED_OBJ(_endPoints);
        auto& endPoints = _endPoints.Ref();
        if (endPoints.end() == endPoints.find(consumer)) {
            auto endPoint = std::make_shared<TranslatorEndPoint>(_serviceUri,
                                                                 _serviceUser,
                                                                 _servicePassword);
#ifdef NO_TRANSLATION_SERVICE
            if (_endPoints->empty()) {
                _producer->AddSink(&_manager->_packetsPlayer);
            }
#else
            endPoint->SetInputLanguageId(_producer->GetLanguageId());
            endPoint->SetOutputLanguageId(consumer->GetLanguageId());
            endPoint->SetOutputVoiceId(consumer->GetVoiceId());
            endPoint->SetInput(_producer.get());
            endPoint->SetOutput(&_manager->_packetsPlayer);
#endif
            endPoints[consumer] = std::move(endPoint);
        }
    }
}

bool MediaTranslatorsManager::Translator::RemoveConsumer(Consumer* consumer)
{
    if (consumer) {
        LOCK_WRITE_PROTECTED_OBJ(_endPoints);
        const auto it = _endPoints->find(consumer);
        if (it != _endPoints->end()) {
#ifdef NO_TRANSLATION_SERVICE
            if (1UL == _endPoints->size()) {
                _producer->RemoveSink(&_manager->_packetsPlayer);
            }
#else
            it->second->SetInput(nullptr);
            it->second->SetOutput(nullptr);
#endif
            _endPoints->erase(it);
            return true;
        }
    }
    return false;
}

void MediaTranslatorsManager::Translator::UpdateProducerLanguage()
{
#ifndef NO_TRANSLATION_SERVICE
    LOCK_READ_PROTECTED_OBJ(_endPoints);
    for (auto it = _endPoints.ConstRef().begin(); it != _endPoints.ConstRef().end(); ++it) {
        it->second->SetInputLanguageId(_producer->GetLanguageId());
    }
#endif
}

void MediaTranslatorsManager::Translator::UpdateConsumerLanguage(Consumer* consumer)
{
#ifndef NO_TRANSLATION_SERVICE
    if (consumer) {
        LOCK_READ_PROTECTED_OBJ(_endPoints);
        const auto it = _endPoints->find(consumer);
        if (it != _endPoints->end()) {
            it->second->SetOutputLanguageId(consumer->GetLanguageId());
        }
    }
#endif
}

void MediaTranslatorsManager::Translator::UpdateConsumerVoice(Consumer* consumer)
{
#ifndef NO_TRANSLATION_SERVICE
    if (consumer) {
        LOCK_READ_PROTECTED_OBJ(_endPoints);
        const auto it = _endPoints->find(consumer);
        if (it != _endPoints->end()) {
            it->second->SetOutputVoiceId(consumer->GetVoiceId());
        }
    }
#endif
}

void MediaTranslatorsManager::Translator::AddNewRtpStream(RtpStreamRecv* rtpStream,
                                                          uint32_t mappedSsrc)
{
    if (_producer->AddStream(rtpStream, mappedSsrc)) {
        _manager->_packetsPlayer.AddStream(rtpStream->GetSsrc(), rtpStream->GetMimeType(), this, _producer.get());
    }
    else {
        const auto desc = GetStreamInfoString(mappedSsrc, rtpStream);
        MS_ERROR("failed to register stream [%s] for producer %s", desc.c_str(), _producer->GetId().c_str());
    }
}

bool MediaTranslatorsManager::Translator::DispatchProducerPacket(RtpPacket* packet)
{
    if (packet && !packet->IsSynthenized() && _producer->AddPacket(packet)) {
        LOCK_READ_PROTECTED_OBJ(_endPoints);
        for (auto it = _endPoints->begin(); it != _endPoints->end(); ++it) {
#ifdef NO_TRANSLATION_SERVICE
            if (it->second->IsConnected()) {
#else
            if (true) { // always
#endif
                // drop packet's dispatching if connection with translation service was established
                packet->AddRejectedConsumer(it->first);
            }
        }
        return true;
    }
    return false;
}

} // namespace RTC
