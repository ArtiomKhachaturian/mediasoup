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
    ConsumerTranslator(const Consumer* consumer,
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
    void SetProducerLanguage(const std::optional<FBS::TranslationPack::Language>& language);
    void UpdateConsumerLanguageAndVoice();
    // impl. of TranslatorUnit
    const std::string& GetId() const final;
    // impl. of ConsumerTranslatorSettings
    std::optional<FBS::TranslationPack::Language> GetLanguage() const final;
    std::optional<FBS::TranslationPack::Voice> GetVoice() const final;
    void SetEnabled(bool enabled) final;
    bool IsEnabled() const final { return _enabled; }
protected:
    void OnPauseChanged(bool pause) final;
private:
    template <class Method, typename... Args>
    void InvokeObserverMethod(const Method& method, Args&&... args) const;
private:
    const Consumer* const _consumer;
    const std::string _producerId;
    const std::unique_ptr<TranslatorEndPoint> _endPoint;
    Listeners<ConsumerObserver*> _observers;
    bool _enabled = true;
};

} // namespace RTC
