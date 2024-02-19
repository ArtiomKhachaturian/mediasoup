#pragma once
#include "RTC/MediaTranslate/MediaSink.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerCallback.hpp"
#include "RTC/MediaTranslate/ConsumersManager.hpp"
#include "ProtectedObj.hpp"
#include <memory>
#include <string>

//#define WRITE_PRODUCER_RECV_TO_FILE // add MEDIASOUP_DEPACKETIZER_PATH env variable for reference to output folder
//#define READ_PRODUCER_RECV_FROM_FILE

namespace RTC
{

class Consumer;
#ifdef READ_PRODUCER_RECV_FROM_FILE
class FileReader;
#endif
#ifdef WRITE_PRODUCER_RECV_TO_FILE
class FileWriter;
#endif
class MediaFrameSerializer;
class RtpDepacketizer;
class RtpPacket;
class RtpCodecMimeType;
class RtpPacketsPlayer;
class RtpPacketsCollector;
class TranslatorEndPointFactory;

class TranslatorSource : private RtpPacketsPlayerCallback, // receiver of translated audio (RTP) packets
                         private MediaSink // receiver of translated audio frames from end-point
{
public:
	~TranslatorSource() final;
	static std::shared_ptr<TranslatorSource> Create(const RtpCodecMimeType& mime,
                                                	uint32_t clockRate,
                                                	uint32_t originalSsrc,
                                                	uint8_t payloadType,
                                                	TranslatorEndPointFactory* endPointsFactory,
                                                	RtpPacketsPlayer* rtpPacketsPlayer,
                                                	RtpPacketsCollector* output,
                                                	const std::string& producerId);
    const RtpCodecMimeType& GetMime() const;
    uint32_t GetClockRate() const;
    uint8_t GetPayloadType() const { return _payloadType; }
    uint32_t GetOriginalSsrc() const { return _originalSsrc; }
    uint64_t GetAddedPacketsCount() const { return _addedPacketsCount; }
    bool AddOriginalRtpPacketForTranslation(RtpPacket* packet);
    void SetInputLanguage(const std::string& languageId);
    void AddConsumer(Consumer* consumer);
    void UpdateConsumer(Consumer* consumer);
    void RemoveConsumer(Consumer* consumer);
    bool IsConnected(Consumer* consumer) const;
    void SaveProducerRtpPacketInfo(Consumer* consumer, const RtpPacket* packet);
private:
	TranslatorSource(uint32_t clockRate, uint32_t originalSsrc, uint8_t payloadType,
                 	 std::unique_ptr<MediaFrameSerializer> serializer,
                 	 std::unique_ptr<RtpDepacketizer> depacketizer,
                 	 TranslatorEndPointFactory* endPointsFactory,
	                 RtpPacketsPlayer* rtpPacketsPlayer,
                 	 RtpPacketsCollector* output,
                 	 const std::string& producerId);
#ifdef READ_PRODUCER_RECV_FROM_FILE
    static std::unique_ptr<FileReader> CreateFileReader();
#endif
#ifdef WRITE_PRODUCER_RECV_TO_FILE
    static std::unique_ptr<FileWriter> CreateFileWriter(uint32_t ssrc, const std::string& producerId,
                                                        const std::string_view& fileExtension);
    static std::unique_ptr<FileWriter> CreateFileWriter(uint32_t ssrc, const std::string& producerId,
                                                        const MediaFrameSerializer* serializer);
#endif
    // source of original audio packets, maybe mock (audio file) if READ_PRODUCER_RECV_FROM_FILE defined
    MediaSource* GetMediaSource() const;
    MediaSink* GetMediaReceiver() { return this; }
    // impl. of RtpPacketsPlayerCallback
    void OnPlayStarted(uint32_t ssrc, uint64_t mediaId, uint64_t mediaSourceId);
    void OnPlay(const Timestamp& timestampOffset, RtpPacket* packet,
                uint64_t mediaId, uint64_t mediaSourceId) final;
    void OnPlayFinished(uint32_t ssrc, uint64_t mediaId, uint64_t mediaSourceId) final;
    // impl. of MediaSink
    void WriteMediaPayload(const MediaObject& sender, const std::shared_ptr<MemoryBuffer>& buffer) final;
private:
#ifdef READ_PRODUCER_RECV_FROM_FILE
    //static inline const char* _testFileName = "/Users/user/Downloads/1b0cefc4-abdb-48d0-9c50-f5050755be94.webm";
    //static inline const char* _testFileName = "/Users/user/Documents/Sources/mediasoup_rtp_packets/producer_test.webm";
    static inline const char* _testFileName = "/Users/user/Documents/Sources/mediasoup_rtp_packets/received_translation_stereo_example.webm";
#endif
    const uint32_t _originalSsrc;
    const uint8_t _payloadType;
    const std::unique_ptr<MediaFrameSerializer> _serializer;
    const std::unique_ptr<RtpDepacketizer> _depacketizer;
    RtpPacketsPlayer* const _rtpPacketsPlayer;
    RtpPacketsCollector* const _output;
#ifdef READ_PRODUCER_RECV_FROM_FILE
    const std::unique_ptr<FileReader> _fileReader;
#endif
#ifdef WRITE_PRODUCER_RECV_TO_FILE
    std::unique_ptr<FileWriter> _fileWriter;
#endif
    ProtectedObj<ConsumersManager> _consumersManager;
    uint64_t _addedPacketsCount = 0ULL;
};

} // namespace RTC
