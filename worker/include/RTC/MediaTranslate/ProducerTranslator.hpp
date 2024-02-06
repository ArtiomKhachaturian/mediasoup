#pragma once

#include "RTC/RtpDictionaries.hpp"
#include "RTC/RtpPacketsCollector.hpp"
#include "RTC/MediaTranslate/RtpPacketsInfoProvider.hpp"
#include "RTC/MediaTranslate/TranslatorUnit.hpp"
#include "RTC/MediaTranslate/ProducerObserver.hpp"
#include "RTC/MediaTranslate/MediaSource.hpp"
#include "RTC/Listeners.hpp"
#include <absl/container/flat_hash_map.h>
#include <list>

//#define WRITE_PRODUCER_RECV_TO_FILE // add MEDIASOUP_DEPACKETIZER_PATH env variable for reference to output folder
//#define READ_PRODUCER_RECV_FROM_FILE

namespace RTC
{

class MediaSink;
class MediaFrameSerializationFactory;
class ProducerObserver;
class RtpStream;
class Producer;

class ProducerTranslator : public TranslatorUnit,
                           public RtpPacketsCollector,
                           public RtpPacketsInfoProvider,
                           public MediaSource
{
    class StreamInfo;
public:
    ProducerTranslator(Producer* producer, const std::shared_ptr<MediaFrameSerializationFactory>& serializationFactory);
    ~ProducerTranslator() final;
    static std::unique_ptr<ProducerTranslator> Create(Producer* producer,
                                                      const std::shared_ptr<MediaFrameSerializationFactory>& serializationFactory);
    Producer* GetProducer() const { return _producer; }
    bool IsAudio() const;
    void AddObserver(ProducerObserver* observer);
    void RemoveObserver(ProducerObserver* observer);
    bool AddStream(const RtpStream* stream, uint32_t mappedSsrc);
    bool AddStream(const RtpCodecMimeType& mime, uint32_t clockRate, uint32_t mappedSsrc,
                   uint32_t originalSsrc, uint8_t payloadType);
    bool RemoveStream(uint32_t mappedSsrc);
    // list of mapped ssrcs
    std::list<uint32_t> GetAddedStreams() const;
    // impl. of TranslatorUnit
    const std::string& GetId() const final;
    // impl. of RtpPacketsCollector
    bool AddPacket(RtpPacket* packet) final;
    // impl. of RtpPacketsInfoProvider
    uint8_t GetPayloadType(uint32_t ssrc) const final;
    uint16_t GetLastOriginalRtpSeqNumber(uint32_t ssrc) const final;
    uint32_t GetLastOriginalRtpTimestamp(uint32_t ssrc) const final;
    uint32_t GetClockRate(uint32_t ssrc) const final;
    // impl. of TranslatorUnit
    std::optional<FBS::TranslationPack::Language> GetLanguage() const final;
    // impl. of MediaSource
    bool AddSink(MediaSink* sink) final;
    bool RemoveSink(MediaSink* sink) final;
    void RemoveAllSinks() final;
    bool HasSinks() const final;
    size_t GetSinksCout() const final;
protected:
    // SSRC maybe mapped or original
    std::shared_ptr<StreamInfo> GetStream(uint32_t ssrc) const;
    template <class Method, typename... Args>
    void InvokeObserverMethod(const Method& method, Args&&... args) const;
    // override of TranslatorUnit
    void OnPauseChanged(bool pause) final;
private:
    Producer* const _producer;
    const std::shared_ptr<MediaFrameSerializationFactory> _serializationFactory;
    Listeners<ProducerObserver*> _observers;
    // key is mapped media SSRC
    ProtectedObj<absl::flat_hash_map<uint32_t, std::shared_ptr<StreamInfo>>> _streams;
    // key is original SSRC, value - mapped media SSRC
    ProtectedObj<absl::flat_hash_map<uint32_t, uint32_t>> _originalToMappedSsrcs;
    ProtectedObj<std::list<MediaSink*>> _sinks;
};

} // namespace RTC
