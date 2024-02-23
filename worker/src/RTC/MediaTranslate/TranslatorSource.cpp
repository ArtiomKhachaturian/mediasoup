#define MS_CLASS "RTC::TranslatorSource"
#include "RTC/MediaTranslate/TranslatorSource.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/MediaTranslate/RtpDepacketizer.hpp"
#include "RTC/MediaTranslate/MediaSource.hpp"
#include "RTC/MediaTranslate/WebM/WebMSerializer.hpp"
#include "RTC/MediaTranslate/ConsumerInfo.hpp"
#ifdef WRITE_PRODUCER_RECV_TO_FILE
#include "RTC/MediaTranslate/FileWriter.hpp"
#endif
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayer.hpp"
#include "RTC/Timestamp.hpp"
#include "RTC/RtpPacket.hpp"
#include "Logger.hpp"

namespace RTC
{

TranslatorSource::TranslatorSource(uint32_t clockRate, uint32_t originalSsrc, uint8_t payloadType,
                                   std::unique_ptr<MediaFrameSerializer> serializer,
                                   std::unique_ptr<RtpDepacketizer> depacketizer,
                                   TranslatorEndPointFactory* endPointsFactory,
                                   RtpPacketsPlayer* rtpPacketsPlayer,
                                   RtpPacketsCollector* output,
                                   const std::string& producerId)
    : _originalSsrc(originalSsrc)
    , _payloadType(payloadType)
    , _serializer(std::move(serializer))
    , _depacketizer(std::move(depacketizer))
    , _rtpPacketsPlayer(rtpPacketsPlayer)
    , _output(output)
#ifdef WRITE_PRODUCER_RECV_TO_FILE
    , _fileWriter(CreateFileWriter(GetOriginalSsrc(), producerId, _serializer.get()))
#endif
    , _consumersManager(endPointsFactory, _serializer.get(), GetMediaReceiver())
{
#ifdef WRITE_PRODUCER_RECV_TO_FILE
    if (_fileWriter && !_serializer->AddTestSink(_fileWriter.get())) {
        // TODO: log error
        _fileWriter->DeleteFromStorage();
        _fileWriter.reset();
    }
#endif
}

TranslatorSource::~TranslatorSource()
{
    _rtpPacketsPlayer->RemoveStream(GetOriginalSsrc());
    _serializer->RemoveAllSinks();
#ifdef WRITE_PRODUCER_RECV_TO_FILE
    if (_fileWriter) {
        _serializer->RemoveTestSink();
    }
#endif
}

std::shared_ptr<TranslatorSource> TranslatorSource::Create(const RtpCodecMimeType& mime,
                                                           uint32_t clockRate,
                                                           uint32_t originalSsrc,
                                                           uint8_t payloadType,
                                                           TranslatorEndPointFactory* endPointsFactory,
                                                           RtpPacketsPlayer* rtpPacketsPlayer,
                                                           RtpPacketsCollector* output,
                                                           const std::string& producerId,
                                                           const std::weak_ptr<BufferAllocator>& allocator)
{
    std::shared_ptr<TranslatorSource> source;
    if (endPointsFactory && rtpPacketsPlayer && output) {
        if (auto depacketizer = RtpDepacketizer::Create(mime, clockRate, allocator)) {
            auto serializer = std::make_unique<WebMSerializer>(mime, allocator);
            source.reset(new TranslatorSource(clockRate, originalSsrc, payloadType,
                                             std::move(serializer), std::move(depacketizer),
                                              endPointsFactory, rtpPacketsPlayer,
                                              output, producerId));
        }
        else {
            MS_ERROR_STD("failed to create depacketizer for %s",
                         GetStreamInfoString(mime, originalSsrc).c_str());
        }
    }
    return source;
}

const RtpCodecMimeType& TranslatorSource::GetMime() const
{
    return _depacketizer->GetMimeType();
}

uint32_t TranslatorSource::GetClockRate() const
{
    return _depacketizer->GetClockRate();
}

bool TranslatorSource::AddOriginalRtpPacketForTranslation(RtpPacket* packet)
{
    bool handled = false;
    if (packet && (_serializer->HasSinks() || _serializer->HasTestSink())) {
        if (const auto frame = _depacketizer->AddPacket(packet)) {
            handled = _serializer->Push(frame);
            if (handled) {
                ++_addedPacketsCount;
            }
        }
    }
    return handled;
}

void TranslatorSource::SetInputLanguage(const std::string& languageId)
{
    LOCK_WRITE_PROTECTED_OBJ(_consumersManager);
    _consumersManager->SetInputLanguage(languageId);
}

void TranslatorSource::AddConsumer(Consumer* consumer)
{
    if (consumer) {
        LOCK_WRITE_PROTECTED_OBJ(_consumersManager);
        if (0U == _consumersManager->GetSize()) { // 1st
            _rtpPacketsPlayer->AddStream(GetOriginalSsrc(), GetClockRate(),
                                         GetPayloadType(), GetMime(), this);
        }
        _consumersManager->AddConsumer(consumer);
    }
}

void TranslatorSource::UpdateConsumer(Consumer* consumer)
{
    if (consumer) {
        LOCK_WRITE_PROTECTED_OBJ(_consumersManager);
        _consumersManager->UpdateConsumer(consumer);
    }
}

void TranslatorSource::RemoveConsumer(Consumer* consumer)
{
    if (consumer) {
        LOCK_WRITE_PROTECTED_OBJ(_consumersManager);
        _consumersManager->RemoveConsumer(consumer);
        if (0U == _consumersManager->GetSize()) { // last
            _rtpPacketsPlayer->RemoveStream(GetOriginalSsrc());
        }
    }
}

bool TranslatorSource::IsConnected(Consumer* consumer) const
{
    if (consumer) {
        LOCK_READ_PROTECTED_OBJ(_consumersManager);
        if (const auto info = _consumersManager->GetConsumer(consumer)) {
            return info->IsConnected();
        }
    }
    return false;
}

void TranslatorSource::SaveProducerRtpPacketInfo(Consumer* consumer,
                                                         const RtpPacket* packet)
{
    if (consumer && packet) {
        LOCK_READ_PROTECTED_OBJ(_consumersManager);
        if (const auto info = _consumersManager->GetConsumer(consumer)) {
            info->SaveProducerRtpPacketInfo(packet);
        }
    }
}

#ifdef WRITE_PRODUCER_RECV_TO_FILE
std::unique_ptr<FileWriter> TranslatorSource::CreateFileWriter(uint32_t ssrc,
                                                               const std::string& producerId,
                                                               const std::string_view& fileExtension)
{
    if (!fileExtension.empty()) {
        const auto depacketizerPath = std::getenv("MEDIASOUP_DEPACKETIZER_PATH");
        if (depacketizerPath && std::strlen(depacketizerPath)) {
            std::string fileName = producerId + "_" + std::to_string(ssrc) + "." + std::string(fileExtension);
            fileName = std::string(depacketizerPath) + "/" + fileName;
            auto fileWriter = std::make_unique<FileWriter>();
            if (!fileWriter->Open(fileName)) {
                fileWriter.reset();
                MS_WARN_DEV_STD("Failed to open producer file output %s", fileName.c_str());
            }
            return fileWriter;
        }
    }
    return nullptr;
}

std::unique_ptr<FileWriter> TranslatorSource::CreateFileWriter(uint32_t ssrc,
                                                               const std::string& producerId,
                                                               const MediaFrameSerializer* serializer)
{
    if (serializer) {
        return CreateFileWriter(ssrc, producerId, serializer->GetFileExtension());
    }
    return nullptr;
}
#endif

void TranslatorSource::OnPlayStarted(uint32_t ssrc, uint64_t mediaId,
                                     uint64_t mediaSourceId)
{
    RtpPacketsPlayerCallback::OnPlayStarted(ssrc, mediaId, mediaSourceId);
    LOCK_WRITE_PROTECTED_OBJ(_consumersManager);
    _consumersManager->BeginPacketsSending(mediaId, mediaSourceId);
}

void TranslatorSource::OnPlay(const Timestamp& timestampOffset, RtpPacket* packet,
                              uint64_t mediaId, uint64_t mediaSourceId)
{
    if (packet) {
        const auto rtpOffset = timestampOffset.GetRtpTime();
        LOCK_WRITE_PROTECTED_OBJ(_consumersManager);
        _consumersManager->SendPacket(rtpOffset, mediaId, mediaSourceId, packet, _output);
    }
}

void TranslatorSource::OnPlayFinished(uint32_t ssrc, uint64_t mediaId,
                                      uint64_t mediaSourceId)
{
    RtpPacketsPlayerCallback::OnPlayFinished(ssrc, mediaId, mediaSourceId);
    LOCK_WRITE_PROTECTED_OBJ(_consumersManager);
    _consumersManager->EndPacketsSending(mediaId, mediaSourceId);
}

void TranslatorSource::NotifyThatConnectionEstablished(const MediaObject& endPoint,
                                                       bool connected)
{
    TranslatorEndPointSink::NotifyThatConnectionEstablished(endPoint, connected);
    if (!connected) {
        _rtpPacketsPlayer->Stop(GetOriginalSsrc(), endPoint.GetId());
    }
}

void TranslatorSource::WriteMediaPayload(const MediaObject& endPoint,
                                         const std::shared_ptr<Buffer>& buffer)
{
    if (buffer) {
        _rtpPacketsPlayer->Play(GetOriginalSsrc(), endPoint.GetId(), buffer);
    }
}

} // namespace RTC
