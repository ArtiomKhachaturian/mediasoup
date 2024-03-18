#define MS_CLASS "RTC::TranslatorSource"
#include "RTC/MediaTranslate/TranslatorSource.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/MediaTranslate/MediaSource.hpp"
#include "RTC/MediaTranslate/WebM/WebMSerializer.hpp"
#ifdef WRITE_PRODUCER_RECV_TO_FILE
#include "RTC/MediaTranslate/FileSinkWriter.hpp"
#endif
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayer.hpp"
#include "RTC/RtpStream.hpp"
#include "Logger.hpp"

namespace RTC
{

TranslatorSource::TranslatorSource(uint32_t clockRate, uint32_t originalSsrc,
                                   uint32_t mappedSsrc, uint8_t payloadType,
                                   std::unique_ptr<MediaFrameSerializer> serializer,
                                   TranslatorEndPointFactory* endPointsFactory,
                                   RtpPacketsPlayer* rtpPacketsPlayer,
                                   RtpPacketsCollector* output)
    : _mappedSsrc(mappedSsrc)
    , _payloadType(payloadType)
    , _serializer(std::move(serializer))
    , _rtpPacketsPlayer(rtpPacketsPlayer)
    , _output(output)
    , _consumersManager(endPointsFactory, _serializer.get(), GetMediaReceiver(),
                        mappedSsrc, clockRate, _serializer->GetMime())
{
    _rtpPacketsPlayer->AddStream(GetOriginalSsrc(), GetClockRate(),
                                 GetPayloadType(), GetMime(), this);
}

TranslatorSource::~TranslatorSource()
{
    _rtpPacketsPlayer->RemoveStream(GetOriginalSsrc());
    _serializer->RemoveAllSinks();
}

std::unique_ptr<TranslatorSource> TranslatorSource::Create(const RtpCodecMimeType& mime,
                                                           uint32_t clockRate,
                                                           uint32_t originalSsrc,
                                                           uint32_t mappedSsrc,
                                                           uint8_t payloadType,
                                                           TranslatorEndPointFactory* endPointsFactory,
                                                           RtpPacketsPlayer* rtpPacketsPlayer,
                                                           RtpPacketsCollector* output,
                                                           const std::shared_ptr<BufferAllocator>& allocator)
{
    std::unique_ptr<TranslatorSource> source;
    if (endPointsFactory && rtpPacketsPlayer && output) {
        MS_ASSERT(clockRate, "clock rate must be greater than zero");
        MS_ASSERT(originalSsrc, "original SSRC must be greater than zero");
        MS_ASSERT(mappedSsrc, "mapped SSRC must be greater than zero");
        MS_ASSERT(payloadType, "payload type must be greater than zero");
        auto serializer = std::make_unique<WebMSerializer>(mime, originalSsrc, clockRate, allocator);
        source.reset(new TranslatorSource(clockRate, originalSsrc, mappedSsrc,
                                          payloadType, std::move(serializer),
                                          endPointsFactory, rtpPacketsPlayer, output));
    }
    return source;
}

std::unique_ptr<TranslatorSource> TranslatorSource::Create(const RtpStream* stream, uint32_t mappedSsrc,
                                                           TranslatorEndPointFactory* endPointsFactory,
                                                           RtpPacketsPlayer* rtpPacketsPlayer,
                                                           RtpPacketsCollector* output,
                                                           const std::shared_ptr<BufferAllocator>& allocator)
{
    if (stream && endPointsFactory && rtpPacketsPlayer && output) {
        return Create(stream->GetMimeType(), stream->GetClockRate(),
                      stream->GetSsrc(), mappedSsrc, stream->GetPayloadType(),
                      endPointsFactory, rtpPacketsPlayer, output, allocator);
    }
    return nullptr;
}

const RtpCodecMimeType& TranslatorSource::GetMime() const
{
    return _serializer->GetMime();
}

uint32_t TranslatorSource::GetClockRate() const
{
    return _serializer->GetClockRate();
}

uint32_t TranslatorSource::GetOriginalSsrc() const
{
    return _serializer->GetSsrc();
}

void TranslatorSource::SetPaused(bool paused)
{
    _serializer->SetPaused(paused);
    _rtpPacketsPlayer->Pause(GetOriginalSsrc(), paused);
}

void TranslatorSource::AddOriginalRtpPacketForTranslation(RtpPacket* packet)
{
    if (packet) {
        _consumersManager.DispatchOriginalPacket(packet, _output);
        _serializer->Write(packet);
    }
}

void TranslatorSource::SetInputLanguage(const std::string& languageId)
{
    _consumersManager.SetInputLanguage(languageId);
}

bool TranslatorSource::AddConsumer(const std::shared_ptr<ConsumerTranslator>& consumer)
{
    return _consumersManager.AddConsumer(consumer);
}

bool TranslatorSource::UpdateConsumer(const std::shared_ptr<ConsumerTranslator>& consumer)
{
    return _consumersManager.UpdateConsumer(consumer);
}

bool TranslatorSource::RemoveConsumer(const std::shared_ptr<ConsumerTranslator>& consumer)
{
    return _consumersManager.RemoveConsumer(consumer);
}

#ifdef WRITE_PRODUCER_RECV_TO_FILE
void TranslatorSource::AddProducerInputFileWriter(const std::string& producerId)
{
    const auto fileExtension = _serializer->GetFileExtension();
    if (!fileExtension.empty()) {
        const auto depacketizerPath = std::getenv("MEDIASOUP_DEPACKETIZER_PATH");
        if (depacketizerPath && std::strlen(depacketizerPath)) {
            std::string fileName = producerId + "_" + std::to_string(GetOriginalSsrc()) +
                "." + std::string(fileExtension);
            fileName = std::string(depacketizerPath) + "/" + fileName;
            if (auto writer = FileSinkWriter::Create(std::move(fileName))) {
                if (!_serializer->AddSink(writer.get())) {
                    writer->DeleteFromStorage();
                    writer.reset();
                }
                if (_producerInputFileWriter != writer) {
                    _serializer->RemoveSink(_producerInputFileWriter.get());
                    _producerInputFileWriter = std::move(writer);
                }
            }
        }
    }
}
#endif

void TranslatorSource::OnPlayStarted(uint64_t mediaId, uint64_t mediaSourceId, uint32_t ssrc)
{
    RtpPacketsPlayerCallback::OnPlayStarted(mediaId, mediaSourceId, ssrc);
    _consumersManager.BeginPacketsSending(mediaId, mediaSourceId);
}

void TranslatorSource::OnPlay(uint64_t mediaId, uint64_t mediaSourceId, RtpTranslatedPacket packet)
{
    if (packet) {
        _consumersManager.SendPacket(mediaId, mediaSourceId, std::move(packet), _output);
    }
}

void TranslatorSource::OnPlayFinished(uint64_t mediaId, uint64_t mediaSourceId, uint32_t ssrc)
{
    RtpPacketsPlayerCallback::OnPlayFinished(mediaId, mediaSourceId, ssrc);
    _consumersManager.EndPacketsSending(mediaId, mediaSourceId);
}

void TranslatorSource::NotifyThatConnectionEstablished(uint64_t endPointId, bool connected)
{
    TranslatorEndPointSink::NotifyThatConnectionEstablished(endPointId, connected);
    _consumersManager.NotifyThatConnected(endPointId, connected);
    if (!connected) {
        _rtpPacketsPlayer->Stop(GetOriginalSsrc(), endPointId);
    }
}

void TranslatorSource::WriteMediaPayload(uint64_t endPointId, const std::shared_ptr<Buffer>& buffer)
{
    if (buffer) {
        _rtpPacketsPlayer->Play(GetOriginalSsrc(), endPointId, buffer);
    }
}

} // namespace RTC
