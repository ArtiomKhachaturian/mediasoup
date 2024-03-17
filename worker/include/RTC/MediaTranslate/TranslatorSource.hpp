#pragma once
#include "RTC/MediaTranslate/TranslatorEndPoint/TranslatorEndPointSink.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerCallback.hpp"
#include "RTC/MediaTranslate/ConsumersManager.hpp"
#include "RTC/MediaTranslate/TranslatorDefines.hpp"
#include <memory>
#include <string>

namespace RTC
{

class ConsumerTranslator;
class BufferAllocator;
class MediaFrameSerializer;
class RtpPacket;
class RtpCodecMimeType;
class RtpStream;
class RtpPacketsPlayer;
class RtpPacketsCollector;
class TranslatorEndPointFactory;

class TranslatorSource : private RtpPacketsPlayerCallback, // receiver of translated audio (RTP) packets
                         private TranslatorEndPointSink // receiver of translated audio frames from end-point
{
public:
	~TranslatorSource() final;
	static std::unique_ptr<TranslatorSource> Create(const RtpCodecMimeType& mime,
                                                	uint32_t clockRate,
                                                	uint32_t originalSsrc,
                                                    uint32_t mappedSsrc,
                                                	uint8_t payloadType,
                                                	TranslatorEndPointFactory* endPointsFactory,
                                                	RtpPacketsPlayer* rtpPacketsPlayer,
                                                	RtpPacketsCollector* output,
                                                	const std::string& producerId,
                                                    const std::shared_ptr<BufferAllocator>& allocator = nullptr);
    static std::unique_ptr<TranslatorSource> Create(const RtpStream* stream, uint32_t mappedSsrc,
                                                    TranslatorEndPointFactory* endPointsFactory,
                                                    RtpPacketsPlayer* rtpPacketsPlayer,
                                                    RtpPacketsCollector* output,
                                                    const std::string& producerId,
                                                    const std::shared_ptr<BufferAllocator>& allocator = nullptr);
    const RtpCodecMimeType& GetMime() const;
    uint32_t GetClockRate() const;
    uint8_t GetPayloadType() const { return _payloadType; }
    uint32_t GetOriginalSsrc() const { return _originalSsrc; }
    uint32_t GetMappedSsrc() const { return _mappedSsrc; }
    void SetPaused(bool paused);
    void AddOriginalRtpPacketForTranslation(RtpPacket* packet);
    void SetInputLanguage(const std::string& languageId);
    bool AddConsumer(const std::shared_ptr<ConsumerTranslator>& consumer);
    bool UpdateConsumer(const std::shared_ptr<ConsumerTranslator>& consumer);
    bool RemoveConsumer(const std::shared_ptr<ConsumerTranslator>& consumer);
private:
	TranslatorSource(uint32_t clockRate, uint32_t originalSsrc,
                     uint32_t mappedSsrc, uint8_t payloadType,
                 	 std::unique_ptr<MediaFrameSerializer> serializer,
                 	 TranslatorEndPointFactory* endPointsFactory,
	                 RtpPacketsPlayer* rtpPacketsPlayer,
                 	 RtpPacketsCollector* output,
                 	 const std::string& producerId);
    TranslatorEndPointSink* GetMediaReceiver() { return this; }
#ifdef WRITE_PRODUCER_RECV_TO_FILE
    std::unique_ptr<MediaSink> CreateSerializerFileWriter(const std::string& producerId) const;
#endif
    // impl. of RtpPacketsPlayerCallback
    void OnPlayStarted(uint64_t mediaId, uint64_t mediaSourceId, uint32_t ssrc) final;
    void OnPlay(uint64_t mediaId, uint64_t mediaSourceId, RtpTranslatedPacket packet) final;
    void OnPlayFinished(uint64_t mediaId, uint64_t mediaSourceId, uint32_t ssrc) final;
    // impl. of TranslatorEndPointSink
    void NotifyThatConnectionEstablished(uint64_t endPointId, bool connected) final;
    void WriteMediaPayload(uint64_t endPointId, const std::shared_ptr<Buffer>& buffer) final;
private:
    const uint32_t _originalSsrc;
    const uint32_t _mappedSsrc;
    const uint8_t _payloadType;
    const std::unique_ptr<MediaFrameSerializer> _serializer;
    RtpPacketsPlayer* const _rtpPacketsPlayer;
    RtpPacketsCollector* const _output;
#ifdef WRITE_PRODUCER_RECV_TO_FILE
    const std::unique_ptr<MediaSink> _serializerFileWriter;
#endif
    ConsumersManager _consumersManager;
};

} // namespace RTC
