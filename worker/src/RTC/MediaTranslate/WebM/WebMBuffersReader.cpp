#define MS_CLASS "RTC::WebMBuffersReader"
#include "RTC/MediaTranslate/WebM/WebMBuffersReader.hpp"
#include "MemoryBuffer.hpp"
#include "Logger.hpp"

namespace RTC
{

bool WebMBuffersReader::AddBuffer(const std::shared_ptr<const MemoryBuffer>& buffer)
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

int WebMBuffersReader::Read(long long pos, long len, unsigned char* buf)
{
    if (len >= 0 && pos + len < _bufferSize) {
        std::memcpy(buf, _buffer.data() + pos, len);
        return 0;
    }
    return -1;
}

int WebMBuffersReader::Length(long long* total, long long* available)
{
    if (total) {
        // https://chromium.googlesource.com/webm/webmlive/+/v2/client_encoder/webm_buffer_parser.cc#99
        //*total = -1; // // total file size is unknown
        *total = _bufferSize; // std::numeric_limits<long long>::max();
    }
    if (available) {
        *available = _bufferSize;
    }
    return 0;
}

} // namespace RTC