#pragma once

#include "RTC/RtpDictionaries.hpp"
#include "RTC/RtpPacketsCollector.hpp"
#include "RTC/MediaTranslate/ProducerTranslatorSettings.hpp"
#include "RTC/MediaTranslate/ProducerObserver.hpp"
#include "ProtectedObj.hpp"
#include <absl/container/flat_hash_map.h>
#include <list>

#define WRITE_PRODUCER_RECV_TO_FILE

namespace RTC
{

class ProducerObserver;
class RtpMediaFrameSerializer;
class RtpStream;
class RtpCodecMimeType;
class MediaPacketsSink;
#ifdef WRITE_PRODUCER_RECV_TO_FILE
class MediaFileWriter;
#endif
class Producer;

class ProducerTranslator : public ProducerTranslatorSettings, public RtpPacketsCollector
{
    class StreamInfo;
public:
    ProducerTranslator(Producer* producer);
    ~ProducerTranslator() final;
    bool IsAudio() const;
    void AddObserver(ProducerObserver* observer);
    void RemoveObserver(ProducerObserver* observer);
    bool RegisterStream(const RtpStream* stream, uint32_t mappedSsrc);
    bool UnRegisterStream(uint32_t mappedSsrc);
    bool SetSink(const std::shared_ptr<MediaPacketsSink>& sink);
    // list of ssrcs
    std::list<uint32_t> GetRegisteredSsrcs(bool mapped) const;
    // impl. of TranslatorUnit
    const std::string& GetId() const final;
    // impl. of RtpPacketsCollector
    void AddPacket(const RtpPacket* packet) final;
    // impl. of ProducerTranslator
    void SetLanguage(const std::optional<MediaLanguage>& language = std::nullopt) final;
    std::optional<MediaLanguage> GetLanguage() const final { return _language; }
protected:
    template <class Method, typename... Args>
    void InvokeObserverMethod(const Method& method, Args&&... args) const;
    void onProducerStreamRegistered(const std::shared_ptr<StreamInfo>& streamInfo,
                                    uint32_t mappedSsrc, bool registered);
    // impl. of
    void OnPauseChanged(bool pause) final;
#ifdef WRITE_PRODUCER_RECV_TO_FILE
private:
    static std::shared_ptr<MediaFileWriter> CreateFileWriter(const RTC::RtpCodecMimeType& mime,
                                                             uint32_t ssrc, uint32_t sampleRate);
#endif
private:
    Producer* const _producer;
    std::shared_ptr<MediaPacketsSink> _sink;
    std::list<ProducerObserver*> _observers;
    // key is mapped media SSRC
    absl::flat_hash_map<uint32_t, std::shared_ptr<StreamInfo>> _streams;
#ifdef WRITE_PRODUCER_RECV_TO_FILE
    // key is mapped media SSRC
    absl::flat_hash_map<uint32_t, std::shared_ptr<MediaFileWriter>> _mediaFileWriters;
#endif
    // input language
    std::optional<MediaLanguage> _language = DefaultInputMediaLanguage();
};

} // namespace RTC
