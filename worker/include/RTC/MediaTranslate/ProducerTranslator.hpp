#pragma once

#include "RTC/RtpDictionaries.hpp"
#include "RTC/RtpPacketsCollector.hpp"
#include "RTC/MediaTranslate/ProducerInputMediaStreamer.hpp"
#include "RTC/MediaTranslate/ProducerObserver.hpp"
#include "RTC/MediaTranslate/ProducerObserver.hpp"
#include "RTC/MediaTranslate/TranslatorUnitImpl.hpp"
#include "ProtectedObj.hpp"
#include <absl/container/flat_hash_map.h>
#include <list>

#define WRITE_PRODUCER_RECV_AUDIO_TO_FILE

namespace RTC
{

class ProducerObserver;
class RtpMediaFrameSerializer;
class RtpStream;
class MediaFileWriter;

class ProducerTranslator : public TranslatorUnitImpl<ProducerObserver, ProducerInputMediaStreamer>,
                           public RtpPacketsCollector
{
    class MediaPacketsSinkImpl;
    template <typename T>
    using SsrcMap = absl::flat_hash_map<uint32_t, T>;
    using MediaSinksMap = SsrcMap<MediaPacketsSinkImpl*>;
    using MappedMediaMap = SsrcMap<uint32_t>;
public:
    ProducerTranslator(const std::string& id, const std::weak_ptr<ProducerObserver>& observerRef);
    ~ProducerTranslator() final;
    bool AddOutputDevice(uint32_t audioSsrc, OutputDevice* outputDevice);
    bool RemoveOutputDevice(uint32_t audioSsrc, OutputDevice* outputDevice);
    // audio
    std::list<uint32_t> GetRegisteredAudio() const; // list of ssrcs
    bool IsAudioRegistered(uint32_t ssrc) const;
    // return true if audio params was valid & this audio is not registered before
    bool RegisterAudio(uint32_t ssrc, uint32_t mappedSsrc, RtpCodecMimeType::Subtype codecType);
    bool RegisterAudio(const RtpStream* audioStream, uint32_t mappedSsrc);
    bool UnRegisterAudio(uint32_t ssrc);
    bool UnRegisterAudio(const RtpStream* audioStream);
    // video
    std::list<uint32_t> GetRegisteredVideo() const; // list of ssrcs
    bool IsVideoRegistered(uint32_t ssrc) const;
    bool RegisterVideo(uint32_t ssrc, uint32_t mappedSsrc, RtpCodecMimeType::Subtype codecType);
    bool RegisterVideo(const RtpStream* videoStream, uint32_t mappedSsrc);
    bool UnRegisterVideo(uint32_t ssrc);
    bool UnRegisterVideo(const RtpStream* videoStream);
    // video dubbing
    bool RegisterVideoDubbing(uint32_t audioSsrc, uint32_t videoSsrc);
    bool UnRegisterVideoDubbing(uint32_t audioSsrc, uint32_t videoSsrc);
    // impl. of RtpPacketsCollector
    void AddPacket(const RtpPacket* packet) final;
    // impl. of ProducerTranslator
    void SetLanguage(const std::optional<MediaLanguage>& language = std::nullopt) final;
    std::optional<MediaLanguage> GetLanguage() const final;
protected:
    void OnPauseChanged(bool pause) final;
    MediaPacketsSinkImpl* GetSink(uint32_t mappedSsrc, bool& isAudioSsrc) const;
    uint32_t GetAudioSsrc(uint32_t mappedSsrc) const;
    uint32_t GetVideoSsrc(uint32_t mappedSsrc) const;
    void RemoveVideoDubbing(uint32_t ssrc, bool isAudioSsrc);
private:
    // audio
    // sinks, key is original audio SSRC
    SsrcMap<std::unique_ptr<MediaPacketsSinkImpl>> _sinks;
    // key is original audio SSRC, value - mapped SSRC
    MappedMediaMap _audioSsrcToMappedSsrc;
    // key is mapped audio SSRC, value - original SSRC
    MappedMediaMap _audioMappedSsrcToSsrc;
#ifdef WRITE_PRODUCER_RECV_AUDIO_TO_FILE
    // key is mapped audio SSRC
    absl::flat_hash_map<uint32_t, std::shared_ptr<MediaFileWriter>> _audioFileWriters;
#endif
    // video
    // key is original video SSRC, value - mapped SSRC
    MappedMediaMap _videoSsrcToMappedSsrc;
    // key is mapped video SSRC, value - original SSRC
    MappedMediaMap _videoMappedSsrcToSsrc;
    // key is original video SSRC, value - codec type
    SsrcMap<RtpCodecMimeType::Subtype> _videoSsrcToCodecType;
    // dubbing
    // key is original video SSRC, value - original audio SSRC
    MappedMediaMap _videoSsrcToAudioSsrc;
    // key is original audio SSRC, value - original video SSRC
    MappedMediaMap _audioSsrcToVideoSsrc;
    // input language
    ProtectedOptional<MediaLanguage> _language = DefaultInputMediaLanguage();
};

} // namespace RTC
