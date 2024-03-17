#pragma once
#include "RTC/Buffers/BufferAllocations.hpp"
#include "RTC/MediaTranslate/FileDevice.hpp"

namespace RTC
{

class FileReader : public BufferAllocations<FileDevice>
{
    using Base = BufferAllocations<FileDevice>;
public:
	FileReader(const std::shared_ptr<BufferAllocator>& allocator = nullptr);
    FileReader(FileReader&& tmp);
    ~FileReader() final;
    FileReader& operator = (FileReader&& tmp) = delete;
    bool Open(const std::string_view& fileNameUtf8);
    bool IsEOF() const;
    std::shared_ptr<Buffer> ReadAll();
    std::shared_ptr<Buffer> Read(size_t size);
    static bool IsReadable(const std::string_view& fileNameUtf8);
private:
    size_t Read(FILE* handle, const std::shared_ptr<Buffer>& to);
    bool Seek(FILE* handle, int command, long offset = 0L);
    int64_t Tell(FILE* handle);
    int64_t GetFileSize(FILE* handle);
};

} // namespace RTC
