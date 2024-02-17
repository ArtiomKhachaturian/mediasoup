#pragma once
#include "RTC/MediaTranslate/FileDevice.hpp"
#include "RTC/MediaTranslate/MediaSink.hpp"
#include <memory>
#include <string_view>
#include <vector>

namespace RTC
{

class FileWriter : public FileDevice<MediaSink>
{
    using Base = FileDevice<MediaSink>;
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
    void StartMediaWriting(const MediaObject& sender) final;
    void WriteMediaPayload(const MediaObject&, const std::shared_ptr<MemoryBuffer>& buffer) final;
    void EndMediaWriting(const MediaObject& sender) final;
private:
    static size_t FileWrite(const std::shared_ptr<FILE>& handle, const std::shared_ptr<MemoryBuffer>& buffer);
};

} // namespace RTC
