#ifndef MS_RTC_MEDIA_TRANSLATORS_MANAGER_HPP
#define MS_RTC_MEDIA_TRANSLATORS_MANAGER_HPP

#include "common.hpp"
#include "RTC/MediaTranslate/ConsumerTranslatorsManager.hpp"
#include "RTC/MediaTranslate/ProducerTranslatorsManager.hpp"

namespace RTC
{

class RtpPacketsCollector;
class ProducerTranslator;
class ConsumerTranslator;

class MediaTranslatorsManager : public ProducerTranslatorsManager,
                                public ConsumerTranslatorsManager
{
    class MediaPacketsCollector;
    class Producer;
    class Consumer;
    class ProducerObserver;
    class ConsumerObserver;
    class Impl;
public:
    MediaTranslatorsManager(const std::string& serviceUri,
                            const std::string& serviceUser = std::string(),
                            const std::string& servicePassword = std::string());
    ~MediaTranslatorsManager();
    // producers API -> impl. of ProducerTranslatorsManager
    std::shared_ptr<ProducerTranslator> RegisterProducer(const std::string& producerId) final;
    std::shared_ptr<ProducerTranslator> GetRegisteredProducer(const std::string& producerId) const final;
    void UnegisterProducer(const std::string& producerId) final;
    // consumers API -> impl. of ConsumerTranslatorsManager
    std::shared_ptr<ConsumerTranslator> RegisterConsumer(const std::string& consumerId) final;
    std::shared_ptr<ConsumerTranslator> GetRegisteredConsumer(const std::string& consumerId) const final;
    void UnegisterConsumer(const std::string& consumerId) final;
private:
    const std::shared_ptr<Impl> _impl;
};

} // namespace RTC

#endif
