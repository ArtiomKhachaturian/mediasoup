#include "RTC/MediaTranslate/FileDevice.hpp"
#include <stddef.h>
#include <stdio.h>
#include <utility>
#ifdef _WIN32
#include <Windows.h>
#include <string>
#else
#include <unistd.h>
#endif

namespace RTC
{

FileDevice::FileDevice(FileDevice&& tmp)
    : _error(std::move(tmp._error))
{
    std::swap(_handle, tmp._handle);
}

FileDevice::~FileDevice()
{
    Close();
}

FileDevice& FileDevice::operator = (FileDevice&& tmp)
{
    if (&tmp != this) {
        _error = std::move(tmp._error);
        std::swap(_handle, tmp._handle);
    }
    return *this;
}

bool FileDevice::Close()
{
    if (_handle) {
        const auto ok = 0 == SetError(::fclose(_handle));
        _handle = nullptr;
        return ok;
    }
    return true; // already closed
}

bool FileDevice::Open(const std::string_view& fileNameUtf8, bool readOnly)
{
    int error = 0;
    if (auto handle = OpenFile(fileNameUtf8, readOnly, &error)) {
        _handle = handle;
    }
    return 0 == SetError(error);
}

void FileDevice::SetError(std::error_code error)
{
    _error = std::move(error);
}

int FileDevice::SetError(int error)
{
    SetError(ToGenericError(error));
    return error;
}

std::error_code FileDevice::ToGenericError(int error)
{
    return std::error_code(error, std::generic_category());
}

FILE* FileDevice::OpenFile(const std::string_view& fileNameUtf8, bool readOnly, int* error)
{
#if defined(_WIN32)
    std::string fileName(fileNameUtf8);
    int len = ::MultiByteToWideChar(CP_UTF8, 0, fileName.c_str(), -1, nullptr, 0);
    std::wstring wstr(len, 0);
    ::MultiByteToWideChar(CP_UTF8, 0, fileName.c_str(), -1, &wstr[0], len);
    FILE* handle = ::_wfopen(wstr.c_str(), readOnly ? L"rb" : L"wb");
#else
    FILE* handle = ::fopen(fileNameUtf8.data(), readOnly ? "rb" : "wb");
#endif
    if (!handle && error) {
        *error = errno;
    }
    return handle;
}

} // namespace RTC
