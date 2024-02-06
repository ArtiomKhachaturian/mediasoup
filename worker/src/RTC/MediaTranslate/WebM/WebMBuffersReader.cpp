#define MS_CLASS "RTC::WebMBuffersReader"
#include "RTC/MediaTranslate/WebM/WebMBuffersReader.hpp"
#include "MemoryBuffer.hpp"
#include "Logger.hpp"

namespace RTC
{

WebMBuffersReader::WebMBuffersReader()
    : _buffer(_maxBufferSize)
{
}

MediaFrameDeserializeResult WebMBuffersReader::AddBuffer(const std::shared_ptr<MemoryBuffer>& buffer)
{
    MediaFrameDeserializeResult result = MediaFrameDeserializeResult::InvalidArg;
    if (buffer && !buffer->IsEmpty()) {
        result = MediaFrameDeserializeResult::OutOfMemory;
        PRId64;
        if (buffer->GetSize() > _buffer.GetCapacity()) {
            MS_WARN_DEV_STD("size of input data buffer (%llu bytes) is too big "
                            "for WebM decoding, max allowed size is %llu bytes",
                            buffer->GetSize(), _buffer.GetCapacity());
        }
        else if (_buffer.Add(buffer)) {
            result = MediaFrameDeserializeResult::Success;
        }
    }
    return result;
}

int WebMBuffersReader::Read(long long pos, long len, unsigned char* buf)
{
    if (len >= 0 && _buffer.GetData(pos, len, buf)) {
        return 0;
    }
    return -1;
}

int WebMBuffersReader::Length(long long* total, long long* available)
{
    if (total) {
        // https://chromium.googlesource.com/webm/webmlive/+/v2/client_encoder/webm_buffer_parser.cc#99
        *total = -1; // // total file size is unknown
        //*total = _bufferSize; // std::numeric_limits<long long>::max();
    }
    if (available) {
        *available = _buffer.GetSize();
    }
    return 0;
}

} // namespace RTC
