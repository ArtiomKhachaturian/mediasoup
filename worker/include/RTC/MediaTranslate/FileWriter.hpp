#pragma once
#include "RTC/MediaTranslate/FileDevice.hpp"
#include "RTC/MediaTranslate/MediaSink.hpp"

namespace RTC
{

class FileWriter : public FileDevice<MediaSink>
{
    using Base = FileDevice<MediaSink>;
public:
    FileWriter() = default;
    ~FileWriter() final;
    static bool WriteAll(const std::string_view& fileNameUtf8, const std::shared_ptr<Buffer>& buffer);
    static bool WriteAll(const std::string_view& fileNameUtf8, const uint8_t* data, size_t len);
    bool DeleteFromStorage();
    bool Open(const std::string_view& fileNameUtf8, int* error = nullptr);
    // Write any buffered data to the underlying file. Returns true on success,
    // false on write error. Note: Flushing when closing, is not required.
    bool Flush();
    // impl. of MediaSink
    void StartMediaWriting(uint64_t senderId) final;
    void WriteMediaPayload(uint64_t senderId, const std::shared_ptr<Buffer>& buffer) final;
    void EndMediaWriting(uint64_t senderId) final;
private:
    static size_t Write(const std::shared_ptr<FILE>& handle, const uint8_t* data, size_t len);
};

} // namespace RTC
