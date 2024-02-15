#define MS_CLASS "RTC::ConsumersManager"
#include "RTC/MediaTranslate/ConsumersManager.hpp"
#include "RTC/MediaTranslate/ConsumerInfo.hpp"
#include "RTC/MediaTranslate/TranslatorEndPoint/TranslatorEndPoint.hpp"
#include "RTC/MediaTranslate/TranslatorEndPoint/TranslatorEndPointFactory.hpp"
#include "RTC/MediaTranslate/RtpPacketsTimeline.hpp"
#include "RTC/RtpPacketsCollector.hpp"
#include "RTC/Consumer.hpp"
#include "ProtectedObj.hpp"
#include "Logger.hpp"
#include <atomic>

namespace RTC
{

class ConsumersManager::ConsumerInfoImpl : public ConsumerInfo
{
public:
    ConsumerInfoImpl(size_t languageVoiceKey);
    size_t GetLanguageVoiceKey() const { return _languageVoiceKey.load(); }
    void SetLanguageVoiceKey(size_t languageVoiceKey);
    void SetEndPointRef(const std::weak_ptr<const TranslatorEndPoint>& endPointRef);
    void ResetEndPointRef();
    void BeginPacketsSending(uint64_t mediaId);
    void SendPacket(uint32_t rtpTimestampOffset, uint64_t mediaId,
                    RtpPacket* packet, RtpPacketsCollector* output);
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
};

ConsumersManager::ConsumersManager(TranslatorEndPointFactory* endPointsFactory,
                                   MediaSource* translationsInput,
                                   MediaSink* translationsOutput)
    : _endPointsFactory(endPointsFactory)
    , _translationsInput(translationsInput)
    , _translationsOutput(translationsOutput)
{
}

ConsumersManager::~ConsumersManager()
{
    for (auto ite = _endpoints.begin(); ite != _endpoints.end(); ++ite) {
        ite->second.first->SetInputMediaSource(nullptr);
        ite->second.first->RemoveSink(_translationsOutput);
    }
}

void ConsumersManager::SetInputLanguage(const std::string& languageId)
{
    if (_inputLanguageId != languageId) {
        _inputLanguageId = languageId;
        for (auto it = _endpoints.begin(); it != _endpoints.end(); ++it) {
            it->second.first->SetInputLanguageId(languageId);
        }
    }
}

std::shared_ptr<ConsumerInfo> ConsumersManager::AddConsumer(Consumer* consumer)
{
    std::shared_ptr<ConsumerInfoImpl> info;
    if (consumer) {
        const auto it = _consumersInfo.find(consumer);
        if (it == _consumersInfo.end()) {
            const auto key = consumer->GetLanguageVoiceKey();
            auto endPoint = GetEndPoint(key);
            if (!endPoint) {
                endPoint = AddNewEndPoint(consumer, key);
            }
            if (endPoint) {
                info = std::make_shared<ConsumerInfoImpl>(key);
                info->SetEndPointRef(endPoint);
                _consumersInfo[consumer] = info;
            }
        }
        else {
            UpdateConsumer(consumer, it->second);
            info = it->second;
        }
    }
    return info;
}

void ConsumersManager::UpdateConsumer(Consumer* consumer)
{
    if (consumer) {
        const auto it = _consumersInfo.find(consumer);
        if (it != _consumersInfo.end()) {
            UpdateConsumer(consumer, it->second);
        }
    }
}

std::shared_ptr<ConsumerInfo> ConsumersManager::GetConsumer(Consumer* consumer) const
{
    if (consumer) {
        const auto it = _consumersInfo.find(consumer);
        if (it != _consumersInfo.end()) {
            return it->second;
        }
    }
    return nullptr;
}

bool ConsumersManager::RemoveConsumer(Consumer* consumer)
{
    if (consumer) {
        const auto it = _consumersInfo.find(consumer);
        if (it != _consumersInfo.end()) {
            MS_ASSERT(it->second->GetLanguageVoiceKey() == consumer->GetLanguageVoiceKey(),
                      "output language & voice changes was not reflected before");
            const auto ite = _endpoints.find(it->second->GetLanguageVoiceKey());
            if (ite != _endpoints.end() && 0ULL == --ite->second.second) { // decrease counter
                ite->second.first->SetInputMediaSource(nullptr);
                ite->second.first->RemoveSink(_translationsOutput);
                _endpoints.erase(ite);
            }
            it->second->ResetEndPointRef();
            _consumersInfo.erase(it);
            return true;
        }
    }
    return false;
}

void ConsumersManager::BeginPacketsSending(uint64_t mediaId, uint64_t endPointId)
{
    for (auto it = _consumersInfo.begin(); it != _consumersInfo.end(); ++it) {
        if (it->second->GetEndPointId() == endPointId) {
            it->second->BeginPacketsSending(mediaId);
        }
    }
}

void ConsumersManager::SendPacket(uint32_t rtpTimestampOffset, uint64_t mediaId,
                                  uint64_t endPointId, RtpPacket* packet,
                                  RtpPacketsCollector* output)
{
    if (packet) {
        if (output) {
            std::list<std::shared_ptr<ConsumerInfoImpl>> accepted;
            for (auto it = _consumersInfo.begin(); it != _consumersInfo.end(); ++it) {
                if (it->second->GetEndPointId() == endPointId) {
                    accepted.push_back(it->second);
                }
                else {
                    packet->AddRejectedConsumer(it->first);
                }
            }
            if (!accepted.empty()) {
                const auto needsCopy = accepted.size() > 1U;
                for (const auto& consumerInfo : accepted) {
                    const auto consumerPacket = needsCopy ? packet->Clone() : packet;
                    consumerInfo->SendPacket(rtpTimestampOffset, mediaId, consumerPacket, output);
                }
                if (needsCopy) {
                    delete packet;
                }
                packet = nullptr;
            }
        }
        delete packet;
    }
}

void ConsumersManager::EndPacketsSending(uint64_t mediaId, uint64_t endPointId)
{
    for (auto it = _consumersInfo.begin(); it != _consumersInfo.end(); ++it) {
        if (it->second->GetEndPointId() == endPointId) {
            it->second->EndPacketsSending(mediaId);
        }
    }
}

std::shared_ptr<TranslatorEndPoint> ConsumersManager::AddNewEndPoint(const Consumer* consumer,
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
    if (auto endPoint = _endPointsFactory->CreateEndPoint(_endpoints.empty())) {
        if (!endPoint->AddSink(_translationsOutput)) {
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
}

void ConsumersManager::UpdateConsumer(const Consumer* consumer,
                                      const std::shared_ptr<ConsumerInfoImpl>& consumerInfo)
{
    if (consumer && consumerInfo) {
        // TODO: complete this method
        /*const auto it = _consumersSettings.find(consumer);
        if (it != _consumersSettings.end()) {
            const auto settingsKey = GetSettingsKey(consumer);
            if (it->second != settingsKey) { // language or voice changed
                const auto iteOld = _endpoints.find(it->second);
                MS_ASSERT(iteOld != _endpoints.end(), "wrong consumer reference");
                --iteOld->second.second;
                const auto iteNew = _endpoints.find(settingsKey);
                
            }
            return GetEndPoint(settingsKey);
        }*/
        MS_ASSERT(false, "not yet implemented");

    }
}

ConsumersManager::ConsumerInfoImpl::ConsumerInfoImpl(size_t languageVoiceKey)
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
                                                    RtpPacketsCollector* output)
{
    if (packet) {
        if (output) {
            const auto it = _mediaTimelines.find(mediaId);
            if (it != _mediaTimelines.end()) {
                packet->SetTimestamp(_producersTimeline.GetNextTimestamp() + rtpTimestampOffset);
                packet->SetSequenceNumber(it->second.GetNextSeqNumber());
                it->second.SetLastSeqNumber(packet->GetSequenceNumber());
                it->second.SetLastTimestamp(packet->GetTimestamp());
                output->AddPacket(packet);
                packet = nullptr;
            }
        }
        delete packet;
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
}

} // namespace RTC
