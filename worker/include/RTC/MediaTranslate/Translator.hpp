#pragma once

#include "RTC/RtpDictionaries.hpp"
#include "RTC/RtpPacketsCollector.hpp"
#include "RTC/MediaTranslate/RtpPacketsInfoProvider.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerCallback.hpp"
#include "RTC/MediaTranslate/TranslatorEndPoint/TranslatorEndPointListener.hpp"
#include "RTC/MediaTranslate/TranslatorEndPoint/TranslatorEndPointFactory.hpp"
#include "RTC/MediaTranslate/MediaSource.hpp"
#include "ProtectedObj.hpp"
#include <absl/container/flat_hash_map.h>

//#define WRITE_PRODUCER_RECV_TO_FILE // add MEDIASOUP_DEPACKETIZER_PATH env variable for reference to output folder
#define READ_PRODUCER_RECV_FROM_FILE
#define NO_TRANSLATION_SERVICE
#define SINGLE_TRANSLATION_POINT_CONNECTION

namespace RTC
{

class MediaSink;
class RtpStream;
class RtpPacketsPlayer;
class Producer;
class Consumer;
class RtpPacket;
class RtpPacketsCollector;

class Translator : private MediaSource,
                   private TranslatorEndPointListener, // for receiving of translated packets
                   private RtpPacketsPlayerCallback,
                   private TranslatorEndPointFactory,
                   private RtpPacketsInfoProvider
{
    class SourceStream;
    template <typename K, typename V> using Map = absl::flat_hash_map<K, V>;
    template <typename V> using StreamMap = Map<uint32_t, V>;
public:
    ~Translator() final;
    static std::unique_ptr<Translator> Create(const Producer* producer,
                                              RtpPacketsPlayer* rtpPacketsPlayer,
                                              RtpPacketsCollector* output);
    bool AddStream(uint32_t mappedSsrc, const RtpStream* stream);
    bool RemoveStream(uint32_t mappedSsrc);
    void AddOriginalRtpPacketForTranslation(RtpPacket* packet);
    const std::string& GetId() const;
    // list of mapped or original ssrcs for added streams
    std::list<uint32_t> GetSsrcs(bool mapped) const;
    void AddConsumer(Consumer* consumer);
    void RemoveConsumer(Consumer* consumer);
    void UpdateProducerLanguage();
    void UpdateConsumerLanguageOrVoice(Consumer* consumer);
private:
    Translator(const Producer* producer, RtpPacketsPlayer* rtpPacketsPlayer,
               RtpPacketsCollector* output);
    // SSRC maybe mapped or original
    std::shared_ptr<SourceStream> GetStream(uint32_t ssrc) const;
    void AddSinksToStream(const std::shared_ptr<SourceStream>& stream) const;
    void AddConsumersToStream(const std::shared_ptr<SourceStream>& stream) const;
    void PostProcessAfterAdding(RtpPacket* packet, bool added,
                                const std::shared_ptr<SourceStream>& stream);
#ifdef NO_TRANSLATION_SERVICE
    std::shared_ptr<TranslatorEndPoint> CreateStubEndPoint(bool firstEndPoint, uint32_t ssrc) const;
#else
    std::shared_ptr<TranslatorEndPoint> CreateMaybeStubEndPoint(bool firstEndPoint, uint32_t ssrc) const;
#endif
    // impl. of MediaSource
    bool AddSink(MediaSink* sink) final;
    bool RemoveSink(MediaSink* sink) final;
    void RemoveAllSinks() final;
    bool HasSinks() const final;
    size_t GetSinksCout() const final;
    // impl. of TranslatorEndPointListener
    void OnTranslatedMediaReceived(const TranslatorEndPoint* endPoint, uint64_t mediaSeqNum,
                                   const std::shared_ptr<MemoryBuffer>& media) final;
    // impl. of RtpPacketsPlayerCallback
    void OnPlay(uint32_t rtpTimestampOffset, RtpPacket* packet, uint64_t mediaId, const void* userData) final;
    // impl. of TranslatorEndPointFactory
    std::shared_ptr<TranslatorEndPoint> CreateEndPoint(bool firstEndPoint, uint32_t ssrc) final;
    // impl. of RtpPacketsInfoProvider
    uint8_t GetPayloadType(uint32_t ssrc) const final;
    uint32_t GetClockRate(uint32_t ssrc) const final;
private:
#ifdef NO_TRANSLATION_SERVICE
    static inline const char* _mockTranslationFileName = "/Users/user/Documents/Sources/mediasoup_rtp_packets/received_translation_stereo_example.webm";
    static inline constexpr uint32_t _mockTranslationFileNameLenMs = 3000; // 3 sec
#endif
    const Producer* const _producer;
    RtpPacketsPlayer* const _rtpPacketsPlayer;
    RtpPacketsCollector* const _output;
#ifdef SINGLE_TRANSLATION_POINT_CONNECTION
    const uint64_t _instanceIndex;
#endif
    // key is mapped media SSRC
    ProtectedObj<StreamMap<std::shared_ptr<SourceStream>>> _mappedSsrcToStreams;
    // key is original media SSRC, under protection of [_mappedSsrcToStreams]
    StreamMap<std::weak_ptr<SourceStream>> _originalSsrcToStreams;
    ProtectedObj<std::list<MediaSink*>> _sinks;
    ProtectedObj<std::list<Consumer*>> _consumers;
};

} // namespace RTC
