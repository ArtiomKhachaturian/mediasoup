#pragma once

#include "RTC/MediaTranslate/ConsumerTranslatorSettings.hpp"
#include "RTC/MediaTranslate/ConsumerObserver.hpp"
#include "RTC/Listeners.hpp"
#include <memory>

namespace RTC
{

class Consumer;
class TranslatorEndPoint;
class ProducerInputMediaStreamer;

class ConsumerTranslator : public ConsumerTranslatorSettings
{
public:
    ConsumerTranslator(Consumer* consumer,
                       const std::string& producerId,
                       const std::string& serviceUri,
                       const std::string& serviceUser = std::string(),
                       const std::string& servicePassword = std::string());
    ~ConsumerTranslator() final;
    const std::string& GetProducerId() const { return _producerId; }
    void AddObserver(ConsumerObserver* observer);
    void RemoveObserver(ConsumerObserver* observer);
    bool HasProducerInput() const;
    void SetProducerInput(const std::shared_ptr<ProducerInputMediaStreamer>& input);
    void SetProducerLanguage(const std::optional<MediaLanguage>& language);
    // impl. of TranslatorUnit
    const std::string& GetId() const final;
    // impl. of ConsumerTranslatorSettings
    void SetLanguage(MediaLanguage language) final;
    MediaLanguage GetLanguage() const final { return _language; }
    void SetVoice(MediaVoice voice) final;
    MediaVoice GetVoice() const final { return _voice; }
    void SetEnabled(bool enabled) final;
    bool IsEnabled() const final { return _enabled; }
protected:
    void OnPauseChanged(bool pause) final;
private:
    template <class Method, typename... Args>
    void InvokeObserverMethod(const Method& method, Args&&... args) const;
private:
    Consumer* const _consumer;
    const std::string _producerId;
    const std::unique_ptr<TranslatorEndPoint> _endPoint;
    Listeners<ConsumerObserver*> _observers;
    // output language
    MediaLanguage _language = DefaultOutputMediaLanguage();
    // voice
    MediaVoice _voice = DefaultMediaVoice();
    bool _enabled = true;
};

} // namespace RTC
