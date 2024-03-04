#pragma once
#include "RTC/MediaTranslate/RtpTranslatedPacket.hpp"
#include "RTC/MediaTranslate/RtpPacketsTimeline.hpp"
#include "ProtectedObj.hpp"
#include <unordered_map>
#include <unordered_set>
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
class RtpCodecMimeType;

class ConsumersManager
{
    // second is counter of consumers with the same output language & voice
    //using EndPointEntry = std::pair<std::shared_ptr<TranslatorEndPoint>, uint64_t>;
    class ConsumerInfoImpl;
    class EndPointInfo;
public:
    ConsumersManager(TranslatorEndPointFactory* endPointsFactory,
                     MediaSource* translationsInput,
                     TranslatorEndPointSink* translationsOutput,
                     uint32_t mappedSsrc, uint32_t clockRate, const RtpCodecMimeType& mime);
    ~ConsumersManager();
    void SetInputLanguage(const std::string& languageId);
    std::string GetInputLanguage() const;
    void AddConsumer(Consumer* consumer);
    void UpdateConsumer(Consumer* consumer);
    bool RemoveConsumer(Consumer* consumer);
    bool DispatchOriginalPacket(RtpPacket* packet, RtpPacketsCollector* collector);
    void NotifyThatConnected(uint64_t endPointId, bool connected);
    void BeginPacketsSending(uint64_t mediaId, uint64_t endPointId);
    void SendPacket(uint64_t mediaId, uint64_t endPointId, RtpTranslatedPacket packet,
                    RtpPacketsCollector* output);
    void EndPacketsSending(uint64_t mediaId, uint64_t endPointId);
private:
    std::shared_ptr<EndPointInfo> CreateEndPoint() const;
    std::shared_ptr<EndPointInfo> GetEndPoint(uint64_t endPointId) const;
    std::shared_ptr<EndPointInfo> GetEndPoint(Consumer* consumer) const;
    std::unordered_set<Consumer*> GetConsumers(uint64_t endPointId) const;
private:
    TranslatorEndPointFactory* const _endPointsFactory;
    MediaSource* const _translationsInput;
    TranslatorEndPointSink* const _translationsOutput;
    const uint32_t _mappedSsrc;
    RtpPacketsTimeline _originalTimeline;
    ProtectedObj<std::string> _inputLanguageId;
    // key is end-point ID
    ProtectedObj<std::unordered_map<uint64_t, std::shared_ptr<EndPointInfo>>> _endpoints;
    // key is consumer ptr, value - end-point ID
    ProtectedObj<std::unordered_map<Consumer*, uint64_t>> _consumerToEndpointId;
};

} // namespace RTC
