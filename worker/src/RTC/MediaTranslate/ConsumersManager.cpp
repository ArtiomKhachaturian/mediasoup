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

/*class ConsumersManager::ConsumerInfoImpl : public ConsumerInfo
{
public:
    ConsumerInfoImpl(size_t languageVoiceKey);
    size_t GetLanguageVoiceKey() const { return _languageVoiceKey.load(); }
    void SetLanguageVoiceKey(size_t languageVoiceKey);
    void SetEndPointRef(const std::weak_ptr<const TranslatorEndPoint>& endPointRef);
    void ResetEndPointRef();
    void BeginPacketsSending(uint64_t mediaId);
    void SendPacket(uint32_t rtpTimestampOffset, uint64_t mediaId,
                    RtpPacket* packet, uint32_t mappedSsrc, RtpPacketsCollector* output);
    void SendPacket(const Timestamp& offset, uint64_t mediaId,
                    RtpPacket* packet, uint32_t mappedSsrc, RtpPacketsCollector* output);
    void EndPacketsSending(uint64_t mediaId);
    // impl. of ConsumerInfo
    void SaveProducerRtpPacketInfo(const RtpPacket* packet) final;
    uint64_t GetEndPointId() const final;
    bool IsConnected() const final;
private:
    std::atomic<size_t> _languageVoiceKey = 0U;
    ProtectedWeakPtr<const TranslatorEndPoint> _endPointRef;
    RtpPacketsTimeline _producersTimeline;
    absl::flat_hash_map<uint64_t, RtpPacketsTimeline> _mediaTimelines;
};*/

