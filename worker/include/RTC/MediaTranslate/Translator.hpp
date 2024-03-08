#pragma once
#include "RTC/MediaTranslate/TranslatorEndPoint/TranslatorEndPointFactory.hpp"
#include "RTC/MediaTranslate/TranslatorDefines.hpp"
#include "RTC/Buffers/BufferAllocations.hpp"
#include "RTC/RtpDictionaries.hpp"
#include "RTC/RtpPacketsCollector.hpp"
#include <unordered_map>

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
    template <typename K, typename V> using Map = std::unordered_map<K, V>;
    template <typename V> using StreamMap = Map<uint32_t, V>;
    class ConsumerTranslatorImpl;
public:
    ~Translator() final;
    static std::unique_ptr<Translator> Create(const Producer* producer,
                                              const WebsocketFactory* websocketFactory,
                                              RtpPacketsPlayer* rtpPacketsPlayer,
                                              RtpPacketsCollector* output,
                                              const std::shared_ptr<BufferAllocator>& allocator = nullptr);
    bool AddStream(const RtpStream* stream, uint32_t mappedSsrc);
    bool RemoveStream(uint32_t ssrc);
    void AddOriginalRtpPacketForTranslation(RtpPacket* packet);
    void AddConsumer(const Consumer* consumer);
    void RemoveConsumer(const Consumer* consumer);
    void SetProducerPaused(bool paused);
    void SetProducerLanguageId(std::string languageId);
    const std::string& GetProducerLanguageId() const { return _producerLanguageId; }
    void UpdateConsumerLanguageOrVoice(const Consumer* consumer);
private:
    Translator(const Producer* producer,
               const WebsocketFactory* websocketFactory,
               RtpPacketsPlayer* rtpPacketsPlayer,
               RtpPacketsCollector* output,
               const std::shared_ptr<BufferAllocator>& allocator);
    const std::string& GetId() const { return _producerId; }
    void AddConsumersToSource(TranslatorSource* source) const;
#ifdef NO_TRANSLATION_SERVICE
    std::shared_ptr<TranslatorEndPoint> CreateStubEndPoint() const;
    std::shared_ptr<TranslatorEndPoint> CreateMaybeFileEndPoint() const;
#else
    std::shared_ptr<TranslatorEndPoint> CreateMaybeStubEndPoint() const;
#endif
    // impl. of TranslatorEndPointFactory
    std::shared_ptr<TranslatorEndPoint> CreateEndPoint() final;
private:
    const std::string _producerId;
#ifndef NO_TRANSLATION_SERVICE
    const WebsocketFactory* const _websocketFactory;
#endif
    RtpPacketsPlayer* const _rtpPacketsPlayer;
    RtpPacketsCollector* const _output;
#if defined(SINGLE_TRANSLATION_POINT_CONNECTION) || defined(NO_TRANSLATION_SERVICE)
    // websocket or file end-point, valid for 1st created instance, just for debug
    mutable std::weak_ptr<TranslatorEndPoint> _nonStubEndPointRef;
#endif
    bool _producerPaused = false;
    std::string _producerLanguageId;
    // key is original SSRC
    StreamMap<std::unique_ptr<TranslatorSource>> _originalSsrcToStreams;
    // key is mapped SSRC
    StreamMap<uint32_t> _mappedSsrcToOriginal;
    // key is consumer integer ID
    Map<uint64_t, std::shared_ptr<ConsumerTranslatorImpl>> _consumers;
};

} // namespace RTC
