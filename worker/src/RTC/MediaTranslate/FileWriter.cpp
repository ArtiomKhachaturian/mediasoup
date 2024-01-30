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

bool FileWriter::Flush()
{
    return GetHandle() && 0 == ::fflush(GetHandle());
}

void FileWriter::StartMediaWriting(bool restart)
{
    MediaSink::StartMediaWriting(restart);
    if (const auto handle = GetHandle()) {
        MS_DEBUG_DEV(restart ? "media stream restarted" : "media stream started");
        if (restart) {
            int error = ::ftruncate(fileno(handle), 0ULL);
            if (0 == error) {
                error = ::fseek(handle, 0L, SEEK_SET);
                if (0 == error) {
                    const auto pos = ::ftell(handle);
                    if (0L != pos) {
                        MS_ERROR("file position is not at beginning, actual position: %ld", pos);
                    }
                }
                else {
                    MS_ERROR("failed seek to beginning after file truncation, error code: %d", error);
                }
            }
            else {
                MS_ERROR("failed to truncate file, error code: %d", error);
            }
        }
    }
}

void FileWriter::WriteMediaPayload(uint32_t /*ssrc*/,
                                   const std::shared_ptr<const MemoryBuffer>& buffer)
{
    if (const auto handle = GetHandle()) {
        if (buffer && !buffer->IsEmpty()) {
            const auto expected = buffer->GetSize();
            const auto actual = ::fwrite(buffer->GetData(), 1U, expected, handle);
            if (expected != actual) {
                MS_ERROR("file write error, expected %lu but "
                         "written only %lu bytes, error code: %d",
                         expected, actual, errno);
            }
        }
    }
}

} // namespace RTC
