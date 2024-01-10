#pragma once

#include "common.hpp"
#include "RTC/TransportListener.hpp"
#include "RTC/MediaTranslate/ProducerObserver.hpp"
#include "absl/container/flat_hash_map.h"
#include <string>
#include <list>

#define WRITE_PRODUCER_RECV_TO_FILE
#define SINGLE_TRANSLATION_POINT_CONNECTION

namespace RTC
{

class TranslatorUnit;
class ConsumerTranslatorSettings;
class RtpStream;
class ProducerTranslator;
class ConsumerTranslator;
class Producer;
class Consumer;
class FileWriter;

class MediaTranslatorsManager : public TransportListener, private ProducerObserver
{
    class Impl;
    using ConsumerTranslatorsList = std::list<std::shared_ptr<ConsumerTranslator>>;
    template<class TTranslatorUnit>
    using AudioTranslators = absl::flat_hash_map<std::string, std::shared_ptr<TTranslatorUnit>>;
public:
    MediaTranslatorsManager(TransportListener* router,
                            const std::string& serviceUri,
                            const std::string& serviceUser = std::string(),
                            const std::string& servicePassword = std::string());
    ~MediaTranslatorsManager();
    // producers API
    std::shared_ptr<TranslatorUnit> GetTranslatorSettings(const Producer* producer) const;
    // consumers API
    std::shared_ptr<ConsumerTranslatorSettings> GetTranslatorSettings(const Consumer* consumer) const;
    // impl. of TransportListener
    void OnTransportNewProducer(Transport* transport, Producer* producer) final;
    void OnTransportProducerLanguageChanged(RTC::Transport* transport, RTC::Producer* producer) final;
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
    void OnTransportConsumerLanguageChanged(RTC::Transport* transport, RTC::Consumer* consumer) final;
    void OnTransportConsumerVoiceChanged(RTC::Transport* transport, RTC::Consumer* consumer) final;
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
    bool Register(Producer* producer); // audio only
    std::shared_ptr<ProducerTranslator> GetRegistered(const Producer* producer) const;
    std::shared_ptr<ProducerTranslator> GetRegisteredProducer(const std::string& id) const;
    bool UnRegister(const Producer* producer);
    void AddProducerStream(const std::string& id, const RtpStream* stream, uint32_t mappedSsrc) const;
    void AddProducerStream(const std::shared_ptr<ProducerTranslator>& audioProducer,
                           const RtpStream* stream, uint32_t mappedSsrc) const;
    // consumers API
    bool Register(Consumer* consumer, const std::string& producerId);
    std::shared_ptr<ConsumerTranslator> GetRegistered(const Consumer* consumer) const;
    std::shared_ptr<ConsumerTranslator> GetRegisteredConsumer(const std::string& id) const;
    ConsumerTranslatorsList GetAssociated(const std::shared_ptr<ProducerTranslator>& producer) const;
    ConsumerTranslatorsList GetAssociated(const std::string& producerId) const;
    ConsumerTranslatorsList GetAssociated(const Producer* producer) const;
    bool UnRegister(const Consumer* consumer);
    // impl. of ProducerObserver
#ifdef WRITE_PRODUCER_RECV_TO_FILE
    void onStreamAdded(const std::string& producerId, uint32_t mappedSsrc,
                       const RtpCodecMimeType& /*mime*/, uint32_t /*clockRate*/) final;
#endif
private:
    TransportListener* const _router;
    const std::string _serviceUri;
    const std::string _serviceUser;
    const std::string _servicePassword;
    // key is audio producer ID
    AudioTranslators<ProducerTranslator> _audioProducers;
    // key is audio consumer ID
    AudioTranslators<ConsumerTranslator> _audioConsumers;
#ifdef WRITE_PRODUCER_RECV_TO_FILE
    absl::flat_hash_map<std::string, std::unique_ptr<FileWriter>> _fileWriters;
#endif
#ifdef SINGLE_TRANSLATION_POINT_CONNECTION
    bool _alreadyHasTranslationPointConnection = false;
#endif
};

} // namespace RTC
