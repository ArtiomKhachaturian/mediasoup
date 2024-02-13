#define MS_CLASS "RTC::EndPointsManager"
#include "RTC/MediaTranslate/EndPointsManager.hpp"
#include "RTC/MediaTranslate/TranslatorEndPoint/TranslatorEndPoint.hpp"
#include "RTC/MediaTranslate/TranslatorEndPoint/TranslatorEndPointFactory.hpp"
#include "RTC/Consumer.hpp"
#include "ProtectedObj.hpp"
#include "Logger.hpp"
#include <atomic>

namespace RTC
{

class EndPointsManager::ConsumerInfo
{
public:
    ConsumerInfo(size_t languageVoiceKey);
    size_t GetLanguageVoiceKey() const { return _languageVoiceKey.load(); }
    void SetLanguageVoiceKey(size_t languageVoiceKey);
    void SetLastRtpPacketInfo(const RtpPacket* packet);
private:
    std::atomic<size_t> _languageVoiceKey = 0U;
    uint32_t _lastRtpTimestamp = 0U;
    uint32_t _rtpTimestampDelta = 0U;
    uint16_t _lastRtpSeqNumber = 0U;
};

EndPointsManager::EndPointsManager(uint32_t ssrc,
                                   TranslatorEndPointFactory* endPointsFactory,
                                   MediaSource* translationsInput,
                                   TranslatorEndPointListener* translationsOutput)
    : _ssrc(ssrc)
    , _endPointsFactory(endPointsFactory)
    , _translationsInput(translationsInput)
    , _translationsOutput(translationsOutput)
{
}

EndPointsManager::~EndPointsManager()
{
}

void EndPointsManager::SetInputLanguage(const std::string& languageId)
{
    if (_inputLanguageId != languageId) {
        _inputLanguageId = languageId;
        for (auto it = _endpoints.begin(); it != _endpoints.end(); ++it) {
            it->second.first->SetInputLanguageId(languageId);
        }
    }
}

void EndPointsManager::AddConsumer(const Consumer* consumer)
{
    if (consumer) {
        if (const auto key = consumer->GetLanguageVoiceKey()) {
            if (!GetEndPoint(key)) {
                if (AddNewEndPoint(consumer, key)) {
                    _consumersInfo.emplace(consumer, std::make_unique<ConsumerInfo>(key));
                }
            }
            else {
                UpdateConsumer(consumer);
            }
        }
    }
}

void EndPointsManager::UpdateConsumer(const Consumer* consumer)
{
    if (consumer) {
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

void EndPointsManager::RemoveConsumer(const Consumer* consumer)
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
            _consumersInfo.erase(it);
        }
    }
}

void EndPointsManager::SetLastRtpPacketInfo(const Consumer* consumer, const RtpPacket* packet)
{
    if (consumer && packet) {
        const auto it = _consumersInfo.find(consumer);
        if (it != _consumersInfo.end()) {
            it->second->SetLastRtpPacketInfo(packet);
        }
    }
}

bool EndPointsManager::IsConnected(const Consumer* consumer) const
{
    if (consumer) {
        const auto endPoint = GetEndPoint(consumer);
        return endPoint && endPoint->IsConnected();
    }
    return false;
}

std::shared_ptr<TranslatorEndPoint> EndPointsManager::AddNewEndPoint(const Consumer* consumer,
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

std::shared_ptr<TranslatorEndPoint> EndPointsManager::CreateEndPoint() const
{
    if (auto endPoint = _endPointsFactory->CreateEndPoint(_ssrc)) {
        endPoint->SetInput(_translationsInput);
        endPoint->SetOutput(_translationsOutput);
        endPoint->SetInputLanguageId(_inputLanguageId);
        return endPoint;
    }
    return nullptr;
}

std::shared_ptr<TranslatorEndPoint> EndPointsManager::GetEndPoint(const Consumer* consumer) const
{
    if (consumer) {
        return GetEndPoint(consumer->GetLanguageVoiceKey());
    }
    return nullptr;
}

std::shared_ptr<TranslatorEndPoint> EndPointsManager::GetEndPoint(size_t key) const
{
    if (key) {
        const auto ite = _endpoints.find(key);
        if (ite != _endpoints.end()) {
            return ite->second.first;
        }
    }
    return nullptr;
}

EndPointsManager::ConsumerInfo::ConsumerInfo(size_t languageVoiceKey)
{
    SetLanguageVoiceKey(languageVoiceKey);
}

void EndPointsManager::ConsumerInfo::SetLanguageVoiceKey(size_t languageVoiceKey)
{
    MS_ASSERT(languageVoiceKey, "language/voice key must not be zero");
    _languageVoiceKey = languageVoiceKey;
}

void EndPointsManager::ConsumerInfo::SetLastRtpPacketInfo(const RtpPacket* packet)
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

} // namespace RTC
