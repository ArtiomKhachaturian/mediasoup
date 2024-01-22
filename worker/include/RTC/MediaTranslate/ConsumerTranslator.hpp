#pragma once

#include "RTC/MediaTranslate/ConsumerTranslatorSettings.hpp"
#include "RTC/MediaTranslate/ConsumerObserver.hpp"
#include "RTC/MediaTranslate/MediaSink.hpp"
#include "RTC/Listeners.hpp"
#include "RTC/RtpDictionaries.hpp"
#include <memory>
#include <optional>

namespace RTC
{

class Consumer;
class RtpPacketsCollector;
class MediaFrame;
class MediaFrameDeserializer;
class MediaFrameSerializationFactory;
class RtpPacketizer;

class ConsumerTranslator : public ConsumerTranslatorSettings,
                           public MediaSink
{
public:
    ConsumerTranslator(const Consumer* consumer, RtpPacketsCollector* packetsCollector,
                       const std::shared_ptr<MediaFrameSerializationFactory>& serializationFactory);
    ~ConsumerTranslator() final;
    void AddObserver(ConsumerObserver* observer);
    void RemoveObserver(ConsumerObserver* observer);
    bool HadIncomingMedia() const { return _hadIncomingMedia; }
    // impl. of TranslatorUnit
    const std::string& GetId() const final;
    // impl. of ConsumerTranslatorSettings
    std::optional<FBS::TranslationPack::Language> GetLanguage() const final;
    std::optional<FBS::TranslationPack::Voice> GetVoice() const final;
    // impl. of MediaSink
    void StartStream(bool restart) noexcept final;
    void WriteMediaPayload(const std::shared_ptr<const MemoryBuffer>& buffer) noexcept final;
    void EndStream(bool failure) noexcept final;
protected:
    void OnPauseChanged(bool pause) final;
private:
    bool IsAudio() const;
    RtpPacketizer* GetPacketizer(const std::shared_ptr<const MediaFrame>& frame);
    template <class Method, typename... Args>
    void InvokeObserverMethod(const Method& method, Args&&... args) const;
private:
    const Consumer* const _consumer;
    RtpPacketsCollector* const _packetsCollector;
    const std::shared_ptr<MediaFrameSerializationFactory> _serializationFactory;
    std::unique_ptr<MediaFrameDeserializer> _deserializer;
    Listeners<ConsumerObserver*> _observers;
    std::optional<size_t> _deserializedMediaTrackIndex;
    bool _hadIncomingMedia = false;
    std::unordered_map<RtpCodecMimeType::Subtype, std::unique_ptr<RtpPacketizer>> _packetizers;
};

} // namespace RTC
