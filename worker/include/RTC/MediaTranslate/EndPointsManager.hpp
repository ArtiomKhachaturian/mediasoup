#pragma once
#include <absl/container/flat_hash_map.h>
#include <memory>
#include <string>

namespace RTC
{

class Consumer;
class MediaSource;
class TranslatorEndPoint;
class TranslatorEndPointFactory;
class TranslatorEndPointListener;
class RtpPacket;

class EndPointsManager
{
    // second is counter of consumers with the same output language & voice
    using EndPointEntry = std::pair<std::shared_ptr<TranslatorEndPoint>, uint64_t>;
    class ConsumerInfo;
public:
    EndPointsManager(uint32_t ssrc,
                     TranslatorEndPointFactory* endPointsFactory,
                     MediaSource* translationsInput,
                     TranslatorEndPointListener* translationsOutput);
    ~EndPointsManager();
    void SetInputLanguage(const std::string& languageId);
    void AddConsumer(const Consumer* consumer);
    void UpdateConsumer(const Consumer* consumer);
    void RemoveConsumer(const Consumer* consumer);
    bool IsConnected(const Consumer* consumer) const;
    void SetLastRtpPacketInfo(const Consumer* consumer, const RtpPacket* packet);
private:
    std::shared_ptr<TranslatorEndPoint> AddNewEndPoint(const Consumer* consumer, size_t key);
    std::shared_ptr<TranslatorEndPoint> CreateEndPoint() const;
    std::shared_ptr<TranslatorEndPoint> GetEndPoint(const Consumer* consumer) const;
    std::shared_ptr<TranslatorEndPoint> GetEndPoint(size_t key) const;
private:
    const uint32_t _ssrc;
    TranslatorEndPointFactory* const _endPointsFactory;
    MediaSource* const _translationsInput;
    TranslatorEndPointListener* const _translationsOutput;
    std::string _inputLanguageId;
    // consumer info contains combined hash of output language & voice for consumers
    absl::flat_hash_map<const Consumer*, std::unique_ptr<ConsumerInfo>> _consumersInfo;
    // key is combined hash from output language & voice
    absl::flat_hash_map<size_t, EndPointEntry> _endpoints;
};

} // namespace RTC
