#define MS_CLASS "RTC::RtpPacketsMediaFragmentPlayer"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsMediaFragmentPlayer.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerCallback.hpp"
#include "RTC/MediaTranslate/MediaFrameDeserializer.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "RTC/MediaTranslate/RtpPacketizerOpus.hpp"
#include "RTC/RtpPacket.hpp"
#include "Logger.hpp"

namespace RTC
{

RtpPacketsMediaFragmentPlayer::RtpPacketsMediaFragmentPlayer(const std::weak_ptr<RtpPacketsPlayerCallback>& playerCallbackRef,
                                                             std::unique_ptr<MediaFrameDeserializer> deserializer,
                                                             uint32_t ssrc, uint32_t clockRate,
                                                             uint8_t payloadType, uint64_t mediaId,
                                                             uint64_t mediaSourceId)
    : _playerCallbackRef(playerCallbackRef)
    , _deserializer(std::move(deserializer))
    , _ssrc(ssrc)
    , _clockRate(clockRate)
    , _payloadType(payloadType)
    , _mediaId(mediaId)
    , _mediaSourceId(mediaSourceId)
{
    MS_ASSERT(_deserializer, "deserializer must not be null");
}

RtpPacketsMediaFragmentPlayer::~RtpPacketsMediaFragmentPlayer()
{
}

bool RtpPacketsMediaFragmentPlayer::Parse(const RtpCodecMimeType& mime,
                                          const std::shared_ptr<MemoryBuffer>& buffer)
{
    bool ok = false;
    if (buffer && !_playerCallbackRef.expired()) {
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
                            _deserializer->SetClockRate(trackIndex, _clockRate);
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

void RtpPacketsMediaFragmentPlayer::OnEvent()
{
    if (!_packetizers.empty()) {
        if (const auto playerCallback = _playerCallbackRef.lock()) {
            MediaFrameDeserializeResult result = MediaFrameDeserializeResult::Success;
            bool started = false;
            for (auto it = _packetizers.begin(); it != _packetizers.end(); ++it) {
                for (const auto& frame : _deserializer->ReadNextFrames(it->first, &result)) {
                    if (!started) {
                        started = true;
                        playerCallback->OnPlayStarted(_ssrc, _mediaId, _mediaSourceId);
                    }
                    ConvertToRtpAndSend(it->first, playerCallback, frame);
                }
                if (!MaybeOk(result)) {
                    MS_ERROR_STD("read of deserialized frames was failed: %s", ToString(result));
                    break;
                }
            }
            if (started) {
                playerCallback->OnPlayFinished(_ssrc, _mediaId, _mediaSourceId);
            }
        }
    }
}

void RtpPacketsMediaFragmentPlayer::ConvertToRtpAndSend(size_t trackIndex,
                                                        const std::shared_ptr<RtpPacketsPlayerCallback>& callback,
                                                        const std::shared_ptr<const RTC::MediaFrame>& frame)
{
    if (frame && callback) {
        if (const auto packet = CreatePacket(trackIndex, frame)) {
            callback->OnPlay(frame->GetTimestamp(), packet, _mediaId, _mediaSourceId);
        }
    }
}

RtpPacket* RtpPacketsMediaFragmentPlayer::CreatePacket(size_t trackIndex,
                                                       const std::shared_ptr<const MediaFrame>& frame) const
{
    if (frame) {
        const auto it = _packetizers.find(trackIndex);
        if (it != _packetizers.end()) {
            if (const auto packet = it->second->AddFrame(frame)) {
                packet->SetSsrc(_ssrc);
                packet->SetPayloadType(_payloadType);
                return packet;
            }
        }
    }
    return nullptr;
}

} // namespace RTC
