#pragma once
#include "RTC/Buffers/BufferAllocations.hpp"
#include "RTC/MediaTranslate/FileDevice.hpp"

namespace RTC
{

class FileReader : public FileDevice<BufferAllocations<void>>
{
    using Base = FileDevice<BufferAllocations<void>>;
public:
	FileReader(const std::shared_ptr<BufferAllocator>& allocator = nullptr);
    ~FileReader() final;
    std::shared_ptr<Buffer> ReadAll() const;
    static bool IsValidForRead(const std::string_view& fileNameUtf8);
    bool Open(const std::string_view& fileNameUtf8, int* error = nullptr);
private:
    static int64_t FileTell(const std::shared_ptr<FILE>& handle);
    static bool FileSeek(const std::shared_ptr<FILE>& handle, int command, long offset = 0L);
    static int64_t GetFileSize(const std::shared_ptr<FILE>& handle);
    static size_t FileRead(const std::shared_ptr<FILE>& handle, const std::shared_ptr<Buffer>& to);
};

} // namespace RTC
