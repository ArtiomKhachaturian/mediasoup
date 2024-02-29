#pragma once
#include "RTC/MediaTranslate/RtpTranslatedPacket.hpp"
#include <absl/container/flat_hash_map.h>
#include <memory>
#include <string>

namespace RTC
{

class Consumer;
class ConsumerInfo;
class MediaSource;
class TranslatorEndPointSink;
class TranslatorEndPoint;
class TranslatorEndPointFactory;
class RtpPacket;
class RtpPacketsCollector;

class ConsumersManager
{
    // second is counter of consumers with the same output language & voice
    using EndPointEntry = std::pair<std::shared_ptr<TranslatorEndPoint>, uint64_t>;
    class ConsumerInfoImpl;
public:
    ConsumersManager(TranslatorEndPointFactory* endPointsFactory,
                     MediaSource* translationsInput,
                     TranslatorEndPointSink* translationsOutput);
    ~ConsumersManager();
    void SetInputLanguage(const std::string& languageId);
    std::shared_ptr<ConsumerInfo> AddConsumer(Consumer* consumer);
    void UpdateConsumer(Consumer* consumer);
    std::shared_ptr<ConsumerInfo> GetConsumer(Consumer* consumer) const;
    bool RemoveConsumer(Consumer* consumer);
    size_t GetSize() const { return _consumersInfo.size(); }
    void BeginPacketsSending(uint64_t mediaId, uint64_t endPointId);
    void SendPacket(uint64_t mediaId, uint64_t endPointId, RtpTranslatedPacket packet,
                    uint32_t mappedSsrc, RtpPacketsCollector* output);
    void EndPacketsSending(uint64_t mediaId, uint64_t endPointId);
private:
    std::shared_ptr<TranslatorEndPoint> AddNewEndPoint(const Consumer* consumer, size_t key);
    std::shared_ptr<TranslatorEndPoint> CreateEndPoint() const;
    std::shared_ptr<TranslatorEndPoint> GetEndPoint(const Consumer* consumer) const;
    std::shared_ptr<TranslatorEndPoint> GetEndPoint(size_t key) const;
    void UpdateConsumer(const Consumer* consumer, const std::shared_ptr<ConsumerInfoImpl>& consumerInfo);
private:
    TranslatorEndPointFactory* const _endPointsFactory;
    MediaSource* const _translationsInput;
    TranslatorEndPointSink* const _translationsOutput;
    std::string _inputLanguageId;
    // consumer info contains combined hash of output language & voice for consumers
    absl::flat_hash_map<Consumer*, std::shared_ptr<ConsumerInfoImpl>> _consumersInfo;
    // key is combined hash from output language & voice
    absl::flat_hash_map<size_t, EndPointEntry> _endpoints;
};

} // namespace RTC
