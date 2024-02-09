#define MS_CLASS "RTC::FileWriter"
#include "RTC/MediaTranslate/FileWriter.hpp"
#include "MemoryBuffer.hpp"
#include "Logger.hpp"

namespace {

class BufferView : public RTC::MemoryBuffer
{
public:
    BufferView(const std::vector<uint8_t>& buffer);
    // impl. of RTC::MemoryBuffer
    size_t GetSize() const final { return _buffer.size(); }
    uint8_t* GetData() final { return const_cast<uint8_t*>(_buffer.data()); }
    const uint8_t* GetData() const { return _buffer.data(); }
private:
    const std::vector<uint8_t>& _buffer;
};

}

namespace RTC
{

FileWriter::FileWriter(const std::string_view& fileNameUtf8, int* error)
    : FileDevice<MediaSink>(fileNameUtf8, false, error)
{
}

FileWriter::~FileWriter()
{
    Flush();
}

bool FileWriter::WriteAll(const std::string_view& fileNameUtf8, const std::vector<uint8_t>& buffer)
{
    if (!fileNameUtf8.empty() && !buffer.empty()) {
        return WriteAll(fileNameUtf8, std::make_shared<BufferView>(buffer));
    }
    return false;
}

bool FileWriter::WriteAll(const std::string_view& fileNameUtf8, const std::shared_ptr<MemoryBuffer>& buffer)
{
    bool written = false;
    if (buffer && !buffer->IsEmpty()) {
        if (const auto handle = Open(fileNameUtf8, false)) {
            written = FileWrite(handle, buffer) == buffer->GetSize();
            Close(handle);
        }
    }
    return written;
}

bool FileWriter::Flush()
{
    return GetHandle() && 0 == ::fflush(GetHandle());
}

void FileWriter::WriteMediaPayload(uint32_t /*ssrc*/,
                                   const std::shared_ptr<MemoryBuffer>& buffer)
{
    if (const auto handle = GetHandle()) {
        if (buffer && !buffer->IsEmpty()) {
            const auto expected = buffer->GetSize();
            const auto actual = FileWrite(handle, buffer);
            if (expected != actual) {
                MS_ERROR_STD("file write error, expected %lu but "
                             "written only %lu bytes, error code: %d",
                             expected, actual, errno);
            }
        }
    }
}

size_t FileWriter::FileWrite(FILE* handle, const std::shared_ptr<MemoryBuffer>& buffer)
{
    if (handle && buffer) {
        return ::fwrite(buffer->GetData(), 1U, buffer->GetSize(), handle);
    }
    return 0UL;
}

} // namespace RTC

namespace {

BufferView::BufferView(const std::vector<uint8_t>& buffer)
    : _buffer(buffer)
{
}

}
