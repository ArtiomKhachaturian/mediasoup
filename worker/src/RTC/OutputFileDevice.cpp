#include "RTC/OutputFileDevice.hpp"
#include <utility>
#ifdef _WIN32
#include <Windows.h>
#include <string>
#endif

namespace RTC
{

OutputFileDevice::OutputFileDevice(FILE* file)
    : _file(file)
{
}

OutputFileDevice::OutputFileDevice(std::string_view fileNameUtf8, int* error)
    : OutputFileDevice(FileOpen(std::move(fileNameUtf8), error))
{
}

OutputFileDevice::OutputFileDevice(OutputFileDevice&& tmp)
{
    operator=(std::move(tmp));
}

OutputFileDevice& OutputFileDevice::operator=(OutputFileDevice&& tmp)
{
    if (this != &tmp) {
        Close();
        std::swap(_file, tmp._file);
    }
    return *this;
}

bool OutputFileDevice::Close()
{
    if (_file) {
        const auto success = 0 == ::fclose(_file);
        _file = nullptr;
        return success;
    }
    return true; // already closed
}

bool OutputFileDevice::Flush()
{
    return _file && 0 == ::fflush(_file);
}

bool OutputFileDevice::Write(const void* buf, uint32_t len)
{
    if (_file && buf) {
        return len == ::fwrite(buf, 1U, len, _file);
    }
    return false;
}

int64_t OutputFileDevice::GetPosition() const
{
    if (_file) {
        return ::ftell(_file);
    }
    return -1LL;
}

bool OutputFileDevice::SetPosition(int64_t position)
{
    return _file && 0 == ::fseek(_file, static_cast<long>(position), SEEK_SET);
}

FILE* OutputFileDevice::FileOpen(std::string_view fileNameUtf8, int* error)
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
