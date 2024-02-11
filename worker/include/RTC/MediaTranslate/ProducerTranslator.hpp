#pragma once

#include "RTC/RtpDictionaries.hpp"
#include "RTC/RtpPacketsCollector.hpp"
#include "RTC/MediaTranslate/RtpPacketsInfoProvider.hpp"
#include "RTC/MediaTranslate/TranslationEndPoint/TranslatorEndPointListener.hpp"
#include "RTC/MediaTranslate/MediaSource.hpp"
#include "ProtectedObj.hpp"
#include <absl/container/flat_hash_map.h>

//#define WRITE_PRODUCER_RECV_TO_FILE // add MEDIASOUP_DEPACKETIZER_PATH env variable for reference to output folder
//#define READ_PRODUCER_RECV_FROM_FILE
#define NO_TRANSLATION_SERVICE
#define SINGLE_TRANSLATION_POINT_CONNECTION

namespace RTC
{

class MediaSink;
class RtpStream;
class RtpPacketsPlayer;
class Producer;
class Consumer;
class TranslatorEndPoint;
class RtpPacket;

class ProducerTranslator : private MediaSource,
                           private TranslatorEndPointListener, // for receiving of translated packets
                           private RtpPacketsInfoProvider
{
    class StreamInfo;
    template <typename T>
    using StreamsMap = absl::flat_hash_map<uint32_t, T>;
    using TranslationEndPointsMap = absl::flat_hash_map<Consumer*, std::unique_ptr<TranslatorEndPoint>>;
public:
    ~ProducerTranslator() final;
    static std::unique_ptr<ProducerTranslator> Create(const Producer* producer,
                                                      RtpPacketsPlayer* translationsOutput,
                                                      const std::string& serviceUri,
                                                      const std::string& serviceUser,
                                                      const std::string& servicePassword);
    bool AddStream(uint32_t mappedSsrc, const RtpStream* stream);
    bool RemoveStream(uint32_t mappedSsrc);
    void AddOriginalRtpPacketForTranslation(RtpPacket* packet);
    const std::string& GetId() const;
    // list of mapped or original ssrcs for added streams
    std::list<uint32_t> GetSsrcs(bool mapped) const;
    void AddConsumer(Consumer* consumer);
    void RemoveConsumer(Consumer* consumer);
    void UpdateProducerLanguage();
    void UpdateConsumerLanguage(Consumer* consumer);
    void UpdateConsumerVoice(Consumer* consumer);
private:
    ProducerTranslator(const Producer* producer, RtpPacketsPlayer* translationsOutput,
                       const std::string& serviceUri,
                       const std::string& serviceUser,
                       const std::string& servicePassword);
    // SSRC maybe mapped or original
    std::shared_ptr<StreamInfo> GetStream(uint32_t ssrc) const;
    void AddSinksToStream(const std::shared_ptr<StreamInfo>& stream) const;
    // impl. of MediaSource
    bool AddSink(MediaSink* sink) final;
    bool RemoveSink(MediaSink* sink) final;
    void RemoveAllSinks() final;
    bool HasSinks() const final;
    size_t GetSinksCout() const final;
    // impl. of TranslatorEndPointListener
    void OnTranslatedMediaReceived(uint64_t endPointId, uint64_t mediaSeqNum,
                                   const std::shared_ptr<MemoryBuffer>& media) final;
    // impl. of RtpPacketsInfoProvider
    uint8_t GetPayloadType(uint32_t ssrc) const final;
    uint16_t GetLastOriginalRtpSeqNumber(uint32_t ssrc) const final;
    uint32_t GetLastOriginalRtpTimestamp(uint32_t ssrc) const final;
    uint32_t GetClockRate(uint32_t ssrc) const final;
private:
#ifdef NO_TRANSLATION_SERVICE
    static inline const char* _mockTranslationFileName = "/Users/user/Documents/Sources/mediasoup_rtp_packets/received_translation_stereo_example.webm";
#endif
    const Producer* const _producer;
    RtpPacketsPlayer* const _translationsOutput;
    const std::string _serviceUri;
    const std::string _serviceUser;
    const std::string _servicePassword;
#ifdef SINGLE_TRANSLATION_POINT_CONNECTION
    const uint64_t _instanceIndex;
#endif
    // key is mapped media SSRC
    ProtectedObj<StreamsMap<std::shared_ptr<StreamInfo>>> _mappedSsrcToStreams;
    // key is original media SSRC, under protection of [_mappedSsrcToStreams]
    StreamsMap<std::weak_ptr<StreamInfo>> _originalSsrcToStreams;
    ProtectedObj<std::list<MediaSink*>> _sinks;
    // TODO: revise this logic for better resources consumption if more than 1 consumers has the same language and voice
    ProtectedObj<TranslationEndPointsMap> _endPoints;
};

} // namespace RTC
