#pragma once
#include "RTC/MediaTranslate/Buffers/BufferAllocations.hpp"
#include "RTC/MediaTranslate/TranslatorEndPoint/TranslatorEndPointFactory.hpp"
#include "RTC/MediaTranslate/TranslatorDefines.hpp"
#include "RTC/RtpDictionaries.hpp"
#include "RTC/RtpPacketsCollector.hpp"
#include "ProtectedObj.hpp"
#include <absl/container/flat_hash_map.h>

namespace RTC
{

class RtpStream;
class RtpPacketsPlayer;
class Producer;
class Consumer;
class TranslatorSource;
class RtpPacket;
class RtpPacketsCollector;
class WebsocketFactory;

class Translator : private BufferAllocations<TranslatorEndPointFactory>
{
    template <typename K, typename V> using Map = absl::flat_hash_map<K, V>;
    template <typename V> using StreamMap = Map<uint32_t, V>;
public:
    ~Translator() final;
    static std::unique_ptr<Translator> Create(const Producer* producer,
                                              const WebsocketFactory* websocketFactory,
                                              RtpPacketsPlayer* rtpPacketsPlayer,
                                              RtpPacketsCollector* output,
                                              const std::weak_ptr<BufferAllocator>& allocator);
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
    Translator(const Producer* producer,
               const WebsocketFactory* websocketFactory,
               RtpPacketsPlayer* rtpPacketsPlayer,
               RtpPacketsCollector* output,
               const std::weak_ptr<BufferAllocator>& allocator);
    // SSRC maybe mapped or original
    std::shared_ptr<TranslatorSource> GetSource(uint32_t ssrc) const;
    void AddConsumersToSource(const std::shared_ptr<TranslatorSource>& source) const;
    void PostProcessAfterAdding(RtpPacket* packet, bool added,
                                const std::shared_ptr<TranslatorSource>& source);
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
    static inline const char* _mockTranslationFileName = "/Users/user/Documents/Sources/mediasoup_rtp_packets/received_translation_long.webm";
    static inline constexpr uint32_t _mockTranslationConnectionTimeoutMs = 300U; // 1 sec
    static inline constexpr uint32_t _mockTranslationFileNameLenMs = 17000U; // ~4 sec
#endif
    const Producer* const _producer;
#ifndef NO_TRANSLATION_SERVICE
    const WebsocketFactory* const _websocketFactory;
#endif
    RtpPacketsPlayer* const _rtpPacketsPlayer;
    RtpPacketsCollector* const _output;
#if defined(SINGLE_TRANSLATION_POINT_CONNECTION) || defined(NO_TRANSLATION_SERVICE)
    // websocket or file end-point, valid for 1st created instance, just for debug
    mutable std::weak_ptr<TranslatorEndPoint> _nonStubEndPointRef;
#endif
    // key is mapped media SSRC
    ProtectedObj<StreamMap<std::shared_ptr<TranslatorSource>>> _mappedSsrcToStreams;
    // key is original media SSRC, under protection of [_mappedSsrcToStreams]
    StreamMap<std::weak_ptr<TranslatorSource>> _originalSsrcToStreams;
    ProtectedObj<std::list<Consumer*>> _consumers;
};

} // namespace RTC
