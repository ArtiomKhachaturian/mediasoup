#ifndef MS_RTC_TRANSPORT_LISTENER_HPP
#define MS_RTC_TRANSPORT_LISTENER_HPP

#include <string>


namespace RTC
{

class Transport;
class Producer;
class Consumer;
class RtpStreamRecv;
class RtpPacket;
class DataProducer;
class DataConsumer;

class TransportListener
{
public:
	virtual ~TransportListener() = default;

public:
	virtual void OnTransportNewProducer(Transport* transport, Producer* producer) = 0;
	virtual void OnTransportProducerClosed(Transport* transport, Producer* producer) = 0;
	virtual void OnTransportProducerPaused(Transport* transport, Producer* producer) = 0;
	virtual void OnTransportProducerResumed(Transport* transport, Producer* producer) = 0;
	virtual void OnTransportProducerNewRtpStream(Transport* transport, Producer* producer,
                                                 RtpStreamRecv* rtpStream, uint32_t mappedSsrc) = 0;
	virtual void OnTransportProducerRtpStreamScore(Transport* transport, Producer* producer,
                                                   RtpStreamRecv* rtpStream,
                                                   uint8_t score, uint8_t previousScore) = 0;
	virtual void OnTransportProducerRtcpSenderReport(Transport* transport, Producer* producer,
                                                     RtpStreamRecv* rtpStream, bool first) = 0;
	virtual void OnTransportProducerRtpPacketReceived(Transport* transport, Producer* producer,
                                                      RtpPacket* packet) = 0;
	virtual void OnTransportNeedWorstRemoteFractionLost(Transport* transport, Producer* producer,
                                                        uint32_t mappedSsrc,
                                                        uint8_t& worstRemoteFractionLost) = 0;
	virtual void OnTransportNewConsumer(Transport* transport, Consumer* consumer,
                                        const std::string& producerId) = 0;
	virtual void OnTransportConsumerClosed(Transport* transport, Consumer* consumer) = 0;
	virtual void OnTransportConsumerProducerClosed(Transport* transport, Consumer* consumer) = 0;
	virtual void OnTransportConsumerKeyFrameRequested(Transport* transport, Consumer* consumer,
                                                      uint32_t mappedSsrc) = 0;
	virtual void OnTransportNewDataProducer(Transport* transport, DataProducer* dataProducer) = 0;
	virtual void OnTransportDataProducerClosed(Transport* transport, DataProducer* dataProducer) = 0;
	virtual void OnTransportDataProducerMessageReceived(Transport* transport, DataProducer* dataProducer,
                                                        uint32_t ppid, const uint8_t* msg, size_t len) = 0;
	virtual void OnTransportNewDataConsumer(Transport* transport, DataConsumer* dataConsumer,
                                            const std::string& dataProducerId) = 0;
	virtual void OnTransportDataConsumerClosed(Transport* transport, DataConsumer* dataConsumer) = 0;
	virtual void OnTransportDataConsumerDataProducerClosed(Transport* transport,
                                                           DataConsumer* dataConsumer) = 0;
	virtual void OnTransportListenServerClosed(Transport* transport) = 0;
};


} // namespace RTC

#endif
