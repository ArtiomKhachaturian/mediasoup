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

class ConsumerTranslator;
class MediaSource;
class TranslatorEndPointSink;
class TranslatorEndPoint;
class TranslatorEndPointFactory;
class RtpPacket;
class RtpPacketsCollector;
class RtpCodecMimeType;

class ConsumersManager
{
    class EndPointInfo;
    template <typename T>
    using Protected = ProtectedObj<T, std::mutex>;
public:
    ConsumersManager(TranslatorEndPointFactory* endPointsFactory,
                     MediaSource* translationsInput,
                     TranslatorEndPointSink* translationsOutput,
                     uint32_t mappedSsrc, uint32_t clockRate, const RtpCodecMimeType& mime);
    ~ConsumersManager();
    void SetInputLanguage(const std::string& languageId);
    std::string GetInputLanguage() const;
    bool AddConsumer(const std::shared_ptr<ConsumerTranslator>& consumer);
    bool UpdateConsumer(const std::shared_ptr<ConsumerTranslator>& consumer);
    bool RemoveConsumer(const std::shared_ptr<ConsumerTranslator>& consumer);
    void DispatchOriginalPacket(RtpPacket* packet, RtpPacketsCollector* collector);
    void NotifyThatConnected(uint64_t endPointId, bool connected);
    void BeginPacketsSending(uint64_t mediaId, uint64_t endPointId);
    void SendPacket(uint64_t mediaId, uint64_t endPointId, RtpTranslatedPacket packet,
                    RtpPacketsCollector* output);
    void EndPacketsSending(uint64_t mediaId, uint64_t endPointId);
private:
    bool AddNewEndPointFor(const std::shared_ptr<ConsumerTranslator>& consumer);
    bool AddNewEndPointFor(const std::shared_ptr<ConsumerTranslator>& consumer,
                           std::string consumerLanguageId,
                           std::string consumerVoiceId);
    std::shared_ptr<EndPointInfo> CreateEndPoint() const;
    std::shared_ptr<EndPointInfo> GetEndPoint(uint64_t endPointId) const;
    // note: this method makes iteration by [_endPoints] without lock/unlock of protection
    std::unordered_set<uint64_t> GetAlienConsumers(uint64_t endPointId) const;
private:
    TranslatorEndPointFactory* const _endPointsFactory;
    MediaSource* const _translationsInput;
    TranslatorEndPointSink* const _translationsOutput;
    const uint32_t _mappedSsrc;
    RtpPacketsTimeline _originalTimeline;
    Protected<std::string> _inputLanguageId;
    // key is end-point ID, value - end-point wrapper
    Protected<std::unordered_map<uint64_t, std::shared_ptr<EndPointInfo>>> _endPoints;
};

} // namespace RTC
