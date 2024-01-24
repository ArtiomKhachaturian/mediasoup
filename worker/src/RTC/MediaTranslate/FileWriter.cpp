#define MS_CLASS "RTC::FileWriter"
#include "RTC/MediaTranslate/FileWriter.hpp"
#include "MemoryBuffer.hpp"
#include <utility>
#ifdef _WIN32
#include <Windows.h>
#include <string>
#endif
#include <unistd.h>
#include "Logger.hpp"

namespace RTC
{

FileWriter::FileWriter(FILE* file)
    : _file(file)
{
}

FileWriter::FileWriter(std::string_view fileNameUtf8, int* error)
    : FileWriter(OpenFile(std::move(fileNameUtf8), error))
{
}

FileWriter::FileWriter(FileWriter&& tmp)
{
    operator=(std::move(tmp));
}

FileWriter& FileWriter::operator=(FileWriter&& tmp)
{
    if (this != &tmp) {
        Close();
        std::swap(_file, tmp._file);
    }
    return *this;
}

bool FileWriter::Close()
{
    if (_file) {
        const auto success = 0 == ::fclose(_file);
        _file = nullptr;
        return success;
    }
    return true; // already closed
}

bool FileWriter::Flush()
{
    return _file && 0 == ::fflush(_file);
}

void FileWriter::StartMediaWriting(bool restart, uint32_t startTimestamp) noexcept
{
    MediaSink::StartMediaWriting(restart, startTimestamp);
    if (_file) {
        MS_DEBUG_DEV(restart ? "media stream restarted" : "media stream started");
        if (restart) {
            int error = ::ftruncate(fileno(_file), 0ULL);
            if (0 == error) {
                error = ::fseek(_file, 0L, SEEK_SET);
                if (0 == error) {
                    const auto pos = ::ftell(_file);
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

void FileWriter::WriteMediaPayload(const std::shared_ptr<const MemoryBuffer>& buffer) noexcept
{
    if (_file && buffer && !buffer->IsEmpty()) {
        const auto expected = buffer->GetSize();
        const auto actual = ::fwrite(buffer->GetData(), 1U, expected, _file);
        if (expected != actual) {
            MS_ERROR("file write error, expected %lu but "
                     "written only %lu bytes, error code: %d",
                     expected, actual, errno);
        }
    }
}

FILE* FileWriter::OpenFile(std::string_view fileNameUtf8, int* error)
{
#if defined(_WIN32)
    std::string fileName(fileNameUtf8);
    int len = ::MultiByteToWideChar(CP_UTF8, 0, fileName.c_str(), -1, nullptr, 0);
    std::wstring wstr(len, 0);
    ::MultiByteToWideChar(CP_UTF8, 0, fileName.c_str(), -1, &wstr[0], len);
    FILE* file = ::_wfopen(wstr.c_str(), L"wb");
#else
    FILE* file = ::fopen(fileNameUtf8.data(), "wb");
#endif
    if (!file && error) {
        *error = errno;
    }
    return file;
}

} // namespace RTC
