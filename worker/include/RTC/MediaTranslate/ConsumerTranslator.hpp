#pragma once

#include "RTC/MediaTranslate/ConsumerTranslatorSettings.hpp"
#include "RTC/MediaTranslate/ConsumerObserver.hpp"
#include "RTC/MediaTranslate/MediaSink.hpp"
#include "RTC/Listeners.hpp"
#include "RTC/RtpDictionaries.hpp"
#include "absl/container/flat_hash_map.h"
#include <memory>
#include <optional>

namespace RTC
{

class Consumer;
class RtpPacket;
class RtpPacketsCollector;
class MediaFrameSerializationFactory;
class RtpCodecMimeType;

class ConsumerTranslator : public ConsumerTranslatorSettings,
                           public MediaSink
{
    class MediaGrabber;
    // key is payload type, value - RTP timestamp, SSRC & seq. number
    using ProducerPacketsInfo = absl::flat_hash_map<uint8_t, std::tuple<uint32_t, uint32_t, uint16_t>>;
public:
    ConsumerTranslator(const Consumer* consumer, RtpPacketsCollector* packetsCollector,
                       const std::shared_ptr<MediaFrameSerializationFactory>& serializationFactory);
    ~ConsumerTranslator() final;
    void AddObserver(ConsumerObserver* observer);
    void RemoveObserver(ConsumerObserver* observer);
    bool HadIncomingMedia() const;
    void ProcessProducerRtpPacket(const RtpPacket* packet);
    // impl. of TranslatorUnit
    const std::string& GetId() const final;
    // impl. of ConsumerTranslatorSettings
    std::optional<FBS::TranslationPack::Language> GetLanguage() const final;
    std::optional<FBS::TranslationPack::Voice> GetVoice() const final;
    // impl. of MediaSink
    void StartMediaWriting(bool restart) final;
    void WriteMediaPayload(uint32_t ssrc, const std::shared_ptr<const MemoryBuffer>& buffer) final;
    void EndMediaWriting() final;
protected:
    void OnPauseChanged(bool pause) final;
private:
    template <class Method, typename... Args>
    void InvokeObserverMethod(const Method& method, Args&&... args) const;
private:
    const Consumer* const _consumer;
    RtpPacketsCollector* const _packetsCollector;
    const std::shared_ptr<MediaFrameSerializationFactory> _serializationFactory;
    Listeners<ConsumerObserver*> _observers;
    ProtectedUniquePtr<MediaGrabber> _mediaGrabber;
    ProtectedObj<ProducerPacketsInfo> _producerPacketsInfo;
};

} // namespace RTC
