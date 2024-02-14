#define MS_CLASS "RTC::WebMDeserializer"
#include "RTC/MediaTranslate/WebM/WebMDeserializer.hpp"
#include "RTC/MediaTranslate/WebM/WebMCodecs.hpp"
#include "RTC/MediaTranslate/WebM/MkvBufferedReader.hpp"
#include "RTC/MediaTranslate/AudioFrameConfig.hpp"
#include "RTC/MediaTranslate/VideoFrameConfig.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "Logger.hpp"

namespace {

template<typename T>
inline constexpr T ValueFromNano(unsigned long long nano) {
    return static_cast<T>(nano / 1000 / 1000 / 1000);
}

enum class StreamState {
    BeforeTheStart,
    InTheMiddle,
    End
};

}

namespace RTC
{

class WebMDeserializer::TrackInfo
{
public:
    TrackInfo(RtpCodecMimeType::Type type,
              RtpCodecMimeType::Subtype subType,
              std::shared_ptr<MediaFrameConfig> config,
              mkvparser::Segment* segment,
              long long trackNum);
    const RtpCodecMimeType& GetMime() const { return _mime; }
    const std::shared_ptr<MediaFrameConfig>& GetConfig() const { return _config; }
    StreamState GetState() const;
    void Reset();
    void SetClockRate(uint32_t clockRate);
    MediaFrameDeserializeResult ReadFrames(std::vector<std::shared_ptr<const MediaFrame>>& output);
    static std::unique_ptr<TrackInfo> Create(const mkvparser::Tracks* tracks,
                                             mkvparser::Segment* segment,
                                             unsigned long trackIndex);
private:
    MkvReadResult GetNextBlock(const mkvparser::Block*& block);
    std::optional<webrtc::Timestamp> GetBlockTime(const mkvparser::Block* block) const;
    uint32_t GetClockRate() const { return _clockRate; }
    void SetNextBlockIterationResult(long result, const char* operationName);
private:
    const RtpCodecMimeType _mime;
    const std::shared_ptr<MediaFrameConfig> _config;
    mkvparser::Segment* const _segment;
    const long long _trackNum;
    const mkvparser::Cluster* _cluster = nullptr;
    const mkvparser::BlockEntry* _blockEntry = nullptr;
    long _blockEntryIndex = 0L;
    long _nextBlockIterationResult = 0L;
    uint32_t _clockRate = 0U;
};

WebMDeserializer::WebMDeserializer()
    : _reader(std::make_unique<MkvBufferedReader>())
{
}

WebMDeserializer::~WebMDeserializer()
{
}

MediaFrameDeserializeResult WebMDeserializer::AddBuffer(const std::shared_ptr<MemoryBuffer>& buffer)
{
    auto result = _reader->AddBuffer(buffer);
    if (MaybeOk(result) && _tracks.empty()) {
        if (const auto tracks = _reader->GetTracks()) {
            if (const auto tracksCount = tracks->GetTracksCount()) {
                for (unsigned long i = 0UL; i < tracksCount; ++i) {
                    if (auto trackInfo = TrackInfo::Create(tracks, _reader->GetSegment(), i)) {
                        _tracks[i] = std::move(trackInfo);
                    }
                }
                result = MkvReadResult::Success;
            }
        }
    }
    return FromMkvReadResult(result);
}

std::vector<std::shared_ptr<const MediaFrame>> WebMDeserializer::ReadNextFrames(size_t trackIndex,
                                                                                MediaFrameDeserializeResult* outResult)
{
    std::vector<std::shared_ptr<const MediaFrame>> output;
    if (_reader->GetSegment()) {
        const auto it = _tracks.find(trackIndex);
        if (it != _tracks.end()) {
            const auto& track = it->second;
            MediaFrameDeserializeResult result = MediaFrameDeserializeResult::NeedMoreData;
            if (StreamState::End != track->GetState()) {
                result = track->ReadFrames(output);
            }
            // lookup the next blocks if any
            if (MediaFrameDeserializeResult::NeedMoreData == result) {
                if (!MkvBufferedReader::IsLive()) { // check if not live stream
                    track->Reset();
                }
                if (StreamState::End == track->GetState()) {
                    // reading of all frames was completed
                    result = MediaFrameDeserializeResult::Success;
                }
            }
            if (outResult) {
                *outResult = result;
            }
        }
        else if (outResult) {
            *outResult = MediaFrameDeserializeResult::InvalidArg;
        }
    }
    else if (outResult) {
        *outResult = MediaFrameDeserializeResult::ParseError;
    }
    return output;
}

size_t WebMDeserializer::GetTracksCount() const
{
    return _tracks.size();
}

std::optional<RtpCodecMimeType> WebMDeserializer::GetTrackMimeType(size_t trackIndex) const
{
    const auto it = _tracks.find(trackIndex);
    if (it != _tracks.end()) {
        return it->second->GetMime();
    }
    return std::nullopt;
}

void WebMDeserializer::SetClockRate(size_t trackIndex, uint32_t clockRate)
{
    MediaFrameDeserializer::SetClockRate(trackIndex, clockRate);
    const auto it = _tracks.find(trackIndex);
    if (it != _tracks.end()) {
        it->second->SetClockRate(clockRate);
    }
}

MediaFrameDeserializeResult WebMDeserializer::FromMkvReadResult(MkvReadResult result)
{
    switch (result) {
        case MkvReadResult::InvalidInputArg:
            return MediaFrameDeserializeResult::InvalidArg;
        case MkvReadResult::OutOfMemory:
            return MediaFrameDeserializeResult::OutOfMemory;
        case MkvReadResult::BufferNotFull:
        case MkvReadResult::NoMoreClusters:
            return MediaFrameDeserializeResult::NeedMoreData;
        case MkvReadResult::Success:
            return MediaFrameDeserializeResult::Success;
        default:
            break;
    }
    return MediaFrameDeserializeResult::ParseError;
}

WebMDeserializer::TrackInfo::TrackInfo(RtpCodecMimeType::Type type,
                                       RtpCodecMimeType::Subtype subType,
                                       std::shared_ptr<MediaFrameConfig> config,
                                       mkvparser::Segment* segment,
                                       long long trackNum)
    : _mime(type, subType)
    , _config(std::move(config))
    , _segment(segment)
    , _trackNum(trackNum)
{
}

StreamState WebMDeserializer::TrackInfo::GetState() const
{
    if (_blockEntry) {
        return _blockEntry->EOS() ? StreamState::End : StreamState::InTheMiddle;
    }
    if (_cluster && _cluster->GetEntryCount() == _blockEntryIndex) {
        return StreamState::End;
    }
    return StreamState::BeforeTheStart;
}

void WebMDeserializer::TrackInfo::Reset()
{
    _cluster = _segment->GetFirst();
    _blockEntry = nullptr;
    _blockEntryIndex = _blockEntryIndex = 0L;
}

void WebMDeserializer::TrackInfo::SetClockRate(uint32_t clockRate)
{
    MS_ASSERT(clockRate > 0U, "clock rate must be greater than zero");
    _clockRate = clockRate;
}

MediaFrameDeserializeResult WebMDeserializer::TrackInfo::
    ReadFrames(std::vector<std::shared_ptr<const MediaFrame>>& output)
{
    MkvReadResult mkvResult = MkvReadResult::Success;
    output.clear();
    while (IsOk(mkvResult)) {
        const mkvparser::Block* block = nullptr;
        mkvResult = GetNextBlock(block);
        if (MaybeOk(mkvResult) && block) {
            auto ts = GetBlockTime(block);
            output.reserve(output.size() + static_cast<size_t>(block->GetFrameCount()));
            for (int i = 0; i < block->GetFrameCount(); ++i) {
                const auto& frame = block->GetFrame(i);
                std::vector<uint8_t> buffer(frame.len);
                mkvResult = ToMkvReadResult(frame.Read(_segment->m_pReader, buffer.data()));
                if (MaybeOk(mkvResult)) {
                    auto mediaFrame = std::make_shared<MediaFrame>(GetMime(), GetClockRate());
                    if (mediaFrame->AddPayload(std::move(buffer))) {
                        mediaFrame->SetKeyFrame(block->IsKey());
                        mediaFrame->SetMediaConfig(GetConfig());
                        if (ts.has_value()) {
                            mediaFrame->SetTimestamp(ts.value());
                        }
                        output.push_back(std::move(mediaFrame));
                    }
                }
                else {
                    break;
                }
            }
        }
        else {
            break;
        }
    }
    return FromMkvReadResult(mkvResult);
}

std::unique_ptr<WebMDeserializer::TrackInfo> WebMDeserializer::TrackInfo::
    Create(const mkvparser::Tracks* tracks, mkvparser::Segment* segment, unsigned long trackIndex)
{
    if (tracks && segment) {
        if (const auto track = tracks->GetTrackByIndex(trackIndex)) {
            std::optional<RtpCodecMimeType::Type> type;
            switch (track->GetType()) {
                case mkvparser::Track::Type::kVideo:
                    type = RtpCodecMimeType::Type::VIDEO;
                    break;
                case mkvparser::Track::Type::kAudio:
                    type = RtpCodecMimeType::Type::AUDIO;
                    break;
                default:
                    MS_WARN_TAG(rtp, "Unsupported WebM media track type: %ld", track->GetType());
                    break;
            }
            if (type.has_value()) {
                std::optional<RtpCodecMimeType::Subtype> subtype;
                for (auto it = RtpCodecMimeType::subtype2String.begin();
                     it != RtpCodecMimeType::subtype2String.end(); ++it) {
                    const auto codecId = WebMCodecs::GetCodecId(it->first);
                    if (codecId && 0 == std::strcmp(track->GetCodecId(), codecId)) {
                        subtype = it->first;
                        break;
                    }
                }
                if (subtype.has_value()) {
                    if (subtype.has_value()) {
                        std::shared_ptr<RTC::MediaFrameConfig> config;
                        if (RtpCodecMimeType::Type::VIDEO == type.value()) {
                            auto videoConfig = std::make_shared<VideoFrameConfig>();
                            auto videoTrack = static_cast<const mkvparser::VideoTrack*>(track);
                            videoConfig->SetWidth(videoTrack->GetWidth());
                            videoConfig->SetHeight(videoTrack->GetHeight());
                            videoConfig->SetFrameRate(videoTrack->GetFrameRate());
                            config = std::move(videoConfig);
                        }
                        else {
                            auto audioConfig = std::make_shared<AudioFrameConfig>();
                            auto audioTrack = static_cast<const mkvparser::AudioTrack*>(track);
                            audioConfig->SetChannelCount(audioTrack->GetChannels());
                            audioConfig->SetBitsPerSample(audioTrack->GetBitDepth());
                            config = std::move(audioConfig);
                        }
                        size_t len = 0UL;
                        if (const auto data = track->GetCodecPrivate(len)) {
                            config->SetCodecSpecificData(data, len);
                        }
                        auto trackInfo = std::make_unique<TrackInfo>(type.value(),
                                                                     subtype.value(),
                                                                     std::move(config),
                                                                     segment,
                                                                     track->GetNumber());
                        if (RtpCodecMimeType::Type::AUDIO == type.value()) {
                            trackInfo->Reset();
                            trackInfo->SetClockRate(static_cast<const mkvparser::AudioTrack*>(track)->GetSamplingRate());
                        }
                        return trackInfo;
                    }
                }
                else {
                    MS_WARN_TAG(rtp, "Unsupported WebM codec type: %s", track->GetCodecId());
                }
            }
        }
    }
    return nullptr;
}

MkvReadResult WebMDeserializer::TrackInfo::GetNextBlock(const mkvparser::Block*& block)
{
    if (_cluster) {
        for (;;) {
            long res = _cluster->GetEntry(_blockEntryIndex, _blockEntry);
            MS_DEBUG_DEV("Cluster::GetEntry returned: %s", MkvReadResultToString(res));
            if (1L == res) {
                res = 0L;
                goto block_exit;
            }
            long long pos;
            long len;
            if (res < 0L) {
                // Need to parse this cluster some more
                if (mkvparser::E_BUFFER_NOT_FULL == res) {
                    res = _cluster->Parse(pos, len);
                    MS_DEBUG_DEV("Cluster::Parse returned: %s", MkvReadResultToString(res));
                }
                SetNextBlockIterationResult(res, "Cluster::Parse");
                if (res < 0L) { // I/O error
                    return ToMkvReadResult(res);
                }
                continue;
            } else if (res == 0L) {
                // We're done with this cluster
                const mkvparser::Cluster* nextCluster = nullptr;
                res = _segment->ParseNext(_cluster, nextCluster, pos, len);
                MS_DEBUG_DEV("Segment::ParseNext returned: %s", MkvReadResultToString(res));
                SetNextBlockIterationResult(res, "Segment::ParseNext");
                if (res != 0L) {
                    return ToMkvReadResult(res);
                }
                MS_ASSERT(nextCluster, "next WebM cluster must not be null");
                MS_ASSERT(!nextCluster->EOS(), "next WebM cluster is end of the stream");
                _cluster = nextCluster;
                res = _cluster->Parse(pos, len);
                MS_DEBUG_DEV("Cluster::Parse (2) returned: %s", MkvReadResultToString(res));
                SetNextBlockIterationResult(res, "Cluster::Parse (2)");
                if (res < 0L) {
                    return ToMkvReadResult(res);
                }
                _blockEntryIndex = 0L;
                continue;
            }
block_exit:
            MS_ASSERT(_blockEntry, "block entry must not be null");
            MS_ASSERT(_blockEntry->GetBlock(), "block must not be null");
            ++_blockEntryIndex;
            if (_blockEntry->GetBlock()->GetTrackNumber() == _trackNum) {
                block = _blockEntry->GetBlock();
                break;
            }
        }
    }
    return ToMkvReadResult(_nextBlockIterationResult);
}

std::optional<webrtc::Timestamp> WebMDeserializer::TrackInfo::GetBlockTime(const mkvparser::Block* block) const
{
    if (block && _blockEntry) {
        return webrtc::Timestamp::us(block->GetTime(_blockEntry->GetCluster()) / 1000);
    }
    return std::nullopt;
}

void WebMDeserializer::TrackInfo::SetNextBlockIterationResult(long result, const char* operationName)
{
    if (_nextBlockIterationResult != result) {
        if (result < 0) {
            const auto mkvResult = ToMkvReadResult(result);
            if (MkvReadResult::BufferNotFull != mkvResult) {
                MS_ERROR_STD("%s error: %s", operationName, MkvReadResultToString(mkvResult));
                _cluster = nullptr;
            }
        }
        _nextBlockIterationResult = result;
    }
}

} // namespace RTC
