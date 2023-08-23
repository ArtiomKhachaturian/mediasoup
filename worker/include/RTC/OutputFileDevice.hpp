#ifndef MS_RTC_OUTPUT_FILE_DEVICE_HPP
#define MS_RTC_OUTPUT_FILE_DEVICE_HPP

#include "RTC/OutputDevice.hpp"
#include <string_view>
#include <stddef.h>
#include <stdio.h>

namespace RTC
{

class OutputFileDevice : public OutputDevice
{
public:
    OutputFileDevice() = default;
    explicit OutputFileDevice(FILE* file);
    OutputFileDevice(std::string_view fileNameUtf8, int* error = nullptr);
    OutputFileDevice(OutputFileDevice&& tmp);
    ~OutputFileDevice() final { Close(); }
    OutputFileDevice& operator=(OutputFileDevice&& tmp);
    // Closes the file, and implies Flush. Returns true on success, false if
    // writing buffered data fails. On failure, the file is nevertheless closed.
    // Calling Close on an already closed file does nothing and returns success.
    bool Close();
    // Returns true if a file has been opened. If the file is not open, no methods
    // but is_open and Close may be called.
    bool IsOpen() const { return nullptr != _file; }
    // Write any buffered data to the underlying file. Returns true on success,
    // false on write error. Note: Flushing when closing, is not required.
    bool Flush();
    // Seeks to the beginning of file. Returns true on success, false on failure,
    // e.g., if the underlying file isn't seekable.
    bool Rewind() { return SetPosition(0LL); }
    // impl. of OutputDevice
    bool Write(const void* buf, uint32_t len) final;
    int64_t GetPosition() const final;
    bool SetPosition(int64_t position) final;
    bool Seekable() const final { return true; }
private:
    static FILE* FileOpen(std::string_view fileNameUtf8, int* error = nullptr);
private:
    FILE* _file = nullptr;
};

} // namespace RTC

#endif
