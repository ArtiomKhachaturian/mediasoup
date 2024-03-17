#pragma once
#include <system_error>
#include <cstdio>
#include <string>

namespace RTC
{

class FileDevice
{
public:
    FileDevice(const FileDevice&) = delete;
    FileDevice(FileDevice&& tmp);
    virtual ~FileDevice();
    FileDevice& operator = (const FileDevice&) = delete;
    FileDevice& operator = (FileDevice&& tmp);
    // return last operation error
    const std::error_code& GetError() const { return _error; }
    // Closes the file, and implies Flush
    bool Close();
    // Returns true if a file has been opened. If the file is not open, no methods
    // but IsOpen and Close may be called.
    bool IsOpen() const { return nullptr != GetHandle(); }
protected:
    FileDevice() = default;
    bool Open(const std::string_view& fileNameUtf8, bool readOnly);
    FILE* GetHandle() const { return _handle; }
    void SetError(std::error_code error);
    int SetError(int error);
    static std::error_code ToGenericError(int error);
private:
    static FILE* OpenFile(const std::string_view& fileNameUtf8, bool readOnly,
                          int* error = nullptr);
private:
    std::error_code _error;
    FILE* _handle = nullptr;
};

} // namespace RTC
