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
    virtual void OnTransportNewProducer(RTC::Transport* transport, RTC::Producer* producer) = 0;
    virtual void OnTransportProducerLanguageChanged(RTC::Transport* transport, RTC::Producer* producer) = 0;
    virtual void OnTransportProducerClosed(RTC::Transport* transport, RTC::Producer* producer) = 0;
    virtual void OnTransportProducerPaused(RTC::Transport* transport, RTC::Producer* producer) = 0;
    virtual void OnTransportProducerResumed(RTC::Transport* transport, RTC::Producer* producer) = 0;
    virtual void OnTransportProducerNewRtpStream(
      RTC::Transport* transport,
      RTC::Producer* producer,
      RTC::RtpStreamRecv* rtpStream,
      uint32_t mappedSsrc) = 0;
    virtual void OnTransportProducerRtpStreamScore(
      RTC::Transport* transport,
      RTC::Producer* producer,
      RTC::RtpStreamRecv* rtpStream,
      uint8_t score,
      uint8_t previousScore) = 0;
    virtual void OnTransportProducerRtcpSenderReport(
      RTC::Transport* transport,
      RTC::Producer* producer,
      RTC::RtpStreamRecv* rtpStream,
      bool first) = 0;
    virtual void OnTransportProducerRtpPacketReceived(
      RTC::Transport* transport, RTC::Producer* producer, RTC::RtpPacket* packet) = 0;
    virtual void OnTransportNeedWorstRemoteFractionLost(
      RTC::Transport* transport,
      RTC::Producer* producer,
      uint32_t mappedSsrc,
      uint8_t& worstRemoteFractionLost) = 0;
    virtual void OnTransportNewConsumer(
      RTC::Transport* transport, RTC::Consumer* consumer, const std::string& producerId) = 0;
    virtual void OnTransportConsumerLanguageChanged(RTC::Transport* transport, RTC::Consumer* consumer) = 0;
    virtual void OnTransportConsumerVoiceChanged(RTC::Transport* transport, RTC::Consumer* consumer) = 0;
    virtual void OnTransportConsumerClosed(RTC::Transport* transport, RTC::Consumer* consumer) = 0;
    virtual void OnTransportConsumerProducerClosed(
      RTC::Transport* transport, RTC::Consumer* consumer) = 0;
    virtual void OnTransportDataProducerPaused(
      RTC::Transport* transport, RTC::DataProducer* dataProducer) = 0;
    virtual void OnTransportDataProducerResumed(
      RTC::Transport* transport, RTC::DataProducer* dataProducer) = 0;
    virtual void OnTransportConsumerKeyFrameRequested(
      RTC::Transport* transport, RTC::Consumer* consumer, uint32_t mappedSsrc) = 0;
    virtual void OnTransportNewDataProducer(
      RTC::Transport* transport, RTC::DataProducer* dataProducer) = 0;
    virtual void OnTransportDataProducerClosed(
      RTC::Transport* transport, RTC::DataProducer* dataProducer) = 0;
    virtual void OnTransportDataProducerMessageReceived(
      RTC::Transport* transport,
      RTC::DataProducer* dataProducer,
      const uint8_t* msg,
      size_t len,
      uint32_t ppid,
      const std::vector<uint16_t>& subchannels,
      const std::optional<uint16_t>& requiredSubchannel) = 0;
    virtual void OnTransportNewDataConsumer(
      RTC::Transport* transport, RTC::DataConsumer* dataConsumer, const std::string& dataProducerId) = 0;
    virtual void OnTransportDataConsumerClosed(
      RTC::Transport* transport, RTC::DataConsumer* dataConsumer) = 0;
    virtual void OnTransportDataConsumerDataProducerClosed(
      RTC::Transport* transport, RTC::DataConsumer* dataConsumer)         = 0;
    virtual void OnTransportListenServerClosed(RTC::Transport* transport) = 0;
};


} // namespace RTC

#endif
