#pragma once

#include "RTC/RtpDictionaries.hpp"
#include "RTC/RtpPacketsCollector.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerCallback.hpp"
#include "RTC/MediaTranslate/TranslatorEndPoint/TranslatorEndPointFactory.hpp"
#include "ProtectedObj.hpp"
#include <absl/container/flat_hash_map.h>

//#define WRITE_PRODUCER_RECV_TO_FILE // add MEDIASOUP_DEPACKETIZER_PATH env variable for reference to output folder
//#define READ_PRODUCER_RECV_FROM_FILE
#define NO_TRANSLATION_SERVICE
#define SINGLE_TRANSLATION_POINT_CONNECTION

namespace RTC
{

class RtpStream;
class RtpPacketsPlayer;
class Producer;
class Consumer;
class RtpPacket;
class RtpPacketsCollector;

class Translator : private TranslatorEndPointFactory
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
    // returns true if packet was sent to translation service
    // and further processing by other SFU components no longer needed
    bool AddOriginalRtpPacketForTranslation(RtpPacket* packet);
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
    void AddConsumersToStream(const std::shared_ptr<SourceStream>& stream) const;
    void PostProcessAfterAdding(RtpPacket* packet, bool added,
                                const std::shared_ptr<SourceStream>& stream);
#ifdef NO_TRANSLATION_SERVICE
    std::shared_ptr<TranslatorEndPoint> CreateStubEndPoint() const;
    std::shared_ptr<TranslatorEndPoint> CreateMaybeFileEndPoint() const;
#else
    std::shared_ptr<TranslatorEndPoint> CreateMaybeStubEndPoint() const;
#endif
    // impl. of TranslatorEndPointFactory
    std::shared_ptr<TranslatorEndPoint> CreateEndPoint() final;
private:
#ifdef NO_TRANSLATION_SERVICE
    static inline const char* _mockTranslationFileName = "/Users/user/Documents/Sources/mediasoup_rtp_packets/received_translation_stereo_example.webm";
    static inline constexpr uint32_t _mockTranslationFileNameLenMs = 3000; // 3 sec
#endif
    const Producer* const _producer;
    RtpPacketsPlayer* const _rtpPacketsPlayer;
    RtpPacketsCollector* const _output;
#if defined(SINGLE_TRANSLATION_POINT_CONNECTION) || defined(NO_TRANSLATION_SERVICE)
    // websocket or file end-point, valid for 1st created instance, just for debug
    mutable std::weak_ptr<TranslatorEndPoint> _nonStubEndPointRef;
#endif
    // key is mapped media SSRC
    ProtectedObj<StreamMap<std::shared_ptr<SourceStream>>> _mappedSsrcToStreams;
    // key is original media SSRC, under protection of [_mappedSsrcToStreams]
    StreamMap<std::weak_ptr<SourceStream>> _originalSsrcToStreams;
    ProtectedObj<std::list<Consumer*>> _consumers;
};

} // namespace RTC
