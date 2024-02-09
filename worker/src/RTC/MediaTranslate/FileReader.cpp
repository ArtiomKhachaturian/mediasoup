#define MS_CLASS "RTC::FileReader"
#include "RTC/MediaTranslate/FileReader.hpp"
#include "RTC/MediaTranslate/SimpleMemoryBuffer.hpp"
#include "Logger.hpp"

namespace RTC
{

class FileReader::StartEndNotifier
{
public:
    StartEndNotifier(FileReader* source);
    ~StartEndNotifier();
private:
    FileReader* const _source;
};

FileReader::FileReader(const std::string_view& fileNameUtf8,
                       uint32_t ssrc, bool loop, size_t chunkSize, int* error)
    : Base(fileNameUtf8, true, error)
    , _ssrc(ssrc)
    , _loop(loop)
    , _fileSize(GetFileSize(GetHandle()))
    , _chunkSize(std::min<size_t>(_fileSize, std::max(chunkSize, 1024UL)))
{
}

FileReader::~FileReader()
{
    Stop();
}

bool FileReader::IsOpen() const
{
    return Base::IsOpen() && _fileSize > 0LL;
}

void FileReader::OnSinkWasAdded(MediaSink* sink, bool first)
{
    Base::OnSinkWasAdded(sink, first);
    if (first && IsOpen() && !_thread.joinable()) {
        _thread = std::thread(std::bind(&FileReader::Run, this));
    }
}

void FileReader::OnSinkWasRemoved(MediaSink* sink, bool last)
{
    if (last) {
        Stop();
    }
    Base::OnSinkWasRemoved(sink, last);
}

std::vector<uint8_t> FileReader::ReadAllAsBinary(const std::string_view& fileNameUtf8)
{
    std::vector<uint8_t> data;
    if (auto handle = Base::Open(fileNameUtf8, true)) {
        auto size = GetFileSize(handle);
        if (size > 0) {
            try {
                data.resize(size);
                size = FileRead(handle, data);
                if (size != data.size()) {
                    data.resize(size);
                }
            }
            catch (const std::bad_alloc& e) {
                MS_ERROR_STD("unable to allocate %lld bytes for file data", size);
                data.clear();
            }
        }
        else {
            MS_WARN_DEV_STD("no data for read because file %s is empty", fileNameUtf8.c_str());
        }
        Close(handle);
    }
    return data;
}

std::shared_ptr<MemoryBuffer> FileReader::ReadAllAsBuffer(const std::string_view& fileNameUtf8)
{
    return SimpleMemoryBuffer::Create(ReadAllAsBinary(fileNameUtf8));
}

void FileReader::Run()
{
   if (!IsStopRequested()) {
        bool operationDone = false, ok = true;
        for(; (!operationDone || _loop) && !IsStopRequested();) {
            ok = ReadContent();
            if (ok) {
                operationDone = true;
                if (_loop) {
                    ok = SeekToStart();
                    if (!ok) { // seek to start
                        MS_WARN_DEV_STD("Failed seek to beginning of file");
                    }
                }
            }
            if (!ok) {
                break;
            }
        }
    }
    SeekToStart();
}

void FileReader::Stop()
{
    if (!_stopRequested.exchange(true)) {
        if (_thread.joinable()) {
            _thread.join();
        }
        _stopRequested = false;
    }
}

bool FileReader::ReadContent()
{
    if (!IsStopRequested()) {
        const StartEndNotifier notifier(this);
        bool eof = false, ok = true;
        while (!IsStopRequested() && ok && !eof) {
            if (const auto buffer = ReadBuffer(eof, ok)) {
                WriteMediaSinksPayload(GetSsrc(), buffer);
            }
        }
        if (!ok) {
            return false;
        }
    }
    return !IsStopRequested();
}

bool FileReader::SeekToStart()
{
    return FileSeek(GetHandle(), SEEK_SET, 0L);
}

std::shared_ptr<MemoryBuffer> FileReader::ReadBuffer(bool& eof, bool& ok) const
{
    std::shared_ptr<MemoryBuffer> buffer;
    if (const auto handle = GetHandle()) {
        std::vector<uint8_t> chunk;
        chunk.resize(_chunkSize, 0);
        const auto size = FileRead(handle, chunk);
        if (size) {
            chunk.resize(size);
            buffer = SimpleMemoryBuffer::Create(std::move(chunk));
        }
        else {
            ok = 0 == errno;
            if (!ok) {
                MS_WARN_DEV_STD("Unable to read file chunk, size %ul, error code %d", chunk.size(), errno);
            }
        }
        if (ok) {
            eof = _fileSize == FileTell(handle);
        }
    }
    else {
        eof = true;
    }
    return buffer;
}

int64_t FileReader::GetFileSize(FILE* handle)
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

int64_t FileReader::FileTell(FILE* handle)
{
    if (handle) {
#ifdef _MSC_VER
        return ::_ftelli64(handle)
#else
        return ::ftell(handle);
#endif
    }
    return -1;
}

bool FileReader::FileSeek(FILE* handle, int command, long offset)
{
    if (handle && offset >= 0L) {
#ifdef _MSC_VER
        return 0 == ::_fseeki64(handle, offset, command);
#else
        ::fseek(handle, offset, command);
        return true;
#endif
    }
    return false;
}

size_t FileReader::FileRead(FILE* handle, std::vector<uint8_t>& to)
{
    if (handle && !to.empty()) {
        return ::fread(to.data(), 1UL, to.size(), handle);
    }
    return 0UL;
}

FileReader::StartEndNotifier::StartEndNotifier(FileReader* source)
    : _source(source)
{
    _source->StartMediaSinksWriting(_source->GetSsrc());
}

FileReader::StartEndNotifier::~StartEndNotifier()
{
    _source->EndMediaSinksWriting(_source->GetSsrc());
}

} // namespace RTC
