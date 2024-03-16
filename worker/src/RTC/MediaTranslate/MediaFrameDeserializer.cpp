#define MS_CLASS "RTC::MediaFrameDeserializer"
#include "RTC/MediaTranslate/MediaFrameDeserializer.hpp"
#include "RTC/MediaTranslate/MediaFrameDeserializedTrack.hpp"
#include "RTC/MediaTranslate/RtpPacketizerOpus.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "RTC/RtpPacket.hpp"
#include "Logger.hpp"

namespace RTC
{

MediaFrameDeserializer::MediaFrameDeserializer(const std::shared_ptr<BufferAllocator>& allocator)
    : BufferAllocations<void>(allocator)
{
}

MediaFrameDeserializer::~MediaFrameDeserializer()
{
}

MediaFrameDeserializeResult MediaFrameDeserializer::Add(const std::shared_ptr<Buffer>& buffer)
{
    const auto result = AddBuffer(buffer);
    if (MaybeOk(result) && _tracks.empty()) {
        ParseTracksInfo();
    }
    return result;
}

void MediaFrameDeserializer::Clear()
{
    _tracks.clear();
}

std::optional<RtpTranslatedPacket> MediaFrameDeserializer::NextPacket(size_t trackIndex,
                                                                      bool skipPayload)
{
    if (trackIndex < _tracks.size()) {
        auto& ti = _tracks.at(trackIndex);
        const auto payloadOffset = ti.first->GetPayloadOffset();
        const auto payloadExtraSize = ti.first->GetPayloadExtraSize();
        if (auto frame = ti.second->NextFrame(payloadOffset, skipPayload, payloadExtraSize)) {
            const auto payloadSize = ti.second->GetLastPayloadSize();
            return ti.first->Add(payloadOffset, payloadSize, std::move(frame.value()));
        }
    }
    return std::nullopt;
}

std::optional<RtpCodecMimeType> MediaFrameDeserializer::GetTrackType(size_t trackIndex) const
{
    if (trackIndex < _tracks.size()) {
        return _tracks.at(trackIndex).first->GetType();
    }
    return std::nullopt;
}

MediaFrameDeserializeResult MediaFrameDeserializer::GetTrackLastResult(size_t trackIndex) const
{
    if (trackIndex < _tracks.size()) {
        return _tracks.at(trackIndex).second->GetLastResult();
    }
    return MediaFrameDeserializeResult::InvalidArg;
}

void MediaFrameDeserializer::SetClockRate(size_t trackIndex, uint32_t clockRate)
{
    if (trackIndex < _tracks.size()) {
        _tracks[trackIndex].second->SetClockRate(clockRate);
    }
}

void MediaFrameDeserializer::AddTrack(const RtpCodecMimeType& type,
                                      std::unique_ptr<MediaFrameDeserializedTrack> track)
{
    if (track) {
        std::unique_ptr<RtpPacketizer> packetizer;
        switch (type.GetSubtype()) {
            case RtpCodecMimeType::Subtype::OPUS:
                packetizer = std::make_unique<RtpPacketizerOpus>(GetAllocator());
                if (!track->GetClockRate()) {
                    track->SetClockRate(48000U);
                }
                break;
            default:
                MS_ERROR_STD("packetizer for [%s] not yet implemented", type.ToString().c_str());
                break;
        }
        if (packetizer) {
            auto trackInfo = std::make_pair(std::move(packetizer), std::move(track));
            _tracks.push_back(std::move(trackInfo));
        }
    }
}

void MediaFrameDeserializedTrack::SetClockRate(uint32_t clockRate)
{
    MS_ASSERT(clockRate > 0U, "clock rate must be greater than zero");
    _clockRate = clockRate;
}

} // namespace RTC
