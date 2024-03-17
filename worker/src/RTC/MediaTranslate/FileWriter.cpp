#include "RTC/MediaTranslate/FileWriter.hpp"
#include "RTC/Buffers/Buffer.hpp"
#include <stdio.h>
#ifdef _WIN32
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#if __has_include (<sys/syslimits.h>)
#include <sys/syslimits.h>
#endif
#endif

namespace RTC
{

FileWriter::~FileWriter()
{
    Flush();
}

bool FileWriter::Truncate()
{
    if (const auto handle = GetHandle()) {
#ifdef _WIN32
        const int result = ::_chsize_s(_fileno(handle), 0LL);
#else
        int result = ::ftruncate(fileno(handle), 0);
        if (-1 == result) {
            result = errno;
        }
#endif
        return 0 == SetError(result);
    }
    return false;
}

bool FileWriter::Open(const std::string_view& fileNameUtf8)
{
    return FileDevice::Open(fileNameUtf8, false);
}

bool FileWriter::Flush()
{
    if (const auto handle = GetHandle()) {
        return 0 == SetError(::fflush(handle));
    }
    return false;
}

size_t FileWriter::Write(const uint8_t* data, size_t len)
{
    size_t res = 0;
    if (data && len) {
        if (const auto handle = GetHandle()) {
            res = ::fwrite(data, 1U, len, handle);
            SetError(errno);
        }
    }
    return res;
}

size_t FileWriter::Write(const std::shared_ptr<Buffer>& buffer)
{
    if (buffer && IsOpen()) {
        return Write(buffer->GetData(), buffer->GetSize());
    }
    return 0UL;
}

size_t FileWriter::Write(const std::string_view& fileNameUtf8, const std::shared_ptr<Buffer>& buffer)
{
    if (buffer) {
        return Write(fileNameUtf8, buffer->GetData(), buffer->GetSize());
    }
    return 0UL;
}

size_t FileWriter::Write(const std::string_view& fileNameUtf8, const uint8_t* data, size_t len)
{
    if (data && len) {
        FileWriter writer;
        if (writer.Open(fileNameUtf8)) {
            return writer.Write(data, len);
        }
    }
    return 0UL;
}

std::error_code FileWriter::DeleteFromStorage(const std::string_view& fileNameUtf8)
{
    int result = EBADF;
    if (!fileNameUtf8.empty()) {
        result = ::remove(fileNameUtf8.data());
    }
    return ToGenericError(result);
}

} // namespace RTC
