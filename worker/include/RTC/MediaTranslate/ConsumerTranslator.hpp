#pragma once

#include "RTC/MediaTranslate/ConsumerTranslatorSettings.hpp"
#include "RTC/MediaTranslate/ConsumerObserver.hpp"
#include "RTC/MediaTranslate/OutputDevice.hpp"
#include "RTC/RtpDictionaries.hpp"
#include "RTC/Listeners.hpp"
#include <memory>
#include <optional>

#define USE_TEST_FILE

#ifdef USE_TEST_FILE
namespace mkvparser {
class MkvReader;
}
#endif

namespace RTC
{

class Consumer;
class RtpPacketsCollector;
class RtpMediaFrameDeserializer;
class WebMBuffersReader;

class ConsumerTranslator : public ConsumerTranslatorSettings, public OutputDevice
{
    // 1st is MIME, 2nd - track number
    using DeserializedMediaInfo = std::pair<RtpCodecMimeType, size_t>;
public:
    ConsumerTranslator(const Consumer* consumer,
                       RtpPacketsCollector* packetsCollector);
    ~ConsumerTranslator() final;
    void AddObserver(ConsumerObserver* observer);
    void RemoveObserver(ConsumerObserver* observer);
    // impl. of TranslatorUnit
    const std::string& GetId() const final;
    // impl. of ConsumerTranslatorSettings
    std::optional<FBS::TranslationPack::Language> GetLanguage() const final;
    std::optional<FBS::TranslationPack::Voice> GetVoice() const final;
    // impl. of OutputDevice
    void Write(const std::shared_ptr<const MemoryBuffer>& buffer) noexcept final;
protected:
    void OnPauseChanged(bool pause) final;
private:
    bool IsAudio() const;
    template <class Method, typename... Args>
    void InvokeObserverMethod(const Method& method, Args&&... args) const;
private:
    const Consumer* const _consumer;
    RtpPacketsCollector* const _packetsCollector;
#ifdef USE_TEST_FILE
    std::unique_ptr<mkvparser::MkvReader> _deserializerSource;
#else
    std::unique_ptr<WebMBuffersReader> _deserializerSource;
#endif
    std::unique_ptr<RtpMediaFrameDeserializer> _deserializer;
    Listeners<ConsumerObserver*> _observers;
    std::optional<DeserializedMediaInfo> _deserializedMediaInfo;
};

} // namespace RTC
