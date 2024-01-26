#define MS_CLASS "RTC::WebMDeserializer"
#include "RTC/MediaTranslate/WebM/WebMDeserializer.hpp"
#include "RTC/MediaTranslate/WebM/WebMSerializer.hpp"
#include "RTC/MediaTranslate/WebM/MkvReader.hpp"
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
    void SetInitialTimestamp(uint32_t timestamp);
    MediaFrameDeserializeResult ReadFrames(size_t payloadOffset,
                                           std::vector<std::shared_ptr<const MediaFrame>>& output);
    static std::unique_ptr<TrackInfo> Create(const mkvparser::Tracks* tracks,
                                             mkvparser::Segment* segment,
                                             unsigned long trackIndex);
private:
    MkvResult GetNextBlock(const mkvparser::Block*& block);
    std::optional<uint32_t> GetBlockTime(const mkvparser::Block* block) const;
    uint32_t GetClockRate() const { return _clockRate; }
    uint32_t GetInitialTimestamp() const { return _initialTimestamp; }
private:
    const RtpCodecMimeType _mime;
    const std::shared_ptr<MediaFrameConfig> _config;
    mkvparser::Segment* const _segment;
    const long long _trackNum;
    const mkvparser::Cluster* _cluster = nullptr;
    const mkvparser::BlockEntry* _blockEntry = nullptr;
    long _blockEntryIndex = 0L;
    long _lastBlockIterationResult = 0L;
    uint32_t _clockRate = 0U;
    uint32_t _initialTimestamp = 0U;
    uint32_t _latestTimestamp = 0U;
};

enum class WebMDeserializer::MkvResult {
    UnknownError =  mkvparser::E_PARSE_FAILED - 1L,
    ParseFailed = mkvparser::E_PARSE_FAILED,
    FileFormatInvalid = mkvparser::E_FILE_FORMAT_INVALID,
    BufferNotFull = mkvparser::E_BUFFER_NOT_FULL,
    Success = 0L,
    NoMoreClusters = 1L // EOF ?
};

WebMDeserializer::WebMDeserializer(std::unique_ptr<MkvReader> reader, bool loopback)
    : _reader(std::move(reader))
    , _loopback(loopback)
{
    MS_ASSERT(_reader, "MKV reader must not be null");
}

WebMDeserializer::~WebMDeserializer()
{
    delete _segment;
}

MediaFrameDeserializeResult WebMDeserializer::AddBuffer(const std::shared_ptr<const MemoryBuffer>& buffer)
{
    auto result = _reader->AddBuffer(buffer);
    if (MediaFrameDeserializeResult::Success == result) {
        result = ParseEBMLHeader();
        if (MediaFrameDeserializeResult::Success == result) {
            result = ParseSegment();
        }
    }
    return result;
}

