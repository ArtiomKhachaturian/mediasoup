#pragma once

#include "common.hpp"
#include <string>

namespace RTC
{

class RtpPacketsCollector;
class ProducerTranslator;
class ConsumerTranslator;
class Producer;
class Consumer;

class MediaTranslatorsManager
{
    class MediaPacketsCollector;
    class ProducerTranslatorImpl;
    class ConsumerTranslatorImpl;
    class ProducerObserver;
    class ConsumerObserver;
    class TranslatorService;
    class Impl;
public:
    MediaTranslatorsManager(const std::string& serviceUri,
                            const std::string& serviceUser = std::string(),
                            const std::string& servicePassword = std::string());
    ~MediaTranslatorsManager();
    // producers API
    std::weak_ptr<ProducerTranslator> RegisterProducer(const std::string& producerId);
    std::weak_ptr<ProducerTranslator> RegisterProducer(const Producer* producer);
    std::weak_ptr<ProducerTranslator> GetRegisteredProducer(const std::string& producerId) const;
    bool UnRegisterProducer(const std::string& producerId);
    bool UnRegisterProducer(const Producer* producer);
    // consumers API
    std::weak_ptr<ConsumerTranslator> RegisterConsumer(const std::string& consumerId);
    std::weak_ptr<ConsumerTranslator> RegisterConsumer(const Consumer* consumer);
    std::weak_ptr<ConsumerTranslator> GetRegisteredConsumer(const std::string& consumerId) const;
    bool UnRegisterConsumer(const std::string& consumerId);
    bool UnRegisterConsumer(const Consumer* consumer);
private:
    const std::shared_ptr<Impl> _impl;
};

} // namespace RTC
