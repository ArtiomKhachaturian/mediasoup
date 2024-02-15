#define MS_CLASS "RTC::ConsumersManager"
#include "RTC/MediaTranslate/ConsumersManager.hpp"
#include "RTC/MediaTranslate/ConsumerInfo.hpp"
#include "RTC/MediaTranslate/TranslatorEndPoint/TranslatorEndPoint.hpp"
#include "RTC/MediaTranslate/TranslatorEndPoint/TranslatorEndPointFactory.hpp"
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
    // impl. of ConsumerInfo
    void SaveProducerRtpPacketInfo(const RtpPacket* packet) final;
    void AlignProducerRtpPacketInfo(RtpPacket* packet) final;
    void AlignTranslatedRtpPacketInfo(uint32_t rtpTimestampOffset, RtpPacket* packet) final;
    std::shared_ptr<const TranslatorEndPoint> GetEndPoint() const final;
private:
    std::atomic<size_t> _languageVoiceKey = 0U;
    ProtectedWeakPtr<const TranslatorEndPoint> _endPointRef;
    uint32_t _lastRtpTimestamp = 0U;
    uint32_t _rtpTimestampDelta = 0U;
    uint16_t _lastRtpSeqNumber = 0U;
};

ConsumersManager::ConsumersManager(uint32_t ssrc,
                                   TranslatorEndPointFactory* endPointsFactory,
                                   MediaSource* translationsInput,
                                   TranslatorEndPointListener* translationsOutput)
    : _ssrc(ssrc)
    , _endPointsFactory(endPointsFactory)
    , _translationsInput(translationsInput)
    , _translationsOutput(translationsOutput)
{
}

ConsumersManager::~ConsumersManager()
{
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

std::shared_ptr<ConsumerInfo> ConsumersManager::AddConsumer(const Consumer* consumer)
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

void ConsumersManager::UpdateConsumer(const Consumer* consumer)
{
    if (consumer) {
        const auto it = _consumersInfo.find(consumer);
        if (it != _consumersInfo.end()) {
            UpdateConsumer(consumer, it->second);
        }
    }
}

bool ConsumersManager::RemoveConsumer(const Consumer* consumer)
{
    if (consumer) {
        const auto it = _consumersInfo.find(consumer);
        if (it != _consumersInfo.end()) {
            MS_ASSERT(it->second->GetLanguageVoiceKey() == consumer->GetLanguageVoiceKey(),
                      "output language & voice changes was not reflected before");
            const auto ite = _endpoints.find(it->second->GetLanguageVoiceKey());
            if (ite != _endpoints.end() && 0ULL == --ite->second.second) { // decrease counter
                _endpoints.erase(ite);
            }
            it->second->ResetEndPointRef();
            _consumersInfo.erase(it);
            return true;
        }
    }
    return false;
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
    if (auto endPoint = _endPointsFactory->CreateEndPoint(_endpoints.empty(), _ssrc)) {
        endPoint->SetInput(_translationsInput);
        endPoint->SetOutput(_translationsOutput);
        endPoint->SetInputLanguageId(_inputLanguageId);
        return endPoint;
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

void ConsumersManager::ConsumerInfoImpl::SaveProducerRtpPacketInfo(const RtpPacket* packet)
{
    if (packet) {
        const auto timestamp = packet->GetTimestamp();
        if (timestamp != _lastRtpTimestamp) {
            if (_lastRtpTimestamp && timestamp > _lastRtpTimestamp) {
                _rtpTimestampDelta = _lastRtpTimestamp - timestamp;
                //MS_ERROR_STD("_rtpTimestampDelta = %u", _rtpTimestampDelta);
            }
            _lastRtpTimestamp = timestamp;
        }
        _lastRtpSeqNumber = packet->GetSequenceNumber();
    }
}

void ConsumersManager::ConsumerInfoImpl::AlignProducerRtpPacketInfo(RtpPacket* packet)
{
    if (packet) {
        
    }
}

void ConsumersManager::ConsumerInfoImpl::AlignTranslatedRtpPacketInfo(uint32_t rtpTimestampOffset,
                                                                      RtpPacket* packet)
{
    if (packet) {
        auto nextTimestamp = _lastRtpTimestamp;
        if (rtpTimestampOffset) {
            _rtpTimestampDelta = rtpTimestampOffset;
        }
        nextTimestamp += _rtpTimestampDelta;
        packet->SetTimestamp(nextTimestamp);
        packet->SetSequenceNumber(++_lastRtpSeqNumber);
        _lastRtpTimestamp = nextTimestamp;
    }
}

std::shared_ptr<const TranslatorEndPoint> ConsumersManager::ConsumerInfoImpl::GetEndPoint() const
{
    LOCK_READ_PROTECTED_OBJ(_endPointRef);
    return _endPointRef->lock();
}

/*void ConsumersManager::ConsumerInfo::SetLastRtpPacketInfo(const RtpPacket* packet)
{
    if (packet) {
        const auto timestamp = packet->GetTimestamp();
        if (timestamp != _lastRtpTimestamp) {
            if (_lastRtpTimestamp && timestamp > _lastRtpTimestamp) {
                _rtpTimestampDelta = _lastRtpTimestamp - timestamp;
                //MS_ERROR_STD("_rtpTimestampDelta = %u", _rtpTimestampDelta);
            }
            _lastRtpTimestamp = timestamp;
        }
        _lastRtpSeqNumber = packet->GetSequenceNumber();
    }
}*/

bool ConsumerInfo::IsConnected() const
{
    const auto endPoint = GetEndPoint();
    return endPoint && endPoint->IsConnected();
}

} // namespace RTC
