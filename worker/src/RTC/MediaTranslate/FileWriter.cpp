#define MS_CLASS "RTC::FileWriter"
#include "RTC/MediaTranslate/FileWriter.hpp"
#include "RTC/Buffers/Buffer.hpp"
#include "Logger.hpp"
#include <stdio.h>
#ifdef _WIN32
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#if __has_include (<sys/syslimits.h>)
#include <sys/syslimits.h>
#elif !defined F_GETPATH
#define F_GETPATH 50
#endif
#endif

namespace RTC
{

FileWriter::~FileWriter()
{
    Flush();
}

bool FileWriter::WriteAll(const std::string_view& fileNameUtf8, const std::shared_ptr<Buffer>& buffer)
{
    return buffer && WriteAll(fileNameUtf8, buffer->GetData(), buffer->GetSize());
}

bool FileWriter::WriteAll(const std::string_view& fileNameUtf8, const uint8_t* data, size_t len)
{
    bool ok = false;
    if (data && len) {
        if (const auto handle = Base::OpenFile(fileNameUtf8, false)) {
            ok = Write(handle, data, len) == len;
        }
    }
    return ok;
}

bool FileWriter::DeleteFromStorage()
{
    bool ok = false;
    if (const auto handle = GetHandle()) {
#ifdef _WIN32
        char filePath[MAX_PATH] = { 0 };
        const auto fileHandle = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(handle.get())));
        if (INVALID_HANDLE_VALUE != fileHandle) {
            // TODO: get file name from WinAPI handle
        }
#else
        char filePath[PATH_MAX] = { 0 };
        ok = -1 != fcntl(fileno(handle.get()), F_GETPATH, filePath);
#endif
        if (ok) {
            Close();
            ok = 0 == ::remove(filePath);
        }
    }
    return ok;
}

bool FileWriter::Open(const std::string_view& fileNameUtf8, int* error)
{
    return Base::Open(fileNameUtf8, false, error);
}

bool FileWriter::Flush()
{
    const auto handle = GetHandle();
    return handle && 0 == ::fflush(handle.get());
}

void FileWriter::StartMediaWriting(const ObjectId& sender)
{
    MediaSink::StartMediaWriting(sender);
    if (const auto handle = GetHandle()) { // truncate file
#ifdef _WIN32
        const auto result = ::_chsize_s(_fileno(handle.get()), 0LL);
#else
        auto result = ::ftruncate(fileno(handle.get()), 0);
#endif
        if (0 != result) {
#ifndef _WIN32
            result = errno;
#endif
            MS_WARN_DEV_STD("failed truncated file, error code %d", int(result));
        }
    }
}

void FileWriter::WriteMediaPayload(const ObjectId&, const std::shared_ptr<Buffer>& buffer)
{
    if (const auto handle = GetHandle()) {
        if (buffer && !buffer->IsEmpty()) {
            const auto expected = buffer->GetSize();
            const auto actual = Write(handle, buffer->GetData(), expected);
            if (expected != actual) {
                MS_ERROR_STD("file write error, expected %zu but "
                             "written only %zu bytes, error code: %d",
                             expected, actual, errno);
            }
        }
    }
}

void FileWriter::EndMediaWriting(const ObjectId& sender)
{
    MediaSink::EndMediaWriting(sender);
    Flush();
}

size_t FileWriter::Write(const std::shared_ptr<FILE>& handle, const uint8_t* data, size_t len)
{
    if (handle && data && len) {
        return ::fwrite(data, 1U, len, handle.get());
    }
    return 0UL;
}

} // namespace RTC
