#define MS_CLASS "RTC::ConsumersManager"
#include "RTC/MediaTranslate/ConsumersManager.hpp"
#include "RTC/MediaTranslate/ConsumerInfo.hpp"
#include "RTC/MediaTranslate/TranslatorEndPoint/TranslatorEndPoint.hpp"
#include "RTC/MediaTranslate/TranslatorEndPoint/TranslatorEndPointFactory.hpp"
#include "RTC/MediaTranslate/RtpPacketsTimeline.hpp"
#include "RTC/RtpPacketsCollector.hpp"
#include "RTC/Consumer.hpp"
#include "RTC/RtpPacket.hpp"
#include "Logger.hpp"
#include <atomic>

namespace RTC
{

class ConsumersManager::EndPointInfo
{
    // 1st is media ID, 2nd - timestamp in start moment
    using PlayInfo = std::pair<uint64_t, uint32_t>;
public:
    EndPointInfo(std::shared_ptr<TranslatorEndPoint> endPoint);
    void BeginMediaPlay(uint64_t mediaId, const RtpPacketsTimeline& timeline);
    void EndMediaPlay(uint64_t mediaId);
    bool IsPlaying() const;
    bool AdvanceTranslatedPacket(const Timestamp& offset, RtpPacket* packet);
    bool AdvanceTranslatedPacket(uint32_t offset, RtpPacket* packet);
    std::unique_ptr<RtpPacket> MapOriginalPacket(uint32_t offset, RtpPacket* packet);
    bool IsConnected() const { return _endPoint->IsConnected(); }
    uint64_t GetId() const { return _endPoint->GetId(); }
    void SetInputLanguageId(const std::string& languageId);
    void SetOutputLanguageAndVoiceId(size_t languageAndVoiceIdKey,
                                     const std::string& languageId,
                                     const std::string& voiceId);
    void SetOutputLanguageAndVoiceId(size_t languageAndVoiceIdKey,
                                     const Consumer* consumer);
    void SetOutputLanguageAndVoiceId(const Consumer* consumer);
    size_t GetOutputLanguageAndVoiceIdKey() const { return _outputLanguageAndVoiceIdKey.load(); }
private:
    const std::shared_ptr<TranslatorEndPoint> _endPoint;
    ProtectedUniquePtr<RtpPacketsTimeline> _timeline;
    ProtectedObj<PlayInfo> _playInfo;
    std::atomic<size_t> _outputLanguageAndVoiceIdKey = 0U;
};

ConsumersManager::ConsumersManager(TranslatorEndPointFactory* endPointsFactory,
                                   MediaSource* translationsInput,
                                   TranslatorEndPointSink* translationsOutput,
                                   uint32_t mappedSsrc,
                                   uint32_t clockRate, const RtpCodecMimeType& mime)
    : _endPointsFactory(endPointsFactory)
    , _translationsInput(translationsInput)
    , _translationsOutput(translationsOutput)
    , _mappedSsrc(mappedSsrc)
    , _originalTimeline(clockRate, mime)
    , _inputLanguageId("auto")
{
}

ConsumersManager::~ConsumersManager()
{
    LOCK_WRITE_PROTECTED_OBJ(_endpoints);
    _endpoints->clear();
}

void ConsumersManager::SetInputLanguage(const std::string& languageId)
{
    if (!languageId.empty()) {
        LOCK_WRITE_PROTECTED_OBJ(_inputLanguageId);
        if (_inputLanguageId.ConstRef() != languageId) {
            _inputLanguageId = languageId;
            LOCK_READ_PROTECTED_OBJ(_endpoints);
            for (auto it = _endpoints->begin(); it != _endpoints->end(); ++it) {
                it->second->SetInputLanguageId(languageId);
            }
        }
    }
}

std::string ConsumersManager::GetInputLanguage() const
{
    LOCK_READ_PROTECTED_OBJ(_inputLanguageId);
    return _inputLanguageId.ConstRef();
}

void ConsumersManager::AddConsumer(Consumer* consumer)
{
    if (consumer) {
        LOCK_WRITE_PROTECTED_OBJ(_consumerToEndpointId);
        if (!_consumerToEndpointId->count(consumer)) {
            const auto key = consumer->GetLanguageVoiceKey();
            std::shared_ptr<EndPointInfo> endPoint;
            LOCK_WRITE_PROTECTED_OBJ(_endpoints);
            for (auto it = _endpoints->begin(); it != _endpoints->end(); ++it) {
                if (it->second->GetOutputLanguageAndVoiceIdKey() == key) {
                    endPoint = it->second;
                    break;
                }
            }
            if (!endPoint) {
                endPoint = CreateEndPoint();
                if (endPoint) {
                    endPoint->SetOutputLanguageAndVoiceId(key, consumer);
                    _endpoints->insert(std::make_pair(endPoint->GetId(), endPoint));
                }
            }
            if (endPoint) {
                _consumerToEndpointId->insert(std::make_pair(consumer, endPoint->GetId()));
            }
        }
    }
}

void ConsumersManager::UpdateConsumer(Consumer* consumer)
{
    if (consumer) {
        /*const auto it = _consumersInfo.find(consumer);
        if (it != _consumersInfo.end()) {
            UpdateConsumer(consumer, it->second);
        }*/
    }
}

bool ConsumersManager::RemoveConsumer(Consumer* consumer)
{
    if (consumer) {
        LOCK_WRITE_PROTECTED_OBJ(_consumerToEndpointId);
        const auto itc = _consumerToEndpointId->find(consumer);
        if (itc != _consumerToEndpointId->end()) {
            size_t endPointUsersCount = 0UL;
            for (auto it = _consumerToEndpointId->begin(); it != _consumerToEndpointId->end(); ++it) {
                if (it != itc && it->second == itc->second) {
                    ++endPointUsersCount;
                }
            }
            if (!endPointUsersCount) {
                LOCK_WRITE_PROTECTED_OBJ(_endpoints);
                _endpoints->erase(itc->second);
            }
            _consumerToEndpointId->erase(itc);
            return true;
        }
    }
    return false;
}

void ConsumersManager::DispatchOriginalPacket(RtpPacket* packet, RtpPacketsCollector* collector)
{
    if (packet) {
        _originalTimeline.SetTimestamp(packet->GetTimestamp());
        _originalTimeline.SetSeqNumber(packet->GetSequenceNumber());
        std::unordered_set<Consumer*> rejectedConsumers;
        {
            LOCK_READ_PROTECTED_OBJ(_endpoints);
            if (!_endpoints->empty()) {
                for (auto it = _endpoints->begin(); it != _endpoints->end(); ++it) {
                    if (it->second->IsPlaying()) {
                        rejectedConsumers.merge(GetMyConsumers(it->first));
                    }
                    else {
                        const auto delta = _originalTimeline.GetTimestampDelta();
                        if (auto mapped = it->second->MapOriginalPacket(delta, packet)) {
                            mapped->SetRejectedConsumers(GetAlienConsumers(it->first));
                            if (collector) {
                                collector->AddPacket(mapped.release(), _mappedSsrc, true);
                            }
                        }
                    }
                }
            }
        }
        packet->SetRejectedConsumers(std::move(rejectedConsumers));
    }
}

void ConsumersManager::NotifyThatConnected(uint64_t endPointId, bool connected)
{
    
}

void ConsumersManager::BeginPacketsSending(uint64_t mediaId, uint64_t endPointId)
{
    if (endPointId) {
        LOCK_READ_PROTECTED_OBJ(_endpoints);
        const auto it = _endpoints->find(endPointId);
        if (it != _endpoints->end()) {
            it->second->BeginMediaPlay(mediaId, _originalTimeline);
        }
    }
}

void ConsumersManager::SendPacket(uint64_t mediaId, uint64_t endPointId,
                                  RtpTranslatedPacket packet,
                                  RtpPacketsCollector* output)
{
    if (auto rtp = packet.Take()) {
        LOCK_READ_PROTECTED_OBJ(_endpoints);
        const auto it = _endpoints->find(endPointId);
        if (it != _endpoints->end()) {
            if (it->second->AdvanceTranslatedPacket(packet.GetTimestampOffset(), rtp.get())) {
                if (output) {
                    rtp->SetRejectedConsumers(GetAlienConsumers(endPointId));
                    output->AddPacket(rtp.release(), _mappedSsrc, true);
                }
            }
        }
    }
}

void ConsumersManager::EndPacketsSending(uint64_t mediaId, uint64_t endPointId)
{
    LOCK_READ_PROTECTED_OBJ(_endpoints);
    const auto it = _endpoints->find(endPointId);
    if (it != _endpoints->end()) {
        it->second->EndMediaPlay(mediaId);
    }
}

std::shared_ptr<ConsumersManager::EndPointInfo> ConsumersManager::CreateEndPoint() const
{
    if (auto endPoint = _endPointsFactory->CreateEndPoint()) {
        if (!endPoint->AddOutputMediaSink(_translationsOutput)) {
            // TODO: log error
        }
        else {
            endPoint->SetInputMediaSource(_translationsInput);
            endPoint->SetInputLanguageId(GetInputLanguage());
            endPoint->AddOutputMediaSink(_translationsOutput);
            return std::make_shared<EndPointInfo>(std::move(endPoint));
        }
    }
    return nullptr;
}

std::shared_ptr<ConsumersManager::EndPointInfo> ConsumersManager::GetEndPoint(uint64_t endPointId) const
{
    if (endPointId) {
        LOCK_READ_PROTECTED_OBJ(_endpoints);
        const auto it = _endpoints->find(endPointId);
        if (it != _endpoints->end()) {
            return it->second;
        }
    }
    return nullptr;
}

std::shared_ptr<ConsumersManager::EndPointInfo> ConsumersManager::GetEndPoint(Consumer* consumer) const
{
    if (consumer) {
        LOCK_READ_PROTECTED_OBJ(_consumerToEndpointId);
        const auto it = _consumerToEndpointId->find(consumer);
        if (it != _consumerToEndpointId->end()) {
            return GetEndPoint(it->second);
        }
    }
    return nullptr;
}

std::unordered_set<Consumer*> ConsumersManager::GetConsumers(uint64_t endPointId, bool alien) const
{
    if (endPointId) {
        std::unordered_set<Consumer*> consumers;
        LOCK_READ_PROTECTED_OBJ(_consumerToEndpointId);
        for (auto it = _consumerToEndpointId->begin(); it != _consumerToEndpointId->end(); ++it) {
            if (alien ? it->second != endPointId : it->second == endPointId) {
                consumers.insert(it->first);
            }
        }
        return consumers;
    }
    return {};
}

ConsumersManager::EndPointInfo::EndPointInfo(std::shared_ptr<TranslatorEndPoint> endPoint)
    : _endPoint(std::move(endPoint))
    , _playInfo(0U, 0U)
{
}

void ConsumersManager::EndPointInfo::BeginMediaPlay(uint64_t mediaId, const RtpPacketsTimeline& timeline)
{
    LOCK_WRITE_PROTECTED_OBJ(_playInfo);
    if (0U == _playInfo->first) {
        LOCK_WRITE_PROTECTED_OBJ(_timeline);
        if (!_timeline->get()) {
            _timeline = std::make_unique<RtpPacketsTimeline>(timeline);
        }
        _playInfo->first = mediaId;
        _playInfo->second = _timeline->get()->GetTimestamp();
        /*MS_ERROR_STD("BeginMediaPlay -> fix timeline, TS = %u, seq. num = %u",
                     unsigned(_timeline->get()->GetTimestamp()),
                     unsigned(_timeline->get()->GetSeqNumber()));*/
    }
}

void ConsumersManager::EndPointInfo::EndMediaPlay(uint64_t mediaId)
{
    LOCK_WRITE_PROTECTED_OBJ(_playInfo);
    if (_playInfo->first == mediaId) {
        _playInfo->first = 0U;
        _playInfo->second = 0U;
        /*LOCK_READ_PROTECTED_OBJ(_timeline);
        MS_ERROR_STD("EndMediaPlay -> unfix timeline, TS = %u, seq. num = %u",
                     unsigned(_timeline->get()->GetTimestamp()),
                     unsigned(_timeline->get()->GetSeqNumber()));*/
    }
}

bool ConsumersManager::EndPointInfo::AdvanceTranslatedPacket(const Timestamp& offset,
                                                             RtpPacket* packet)
{
    return packet && AdvanceTranslatedPacket(offset.GetRtpTime(), packet);
}

bool ConsumersManager::EndPointInfo::AdvanceTranslatedPacket(uint32_t offset, RtpPacket* packet)
{
    if (packet) {
        LOCK_READ_PROTECTED_OBJ(_playInfo);
        if (_playInfo->first) {
            LOCK_READ_PROTECTED_OBJ(_timeline);
            if (const auto& timeline = _timeline.ConstRef()) {
                if (0 == offset) { // 1st frame
                    offset = timeline->GetTimestampDelta();
                }
                packet->SetTimestamp(_playInfo->second + offset);
                packet->SetSequenceNumber(timeline->AdvanceSeqNumber());
                timeline->SetTimestamp(packet->GetTimestamp());
                return true;
            }
        }
    }
    return false;
}

bool ConsumersManager::EndPointInfo::IsPlaying() const
{
    LOCK_READ_PROTECTED_OBJ(_playInfo);
    return 0U != _playInfo->first;
}

std::unique_ptr<RtpPacket> ConsumersManager::EndPointInfo::MapOriginalPacket(uint32_t offset,
                                                                             RtpPacket* packet)
{
    if (packet) {
        LOCK_READ_PROTECTED_OBJ(_timeline);
        if (const auto& timeline = _timeline.ConstRef()) {
            auto mapped = packet->Clone();
            const auto timestamp = timeline->GetTimestamp() + offset;
            mapped->SetTimestamp(timestamp);
            timeline->SetTimestamp(timestamp);
            mapped->SetSequenceNumber(timeline->AdvanceSeqNumber());
            return std::unique_ptr<RtpPacket>(mapped);
        }
    }
    return nullptr;
}

void ConsumersManager::EndPointInfo::SetInputLanguageId(const std::string& languageId)
{
    _endPoint->SetInputLanguageId(languageId);
}

void ConsumersManager::EndPointInfo::SetOutputLanguageAndVoiceId(size_t languageAndVoiceIdKey,
                                                                 const std::string& languageId,
                                                                 const std::string& voiceId)
{
    if (!languageId.empty() && !voiceId.empty()) {
        if (languageAndVoiceIdKey != _outputLanguageAndVoiceIdKey.exchange(languageAndVoiceIdKey)) {
            _endPoint->SetOutputLanguageId(languageId);
            _endPoint->SetOutputVoiceId(voiceId);
        }
    }
}

void ConsumersManager::EndPointInfo::SetOutputLanguageAndVoiceId(size_t languageAndVoiceIdKey,
                                                                 const Consumer* consumer)
{
    if (consumer) {
        SetOutputLanguageAndVoiceId(languageAndVoiceIdKey,
                                    consumer->GetLanguageId(),
                                    consumer->GetVoiceId());
    }
}

void ConsumersManager::EndPointInfo::SetOutputLanguageAndVoiceId(const Consumer* consumer)
{
    if (consumer) {
        SetOutputLanguageAndVoiceId(consumer->GetLanguageVoiceKey(), consumer);
    }
}

} // namespace RTC
