#define MS_CLASS "RTC::RtpPacketsPlayerMediaFragment"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerMediaFragment.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerCallback.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimer.hpp"
#include "RTC/MediaTranslate/MediaFrameDeserializer.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "RTC/MediaTranslate/RtpPacketizerOpus.hpp"
#include "RTC/RtpDictionaries.hpp"
#include "RTC/RtpPacket.hpp"
#include "Logger.hpp"

namespace RTC
{

RtpPacketsPlayerMediaFragment::RtpPacketsPlayerMediaFragment(std::unique_ptr<MediaFrameDeserializer> deserializer)
    : _deserializer(std::move(deserializer))
{
}

RtpPacketsPlayerMediaFragment::~RtpPacketsPlayerMediaFragment()
{
}

bool RtpPacketsPlayerMediaFragment::Parse(uint32_t clockRate, const RtpCodecMimeType& mime,
                                          const std::shared_ptr<MemoryBuffer>& buffer)
{
    bool ok = false;
    if (_deserializer) {
        const auto result = _deserializer->AddBuffer(buffer);
        if (MaybeOk(result)) {
            if (const auto tracksCount = _deserializer->GetTracksCount()) {
                size_t acceptedTracksCount = 0UL;
                for (size_t trackIndex = 0UL; trackIndex < tracksCount; ++trackIndex) {
                    const auto trackMime = _deserializer->GetTrackMimeType(trackIndex);
                    if (trackMime.has_value() && trackMime.value() == mime) {
                        std::unique_ptr<RtpPacketizer> packetizer;
                        switch (mime.GetSubtype()) {
                            case RtpCodecMimeType::Subtype::OPUS:
                                packetizer = std::make_unique<RtpPacketizerOpus>();
                                break;
                            default:
                                break;
                        }
                        if (packetizer) {
                            _deserializer->SetClockRate(trackIndex, clockRate);
                            _packetizers[trackIndex] = std::move(packetizer);
                            ++acceptedTracksCount;
                        }
                        else {
                            MS_ERROR_STD("packetizer for [%s] not yet implemented", mime.ToString().c_str());
                        }
                    }
                }
                ok = acceptedTracksCount > 0UL;
            }
            else {
                MS_ERROR_STD("deserialized media buffer has no media tracks");
            }
        }
        else {
            MS_ERROR_STD("media buffer deserialization was failed: %s", ToString(result));
        }
    }
    return ok;
}

void RtpPacketsPlayerMediaFragment::Play(uint32_t ssrc, uint8_t payloadType,
                                         uint64_t mediaId, uint64_t mediaSourceId,
                                         RtpPacketsPlayerCallback* callback)
{
    if (callback && !_packetizers.empty()) {
        MediaFrameDeserializeResult result = MediaFrameDeserializeResult::Success;
        bool started = false;
        for (auto it = _packetizers.begin(); it != _packetizers.end(); ++it) {
            for (const auto& frame : _deserializer->ReadNextFrames(it->first, &result)) {
                if (!started) {
                    started = true;
                    callback->OnPlayStarted(ssrc, mediaId, mediaSourceId);
                }
                if (const auto packet = CreatePacket(it->first, ssrc, payloadType, frame)) {
                    callback->OnPlay(frame->GetTimestamp(), packet, mediaId, mediaSourceId);
                }
            }
            if (!MaybeOk(result)) {
                MS_ERROR_STD("read of deserialized frames was failed: %s", ToString(result));
                break;
            }
        }
        if (started) {
            callback->OnPlayFinished(ssrc, mediaId, mediaSourceId);
        }
    }
}

RtpPacket* RtpPacketsPlayerMediaFragment::CreatePacket(size_t trackIndex,
                                                       uint32_t ssrc,
                                                       uint8_t payloadType,
                                                       const std::shared_ptr<const MediaFrame>& frame) const
{
    if (frame) {
        const auto it = _packetizers.find(trackIndex);
        if (it != _packetizers.end()) {
            if (const auto packet = it->second->AddFrame(frame)) {
                packet->SetSsrc(ssrc);
                packet->SetPayloadType(payloadType);
                return packet;
            }
        }
    }
    return nullptr;
}

} // namespace RTC
