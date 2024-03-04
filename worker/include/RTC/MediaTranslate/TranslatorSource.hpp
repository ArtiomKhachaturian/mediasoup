#pragma once
#include "RTC/MediaTranslate/TranslatorEndPoint/TranslatorEndPointSink.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerCallback.hpp"
#include "RTC/MediaTranslate/ConsumersManager.hpp"
#include "RTC/MediaTranslate/TranslatorDefines.hpp"
#include <memory>
#include <string>

namespace RTC
{

class Consumer;
#ifdef WRITE_PRODUCER_RECV_TO_FILE
class FileWriter;
#endif
class BufferAllocator;
class MediaFrameSerializer;
class RtpDepacketizer;
class RtpPacket;
class RtpCodecMimeType;
class RtpStream;
class RtpPacketsPlayer;
class RtpPacketsCollector;
class TranslatorEndPointFactory;
class RtpPacketsTimeline2;

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
    void AddOriginalRtpPacketForTranslation(RtpPacket* packet);
    void SetInputLanguage(const std::string& languageId);
    void AddConsumer(Consumer* consumer);
    void UpdateConsumer(Consumer* consumer);
    void RemoveConsumer(Consumer* consumer);
private:
	TranslatorSource(uint32_t clockRate, uint32_t originalSsrc,
                     uint32_t mappedSsrc, uint8_t payloadType,
                 	 std::unique_ptr<MediaFrameSerializer> serializer,
                 	 std::unique_ptr<RtpDepacketizer> depacketizer,
                 	 TranslatorEndPointFactory* endPointsFactory,
	                 RtpPacketsPlayer* rtpPacketsPlayer,
                 	 RtpPacketsCollector* output,
                 	 const std::string& producerId);
#ifdef WRITE_PRODUCER_RECV_TO_FILE
    static std::unique_ptr<FileWriter> CreateFileWriter(uint32_t ssrc, const std::string& producerId,
                                                        const std::string_view& fileExtension);
    static std::unique_ptr<FileWriter> CreateFileWriter(uint32_t ssrc, const std::string& producerId,
                                                        const MediaFrameSerializer* serializer);
#endif
    TranslatorEndPointSink* GetMediaReceiver() { return this; }
    // impl. of RtpPacketsPlayerCallback
    void OnPlayStarted(uint64_t mediaId, uint64_t mediaSourceId, uint32_t ssrc) final;
    void OnPlay(uint64_t mediaId, uint64_t mediaSourceId, RtpTranslatedPacket packet) final;
    void OnPlayFinished(uint64_t mediaId, uint64_t mediaSourceId, uint32_t ssrc) final;
    // impl. of TranslatorEndPointSink
    void NotifyThatConnectionEstablished(const ObjectId& endPoint, bool connected) final;
    void WriteMediaPayload(const ObjectId& endPoint, const std::shared_ptr<Buffer>& buffer) final;
private:
    const uint32_t _originalSsrc;
    const uint32_t _mappedSsrc;
    const uint8_t _payloadType;
    const std::unique_ptr<MediaFrameSerializer> _serializer;
    const std::unique_ptr<RtpDepacketizer> _depacketizer;
    RtpPacketsPlayer* const _rtpPacketsPlayer;
    RtpPacketsCollector* const _output;
#ifdef WRITE_PRODUCER_RECV_TO_FILE
    std::unique_ptr<FileWriter> _fileWriter;
#endif
    ConsumersManager _consumersManager;
};

} // namespace RTC
