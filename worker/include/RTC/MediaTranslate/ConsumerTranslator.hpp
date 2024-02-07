#pragma once

#include "RTC/MediaTranslate/ConsumerTranslatorSettings.hpp"
#include "RTC/MediaTranslate/MediaSink.hpp"
#include "RTC/RtpDictionaries.hpp"
#include "absl/container/flat_hash_map.h"
#include <memory>
#include <optional>

namespace RTC
{

class Consumer;
class RtpPacket;
class RtpPacketsCollector;
class RtpPacketsInfoProvider;
class RtpCodecMimeType;

class ConsumerTranslator : public ConsumerTranslatorSettings,
                           public MediaSink
{
    class MediaGrabber;
    class CodecInfo;
public:
    ConsumerTranslator(const Consumer* consumer,
                       RtpPacketsCollector* packetsCollector,
                       const RtpPacketsInfoProvider* packetsInfoProvider);
    ~ConsumerTranslator() final;
    bool HadIncomingMedia() const;
    // impl. of TranslatorUnit
    const std::string& GetId() const final;
    // impl. of ConsumerTranslatorSettings
    const std::string& GetLanguageId() const final;
    const std::string& GetVoiceId() const final;
    // impl. of MediaSink
    void StartMediaWriting(uint32_t ssrc) final;
    void WriteMediaPayload(const std::shared_ptr<MemoryBuffer>& buffer) final;
    void EndMediaWriting() final;
private:
    const Consumer* const _consumer;
    RtpPacketsCollector* const _packetsCollector;
    const RtpPacketsInfoProvider* const _packetsInfoProvider;
    std::shared_ptr <MediaGrabber> _mediaGrabber;
};

} // namespace RTC
