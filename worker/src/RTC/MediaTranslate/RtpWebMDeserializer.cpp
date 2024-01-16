#define MS_CLASS "RTC::RtpWebMDeserializer"

#include "RTC/MediaTranslate/RtpWebMDeserializer.hpp"
#include "RTC/MediaTranslate/RtpWebMSerializer.hpp"
#include "RTC/MediaTranslate/AudioFrameConfig.hpp"
#include "RTC/MediaTranslate/VideoFrameConfig.hpp"
#include "MemoryBuffer.hpp"
#include "Logger.hpp"
#include <array>
#include <mkvparser/mkvreader.h>

namespace {

struct TrackInfo
{
    const RTC::RtpCodecMimeType _mime;
    const std::shared_ptr<RTC::MediaFrameConfig> _config;
    TrackInfo(RTC::RtpCodecMimeType::Type type, RTC::RtpCodecMimeType::Subtype subType,
              std::shared_ptr<RTC::MediaFrameConfig> config);
};

}

namespace RTC
{

class RtpWebMDeserializer::MemoryReader : public mkvparser::IMkvReader
{
public:
    MemoryReader() = default;
    bool AddBuffer(const std::shared_ptr<const MemoryBuffer>& buffer);
    // impl. of mkvparser::IMkvReader
    int Read(long long pos, long len, unsigned char* buf) final;
    int Length(long long* total, long long* available) final;

private:
    static inline constexpr size_t _maxBufferSize = 1024UL * 1024UL * 16UL; // 16 Mb
    std::array<uint8_t, _maxBufferSize> _buffer;
    size_t _bufferSize = 0UL;
};

class RtpWebMDeserializer::WebMStream
{
public:
    WebMStream(mkvparser::IMkvReader* reader);
    ~WebMStream();
    bool ParseEBMLHeader();
    bool ParseSegment();
    size_t GetTracksCount() const { return _segment ? _segment->GetTracks()->GetTracksCount() : 0UL; }
    std::optional<RtpCodecMimeType> GetTrackMimeType(size_t trackIndex) const;
    std::shared_ptr<const MediaFrame> ReadNextFrame(size_t trackIndex);
private:
    const mkvparser::Track* GetTrack(size_t trackIndex) const;
    static std::unique_ptr<TrackInfo> GetTrackInfo(const mkvparser::Tracks* tracks, size_t trackIndex);
private:
    mkvparser::IMkvReader* const _reader;
    std::unique_ptr<mkvparser::EBMLHeader> _ebmlHeader;
    mkvparser::Segment* _segment = nullptr;
    //const mkvparser::Cluster* _cluster = nullptr;
    //const mkvparser::BlockEntry* _blockEntry = nullptr;
    //mkvparser::Block* _block = nullptr;
    absl::flat_hash_map<size_t, std::unique_ptr<TrackInfo>> _tracks;
};

RtpWebMDeserializer::RtpWebMDeserializer()
    : _reader(std::make_unique<MemoryReader>())
    , _stream(std::make_unique<WebMStream>(_reader.get()))
{
}

RtpWebMDeserializer::~RtpWebMDeserializer()
{
}

bool RtpWebMDeserializer::AddBuffer(const std::shared_ptr<const MemoryBuffer>& buffer)
{
    return _reader->AddBuffer(buffer) && ParseLatestIncomingBuffer();
}

size_t RtpWebMDeserializer::GetTracksCount() const
{
    return _stream->GetTracksCount();
}

std::optional<RtpCodecMimeType> RtpWebMDeserializer::GetTrackMimeType(size_t trackIndex) const
{
    return _stream->GetTrackMimeType(trackIndex);
}

std::shared_ptr<const MediaFrame> RtpWebMDeserializer::ReadNextFrame(size_t trackIndex)
{
    return _ok ? _stream->ReadNextFrame(trackIndex) : nullptr;
}

bool RtpWebMDeserializer::ParseLatestIncomingBuffer()
{
    if (_ok) {
        _ok = _stream->ParseEBMLHeader() && _stream->ParseSegment();
    }
    return _ok;
}

bool RtpWebMDeserializer::MemoryReader::AddBuffer(const std::shared_ptr<const MemoryBuffer>& buffer)
{
    if (buffer && !buffer->IsEmpty()) {
        const auto size = buffer->GetSize();
        MS_ASSERT(size <= _maxBufferSize, "data buffer is too big for WebM decoding");
        uint8_t* target = nullptr;
        if (size + _bufferSize >= _maxBufferSize) {
            target = _buffer.data();
            _bufferSize = 0UL;
        }
        else {
            target = _buffer.data() + _bufferSize;
        }
        std::memcpy(target, buffer->GetData(), size);
        _bufferSize += size;
        return true;
    }
    return false;
}

int RtpWebMDeserializer::MemoryReader::Read(long long pos, long len, unsigned char* buf)
{
    if (len >= 0 && pos + len < _bufferSize) {
        std::memcpy(buf, _buffer.data() + pos, len);
        return 0;
    }
    return -1;
}

int RtpWebMDeserializer::MemoryReader::Length(long long* total, long long* available)
{
    if (total) {
        *total = _bufferSize; // std::numeric_limits<long long>::max();
    }
    if (available) {
        *available = _bufferSize;
    }
    return 0;
}

bool RtpWebMDeserializer::WebMStream::ParseEBMLHeader()
{
    if (!_ebmlHeader) {
        auto ebmlHeader = std::make_unique<mkvparser::EBMLHeader>();
        long long pos = 0LL;
        if (0L == ebmlHeader->Parse(_reader, pos)) {
            _ebmlHeader = std::move(ebmlHeader);
        }
    }
    return nullptr != _ebmlHeader;
}

bool RtpWebMDeserializer::WebMStream::ParseSegment()
{
    if (!_segment && _ebmlHeader) {
        mkvparser::Segment* segment = nullptr;
        long long pos = 0LL;
        if (0L == mkvparser::Segment::CreateInstance(_reader, pos, segment) &&
            0L == segment->Load()) {
            if (const auto tracks = segment->GetTracks()) {
                if (const auto tracksCount = tracks->GetTracksCount()) {
                    std::swap(_segment, segment);
                    for (size_t i = 0UL; i < tracksCount; ++i) {
                        if (auto trackInfo = GetTrackInfo(tracks, i)) {
                            _tracks[i] = std::move(trackInfo);
                        }
                    }
                }
            }
        }
        delete segment;
    }
    return nullptr != _segment;
}

std::optional<RtpCodecMimeType> RtpWebMDeserializer::WebMStream::GetTrackMimeType(size_t trackIndex) const
{
    const auto it = _tracks.find(trackIndex);
    if (it != _tracks.end()) {
        return it->second->_mime;
    }
    return std::nullopt;
}

std::shared_ptr<const MediaFrame> RtpWebMDeserializer::WebMStream::ReadNextFrame(size_t trackIndex)
{
    if (_segment) {
        const auto it = _tracks.find(trackIndex);
        if (it != _tracks.end()) {
            if (const auto track = GetTrack(trackIndex)) {
                
            }
        }
    }
    return nullptr;
}

const mkvparser::Track* RtpWebMDeserializer::WebMStream::GetTrack(size_t trackIndex) const
{
    if (_segment) {
        if (const auto tracks = _segment->GetTracks()) {
            return tracks->GetTrackByIndex(trackIndex);
        }
    }
    return nullptr;
}

std::unique_ptr<TrackInfo> RtpWebMDeserializer::WebMStream::GetTrackInfo(const mkvparser::Tracks* tracks,
                                                                         size_t trackIndex)
{
    if (tracks) {
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
                    MS_ASSERT(false, "unsupported WebM media track");
                    break;
            }
            if (type.has_value()) {
                std::optional<RtpCodecMimeType::Subtype> subtype;
                for (auto it = RtpCodecMimeType::subtype2String.begin();
                     it != RtpCodecMimeType::subtype2String.end(); ++it) {
                    const auto codecId = RtpWebMSerializer::GetCodecId(it->first);
                    if (codecId && 0 == std::strcmp(track->GetCodecId(), codecId)) {
                        subtype = it->first;
                        break;
                    }
                }
                MS_ASSERT(subtype.has_value(), "unsupported WebM codec type");
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
                    return std::make_unique<TrackInfo>(type.value(), subtype.value(), std::move(config));
                }
            }
        }
    }
    return nullptr;
}

RtpWebMDeserializer::WebMStream::WebMStream(mkvparser::IMkvReader* reader)
    : _reader(reader)
{
}

RtpWebMDeserializer::WebMStream::~WebMStream()
{
    delete _segment;
}

} // namespace RTC

namespace {

TrackInfo::TrackInfo(RTC::RtpCodecMimeType::Type type,
                     RTC::RtpCodecMimeType::Subtype subType,
                     std::shared_ptr<RTC::MediaFrameConfig> config)
    : _mime(type, subType)
    , _config(std::move(config))
{
}


}
