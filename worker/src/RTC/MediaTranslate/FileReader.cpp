#define MS_CLASS "RTC::FileReader"
#include "RTC/MediaTranslate/FileReader.hpp"
#include "Logger.hpp"
#include <inttypes.h>

namespace RTC
{

FileReader::FileReader(const std::shared_ptr<BufferAllocator>& allocator)
    : Base(allocator)
{
}

FileReader::FileReader(FileReader&& tmp)
    : Base(tmp.GetAllocator(), std::move(tmp))
{
}

FileReader::~FileReader()
{
}

bool FileReader::Open(const std::string_view& fileNameUtf8)
{
    return Base::Open(fileNameUtf8, true);
}

bool FileReader::IsEOF() const
{
    if (const auto handle = GetHandle()) {
        return 0 != ::feof(handle);
    }
    return false;
}

std::shared_ptr<Buffer> FileReader::ReadAll()
{
    return Read(GetFileSize(GetHandle()));
}

std::shared_ptr<Buffer> FileReader::Read(size_t size)
{
    if (size) {
        if (const auto handle = GetHandle()) {
            try {
                if (auto buffer = AllocateBuffer(size)) {
                    size = Read(handle, buffer);
                    return ReallocateBuffer(size, std::move(buffer));
                }
            }
            catch (const std::bad_alloc& e) {
                MS_ERROR_STD("unable to allocate %zu bytes for file data", size);
            }
        }
    }
    return nullptr;
}

bool FileReader::IsReadable(const std::string_view& fileNameUtf8)
{
    FileReader reader;
    return reader.Open(fileNameUtf8);
}

size_t FileReader::Read(FILE* handle, const std::shared_ptr<Buffer>& to)
{
    size_t res = 0;
    if (handle && to && !to->IsEmpty()) {
        res = ::fread(to->GetData(), 1UL, to->GetSize(), handle);
        SetError(errno);
    }
    return res;
}

bool FileReader::Seek(FILE* handle, int command, long offset)
{
    if (handle) {
#ifdef _MSC_VER
        return 0 == SetError(::_fseeki64(handle, offset, command));
#else
        return 0 == SetError(::fseek(handle, offset, command));
#endif
    }
    return false;
}

int64_t FileReader::Tell(FILE* handle)
{
    if (handle) {
#ifdef _MSC_VER
        const auto res = ::_ftelli64(handle);
#else
        const auto res = ::ftell(handle);
#endif
        SetError(errno);
        return res;
    }
    return -1;
}

int64_t FileReader::GetFileSize(FILE* handle)
{
    int64_t len = 0LL;
    if (handle && Seek(handle, SEEK_END)) {
        len = Tell(handle);
        if (len < 0LL) {
            len = 0LL;
        }
        if (!Seek(handle, SEEK_SET)) {
            len = 0LL;
        }
    }
    return len;
}

} // namespace RTC
