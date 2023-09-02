#pragma once

#include "RTC/MediaTranslate/ConsumerTranslatorSettings.hpp"
#include "RTC/MediaTranslate/ConsumerObserver.hpp"
#include "RTC/MediaTranslate/TranslatorUnitImpl.hpp"

namespace RTC
{

class ConsumerTranslator : public TranslatorUnitImpl<ConsumerObserver, ConsumerTranslatorSettings>
{
public:
    ConsumerTranslator(const std::string& id, const std::weak_ptr<ConsumerObserver>& observerRef,
                       const std::string& producerId);
    const std::string& GetProducerId() const { return _producerId; }
    // impl. of ConsumerTranslatorSettings
    void SetLanguage(MediaLanguage language) final;
    MediaLanguage GetLanguage() const final { return _language.load(std::memory_order_relaxed); }
    void SetVoice(MediaVoice voice) final;
    MediaVoice GetVoice() const final { return _voice.load(std::memory_order_relaxed); }
    void SetEnabled(bool enabled) final;
    bool IsEnabled() const final { return _enabled.load(std::memory_order_relaxed); }
protected:
    void OnPauseChanged(bool pause) final;
private:
    const std::string _producerId;
    // output language
    std::atomic<MediaLanguage> _language = DefaultOutputMediaLanguage();
    // voice
    std::atomic<MediaVoice> _voice = DefaultMediaVoice();
    std::atomic_bool _enabled = true;
};

} // namespace RTC
