#pragma once
#include <string>
#include <stddef.h>
#include <stdio.h>
#include <utility>
#ifdef _WIN32
#include <Windows.h>
#include <string>
#endif
#include <unistd.h>

namespace RTC
{

template<class TBase>
class FileDevice : public TBase
{
public:
    FileDevice(const FileDevice&) = delete;
    FileDevice(FileDevice&&) = delete;
    FileDevice& operator = (const FileDevice&) = delete;
    FileDevice& operator = (FileDevice&&) = delete;
    virtual ~FileDevice() { Close(); }
    // Returns true if a file has been opened. If the file is not open, no methods
    // but IsOpen and Close may be called.
    virtual bool IsOpen() const { return nullptr != _handle; }
protected:
    FileDevice() = default;
    explicit FileDevice(FILE* handle);
    FileDevice(const std::string_view& fileNameUtf8, bool readOnly, int* error = nullptr);
    FILE* GetHandle() const { return _handle; }
    // Closes the file, and implies Flush. Returns true on success, false if
    // writing buffered data fails. On failure, the file is nevertheless closed.
    // Calling Close on an already closed file does nothing and returns success.
    bool Close();
    static FILE* Open(const std::string_view& fileNameUtf8, bool readOnly, int* error = nullptr);
    static bool Close(FILE* handle);
private:
    FILE* _handle = nullptr;
};

template<class TBase>
FileDevice<TBase>::FileDevice(FILE* handle)
    : _handle(handle)
{
}

template<class TBase>
FileDevice<TBase>::FileDevice(const std::string_view& fileNameUtf8, bool readOnly, int* error)
    : FileDevice(Open(fileNameUtf8, readOnly, error))
{
}

template<class TBase>
bool FileDevice<TBase>::Close()
{
    if (_handle) {
        const auto success = Close(_handle);
        _handle = nullptr;
        return success;
    }
    return true; // already closed
}

template<class TBase>
FILE* FileDevice<TBase>::Open(const std::string_view& fileNameUtf8, bool readOnly, int* error)
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

template<class TBase>
bool FileDevice<TBase>::Close(FILE* handle)
{
    return handle && 0 == ::fclose(handle);
}

} // namespace RTC
