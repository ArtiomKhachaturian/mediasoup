#pragma once

#include "RTC/RtpDictionaries.hpp"
#include "RTC/RtpPacketsCollector.hpp"
#include "RTC/MediaTranslate/TranslatorUnit.hpp"
#include "RTC/MediaTranslate/MediaSink.hpp"
#include "RTC/MediaTranslate/MediaSource.hpp"
#include "RTC/MediaTranslate/ProducerObserver.hpp"
#include "RTC/MediaTranslate/TranslatorUnit.hpp"
#include <absl/container/flat_hash_map.h>

#define WRITE_PRODUCER_RECV_TO_FILE

namespace RTC
{

class RtpMediaFrameSerializer;
class ProducerObserver;
class RtpStream;
class Producer;
#ifdef WRITE_PRODUCER_RECV_TO_FILE
class FileWriter;
#endif

class ProducerTranslator : public TranslatorUnit,
                           public MediaSource,
                           public RtpPacketsCollector,
                           private MediaSink
{
    class StreamInfo;
public:
    ProducerTranslator(Producer* producer, std::unique_ptr<RtpMediaFrameSerializer> serializer);
    ~ProducerTranslator() final;
    Producer* GetProducer() const { return _producer; }
    bool IsAudio() const;
    void AddObserver(ProducerObserver* observer);
    void RemoveObserver(ProducerObserver* observer);
    bool AddStream(const RtpStream* stream, uint32_t mappedSsrc);
    bool AddStream(const RtpCodecMimeType& mime, uint32_t clockRate, uint32_t mappedSsrc);
    bool RemoveStream(uint32_t mappedSsrc);
    std::string_view GetFileExtension(uint32_t mappedSsrc) const;
    // list of ssrcs
    std::list<uint32_t> GetAddedStreams() const;
    // impl. of TranslatorUnit
    const std::string& GetId() const final;
    // impl. of RtpPacketsCollector
    bool AddPacket(RtpPacket* packet) final;
    // impl. of TranslatorUnit
    std::optional<FBS::TranslationPack::Language> GetLanguage() const final;
protected:
    template <class Method, typename... Args>
    void InvokeObserverMethod(const Method& method, Args&&... args) const;
    // override of TranslatorUnit
    void OnPauseChanged(bool pause) final;
    // impl. of MediaSource
    bool IsSinkValid(const MediaSink* sink) const final;
    void OnFirstSinkAdded() final;
    void OnLastSinkRemoved() final;
private:
    // impl. of MediaSink
    void StartStream(bool restart) noexcept final;
    void BeginWriteMediaPayload(uint32_t ssrc,
                                const std::vector<RtpMediaPacketInfo>& packets) noexcept final;
    void Write(const std::shared_ptr<const MemoryBuffer>& buffer) noexcept final;
    void EndWriteMediaPayload(uint32_t ssrc, const std::vector<RtpMediaPacketInfo>& packets,
                              bool ok) noexcept final;
    void EndStream(bool failure) noexcept final;
private:
    Producer* const _producer;
    const std::unique_ptr<RtpMediaFrameSerializer> _serializer;
    Listeners<ProducerObserver*> _observers;
    // key is mapped media SSRC
    absl::flat_hash_map<uint32_t, std::unique_ptr<StreamInfo>> _streams;
#ifdef WRITE_PRODUCER_RECV_TO_FILE
    // key is mapped SSRC
    absl::flat_hash_map<uint32_t, std::unique_ptr<FileWriter>> _fileWriters;
#endif
};

} // namespace RTC
