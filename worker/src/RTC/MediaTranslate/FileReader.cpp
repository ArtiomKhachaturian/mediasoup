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

FileReader::~FileReader()
{
}

bool FileReader::Open(const std::string_view& fileNameUtf8, int* error)
{
    return Base::Open(fileNameUtf8, true, error);
}

std::shared_ptr<Buffer> FileReader::ReadAll() const
{
    if (const auto handle = GetHandle()) {
        auto size = GetFileSize(handle);
        if (size > 0) {
            try {
                if (auto buffer = AllocateBuffer(size)) {
                    size = FileRead(handle, buffer);
                    return ReallocateBuffer(size, std::move(buffer));
                }
            }
            catch (const std::bad_alloc& e) {
                MS_ERROR_STD("unable to allocate %" PRId64 " bytes for file data", size);
            }
        }
        else {
            MS_WARN_DEV_STD("no data for read because file %s is empty", fileNameUtf8.c_str());
        }
    }
    return nullptr;
}

bool FileReader::IsValidForRead(const std::string_view& fileNameUtf8)
{
    bool valid = false;
    if (const auto handle = Base::OpenFile(fileNameUtf8, true)) {
        auto size = GetFileSize(handle);
        valid = size > 0;
    }
    return valid;
}

int64_t FileReader::FileTell(const std::shared_ptr<FILE>& handle)
{
    if (handle) {
#ifdef _MSC_VER
        return ::_ftelli64(handle.get());
#else
        return ::ftell(handle.get());
#endif
    }
    return -1;
}

bool FileReader::FileSeek(const std::shared_ptr<FILE>& handle, int command, long offset)
{
    if (handle && offset >= 0L) {
#ifdef _MSC_VER
        return 0 == ::_fseeki64(handle.get(), offset, command);
#else
        ::fseek(handle.get(), offset, command);
        return true;
#endif
    }
    return false;
}

int64_t FileReader::GetFileSize(const std::shared_ptr<FILE>& handle)
{
    int64_t len = 0LL;
    if (handle && FileSeek(handle, SEEK_END)) {
        len = FileTell(handle);
        if (len < 0LL) {
            len = 0LL;
        }
        if (!FileSeek(handle, SEEK_SET)) {
            len = 0LL;
        }
    }
    return len;
}

size_t FileReader::FileRead(const std::shared_ptr<FILE>& handle, const std::shared_ptr<Buffer>& to)
{
    if (handle && to && !to->IsEmpty()) {
        return ::fread(to->GetData(), 1UL, to->GetSize(), handle.get());
    }
    return 0UL;
}

} // namespace RTC
