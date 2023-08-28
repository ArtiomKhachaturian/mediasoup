#include "RTC/MediaTranslate/FileWriter.hpp"
#include <utility>
#ifdef _WIN32
#include <Windows.h>
#include <string>
#endif

namespace RTC
{

FileWriter::FileWriter(FILE* file)
    : _file(file)
{
}

FileWriter::FileWriter(std::string_view fileNameUtf8, int* error)
    : FileWriter(FileOpen(std::move(fileNameUtf8), error))
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

bool FileWriter::Write(const void* buf, uint32_t len)
{
    if (_file && buf) {
        return len == ::fwrite(buf, 1U, len, _file);
    }
    return false;
}

int64_t FileWriter::GetPosition() const
{
    if (_file) {
        return ::ftell(_file);
    }
    return -1LL;
}

bool FileWriter::SetPosition(int64_t position)
{
    return _file && 0 == ::fseek(_file, static_cast<long>(position), SEEK_SET);
}

FILE* FileWriter::FileOpen(std::string_view fileNameUtf8, int* error)
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
