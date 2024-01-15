#define MS_CLASS "RTC::ConsumerTranslator"
#include "RTC/MediaTranslate/ConsumerTranslator.hpp"
#include "RTC/Consumer.hpp"
#include "Logger.hpp"

namespace RTC
{

ConsumerTranslator::ConsumerTranslator(const Consumer* consumer)
    : _consumer(consumer)
{
    MS_ASSERT(_consumer, "consumer must not be null");
    if (consumer->IsPaused()) {
        Pause();
    }
}

ConsumerTranslator::~ConsumerTranslator()
{
}

const std::string& ConsumerTranslator::GetId() const
{
    return _consumer->id;
}

void ConsumerTranslator::AddObserver(ConsumerObserver* observer)
{
    _observers.Add(observer);
}

void ConsumerTranslator::RemoveObserver(ConsumerObserver* observer)
{
    _observers.Remove(observer);
}

std::optional<FBS::TranslationPack::Language> ConsumerTranslator::GetLanguage() const
{
    return _consumer->GetLanguage();
}

std::optional<FBS::TranslationPack::Voice> ConsumerTranslator::GetVoice() const
{
    return _consumer->GetVoice();
}

void ConsumerTranslator::OnPauseChanged(bool pause)
{
    InvokeObserverMethod(&ConsumerObserver::OnConsumerPauseChanged, pause);
}

template <class Method, typename... Args>
void ConsumerTranslator::InvokeObserverMethod(const Method& method, Args&&... args) const
{
    _observers.InvokeMethod(method, GetId(), std::forward<Args>(args)...);
}

} // namespace RTC
