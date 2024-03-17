#pragma once
#include "RTC/MediaTranslate/FileDevice.hpp"

namespace RTC
{

class Buffer;

class FileWriter : public FileDevice
{
public:
    FileWriter() = default;
    FileWriter(const FileWriter&) = delete;
    FileWriter(FileWriter&&) = default;
    ~FileWriter() final;
    FileWriter& operator = (const FileWriter&) = delete;
    FileWriter& operator = (FileWriter&&) = default;
    bool Truncate();
    bool Open(const std::string_view& fileNameUtf8);
    // Write any buffered data to the underlying file. 
    // Note: Flushing when closing, is not required.
    bool Flush();
    size_t Write(const uint8_t* data, size_t len);
    size_t Write(const std::shared_ptr<Buffer>& buffer);
    static size_t Write(const std::string_view& fileNameUtf8, const std::shared_ptr<Buffer>& buffer);
    static size_t Write(const std::string_view& fileNameUtf8, const uint8_t* data, size_t len);
    static std::error_code DeleteFromStorage(const std::string_view& fileNameUtf8);
};

} // namespace RTC
