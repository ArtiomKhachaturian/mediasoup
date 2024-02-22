#define MS_CLASS "RTC::FileWriter"
#include "RTC/MediaTranslate/FileWriter.hpp"
#include "RTC/MediaTranslate/Buffers/Buffer.hpp"
#include "Logger.hpp"
#include <stdio.h>
#ifdef _WIN32
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#if __has_include (<sys/syslimits.h>)
#include <sys/syslimits.h>
#elif !defined F_GETPATH
#define F_GETPATH 50
#endif
#endif

namespace {

class BufferView : public RTC::Buffer
{
public:
    BufferView(const std::vector<uint8_t>& buffer);
    // impl. of RTC::Buffer
    size_t GetSize() const final { return _buffer.size(); }
    uint8_t* GetData() final { return const_cast<uint8_t*>(_buffer.data()); }
    const uint8_t* GetData() const { return _buffer.data(); }
private:
    const std::vector<uint8_t>& _buffer;
};

}

namespace RTC
{

FileWriter::FileWriter(const std::string_view& fileNameUtf8, int* error)
    : Base(fileNameUtf8, false, error)
{
}

FileWriter::~FileWriter()
{
    Flush();
}

bool FileWriter::WriteAll(const std::string_view& fileNameUtf8, const std::vector<uint8_t>& buffer)
{
    if (!fileNameUtf8.empty() && !buffer.empty()) {
        return WriteAll(fileNameUtf8, std::make_shared<BufferView>(buffer));
    }
    return false;
}

bool FileWriter::WriteAll(const std::string_view& fileNameUtf8, const std::shared_ptr<Buffer>& buffer)
{
    bool written = false;
    if (buffer && !buffer->IsEmpty()) {
        if (const auto handle = Base::Open(fileNameUtf8, false)) {
            written = FileWrite(handle, buffer) == buffer->GetSize();
        }
    }
    return written;
}

bool FileWriter::DeleteFromStorage()
{
    bool ok = false;
    if (const auto handle = GetHandle()) {
#ifdef _WIN32
        char filePath[MAX_PATH] = { 0 };
        const auto fileHandle = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(handle.get())));
        if (INVALID_HANDLE_VALUE != fileHandle) {
            // TODO: get file name from WinAPI handle
        }
#else
        char filePath[PATH_MAX] = { 0 };
        ok = -1 != fcntl(fileno(handle.get()), F_GETPATH, filePath);
#endif
        if (ok) {
            Close();
            ok = 0 == ::remove(filePath);
        }
    }
    return ok;
}

bool FileWriter::Flush()
{
    const auto handle = GetHandle();
    return handle && 0 == ::fflush(handle.get());
}

void FileWriter::StartMediaWriting(const MediaObject& sender)
{
    MediaSink::StartMediaWriting(sender);
    if (const auto handle = GetHandle()) { // truncate file
#ifdef _WIN32
        ::_chsize_s(_fileno(handle.get()), 0LL);
#else
        ::ftruncate(fileno(handle.get()), 0);
#endif
    }
}

void FileWriter::WriteMediaPayload(const MediaObject&, const std::shared_ptr<Buffer>& buffer)
{
    if (const auto handle = GetHandle()) {
        if (buffer && !buffer->IsEmpty()) {
            const auto expected = buffer->GetSize();
            const auto actual = FileWrite(handle, buffer);
            if (expected != actual) {
                MS_ERROR_STD("file write error, expected %lu but "
                             "written only %lu bytes, error code: %d",
                             expected, actual, errno);
            }
        }
    }
}

void FileWriter::EndMediaWriting(const MediaObject& sender)
{
    MediaSink::EndMediaWriting(sender);
    Flush();
}

size_t FileWriter::FileWrite(const std::shared_ptr<FILE>& handle,
                             const std::shared_ptr<Buffer>& buffer)
{
    if (handle && buffer) {
        return ::fwrite(buffer->GetData(), 1U, buffer->GetSize(), handle.get());
    }
    return 0UL;
}

} // namespace RTC

namespace {

BufferView::BufferView(const std::vector<uint8_t>& buffer)
    : _buffer(buffer)
{
}

}
