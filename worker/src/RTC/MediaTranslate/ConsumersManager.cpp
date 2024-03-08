#define MS_CLASS "RTC::ConsumersManager"
#include "RTC/MediaTranslate/ConsumersManager.hpp"
#include "RTC/MediaTranslate/TranslatorEndPoint/TranslatorEndPoint.hpp"
#include "RTC/MediaTranslate/TranslatorEndPoint/TranslatorEndPointFactory.hpp"
#include "RTC/MediaTranslate/RtpPacketsTimeline.hpp"
#include "RTC/MediaTranslate/ConsumerTranslator.hpp"
#include "RTC/RtpPacketsCollector.hpp"
#include "RTC/RtpPacket.hpp"
#include "Utils.hpp"
#include "Logger.hpp"
#include <atomic>

namespace {

enum class RemoveResult {
    Failed,
    Succeeded,
    SucceededNoMoreConsumers
};

}

namespace RTC
{

class ConsumersManager::EndPointInfo
{
    // 1st is media ID, 2nd - timestamp in start moment
    using PlayInfo = std::pair<uint64_t, uint32_t>;
public:
    EndPointInfo(std::shared_ptr<TranslatorEndPoint> endPoint);
    ~EndPointInfo();
    bool IsStub() const { return _endPoint->IsStub(); }
    void BeginMediaPlay(uint64_t mediaId, const RtpPacketsTimeline& timeline);
    void EndMediaPlay(uint64_t mediaId);
    bool IsPlaying() const;
    std::unordered_set<uint64_t> GetConsumers() const;
    size_t GetConsumersCount() const;
    size_t GetLanguageVoiceKey() const;
    // return true if consumer language/voice ID is matched to this end-point
    bool AddConsumer(const std::shared_ptr<ConsumerTranslator>& consumer);
    RemoveResult RemoveConsumer(const std::shared_ptr<ConsumerTranslator>& consumer);
    bool HasConsumer(const std::shared_ptr<ConsumerTranslator>& consumer) const;
    bool AdvanceTranslatedPacket(const Timestamp& offset, RtpPacket* packet);
    bool AdvanceTranslatedPacket(uint32_t offset, RtpPacket* packet);
    std::unique_ptr<RtpPacket> MapOriginalPacket(uint32_t offset, RtpPacket* packet);
    bool IsConnected() const { return _endPoint->IsConnected(); }
    uint64_t GetId() const { return _endPoint->GetId(); }
    void SetInput(std::string languageId);
    void SetOutput(std::string languageId, std::string voiceId);
    static size_t GetLanguageVoiceKey(const std::shared_ptr<ConsumerTranslator>& consumer);
    static size_t GetLanguageVoiceKey(const std::string& languageId, const std::string& voiceId);
private:
    const std::shared_ptr<TranslatorEndPoint> _endPoint;
    ProtectedUniquePtr<RtpPacketsTimeline> _timeline;
    ProtectedObj<PlayInfo> _playInfo;
    ProtectedObj<std::unordered_set<uint64_t>> _consumers;
    size_t _languageVoiceKey = 0U; // under protection of [_consumers]
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
    LOCK_WRITE_PROTECTED_OBJ(_endPoints);
    _endPoints->clear();
}

void ConsumersManager::SetInputLanguage(const std::string& languageId)
{
    if (!languageId.empty()) {
        bool changed = false;
        {
            LOCK_WRITE_PROTECTED_OBJ(_inputLanguageId);
            if (_inputLanguageId.ConstRef() != languageId) {
                _inputLanguageId = languageId;
                changed = true;
            }
        }
        if (changed) {
            LOCK_READ_PROTECTED_OBJ(_endPoints);
            for (auto it = _endPoints->begin(); it != _endPoints->end(); ++it) {
                it->second->SetInput(languageId);
            }
        }
    }
}

std::string ConsumersManager::GetInputLanguage() const
{
    LOCK_READ_PROTECTED_OBJ(_inputLanguageId);
    return _inputLanguageId.ConstRef();
}

bool ConsumersManager::AddConsumer(const std::shared_ptr<ConsumerTranslator>& consumer)
{
    if (consumer) {
        LOCK_WRITE_PROTECTED_OBJ(_endPoints);
        for (auto it = _endPoints->begin(); it != _endPoints->end(); ++it) {
            if (it->second->AddConsumer(consumer)) {
                return true; // done, language & voice ID was matched
            }
        }
        return AddNewEndPointFor(consumer);
    }
    return false;
}

bool ConsumersManager::UpdateConsumer(const std::shared_ptr<ConsumerTranslator>& consumer)
{
    if (consumer) {
        bool deprecated = false;
        auto languageId = consumer->GetLanguageId();
        auto voiceId = consumer->GetVoiceId();
        const auto key = EndPointInfo::GetLanguageVoiceKey(languageId, voiceId);
        LOCK_WRITE_PROTECTED_OBJ(_endPoints);
        for (auto it = _endPoints->begin(); it != _endPoints->end(); ++it) {
            if (key != it->second->GetLanguageVoiceKey()) {
                switch (it->second->RemoveConsumer(consumer)) {
                    case RemoveResult::Succeeded:
                        deprecated = true;
                        break;
                    case RemoveResult::SucceededNoMoreConsumers:
                        it->second->SetOutput(languageId, voiceId);
                        // add consumer again
                        MS_ASSERT(it->second->AddConsumer(consumer), "failed add consumer to updated end-point");
                        return true; // done
                    default:
                        break;
                }
            }
        }
        if (deprecated) {
            for (auto it = _endPoints->begin(); it != _endPoints->end(); ++it) {
                if (it->second->AddConsumer(consumer)) {
                    return true;
                }
            }
            // no more suitable end-points found
            return AddNewEndPointFor(consumer, std::move(languageId), std::move(voiceId));
        }
    }
    return false;
}

bool ConsumersManager::RemoveConsumer(const std::shared_ptr<ConsumerTranslator>& consumer)
{
    if (consumer) {
        LOCK_WRITE_PROTECTED_OBJ(_endPoints);
        for (auto it = _endPoints->begin(); it != _endPoints->end(); ++it) {
            const auto result = it->second->RemoveConsumer(consumer);
            if (RemoveResult::Failed != result) {
                if (RemoveResult::SucceededNoMoreConsumers == result) {
                    _endPoints->erase(it);
                }
                return true;
            }
        }
    }
    return false;
}

void ConsumersManager::DispatchOriginalPacket(RtpPacket* packet, RtpPacketsCollector* collector)
{
    if (packet) {
        _originalTimeline.SetTimestamp(packet->GetTimestamp());
        _originalTimeline.SetSeqNumber(packet->GetSequenceNumber());
        std::unordered_set<uint64_t> rejectedConsumers;
        {
            LOCK_READ_PROTECTED_OBJ(_endPoints);
            if (!_endPoints->empty()) {
                for (auto it = _endPoints->begin(); it != _endPoints->end(); ++it) {
                    if (it->second->IsPlaying()) {
                        rejectedConsumers.merge(GetMyConsumers(it->first));
                    }
                    else {
                        const auto delta = _originalTimeline.GetTimestampDelta();
                        if (auto mapped = it->second->MapOriginalPacket(delta, packet)) {
                            rejectedConsumers.merge(GetMyConsumers(it->first));
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
    const auto endPoint = GetEndPoint(endPointId);
    if (endPoint && endPoint->IsStub()) {
        // fake media ID
        if (connected) {
            endPoint->BeginMediaPlay(1U, _originalTimeline);
        }
        else {
            endPoint->EndMediaPlay(1U);
        }
    }
}

void ConsumersManager::BeginPacketsSending(uint64_t mediaId, uint64_t endPointId)
{
    const auto endPoint = GetEndPoint(endPointId);
    if (endPoint && !endPoint->IsStub()) {
        endPoint->BeginMediaPlay(mediaId, _originalTimeline);
    }
}

void ConsumersManager::SendPacket(uint64_t mediaId, uint64_t endPointId,
                                  RtpTranslatedPacket packet,
                                  RtpPacketsCollector* output)
{
    if (auto rtp = packet.Take()) {
        const auto endPoint = GetEndPoint(endPointId);
        if (endPoint && !endPoint->IsStub()) {
            if (endPoint->AdvanceTranslatedPacket(packet.GetTimestampOffset(), rtp.get())) {
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
    const auto endPoint = GetEndPoint(endPointId);
    if (endPoint && !endPoint->IsStub()) {
        endPoint->EndMediaPlay(mediaId);
    }
}

bool ConsumersManager::AddNewEndPointFor(const std::shared_ptr<ConsumerTranslator>& consumer)
{
    return consumer && AddNewEndPointFor(consumer, consumer->GetLanguageId(), consumer->GetVoiceId());
}

bool ConsumersManager::AddNewEndPointFor(const std::shared_ptr<ConsumerTranslator>& consumer,
                                         std::string consumerLanguageId,
                                         std::string consumerVoiceId)
{
    bool added = false;
    if (consumer) {
        if (auto endPoint = CreateEndPoint()) {
            const auto endPointId = endPoint->GetId();
            endPoint->SetOutput(std::move(consumerLanguageId), std::move(consumerVoiceId));
            added = endPoint->AddConsumer(consumer);
            if (added) {
                _endPoints->insert(std::make_pair(endPointId, std::move(endPoint)));
            }
            else {
                MS_ERROR_STD("failed add consumer to new end-point for language [%s] & voice [%s]",
                             consumerLanguageId.c_str(),
                             consumerVoiceId.c_str());
            }
        }
        else {
            MS_ERROR_STD("failed create of new end-point for language [%s] & voice [%s]",
                         consumerLanguageId.c_str(),
                         consumerVoiceId.c_str());
        }
    }
    return added;
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
        LOCK_READ_PROTECTED_OBJ(_endPoints);
        const auto it = _endPoints->find(endPointId);
        if (it != _endPoints->end()) {
            return it->second;
        }
    }
    return nullptr;
}

std::unordered_set<uint64_t> ConsumersManager::GetConsumers(uint64_t endPointId, bool alien) const
{
    std::unordered_set<uint64_t> consumers;
    if (endPointId) {
        LOCK_READ_PROTECTED_OBJ(_endPoints);
        if (alien) {
            for (auto it = _endPoints->begin(); it != _endPoints->end(); ++it) {
                if (it->first != endPointId) {
                    consumers.merge(it->second->GetConsumers());
                }
            }
        }
        else {
            const auto it = _endPoints->find(endPointId);
            if (it != _endPoints->end()) {
                consumers = it->second->GetConsumers();
            }
        }
    }
    return consumers;
}

ConsumersManager::EndPointInfo::EndPointInfo(std::shared_ptr<TranslatorEndPoint> endPoint)
    : _endPoint(std::move(endPoint))
    , _playInfo(0U, 0U)
{
}

ConsumersManager::EndPointInfo::~EndPointInfo()
{
    _endPoint->RemoveAllOutputMediaSinks();
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
    }
}

void ConsumersManager::EndPointInfo::EndMediaPlay(uint64_t mediaId)
{
    LOCK_WRITE_PROTECTED_OBJ(_playInfo);
    if (_playInfo->first == mediaId) {
        _playInfo->first = 0U;
        _playInfo->second = 0U;
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
                    _playInfo->second += timeline->GetTimestampDelta();
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

std::unordered_set<uint64_t> ConsumersManager::EndPointInfo::GetConsumers() const
{
    LOCK_READ_PROTECTED_OBJ(_consumers);
    return _consumers.ConstRef();
}

size_t ConsumersManager::EndPointInfo::GetConsumersCount() const
{
    LOCK_READ_PROTECTED_OBJ(_consumers);
    return _consumers->size();
}

size_t ConsumersManager::EndPointInfo::GetLanguageVoiceKey() const
{
    LOCK_READ_PROTECTED_OBJ(_consumers);
    return _languageVoiceKey;
}

bool ConsumersManager::EndPointInfo::AddConsumer(const std::shared_ptr<ConsumerTranslator>& consumer)
{
    if (consumer) {
        if (const auto id = consumer->GetId()) {
            LOCK_WRITE_PROTECTED_OBJ(_consumers);
            if (GetLanguageVoiceKey(consumer) == _languageVoiceKey) {
                _consumers->insert(id);
                return true;
            }
        }
    }
    return false;
}

RemoveResult ConsumersManager::EndPointInfo::RemoveConsumer(const std::shared_ptr<ConsumerTranslator>& consumer)
{
    if (consumer) {
        if (const auto id = consumer->GetId()) {
            LOCK_WRITE_PROTECTED_OBJ(_consumers);
            if (_consumers->erase(id)) {
                if (_consumers->empty()) {
                    return RemoveResult::SucceededNoMoreConsumers;
                }
                return RemoveResult::Succeeded;
            }
        }
    }
    return RemoveResult::Failed;
}

bool ConsumersManager::EndPointInfo::HasConsumer(const std::shared_ptr<ConsumerTranslator>& consumer) const
{
    if (consumer) {
        if (const auto id = consumer->GetId()) {
            LOCK_READ_PROTECTED_OBJ(_consumers);
            return _consumers->count(id) > 0U;
        }
    }
    return false;
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

void ConsumersManager::EndPointInfo::SetInput(std::string languageId)
{
    _endPoint->SetInputLanguageId(std::move(languageId));
}

void ConsumersManager::EndPointInfo::SetOutput(std::string languageId, std::string voiceId)
{
    const auto languageVoiceKey = GetLanguageVoiceKey(languageId, voiceId);
    LOCK_WRITE_PROTECTED_OBJ(_consumers);
    if (languageVoiceKey != _languageVoiceKey) {
        _languageVoiceKey = languageVoiceKey;
        _endPoint->SetOutputLanguageId(std::move(languageId));
        _endPoint->SetOutputVoiceId(std::move(voiceId));
    }
}

size_t ConsumersManager::EndPointInfo::GetLanguageVoiceKey(const std::shared_ptr<ConsumerTranslator>& consumer)
{
    if (consumer) {
        return GetLanguageVoiceKey(consumer->GetLanguageId(), consumer->GetVoiceId());
    }
    return 0U;
}

size_t ConsumersManager::EndPointInfo::GetLanguageVoiceKey(const std::string& languageId,
                                                           const std::string& voiceId)
{
    if (!languageId.empty() && !voiceId.empty()) {
        return Utils::HashCombine(languageId, voiceId);
    }
    return 0U;
}


} // namespace RTC