class ConsumersManager::EndPointInfo
{
public:
    EndPointInfo(std::shared_ptr<TranslatorEndPoint> endPoint);
    void BeginMediaPlay(uint64_t mediaId, const RtpPacketsTimeline& timeline);
    void EndMediaPlay(uint64_t mediaId);
    bool AdvanceTranslatedPacket(const Timestamp& offset, RtpPacket* packet);
    bool AdvanceTranslatedPacket(uint32_t offset, RtpPacket* packet);
    RtpPacket* MapOriginalPacket(uint32_t offset, RtpPacket* packet);
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
    std::atomic<size_t> _outputLanguageAndVoiceIdKey = 0U;
    std::atomic<uint64_t> _playingMediaId = 0UL;
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

bool ConsumersManager::DispatchOriginalPacket(RtpPacket* packet, RtpPacketsCollector* collector)
{
    bool processed = false;
    if (packet) {
        _originalTimeline.SetTimestamp(packet->GetTimestamp());
        _originalTimeline.SetSeqNumber(packet->GetSequenceNumber());
        if (collector) {
            LOCK_READ_PROTECTED_OBJ(_endpoints);
            processed = !_endpoints->empty();
            /*for (auto it = _endpoints->begin(); it != _endpoints->end(); ++it) {
                if (const auto mapped = it->second->MapOriginalPacket(packet,
                                                                      _originalTimeline.GetTimestampDelta())) {
                    mapped->SetAcceptedConsumers(GetConsumers(it->first));
                    collector->AddPacket(mapped, _mappedSsrc, true);
                }
            }*/
        }
    }
    return processed;
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
                    rtp->SetAcceptedConsumers(GetConsumers(endPointId));
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

std::unordered_set<Consumer*> ConsumersManager::GetConsumers(uint64_t endPointId) const
{
    if (endPointId) {
        std::unordered_set<Consumer*> consumers;
        LOCK_READ_PROTECTED_OBJ(_consumerToEndpointId);
        for (auto it = _consumerToEndpointId->begin(); it != _consumerToEndpointId->end(); ++it) {
            if (it->second == endPointId) {
                consumers.insert(it->first);
            }
        }
        return consumers;
    }
    return {};
}

ConsumersManager::EndPointInfo::EndPointInfo(std::shared_ptr<TranslatorEndPoint> endPoint)
    : _endPoint(std::move(endPoint))
{
}

void ConsumersManager::EndPointInfo::BeginMediaPlay(uint64_t mediaId,
                                                    const RtpPacketsTimeline& timeline)
{
    uint64_t expected = 0UL;
    MS_ASSERT(_playingMediaId.compare_exchange_strong(expected, mediaId), "previous playing is not finished");
    LOCK_WRITE_PROTECTED_OBJ(_timeline);
    if (!_timeline->get()) {
        _timeline = std::make_unique<RtpPacketsTimeline>(timeline);
        MS_ERROR_STD("Fix timeline, TS = %u, seq. num = %u",
                     unsigned(_timeline->get()->GetTimestamp()),
                     unsigned(_timeline->get()->GetSeqNumber()));
    }
}

void ConsumersManager::EndPointInfo::EndMediaPlay(uint64_t mediaId)
{
    MS_ASSERT(_playingMediaId.compare_exchange_strong(mediaId, 0UL), "playing was not started");
}

bool ConsumersManager::EndPointInfo::AdvanceTranslatedPacket(const Timestamp& offset,
                                                             RtpPacket* packet)
{
    return packet && AdvanceTranslatedPacket(offset.GetRtpTime(), packet);
}

bool ConsumersManager::EndPointInfo::AdvanceTranslatedPacket(uint32_t offset,
                                                             RtpPacket* packet)
{
    if (packet && _playingMediaId.load()) {
        LOCK_READ_PROTECTED_OBJ(_timeline);
        if (const auto& timeline = _timeline.ConstRef()) {
            if (0 == offset) { // 1st frame
                offset = timeline->GetTimestampDelta();
            }
            packet->SetTimestamp(timeline->AdvanceTimestamp(offset));
            packet->SetSequenceNumber(timeline->AdvanceSeqNumber());
            MS_ERROR_STD("Fixed timeline after advance, TS = %u, seq. num = %u",
                         unsigned(_timeline->get()->GetTimestamp()),
                         unsigned(_timeline->get()->GetSeqNumber()));
            return true;
        }
    }
    return false;
}

RtpPacket* ConsumersManager::EndPointInfo::MapOriginalPacket(uint32_t offset,
                                                             RtpPacket* packet)
{
    if (packet && !_playingMediaId.load()) {
        LOCK_READ_PROTECTED_OBJ(_timeline);
        if (const auto& timeline = _timeline.ConstRef()) {
            packet = packet->Clone();
            packet->SetTimestamp(timeline->AdvanceTimestamp(offset));
            packet->SetSequenceNumber(timeline->AdvanceSeqNumber());
            return packet;
        }
        return nullptr;
    }
    return packet;
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

/*RtpPacket* ConsumersManager::EndPointInfo::GetCorrectedPacket(RtpPacket* packet,
                                                              uint32_t timestampDelta)
{
    if (packet) {
        LOCK_READ_PROTECTED_OBJ(_timeline);
        if (const auto& timeline = _timeline.ConstRef()) {
            if (0UL == _playingMediaId.load()) {
                timeline->AdvanceTimestamp(timestampDelta);
                timeline->AdvanceSeqNumber();
            }
        }
    }
    return packet;
}*/

/*std::shared_ptr<ConsumersManager::EndPointInfo> ConsumersManager::GetEndPoint(size_t languageVoiceKey) const
{
    if (languageVoiceKey) {
        LOCK_READ_PROTECTED_OBJ(_languageVoiceKeyToEndpointId);
        const auto it = _languageVoiceKeyToEndpointId->find(languageVoiceKey);
        if (it != _languageVoiceKeyToEndpointId->end()) {
            return GetEndPointById(it->second);
        }
    }
    return nullptr;
}*/

/*std::shared_ptr<TranslatorEndPoint> ConsumersManager::AddNewEndPoint(const Consumer* consumer,
                                                                     size_t key)
{
    if (consumer && key) {
        MS_ASSERT(0 == _endpoints.count(key), "such end-point is already present");
        MS_ASSERT(key == consumer->GetLanguageVoiceKey(), "settings key mistmatch");
        const auto endPoint = CreateEndPoint();
        MS_ASSERT(endPoint, "null end-point");
        endPoint->SetOutputLanguageId(consumer->GetLanguageId());
        endPoint->SetOutputVoiceId(consumer->GetVoiceId());
        _endpoints[key] = std::make_pair(endPoint, 1UL);
        return endPoint;
    }
    return nullptr;
}

std::shared_ptr<TranslatorEndPoint> ConsumersManager::CreateEndPoint() const
{
    if (auto endPoint = _endPointsFactory->CreateEndPoint()) {
        if (!endPoint->AddOutputMediaSink(_translationsOutput)) {
            // TODO: log error
        }
        else {
            endPoint->SetInputMediaSource(_translationsInput);
            endPoint->SetInputLanguageId(_inputLanguageId);
            return endPoint;
        }
    }
    return nullptr;
}

std::shared_ptr<TranslatorEndPoint> ConsumersManager::GetEndPoint(const Consumer* consumer) const
{
    if (consumer) {
        return GetEndPoint(consumer->GetLanguageVoiceKey());
    }
    return nullptr;
}

std::shared_ptr<TranslatorEndPoint> ConsumersManager::GetEndPoint(size_t key) const
{
    if (key) {
        const auto ite = _endpoints.find(key);
        if (ite != _endpoints.end()) {
            return ite->second.first;
        }
    }
    return nullptr;
}*/

/*ConsumersManager::ConsumerInfoImpl::ConsumerInfoImpl(size_t languageVoiceKey)
{
    SetLanguageVoiceKey(languageVoiceKey);
}

void ConsumersManager::ConsumerInfoImpl::SetLanguageVoiceKey(size_t languageVoiceKey)
{
    MS_ASSERT(languageVoiceKey, "language/voice key must not be zero");
    _languageVoiceKey = languageVoiceKey;
}

void ConsumersManager::ConsumerInfoImpl::
    SetEndPointRef(const std::weak_ptr<const TranslatorEndPoint>& endPointRef)
{
    LOCK_WRITE_PROTECTED_OBJ(_endPointRef);
    _endPointRef = endPointRef;
}

void ConsumersManager::ConsumerInfoImpl::ResetEndPointRef()
{
    SetEndPointRef(std::weak_ptr<const TranslatorEndPoint>());
}

void ConsumersManager::ConsumerInfoImpl::BeginPacketsSending(uint64_t mediaId)
{
    if (!_mediaTimelines.count(mediaId)) {
        _mediaTimelines[mediaId] = _producersTimeline;
    }
}

void ConsumersManager::ConsumerInfoImpl::SendPacket(uint32_t rtpTimestampOffset,
                                                    uint64_t mediaId,
                                                    RtpPacket* packet,
                                                    uint32_t mappedSsrc,
                                                    RtpPacketsCollector* output)
{
    if (packet) {
        if (output) {
            const auto it = _mediaTimelines.find(mediaId);
            if (it != _mediaTimelines.end()) {
                packet->SetTimestamp(_producersTimeline.GetNextTimestamp() + rtpTimestampOffset);
                packet->SetSequenceNumber(it->second.GetNextSeqNumber());
                it->second.SetLastTimestamp(packet->GetTimestamp());
                output->AddPacket(packet, mappedSsrc);
                packet = nullptr;
            }
        }
        delete packet;
    }
}

void ConsumersManager::ConsumerInfoImpl::SendPacket(const Timestamp& offset, uint64_t mediaId,
                                                    RtpPacket* packet, uint32_t mappedSsrc,
                                                    RtpPacketsCollector* output)
{
    if (packet) {
        SendPacket(offset.GetRtpTime(), mediaId, packet, mappedSsrc, output);
    }
}

void ConsumersManager::ConsumerInfoImpl::EndPacketsSending(uint64_t mediaId)
{
    const auto it = _mediaTimelines.find(mediaId);
    if (it != _mediaTimelines.end()) {
        for (auto ito = _mediaTimelines.begin(); ito != _mediaTimelines.end(); ++ito) {
            if (ito != it) {
                ito->second = it->second;
            }
        }
        _producersTimeline = it->second;
        _mediaTimelines.erase(it);
    }
}

void ConsumersManager::ConsumerInfoImpl::SaveProducerRtpPacketInfo(const RtpPacket* packet)
{
    _producersTimeline.CopyPacketInfoFrom(packet);
}

uint64_t ConsumersManager::ConsumerInfoImpl::GetEndPointId() const
{
    LOCK_READ_PROTECTED_OBJ(_endPointRef);
    if (const auto endPoint = _endPointRef->lock()) {
        return endPoint->GetId();
    }
    return 0ULL;
}

bool ConsumersManager::ConsumerInfoImpl::IsConnected() const
{
    LOCK_READ_PROTECTED_OBJ(_endPointRef);
    const auto endPoint = _endPointRef->lock();
    return endPoint && endPoint->IsConnected();
}*/

} // namespace RTC
