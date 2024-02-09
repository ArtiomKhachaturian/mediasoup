#define MS_CLASS "RTC::MkvBufferedReader"
#include "RTC/MediaTranslate/WebM/MkvBufferedReader.hpp"
#include "Logger.hpp"

namespace RTC
{

MkvBufferedReader::MkvBufferedReader()
	: _buffer(_maxBufferSize)
{
}

MkvReadResult MkvBufferedReader::AddBuffer(const std::shared_ptr<MemoryBuffer>& buffer)
{
    auto result = MkvReadResult::InvalidInputArg;
    if (buffer && !buffer->IsEmpty()) {
        result = MkvReadResult::OutOfMemory;
        if (buffer->GetSize() > _buffer.GetCapacity()) {
            MS_ERROR_STD("size of input data buffer (%zu bytes) is too big "
                         "for WebM decoding, max allowed size is %zu bytes",
                         buffer->GetSize(), _buffer.GetCapacity());
        }
        else if (_buffer.Append(buffer)) {
            result = ParseEBMLHeader();
            if (IsOk(result)) {
                result = ParseSegment();
            }
        }
    }
    return result;
}

const mkvparser::Tracks* MkvBufferedReader::GetTracks() const
{
    if (const auto segment = GetSegment()) {
        return segment->GetTracks();
    }
    return nullptr;
}

MkvReadResult MkvBufferedReader::ParseEBMLHeader()
{
    if (!_ebmlHeader) {
        auto ebmlHeader = std::make_unique<mkvparser::EBMLHeader>();
        long long pos = 0LL;
        const auto result = ToMkvReadResult(ebmlHeader->Parse(this, pos));
        if (IsOk(result)) {
            _ebmlHeader = std::move(ebmlHeader);
        }
        return result;
    }
    return MkvReadResult::Success;
}

MkvReadResult MkvBufferedReader::ParseSegment()
{
    if (!_segment) {
        mkvparser::Segment* segment = nullptr;
        long long pos = 0LL;
        auto result = ToMkvReadResult(mkvparser::Segment::CreateInstance(this, pos, segment));
        if (IsOk(result)) {
            result = ToMkvReadResult(segment->Load());
            if (MaybeOk(result)) {
                if (segment->GetTracks() && segment->GetTracks()->GetTracksCount()) {
                    _segment.reset(segment);
                    segment = nullptr;
                    result = MkvReadResult::Success;
                }
                else {
                    result = MkvReadResult::UnknownError;
                }
            }
        }
        delete segment;
        return result;
    }
    return MkvReadResult::Success;
}

int MkvBufferedReader::Read(long long pos, long len, unsigned char* buf)
{
    if (len >= 0 && pos >= 0) {
        const auto expected = static_cast<size_t>(len);
        const auto actual = _buffer.CopyTo(static_cast<size_t>(pos), expected, buf);
        if (expected == actual) {
            return 0;
        }
    }
    return -1;
}

int MkvBufferedReader::Length(long long* total, long long* available)
{
    if (total) {
        // https://chromium.googlesource.com/webm/webmlive/+/v2/client_encoder/webm_buffer_parser.cc#99
        *total = -1; // // total file size is unknown
    }
    if (available) {
        *available = _buffer.GetSize();
    }
    return 0;
}

const char* MkvReadResultToString(MkvReadResult result)
{
    switch (result) {
        case MkvReadResult::InvalidInputArg:
            return "invalid input argument";
        case MkvReadResult::OutOfMemory:
            return "out of memory";
        case MkvReadResult::ParseFailed:
            return "parse failed";
        case MkvReadResult::FileFormatInvalid:
            return "invalid file format";
        case MkvReadResult::BufferNotFull:
            return "buffer not full";
        case MkvReadResult::Success:
            return "ok";
        case MkvReadResult::NoMoreClusters:
            return "no more clusters";
        default:
            break;
    }
    return "unknown error";
}

} // namespace RTC
