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
class RtpPacketsInfoProvider;
class MediaFrameSerializationFactory;
class RtpCodecMimeType;

class ConsumerTranslator : public ConsumerTranslatorSettings,
                           public MediaSink
{
    class MediaGrabber;
    class CodecInfo;
public:
    ConsumerTranslator(const Consumer* consumer,
                       RtpPacketsCollector* packetsCollector,
                       const RtpPacketsInfoProvider* packetsInfoProvider,
                       const std::shared_ptr<MediaFrameSerializationFactory>& serializationFactory);
    ~ConsumerTranslator() final;
    void AddObserver(ConsumerObserver* observer);
    void RemoveObserver(ConsumerObserver* observer);
    bool HadIncomingMedia() const;
    // impl. of TranslatorUnit
    const std::string& GetId() const final;
    // impl. of ConsumerTranslatorSettings
    const std::string& GetLanguageId() const final;
    const std::string& GetVoiceId() const final;
    // impl. of MediaSink
    void StartMediaWriting(uint32_t ssrc) final;
    void WriteMediaPayload(const std::shared_ptr<MemoryBuffer>& buffer) final;
    void EndMediaWriting() final;
protected:
    void OnPauseChanged(bool pause) final;
private:
    template <class Method, typename... Args>
    void InvokeObserverMethod(const Method& method, Args&&... args) const;
private:
    const Consumer* const _consumer;
    RtpPacketsCollector* const _packetsCollector;
    const RtpPacketsInfoProvider* const _packetsInfoProvider;
    const std::shared_ptr<MediaFrameSerializationFactory> _serializationFactory;
    Listeners<ConsumerObserver*> _observers;
    std::shared_ptr <MediaGrabber> _mediaGrabber;
};

} // namespace RTC
