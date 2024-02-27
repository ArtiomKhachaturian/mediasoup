#define MS_CLASS "RTC::WebMDeserializer"
#include "RTC/MediaTranslate/WebM/WebMDeserializer.hpp"
#include "RTC/MediaTranslate/WebM/WebMCodecs.hpp"
#include "RTC/MediaTranslate/WebM/MkvBufferedReader.hpp"
#include "RTC/MediaTranslate/MediaFrameDeserializedTrack.hpp"
#include "RTC/MediaTranslate/AudioFrameConfig.hpp"
#include "RTC/MediaTranslate/VideoFrameConfig.hpp"
#include "RTC/MediaTranslate/MediaFrame.hpp"
#include "RTC/Buffers/BufferAllocator.hpp"
#include "Logger.hpp"

namespace {

enum class StreamState {
    BeforeTheStart,
    InTheMiddle,
    End
};

}

namespace RTC
{

class WebMDeserializer::TrackInfo : public MediaFrameDeserializedTrack
{
public:
    static std::unique_ptr<TrackInfo> Create(const mkvparser::Tracks* tracks,
                                             mkvparser::Segment* segment,
                                             unsigned long trackIndex,
                                             const std::weak_ptr<BufferAllocator>& allocator);
    const RtpCodecMimeType& GetMime() const { return _mime; }
    void Reset();
    StreamState GetState() const;
    // impl. of MediaFrameDeserializedTrack
    std::shared_ptr<MediaFrame> NextFrame(size_t payloadOffset,
                                          const std::weak_ptr<BufferAllocator>& allocator) final;
private:
    TrackInfo(RtpCodecMimeType::Type type,
              RtpCodecMimeType::Subtype subType,
              std::shared_ptr<MediaFrameConfig> config,
              mkvparser::Segment* segment,
              long long trackNum);
    MkvReadResult AdvanceToNextBlockEntry();
    std::optional<webrtc::Timestamp> GetBlockTime(const mkvparser::Block* block) const;
    void SetNextBlockIterationResult(long result, const char* operationName);
private:
    const RtpCodecMimeType _mime;
    const std::shared_ptr<MediaFrameConfig> _config;
    mkvparser::Segment* const _segment;
    const long long _trackNum;
    const mkvparser::Cluster* _cluster = nullptr;
    const mkvparser::BlockEntry* _currentBlockEntry = nullptr;
    int _currentBlockFrameIndex = 0;
    long _blockEntryIndex = 0L;
    long _nextBlockIterationResult = 0L;
};

WebMDeserializer::WebMDeserializer(const std::weak_ptr<BufferAllocator>& allocator)
    : MediaFrameDeserializer(allocator)
    , _reader(std::make_unique<MkvBufferedReader>(allocator))
{
}

WebMDeserializer::~WebMDeserializer()
{
}

void WebMDeserializer::Clear()
{
    MediaFrameDeserializer::Clear();
    _reader->ClearBuffers();
}

MediaFrameDeserializeResult WebMDeserializer::AddBuffer(const std::shared_ptr<Buffer>& buffer)
{
    return FromMkvReadResult(_reader->AddBuffer(buffer));
}

void WebMDeserializer::ParseTracksInfo()
{
    if (const auto tracks = _reader->GetTracks()) {
        if (const auto tracksCount = tracks->GetTracksCount()) {
            for (unsigned long i = 0UL; i < tracksCount; ++i) {
                if (auto trackInfo = TrackInfo::Create(tracks, _reader->GetSegment(),
                                                       i, GetAllocator())) {
                    const auto type = trackInfo->GetMime();
                    AddTrack(type, std::move(trackInfo));
                }
            }
        }
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
    if (_currentBlockEntry) {
        return _currentBlockEntry->EOS() ? StreamState::End : StreamState::InTheMiddle;
    }
    if (_cluster && _cluster->GetEntryCount() == _blockEntryIndex) {
        return StreamState::End;
    }
    return StreamState::BeforeTheStart;
}

void WebMDeserializer::TrackInfo::Reset()
{
    _cluster = _segment->GetFirst();
    _currentBlockEntry = nullptr;
    _blockEntryIndex = 0L;
    _currentBlockFrameIndex = 0;
}

std::shared_ptr<MediaFrame> WebMDeserializer::TrackInfo::NextFrame(size_t payloadOffset,
                                                                   const std::weak_ptr<BufferAllocator>& allocator)
{
    std::shared_ptr<MediaFrame> mediaFrame;
    MkvReadResult mkvResult = MkvReadResult::Success;
    if (_currentBlockEntry && _currentBlockEntry->EOS()) {
        mkvResult = MkvReadResult::NoMoreClusters;
    }
    else {
        if (!_currentBlockEntry || _currentBlockFrameIndex == _currentBlockEntry->GetBlock()->GetFrameCount()) {
            mkvResult = AdvanceToNextBlockEntry();
        }
        if (MaybeOk(mkvResult) && _currentBlockEntry) {
            const auto block = _currentBlockEntry->GetBlock();
            const auto& frame = block->GetFrame(_currentBlockFrameIndex++);
            const auto frameLen = static_cast<size_t>(frame.len);
            const auto buffer = RTC::AllocateBuffer(payloadOffset + frameLen, allocator);
            if (buffer) {
                mkvResult = ToMkvReadResult(frame.Read(_segment->m_pReader,
                                                       buffer->GetData() + payloadOffset));
                if (MaybeOk(mkvResult)) {
                    mediaFrame = std::make_shared<MediaFrame>(_mime, GetClockRate(), allocator);
                    mediaFrame->AddPayload(buffer);
                    mediaFrame->SetKeyFrame(block->IsKey());
                    mediaFrame->SetMediaConfig(_config);
                    const auto ts = GetBlockTime(block);
                    if (ts.has_value()) {
                        mediaFrame->SetTimestamp(ts.value());
                    }
                    SetLastPayloadSize(frameLen);
                }
            }
            else {
                MS_ERROR_STD("allocation of [%zu bytes] buffer failed", payloadOffset + frameLen);
            }
        }
    }
    auto result = FromMkvReadResult(mkvResult);
    if (!mediaFrame && MediaFrameDeserializeResult::NeedMoreData == result &&
        StreamState::End == GetState()) {
        // reading of all frames was completed
        result = MediaFrameDeserializeResult::Success;
    }
    SetLastResult(result);
    return mediaFrame;
}

std::unique_ptr<WebMDeserializer::TrackInfo> WebMDeserializer::TrackInfo::
    Create(const mkvparser::Tracks* tracks, mkvparser::Segment* segment,
           unsigned long trackIndex, const std::weak_ptr<BufferAllocator>& allocator)
{
    std::unique_ptr<TrackInfo> trackInfo;
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
                            config->SetCodecSpecificData(data, len, allocator);
                        }
                        trackInfo.reset(new TrackInfo(type.value(), subtype.value(),
                                                      std::move(config), segment,
                                                      track->GetNumber()));
                        trackInfo->Reset();
                        if (RtpCodecMimeType::Type::AUDIO == type.value()) {
                            trackInfo->SetClockRate(static_cast<const mkvparser::AudioTrack*>(track)->GetSamplingRate());
                        }
                    }
                }
                else {
                    MS_WARN_TAG(rtp, "Unsupported WebM codec type: %s", track->GetCodecId());
                }
            }
        }
    }
    return trackInfo;
}

MkvReadResult WebMDeserializer::TrackInfo::AdvanceToNextBlockEntry()
{
    if (_cluster) {
        for (;;) {
            long res = _cluster->GetEntry(_blockEntryIndex, _currentBlockEntry);
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
            MS_ASSERT(_currentBlockEntry, "block entry must not be null");
            MS_ASSERT(_currentBlockEntry->GetBlock(), "block must not be null");
            ++_blockEntryIndex;
            if (_currentBlockEntry->GetBlock()->GetTrackNumber() == _trackNum) {
                _currentBlockFrameIndex = 0;
                break;
            }
        }
    }
    return ToMkvReadResult(_nextBlockIterationResult);
}

std::optional<webrtc::Timestamp> WebMDeserializer::TrackInfo::GetBlockTime(const mkvparser::Block* block) const
{
    if (block && _currentBlockEntry) {
        return webrtc::Timestamp::us(block->GetTime(_currentBlockEntry->GetCluster()) / 1000);
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
