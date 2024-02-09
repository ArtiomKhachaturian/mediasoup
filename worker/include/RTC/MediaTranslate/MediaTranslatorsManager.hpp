#pragma once

#include "common.hpp"
#include "DepLibUV.hpp"
#include "ProtectedObj.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayer.hpp"
#include "RTC/TransportListener.hpp"
#include "absl/container/flat_hash_map.h"
#include <string>
#include <list>
#include <atomic>

#define SINGLE_TRANSLATION_POINT_CONNECTION
#define NO_TRANSLATION_SERVICE
#define USE_MAIN_THREAD_FOR_PACKETS_RETRANSMISSION

namespace RTC
{

class MediaTranslatorsManager : public TransportListener
{
    class Translator;
#ifdef USE_MAIN_THREAD_FOR_PACKETS_RETRANSMISSION
    using PacketsList = std::list<RtpPacket*>;
    class UVAsyncHandle;
#endif
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
#ifdef USE_MAIN_THREAD_FOR_PACKETS_RETRANSMISSION
    static void PlaybackDefferedRtpPackets(uv_async_t* handle);
    void PlaybackDefferedRtpPackets();
    void PlaybackDefferedRtpPackets(Producer* producer, PacketsList packets);
    bool HasConnectedTransport() const;
#endif
    bool PlaybackRtpPacket(Producer* producer, RtpPacket* packet);
    bool SendRtpPacket(Producer* producer, RtpPacket* packet);
private:
    // 1 sec for 20ms OPUS audio frames
    static inline constexpr size_t _defferedPacketsBatchSize = 50UL;
    TransportListener* const _router;
    const std::string _serviceUri;
    const std::string _serviceUser;
    const std::string _servicePassword;
#ifdef USE_MAIN_THREAD_FOR_PACKETS_RETRANSMISSION
    const std::unique_ptr<UVAsyncHandle> _async;
    ProtectedObj<absl::flat_hash_map<Producer*, PacketsList>> _defferedPackets;
#endif
    RtpPacketsPlayer _packetsPlayer;
    // key is audio producer ID
    absl::flat_hash_map<std::string, std::unique_ptr<Translator>> _translators;
    ProtectedObj<Transport*> _connectedTransport = nullptr;
    uint64_t _sendPackets = 0ULL;
};

} // namespace RTC
