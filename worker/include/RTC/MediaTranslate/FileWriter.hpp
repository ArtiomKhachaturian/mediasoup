#pragma once

#include "RTC/MediaTranslate/OutputDevice.hpp"
#include <string>
#include <stddef.h>
#include <stdio.h>

namespace RTC
{

class FileWriter : public OutputDevice
{
public:
    FileWriter() = default;
    explicit FileWriter(FILE* file);
    FileWriter(std::string_view fileNameUtf8, int* error = nullptr);
    FileWriter(FileWriter&& tmp);
    ~FileWriter() override { Close(); }
    FileWriter& operator=(FileWriter&& tmp);
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
    // impl. of OutputDevice
    void StartStream(bool restart) final;
    void Write(const std::shared_ptr<const MemoryBuffer>& buffer) final;
protected:
    static FILE* FileOpen(std::string_view fileNameUtf8, int* error = nullptr);
private:
    FILE* _file = nullptr;
};

} // namespace RTC
