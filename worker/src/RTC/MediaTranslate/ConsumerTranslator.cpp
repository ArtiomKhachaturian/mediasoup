#include "RTC/MediaTranslate/ConsumerTranslator.hpp"

namespace RTC
{

ConsumerTranslator::ConsumerTranslator(const std::string& id,
                                       const std::weak_ptr<ConsumerObserver>& observerRef,
                                       const std::string& producerId)
    : TranslatorUnitImpl<ConsumerObserver, ConsumerTranslatorSettings>(id, observerRef)
    , _producerId(producerId)
{
}

void ConsumerTranslator::SetLanguage(MediaLanguage language)
{
    if (language != _language.exchange(language)) {
        if (const auto observer = _observerRef.lock()) {
            observer->OnConsumerLanguageChanged(GetId());
        }
    }
}

void ConsumerTranslator::SetVoice(MediaVoice voice)
{
    if (voice != _voice.exchange(voice)) {
        if (const auto observer = _observerRef.lock()) {
            observer->OnConsumerVoiceChanged(GetId());
        }
    }
}

void ConsumerTranslator::SetEnabled(bool enabled)
{
    if (enabled != _enabled.exchange(enabled)) {
        if (const auto observer = _observerRef.lock()) {
            observer->OnConsumerEnabledChanged(GetId(), enabled);
        }
    }
}

void ConsumerTranslator::OnPauseChanged(bool pause)
{
    if (const auto observer = _observerRef.lock()) {
        observer->OnConsumerPauseChanged(GetId(), pause);
    }
}

} // namespace RTC
