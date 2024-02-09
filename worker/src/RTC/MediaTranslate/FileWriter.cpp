#define MS_CLASS "RTC::FileWriter"
#include "RTC/MediaTranslate/FileWriter.hpp"
#include "MemoryBuffer.hpp"
#include "Logger.hpp"

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
            const auto actual = ::fwrite(buffer->GetData(), 1U, expected, handle);
            if (expected != actual) {
                MS_ERROR_STD("file write error, expected %llu but "
                             "written only %lu bytes, error code: %d",
                             expected, actual, errno);
            }
        }
    }
}

} // namespace RTC
