#include "RTC/MediaTranslate/MediaTranslatorsManager.hpp"
#include "RTC/RtpPacketsCollector.hpp"
#include "RTC/MediaTranslate/RtpDepacketizer.hpp"
#include "RTC/MediaTranslate/RtpMediaFrameSerializer.hpp"
#include "RTC/MediaTranslate/OutputDevice.hpp"
#include "RTC/MediaTranslate/ProducerTranslator.hpp"
#include "RTC/MediaTranslate/ConsumerTranslator.hpp"
#include "RTC/RtpPacket.hpp"

namespace RTC
{

template<typename T>
using CodecResources = absl::flat_hash_map<RTC::RtpCodecMimeType::Subtype, T>;

class MediaTranslatorsManager::MediaPacketsCollector : public RtpPacketsCollector
{
public:
    MediaPacketsCollector() = default;
    void SetOutputDevice(OutputDevice* outputDevice);
    bool IsCompatible(const RTC::RtpCodecMimeType& mimeType) const;
    // impl. of RtpPacketsCollector
    void AddPacket(const RTC::RtpCodecMimeType& mimeType, const RtpPacket* packet) final;
private:
    RtpDepacketizer* GetDepackizer(const RTC::RtpCodecMimeType& mimeType);
    RtpMediaFrameSerializer* GetSerializer(const RTC::RtpCodecMimeType& mimeType);
    std::shared_ptr<RtpMediaFrameSerializer> GetCompatibleSerializer(const RTC::RtpCodecMimeType& mimeType) const;
private:
    CodecResources<std::unique_ptr<RtpDepacketizer>> _depacketizers;
    CodecResources<std::shared_ptr<RtpMediaFrameSerializer>> _serializers;
    OutputDevice* _outputDevice;
};

class MediaTranslatorsManager::Producer : public ProducerTranslator
{
public:
    Producer(const std::string& id, const std::weak_ptr<ProducerObserver>& observerRef);
    // impl. of ProducerTranslator
    std::shared_ptr<RtpPacketsCollector> AddAudio(uint32_t audioSsrc) final;
    std::weak_ptr<RtpPacketsCollector> SetVideo(uint32_t videoSsrc, uint32_t associatedAudioSsrc) final;
    void RemoveAudio(uint32_t audioSsrc) final;
    void RemoveVideo(uint32_t videoSsrc) final;
    void SetLanguage(const std::optional<MediaLanguage>& language) final;
    std::optional<MediaLanguage> GetLanguage() const final;
    const std::string& GetId() const final { return _id; }
    void SetOutputDevice(uint32_t audioSsrc, OutputDevice* outputDevice) final;
private:
    const std::string _id;
    const std::weak_ptr<ProducerObserver> _observerRef;
    absl::flat_hash_map<uint32_t, std::shared_ptr<MediaPacketsCollector>> _audio;
    absl::flat_hash_map<uint32_t, std::weak_ptr<MediaPacketsCollector>> _video;
    // input language
    std::optional<MediaLanguage> _language = MediaLanguage::Russian; // RU for tests only
    mutable std::mutex _languageMutex;
};

/*class MediaTranslatorsManager::ProducerData
{
private:
    
};

class MediaTranslatorsManager::OutputDataWriter
{
public:
    //virtual 
};*/

class MediaTranslatorsManager::ProducerObserver
{
public:
    virtual ~ProducerObserver() = default;
    virtual void OnProducerLanguageChanged(const std::string& producerId) = 0;
};

class MediaTranslatorsManager::ConsumerObserver
{
public:
    virtual ~ConsumerObserver() = default;
    virtual void OnConsumerLanguageChanged(const std::string& consumerId) = 0;
    virtual void OnConsumerVoiceChanged(const std::string& consumerId) = 0;
};

class MediaTranslatorsManager::Impl : public std::enable_shared_from_this<Impl>,
                                      public ProducerObserver, public ConsumerObserver
{
public:
    std::shared_ptr<ProducerTranslator> RegisterProducer(const std::string& producerId);
    void UnegisterProducer(const std::string& producerId);
    // impl. of ProducerObserver
    void OnProducerLanguageChanged(const std::string& producerId) final;
    // impl. of ConsumerObserver
    void OnConsumerLanguageChanged(const std::string& consumerId) final;
    void OnConsumerVoiceChanged(const std::string& consumerId) final;
private:
    absl::flat_hash_map<std::string, std::shared_ptr<Producer>> _producers;
};

MediaTranslatorsManager::MediaTranslatorsManager()
    : _impl(std::make_shared<Impl>())
{
}

MediaTranslatorsManager::~MediaTranslatorsManager()
{
}

std::shared_ptr<ProducerTranslator> MediaTranslatorsManager::RegisterProducer(const std::string& producerId)
{
    return _impl->RegisterProducer(producerId);
}

void MediaTranslatorsManager::UnegisterProducer(const std::string& producerId)
{
    _impl->UnegisterProducer(producerId);
}

std::shared_ptr<ProducerTranslator> MediaTranslatorsManager::Impl::RegisterProducer(const std::string& producerId)
{
    if (!producerId.empty()) {
        const auto it = _producers.find(producerId);
        if (it == _producers.end()) {
            auto producer = std::make_shared<Producer>(producerId, weak_from_this());
            _producers[producerId] = producer;
            return producer;
        }
        return it->second;
    }
    return nullptr;
}

void MediaTranslatorsManager::Impl::UnegisterProducer(const std::string& producerId)
{
    if (!producerId.empty()) {
        const auto it = _producers.find(producerId);
        if (it != _producers.end()) {
            _producers.erase(it);
        }
    }
}

void MediaTranslatorsManager::Impl::OnProducerLanguageChanged(const std::string& producerId)
{
    
}

void MediaTranslatorsManager::Impl::OnConsumerLanguageChanged(const std::string& consumerId)
{
    
}

void MediaTranslatorsManager::Impl::OnConsumerVoiceChanged(const std::string& consumerId)
{
    
}

void MediaTranslatorsManager::MediaPacketsCollector::SetOutputDevice(OutputDevice* outputDevice)
{
    _outputDevice = outputDevice;
    if (!_outputDevice) {
        _depacketizers.clear();
        _serializers.clear();
    }
    else {
        for (auto it = _serializers.begin(); it != _serializers.end(); ++it) {
            if (it->second) {
                it->second->SetOutputDevice(outputDevice);
            }
        }
    }
}

bool MediaTranslatorsManager::MediaPacketsCollector::IsCompatible(const RTC::RtpCodecMimeType& mimeType) const
{
    return nullptr != GetCompatibleSerializer(mimeType);
}

void MediaTranslatorsManager::MediaPacketsCollector::AddPacket(const RTC::RtpCodecMimeType& mimeType,
                                                               const RtpPacket* packet)
{
    if (packet && _outputDevice) {
        if (const auto depacketizer = GetDepackizer(mimeType)) {
            if (const auto serializer = GetSerializer(mimeType)) {
                serializer->Push(depacketizer->AddPacket(packet));
            }
        }
    }
}

RtpDepacketizer* MediaTranslatorsManager::MediaPacketsCollector::
    GetDepackizer(const RTC::RtpCodecMimeType& mimeType)
{
    RtpDepacketizer* depacketizer = nullptr;
    auto it = _depacketizers.find(mimeType.subtype);
    if (it == _depacketizers.end()) {
        auto newDepacketizer = RtpDepacketizer::create(mimeType);
        depacketizer = newDepacketizer.get();
        _depacketizers[mimeType.subtype] = std::move(newDepacketizer);
        if (!depacketizer) {
            // TODO: log error
        }
    }
    else {
        depacketizer = it->second.get();
    }
    return depacketizer;
}

RtpMediaFrameSerializer* MediaTranslatorsManager::MediaPacketsCollector::
    GetSerializer(const RTC::RtpCodecMimeType& mimeType)
{
    RtpMediaFrameSerializer* serializer = nullptr;
    if (_outputDevice) {
        auto it = _serializers.find(mimeType.subtype);
        if (it == _serializers.end()) {
            // find compatible serializers if any
            auto newSerializer = GetCompatibleSerializer(mimeType);
            if (!newSerializer) {
                newSerializer = RtpMediaFrameSerializer::create(mimeType);
                if (newSerializer) {
                    newSerializer->SetOutputDevice(_outputDevice);
                }
                else {
                    // TODO: log error// TODO: log error
                }
            }
            serializer = newSerializer.get();
            _serializers[mimeType.subtype] = std::move(newSerializer);
        }
        else {
            serializer = it->second.get();
        }
    }
    return serializer;
}

std::shared_ptr<RtpMediaFrameSerializer> MediaTranslatorsManager::MediaPacketsCollector::
    GetCompatibleSerializer(const RTC::RtpCodecMimeType& mimeType) const
{
    if (RTC::RtpCodecMimeType::Subtype::UNSET != mimeType.subtype) {
        for (auto it = _serializers.begin(); it != _serializers.end(); ++it) {
            if (it->second && it->second->IsCompatible(mimeType)) {
                return it->second;
            }
        }
    }
    return nullptr;
}

MediaTranslatorsManager::Producer::Producer(const std::string& id,
                                            const std::weak_ptr<ProducerObserver>& observerRef)
    : _id(id)
    , _observerRef(observerRef)
{
}

std::shared_ptr<RtpPacketsCollector> MediaTranslatorsManager::Producer::AddAudio(uint32_t audioSsrc)
{
    auto ita = _audio.find(audioSsrc);
    if (ita == _audio.end()) {
        auto collector = std::make_shared<MediaPacketsCollector>();
        _audio[audioSsrc] = collector;
        return collector;
    }
    return ita->second;
}

std::weak_ptr<RtpPacketsCollector> MediaTranslatorsManager::Producer::SetVideo(uint32_t videoSsrc,
                                                                               uint32_t associatedAudioSsrc)
{
    
}

void MediaTranslatorsManager::Producer::RemoveAudio(uint32_t audioSsrc)
{
    auto ita = _audio.find(audioSsrc);
    if (ita != _audio.end()) {
        for (auto itv = _video.begin(); itv != _video.end();) {
            if (itv->second.lock() == ita->second) {
                _video.erase(itv++);
            }
            else {
                ++itv;
            }
        }
        _audio.erase(ita);
    }
}

void MediaTranslatorsManager::Producer::RemoveVideo(uint32_t videoSsrc)
{
    auto itv = _video.find(videoSsrc);
    if (itv != _video.end()) {
        _video.erase(itv);
    }
}

void MediaTranslatorsManager::Producer::SetLanguage(const std::optional<MediaLanguage>& language)
{
    bool changed = false;
    {
        const std::lock_guard<std::mutex> lock(_languageMutex);
        if (_language != language) {
            _language = language;
            changed = true;
        }
    }
    if (changed) {
        if (const auto observer = _observerRef.lock()) {
            observer->OnProducerLanguageChanged(GetId());
        }
    }
}

std::optional<MediaLanguage> MediaTranslatorsManager::Producer::GetLanguage() const
{
    const std::lock_guard<std::mutex> lock(_languageMutex);
    return _language;
}

void MediaTranslatorsManager::Producer::SetOutputDevice(uint32_t audioSsrc, OutputDevice* outputDevice)
{
    auto ita = _audio.find(audioSsrc);
    if (ita != _audio.end()) {
        ita->second->SetOutputDevice(outputDevice);
    }
}

} // namespace RTC
