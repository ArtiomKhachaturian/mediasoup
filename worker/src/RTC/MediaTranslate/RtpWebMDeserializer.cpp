#define MS_CLASS "RTC::RtpWebMDeserializer"

#include "RTC/MediaTranslate/RtpWebMDeserializer.hpp"
#include "RTC/MediaTranslate/RtpWebMSerializer.hpp"
#include "RTC/MediaTranslate/AudioFrameConfig.hpp"
#include "RTC/MediaTranslate/VideoFrameConfig.hpp"
#include "MemoryBuffer.hpp"
#include "Logger.hpp"
#include <array>
#include <mkvparser/mkvreader.h>

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

RtpWebMDeserializer::RtpWebMDeserializer()
    : _reader(std::make_unique<MemoryReader>())
{
}

RtpWebMDeserializer::~RtpWebMDeserializer()
{
    delete _segment;
}

bool RtpWebMDeserializer::AddBuffer(const std::shared_ptr<const MemoryBuffer>& buffer)
{
    return _reader->AddBuffer(buffer) && ParseLatestIncomingBuffer();
}

size_t RtpWebMDeserializer::GetTracksCount() const
{
    if (_segment) {
        if (const auto tracksInfo = _segment->GetTracks()) {
            return tracksInfo->GetTracksCount();
        }
    }
    return 0UL;
}

std::optional<RtpCodecMimeType> RtpWebMDeserializer::GetTrackMimeType(size_t trackIndex) const
{
    if (const auto track = GetTrack(trackIndex)) {
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
            if (subtype.has_value()) {
                return std::make_optional<RtpCodecMimeType>(type.value(), subtype.value());
            }
            MS_ASSERT(false, "unsupported WebM codec type");
        }
    }
    return std::nullopt;
}

std::shared_ptr<AudioFrameConfig> RtpWebMDeserializer::GetTrackAudioFrameConfig(size_t trackIndex) const
{
    if (const auto audioTrack = GetAudioTrack(trackIndex)) {
        auto config = std::make_shared<AudioFrameConfig>();
        config->SetChannelCount(audioTrack->GetChannels());
        config->SetBitsPerSample(audioTrack->GetBitDepth());
        SetCodecSpecificData(config, audioTrack);
        return config;
    }
    return nullptr;
}

std::shared_ptr<VideoFrameConfig> RtpWebMDeserializer::GetTrackVideoFrameConfig(size_t trackIndex) const
{
    if (const auto videoTrack = GetVideoTrack(trackIndex)) {
        auto config = std::make_shared<VideoFrameConfig>();
        config->SetWidth(videoTrack->GetWidth());
        config->SetHeight(videoTrack->GetHeight());
        config->SetFrameRate(videoTrack->GetFrameRate());
        SetCodecSpecificData(config, videoTrack);
        return config;
    }
    return nullptr;
}

uint32_t RtpWebMDeserializer::GetAudioSampleRate(size_t trackIndex) const
{
    if (const auto audioTrack = GetAudioTrack(trackIndex)) {
        return static_cast<uint32_t>(std::round(audioTrack->GetSamplingRate()));
    }
    return 0U;
}

std::shared_ptr<const MediaFrame> RtpWebMDeserializer::ReadNextFrame(size_t trackIndex)
{
    if (_ok && _segment) {
        /*if (!_cluster) {
            _cluster = _segment->GetFirst();
        }
        bool blockEntryEos = false;
        do {
            
        }
        while (blockEntryEos || )
        
        
        auto cluster = _cluster;
        auto blockEntry = _blockEntry;
        auto block = _block;
        do {
            long status = 0;
            bool getNewBlock = false;
            if (!blockEntry && !blockEntryEos) {
                status = cluster->GetFirst(blockEntry);
                getNewBlock = true;
            }
            else if (blockEntryEos || blockEntry->EOS()) {
                cluster = _segment->GetNext(cluster);
            }
        }
        while (block->GetTrackNumber() != trackIndex || blockEntryEos);*/
    }
    return nullptr;
}

bool RtpWebMDeserializer::ParseLatestIncomingBuffer()
{
    return _ok && ParseEBMLHeader() && ParseSegment();
}

bool RtpWebMDeserializer::ParseEBMLHeader()
{
    if (_ok && !_ebmlHeader) {
        auto ebmlHeader = std::make_unique<mkvparser::EBMLHeader>();
        long long pos = 0LL;
        _ok = 0L == ebmlHeader->Parse(_reader.get(), pos);
        if (_ok) {
            _ebmlHeader = std::move(ebmlHeader);
        }
    }
    return _ok && _ebmlHeader;
}

bool RtpWebMDeserializer::ParseSegment()
{
    if (_ok && !_segment && _ebmlHeader) {
        mkvparser::Segment* segment = nullptr;
        long long pos = 0LL;
        _ok = 0L == mkvparser::Segment::CreateInstance(_reader.get(), pos, segment);
        if (_ok) {
            _ok = 0L == segment->Load();
        }
        if (_ok) {
            const auto tracksInfo = segment->GetTracks();
            _ok = tracksInfo && tracksInfo->GetTracksCount() > 0UL;
            if (_ok) {
                _segment = segment;
            }
        }
        if (!_ok) {
            delete segment;
        }
    }
    return _ok && nullptr != _segment;
}

const mkvparser::Track* RtpWebMDeserializer::GetTrack(size_t trackIndex) const
{
    if (_segment) {
        if (const auto tracksInfo = _segment->GetTracks()) {
            return tracksInfo->GetTrackByIndex(trackIndex);
        }
    }
    return nullptr;
}

const mkvparser::AudioTrack* RtpWebMDeserializer::GetAudioTrack(size_t trackIndex) const
{
    const auto track = GetTrack(trackIndex);
    if (track && mkvparser::Track::Type::kAudio == track->GetType()) {
        return static_cast<const mkvparser::AudioTrack*>(track);
    }
    return nullptr;
}

const mkvparser::VideoTrack* RtpWebMDeserializer::GetVideoTrack(size_t trackIndex) const
{
    const auto track = GetTrack(trackIndex);
    if (track && mkvparser::Track::Type::kVideo == track->GetType()) {
        return static_cast<const mkvparser::VideoTrack*>(track);
    }
    return nullptr;
}

void RtpWebMDeserializer::SetCodecSpecificData(const std::shared_ptr<MediaFrameConfig>& config,
                                               const mkvparser::Track* sourceTrack)
{
    if (config && sourceTrack) {
        size_t len = 0UL;
        if (const auto data = sourceTrack->GetCodecPrivate(len)) {
            config->SetCodecSpecificData(data, len);
        }
    }
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


} // namespace RTC
