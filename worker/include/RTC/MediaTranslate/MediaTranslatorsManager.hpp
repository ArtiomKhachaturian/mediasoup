#pragma once

#include "common.hpp"
#include "RTC/TransportListener.hpp"
#include <string>

#define WRITE_PRODUCER_RECV_TO_FILE
//#define DEBUG_UNITED_PRODUCER_MEDIA

namespace RTC
{

class ProducerTranslatorSettings;
class ConsumerTranslatorSettings;
class Producer;
class Consumer;

class MediaTranslatorsManager : public TransportListener
{
    class Impl;
public:
    MediaTranslatorsManager(TransportListener* router,
                            const std::string& serviceUri,
                            const std::string& serviceUser = std::string(),
                            const std::string& servicePassword = std::string());
    ~MediaTranslatorsManager();
    // producers API
    std::weak_ptr<ProducerTranslatorSettings> GetTranslatorSettings(const Producer* producer) const;
    // consumers API
    std::weak_ptr<ConsumerTranslatorSettings> GetTranslatorSettings(const Consumer* consumer) const;
    // impl. of TransportListener
    void OnTransportNewProducer(Transport* transport, Producer* producer) final;
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
    void OnTransportConsumerClosed(Transport* transport, Consumer* consumer) final;
    void OnTransportConsumerProducerClosed(Transport* transport, Consumer* consumer) final;
    void OnTransportConsumerKeyFrameRequested(Transport* transport, Consumer* consumer,
                                              uint32_t mappedSsrc) final;
    void OnTransportNewDataProducer(Transport* transport, DataProducer* dataProducer) final;
    void OnTransportDataProducerClosed(Transport* transport, DataProducer* dataProducer) final;
    void OnTransportDataProducerMessageReceived(Transport* transport, DataProducer* dataProducer,
                                                uint32_t ppid, const uint8_t* msg, size_t len) final;
    void OnTransportNewDataConsumer(Transport* transport, DataConsumer* dataConsumer,
                                    const std::string& dataProducerId);
    void OnTransportDataConsumerClosed(Transport* transport, DataConsumer* dataConsumer) final;
    void OnTransportDataConsumerDataProducerClosed(Transport* transport,
                                                   DataConsumer* dataConsumer) final;
    void OnTransportListenServerClosed(Transport* transport) final;
private:
    TransportListener* const _router;
    const std::shared_ptr<Impl> _impl;
};

} // namespace RTC
