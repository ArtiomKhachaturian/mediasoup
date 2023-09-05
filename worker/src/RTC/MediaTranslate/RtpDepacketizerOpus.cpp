#define MS_CLASS "RTC::RtpDepacketizerOpus"
#include "RTC/MediaTranslate/RtpDepacketizerOpus.hpp"
#include "RTC/MediaTranslate/RtpMediaFrame.hpp"
#include "RTC/MediaTranslate/RtpMediaTimeStampProvider.hpp"
#include "RTC/Codecs/Opus.hpp"
#include "RTC/RtpPacket.hpp"
#include "Logger.hpp"
#include <absl/container/flat_hash_map.h>

namespace RTC
{

class RtpDepacketizerOpus::TimeStampProviderImpl : public RtpMediaTimeStampProvider
{
public:
    TimeStampProviderImpl(uint32_t sampleRate);
    // impl. of RtpMediaTimeStampProvider
    uint64_t GetTimeStampNano(const std::shared_ptr<const RtpMediaFrame>& frame) final;
private:
    const uint32_t _sampleRate;
    absl::flat_hash_map<uint32_t, uint64_t> _granules;
};

RtpDepacketizerOpus::RtpDepacketizerOpus(const RtpCodecMimeType& codecMimeType,
                                         uint32_t sampleRate)
    : RtpDepacketizer(codecMimeType, sampleRate)
    , _timeStampProvider(std::make_shared<TimeStampProviderImpl>(sampleRate))
{
}

std::shared_ptr<RtpMediaFrame> RtpDepacketizerOpus::AddPacket(const RtpPacket* packet)
{
    if (packet && packet->GetPayload()) {
        bool stereo = false;
        Codecs::Opus::FrameSize frameSize;
        Codecs::Opus::ParseTOC(packet->GetPayload()[0], nullptr, nullptr, &frameSize, &stereo);
        RtpAudioFrameConfig config;
        config._channelCount = stereo ? 2U : 1U;
        config._bitsPerSample = 16U;
        return RtpMediaFrame::CreateAudio(packet, GetCodecMimeType().GetSubtype(),
                                          _timeStampProvider, GetSampleRate(), config,
                                          static_cast<uint32_t>(frameSize));
    }
    return nullptr;
}

RtpDepacketizerOpus::TimeStampProviderImpl::TimeStampProviderImpl(uint32_t sampleRate)
    : _sampleRate(sampleRate)
{
}

uint64_t RtpDepacketizerOpus::TimeStampProviderImpl::GetTimeStampNano(const std::shared_ptr<const RtpMediaFrame>& frame)
{
    uint64_t ts = 0ULL;
    if (frame) {
        MS_ASSERT(_sampleRate == frame->GetSampleRate(), "sample rate mistmatch");
        MS_ASSERT(frame->GetDuration(), "invalid duration");
        const auto it = _granules.find(frame->GetSsrc());
        if (it != _granules.end()) {
            it->second += frame->GetDuration();
            ts = (it->second * 1000ULL * 1000ULL * 1000ULL) / _sampleRate;
        }
        else {
            _granules[frame->GetSsrc()] = 0ULL;
        }
    }
    return ts;
}

} // namespace RTC