std::vector<std::shared_ptr<const MediaFrame>> WebMDeserializer::ReadNextFrames(size_t trackIndex,
                                                                                size_t payloadOffset,
                                                                                MediaFrameDeserializeResult* outResult)
{
    std::vector<std::shared_ptr<const MediaFrame>> output;
    if (_segment) {
        const auto it = _tracks.find(trackIndex);
        if (it != _tracks.end()) {
            const auto& track = it->second;
            MediaFrameDeserializeResult result = MediaFrameDeserializeResult::NeedMoreData;
            if (StreamState::End != track->GetState()) {
                result = track->ReadFrames(payloadOffset, output);
            }
            // lookup the next blocks if any
            if (MediaFrameDeserializeResult::NeedMoreData == result && _loopback) {
                long long total = 0LL, available = 0LL;
                // check if not live stream
                if (0L == _reader->Length(&total, &available) && total > 0LL) {
                    track->Reset();
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

void WebMDeserializer::SetInitialTimestamp(size_t trackIndex, uint32_t timestamp)
{
    MediaFrameDeserializer::SetInitialTimestamp(trackIndex, timestamp);
    const auto it = _tracks.find(trackIndex);
    if (it != _tracks.end()) {
        it->second->SetInitialTimestamp(timestamp);
    }
}

MediaFrameDeserializeResult WebMDeserializer::ParseEBMLHeader()
{
    if (!_ebmlHeader) {
        auto ebmlHeader = std::make_unique<mkvparser::EBMLHeader>();
        long long pos = 0LL;
        const auto mkvResult = ToMkvResult(ebmlHeader->Parse(_reader.get(), pos));
        if (IsOk(mkvResult)) {
            _ebmlHeader = std::move(ebmlHeader);
        }
        return FromMkvResult(mkvResult);
    }
    return MediaFrameDeserializeResult::Success;
}

MediaFrameDeserializeResult WebMDeserializer::ParseSegment()
{
    if (!_segment) {
        mkvparser::Segment* segment = nullptr;
        long long pos = 0LL;
        auto mkvResult = ToMkvResult(mkvparser::Segment::CreateInstance(_reader.get(), pos, segment));
        if (IsOk(mkvResult)) {
            mkvResult = ToMkvResult(segment->Load());
            if (MaybeOk(mkvResult)) {
                if (const auto tracks = segment->GetTracks()) {
                    if (const auto tracksCount = tracks->GetTracksCount()) {
                        std::swap(_segment, segment);
                        for (unsigned long i = 0UL; i < tracksCount; ++i) {
                            if (auto trackInfo = TrackInfo::Create(tracks, _segment, i)) {
                                _tracks[i] = std::move(trackInfo);
                            }
                        }
                        mkvResult = MkvResult::Success;
                    }
                }
            }
        }
        delete segment;
        return FromMkvResult(mkvResult);
    }
    return MediaFrameDeserializeResult::Success;
}

WebMDeserializer::MkvResult WebMDeserializer::ToMkvResult(long ret)
{
    switch (ret) {
        case mkvparser::E_PARSE_FAILED:
            return MkvResult::ParseFailed;
        case mkvparser::E_FILE_FORMAT_INVALID:
            return MkvResult::FileFormatInvalid;
        case mkvparser::E_BUFFER_NOT_FULL:
            return MkvResult::BufferNotFull;
        case 1:
            return MkvResult::NoMoreClusters;
        default:
            if (ret >= 0) {
                return MkvResult::Success;
            }
            break;
    }
    return MkvResult::UnknownError;
}

MediaFrameDeserializeResult WebMDeserializer::FromMkvResult(MkvResult result)
{
    switch (result) {
        case MkvResult::BufferNotFull:
        case MkvResult::NoMoreClusters:
            return MediaFrameDeserializeResult::NeedMoreData;
        case MkvResult::Success:
            return MediaFrameDeserializeResult::Success;
        default:
            break;
    }
    return MediaFrameDeserializeResult::ParseError;
}

bool WebMDeserializer::IsOk(MkvResult result)
{
    return MkvResult::Success == result;
}

bool WebMDeserializer::MaybeOk(MkvResult result)
{
    switch (result) {
        case MkvResult::BufferNotFull:
        case MkvResult::NoMoreClusters:
            return true;
        default:
            break;
    }
    return IsOk(result);
}

const char* WebMDeserializer::MkvResultToString(MkvResult result)
{
    switch (result) {
        case MkvResult::ParseFailed:
            return "parse failed";
        case MkvResult::FileFormatInvalid:
            return "invalid file format";
        case MkvResult::BufferNotFull:
            return "buffer not full";
        case MkvResult::Success:
            return "ok";
        case MkvResult::NoMoreClusters:
            return "no more clusters";
        default:
            break;
    }
    return "unknown error";
}

const char* WebMDeserializer::MkvResultToString(long result)
{
    return MkvResultToString(ToMkvResult(result));
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
        return  _blockEntry->EOS() ? StreamState::End : StreamState::InTheMiddle;
    }
    return StreamState::BeforeTheStart;
}

void WebMDeserializer::TrackInfo::Reset()
{
    _cluster = _segment->GetFirst();
    _blockEntry = nullptr;
    _blockEntryIndex = _blockEntryIndex = 0L;
    _initialTimestamp = _latestTimestamp;
}

void WebMDeserializer::TrackInfo::SetClockRate(uint32_t clockRate)
{
    MS_ASSERT(clockRate > 0U, "clock rate must be greater than zero");
    _clockRate = clockRate;
}

void WebMDeserializer::TrackInfo::SetInitialTimestamp(uint32_t timestamp)
{
    if (timestamp != _initialTimestamp) {
        _latestTimestamp = _initialTimestamp = timestamp;
    }
}

MediaFrameDeserializeResult WebMDeserializer::TrackInfo::
    ReadFrames(size_t payloadOffset, std::vector<std::shared_ptr<const MediaFrame>>& output)
{
    const mkvparser::Block* block = nullptr;
    auto mkvResult = GetNextBlock(block);
    if (IsOk(mkvResult)) {
        const auto ts = GetBlockTime(block);
        output.clear();
        output.reserve(static_cast<size_t>(block->GetFrameCount()));
        for (int i = 0; i < block->GetFrameCount(); ++i) {
            const auto& frame = block->GetFrame(i);
            std::vector<uint8_t> buffer(payloadOffset + frame.len);
            mkvResult = ToMkvResult(frame.Read(_segment->m_pReader, buffer.data() + payloadOffset));
            if (MaybeOk(mkvResult)) {
                auto mediaFrame = std::make_shared<MediaFrame>(GetMime());
                if (mediaFrame->AddPayload(std::move(buffer))) {
                    mediaFrame->SetKeyFrame(block->IsKey());
                    mediaFrame->SetMediaConfig(GetConfig());
                    if (ts.has_value()) {
                        mediaFrame->SeTimestamp(ts.value());
                    }
                    output.push_back(std::move(mediaFrame));
                }
            }
            else {
                break;
            }
        }
        if (!output.empty() && ts.has_value()) {
            _latestTimestamp = ts.value();
        }
    }
    return FromMkvResult(mkvResult);
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
                    const auto codecId = WebMSerializer::GetCodecId(it->first);
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

WebMDeserializer::MkvResult WebMDeserializer::TrackInfo::GetNextBlock(const mkvparser::Block*& block)
{
    if (_cluster) {
        for (;;) {
            long res = _cluster->GetEntry(_blockEntryIndex, _blockEntry);
            MS_DEBUG_DEV("Cluster::GetEntry returned: %s", WebMDeserializer::MkvResultToString(res));
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
                    MS_DEBUG_DEV("Cluster::Parse returned: %s", WebMDeserializer::MkvResultToString(res));
                }
                if (res < 0L) {
                    const auto mkvResult = ToMkvResult(res);
                    // I/O error
                    if (_lastBlockIterationResult != res) {
                        MS_ERROR("Cluster::Parse error: %s", WebMDeserializer::MkvResultToString(mkvResult));
                        _lastBlockIterationResult = res;
                    }
                    _cluster = nullptr;
                    return mkvResult;
                }
                continue;
            } else if (res == 0L) {
                // We're done with this cluster
                const mkvparser::Cluster* nextCluster = nullptr;
                res = _segment->ParseNext(_cluster, nextCluster, pos, len);
                MS_DEBUG_DEV("Segment::ParseNext returned: %s", WebMDeserializer::MkvResultToString(res));
                if (res != 0L) {
                    const auto mkvResult = ToMkvResult(res);
                    // EOF or error
                    if (res < 0L) {
                        if (_lastBlockIterationResult != res) {
                            MS_ERROR("Segment::ParseNext error: %s", WebMDeserializer::MkvResultToString(mkvResult));
                            _lastBlockIterationResult = res;
                        }
                        _cluster = nullptr;
                    }
                    return mkvResult;
                }
                MS_ASSERT(nextCluster, "next WebM cluster must not be null");
                MS_ASSERT(!nextCluster->EOS(), "next WebM cluster is end of the stream");
                _cluster = nextCluster;
                res = _cluster->Parse(pos, len);
                MS_DEBUG_DEV("Cluster::Parse (2) returned: %s", WebMDeserializer::MkvResultToString(res));
                if (res < 0L) {
                    const auto mkvResult = ToMkvResult(res);
                    // I/O error
                    if (_lastBlockIterationResult != res) {
                        MS_ERROR("Cluster::Parse (2) error: %s", WebMDeserializer::MkvResultToString(mkvResult));
                        _lastBlockIterationResult = res;
                    }
                    _cluster = nullptr;
                    return mkvResult;
                }
                _blockEntryIndex = 0L;
                continue;
            }
block_exit:
            MS_ASSERT(_blockEntry, "block entry must not be null");
            MS_ASSERT(_blockEntry->GetBlock(), "block must not be null");
            ++_blockEntryIndex;
            if (_blockEntry->GetBlock()->GetTrackNumber() == _trackNum) {
                _lastBlockIterationResult = res;
                block = _blockEntry->GetBlock();
                break;
            }
        }
    }
    return ToMkvResult(_lastBlockIterationResult);
}

std::optional<uint32_t>  WebMDeserializer::TrackInfo::GetBlockTime(const mkvparser::Block* block) const
{
    if (block && _blockEntry) {
        const auto blockTime = block->GetTime(_blockEntry->GetCluster());
        const auto granule = ValueFromNano<uint32_t>(blockTime * GetClockRate());
        return GetInitialTimestamp() + granule;
    }
    return std::nullopt;
}

} // namespace RTC
