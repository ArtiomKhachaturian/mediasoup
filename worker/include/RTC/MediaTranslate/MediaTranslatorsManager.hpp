#pragma once

#include "common.hpp"
#include "DepLibUV.hpp"
#include "ProtectedObj.hpp"
#include "RTC/TransportListener.hpp"
#include "absl/container/flat_hash_map.h"
#include <string>
#include <list>
#include <atomic>

#define SINGLE_TRANSLATION_POINT_CONNECTION
#define NO_TRANSLATION_SERVICE

namespace RTC
{

class MediaFrameSerializationFactory;

class MediaTranslatorsManager : public TransportListener
{
    class Translator;
    using PacketInfo = std::pair<bool, RtpPacket*>; // 1st is flag to router or no
    using PacketsList = std::list<PacketInfo>;
public:
    MediaTranslatorsManager(TransportListener* router,
                            const std::string& serviceUri,
                            const std::string& serviceUser = std::string(),
                            const std::string& servicePassword = std::string());
    ~MediaTranslatorsManager();
    // impl. of TransportListener
    void OnTransportConnected(Transport* transport) final;
    void OnTransportDisconnected(Transport* transport) final;
    void OnTransportDestroyed(Transport* /*transport*/) final {}
    void OnTransportNewProducer(Transport* transport, Producer* producer) final;
    void OnTransportProducerLanguageChanged(Transport* transport, Producer* producer) final;
    void OnTransportProducerClosed(Transport* transport, Producer* producer) final;
    void OnTransportProducerPaused(Transport* transport, Producer* producer) final;
    void OnTransportProducerResumed(Transport* transport, Producer* producer) final;
    void OnTransportProducerNewRtpStream(Transport* transport, Producer* producer,
                                         RtpStreamRecv* rtpStream, uint32_t mappedSsrc) final;
    void OnTransportProducerRtpStreamScore(Transport* transport, Producer* producer,
                                           RtpStreamRecv* rtpStream,
                                           uint8_t score, uint8_t previousScore) final;
    void OnTransportProducerRtcpSenderReport(Transport* transport, Producer* producer,
                                             RtpStreamRecv* rtpStream, bool first) final;
    void OnTransportProducerRtpPacketReceived(Transport* transport, Producer* producer,
                                              RtpPacket* packet) final;
    void OnTransportNeedWorstRemoteFractionLost(Transport* transport, Producer* producer,
                                                uint32_t mappedSsrc,
                                                uint8_t& worstRemoteFractionLost) final;
    void OnTransportNewConsumer(Transport* transport, Consumer* consumer,
                                const std::string& producerId) final;
    void OnTransportConsumerLanguageChanged(Transport* transport, Consumer* consumer) final;
    void OnTransportConsumerVoiceChanged(Transport* transport, Consumer* consumer) final;
    void OnTransportConsumerClosed(Transport* transport, Consumer* consumer) final;
    void OnTransportConsumerProducerClosed(Transport* transport, Consumer* consumer) final;
    void OnTransportDataProducerPaused(Transport* transport, DataProducer* dataProducer) final;
    void OnTransportDataProducerResumed(Transport* transport, DataProducer* dataProducer) final;
    void OnTransportConsumerKeyFrameRequested(Transport* transport, Consumer* consumer,
                                              uint32_t mappedSsrc) final;
    void OnTransportNewDataProducer(Transport* transport, DataProducer* dataProducer) final;
    void OnTransportDataProducerClosed(Transport* transport, DataProducer* dataProducer) final;
    void OnTransportDataProducerMessageReceived(Transport* transport,
                                                DataProducer* dataProducer,
                                                const uint8_t* msg,
                                                size_t len,
                                                uint32_t ppid,
                                                const std::vector<uint16_t>& subchannels,
                                                const std::optional<uint16_t>& requiredSubchannel) final;
    void OnTransportNewDataConsumer(Transport* transport, DataConsumer* dataConsumer,
                                    const std::string& dataProducerId);
    void OnTransportDataConsumerClosed(Transport* transport, DataConsumer* dataConsumer) final;
    void OnTransportDataConsumerDataProducerClosed(Transport* transport,
                                                   DataConsumer* dataConsumer) final;
    void OnTransportListenServerClosed(Transport* transport) final;
private:
    static void ProcessDefferedPackets(uv_async_t* handle);
    bool ProcessRtpPacket(Producer* producer, RtpPacket* packet, bool toRouter);
    bool SendRtpPacket(Producer* producer, RtpPacket* packet, bool toRouter);
private:
    TransportListener* const _router;
    const std::string _serviceUri;
    const std::string _serviceUser;
    const std::string _servicePassword;
    const std::shared_ptr<MediaFrameSerializationFactory> _serializationFactory;
    uv_loop_t* const _ownerLoop;
    uv_async_t _asynHandle;
    // key is audio producer ID
    absl::flat_hash_map<std::string, std::unique_ptr<Translator>> _translators;
    ProtectedObj<Transport*> _connectedTransport = nullptr;
    ProtectedObj<absl::flat_hash_map<Producer*, PacketsList>> _defferedPackets;
};

} // namespace RTC
