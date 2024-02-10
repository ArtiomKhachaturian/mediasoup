#pragma once
#include "RTC/MediaTranslate/FileDevice.hpp"
#include "RTC/MediaTranslate/MediaSink.hpp"

namespace RTC
{

class FileWriter : public FileDevice<MediaSink>
{
public:
    FileWriter(const std::string_view& fileNameUtf8, int* error = nullptr);
    ~FileWriter() final;
    static bool WriteAll(const std::string_view& fileNameUtf8, const std::vector<uint8_t>& buffer);
    static bool WriteAll(const std::string_view& fileNameUtf8, const std::shared_ptr<MemoryBuffer>& buffer);
    bool DeleteFromStorage();
    // Write any buffered data to the underlying file. Returns true on success,
    // false on write error. Note: Flushing when closing, is not required.
    bool Flush();
    // impl. of MediaSink
    void WriteMediaPayload(uint32_t ssrc, const std::shared_ptr<MemoryBuffer>& buffer) final;
private:
    static size_t FileWrite(FILE* handle, const std::shared_ptr<MemoryBuffer>& buffer);
};

} // namespace RTC
