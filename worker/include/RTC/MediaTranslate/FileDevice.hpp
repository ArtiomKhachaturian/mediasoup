#pragma once
#include <atomic>
#include <memory>
#include <string>
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
    virtual bool IsOpen() const { return nullptr != GetHandle(); }
    // Closes the file, and implies Flush
    void Close();
protected:
    FileDevice() = default;
    explicit FileDevice(std::shared_ptr<FILE> handle);
    FileDevice(const std::string_view& fileNameUtf8, bool readOnly, int* error = nullptr);
    std::shared_ptr<FILE> GetHandle() const { return std::atomic_load(&_handle); }
    static std::shared_ptr<FILE> Open(const std::string_view& fileNameUtf8, bool readOnly, int* error = nullptr);
private:
    std::shared_ptr<FILE> _handle = nullptr;
};

template<class TBase>
FileDevice<TBase>::FileDevice(std::shared_ptr<FILE> handle)
    : _handle(std::move(handle))
{
}

template<class TBase>
FileDevice<TBase>::FileDevice(const std::string_view& fileNameUtf8, bool readOnly, int* error)
    : FileDevice(Open(fileNameUtf8, readOnly, error))
{
}

template<class TBase>
void FileDevice<TBase>::Close()
{
    std::atomic_store(&_handle, std::shared_ptr<FILE>());
}

template<class TBase>
std::shared_ptr<FILE> FileDevice<TBase>::Open(const std::string_view& fileNameUtf8, bool readOnly, int* error)
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
    return std::shared_ptr<FILE>(handle, [](FILE* handle) { ::fclose(handle); });
}

} // namespace RTC
