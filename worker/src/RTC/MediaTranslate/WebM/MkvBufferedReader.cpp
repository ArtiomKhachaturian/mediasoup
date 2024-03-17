#define MS_CLASS "RTC::MkvBufferedReader"
#include "RTC/MediaTranslate/WebM/MkvBufferedReader.hpp"
#include "Logger.hpp"

namespace RTC
{

MkvBufferedReader::MkvBufferedReader(const std::shared_ptr<BufferAllocator>& allocator)
	: _buffers(allocator, _maxBufferSize)
{
}

MkvReadResult MkvBufferedReader::AddBuffer(std::shared_ptr<Buffer> buffer)
{
    auto result = MkvReadResult::InvalidInputArg;
    if (buffer && !buffer->IsEmpty()) {
        result = MkvReadResult::OutOfMemory;
        if (buffer->GetSize() + _buffers.GetSize() > _buffers.GetCapacity()) {
            MS_ERROR_STD("size of input data buffer (%zu bytes) is too big "
                         "for WebM decoding, remaining capacity is %zu bytes",
                         buffer->GetSize(), _buffers.GetCapacity() - _buffers.GetSize());
        }
        else if (SegmentsBuffer::Failed != _buffers.Push(std::move(buffer))) {
            result = ParseEBMLHeader();
            if (IsOk(result)) {
                result = ParseSegment();
            }
        }
    }
    return result;
}

void MkvBufferedReader::ClearBuffers()
{
    _buffers.Clear();
    _ebmlHeader.reset();
    _segment.reset();
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
        const auto actual = _buffers.CopyTo(static_cast<size_t>(pos), expected, buf);
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
        *available = _buffers.GetSize();
    }
    return 0;
}

} // namespace RTC
