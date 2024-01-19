#include "RTC/MediaTranslate/WebM/WebMMediaFrameSerializationFactory.hpp"
#include "RTC/MediaTranslate/WebM/WebMSerializer.hpp"
#include "RTC/MediaTranslate/WebM/WebMDeserializer.hpp"
#include "RTC/MediaTranslate/WebM/WebMBuffersReader.hpp"
#ifdef USE_TEST_FILE_FOR_DESERIALIZATION
#include <mkvparser/mkvreader.h>
#endif

#ifdef USE_TEST_FILE_FOR_DESERIALIZATION
namespace {

class FileReader : public RTC::MkvReader
{
public:
    FileReader(std::unique_ptr<mkvparser::MkvReader> fileReader);
    ~FileReader() final = default;
    static std::unique_ptr<RTC::MkvReader> Create(const char* filename);
    // impl. of MkvReader
    bool AddBuffer(const std::shared_ptr<const RTC::MemoryBuffer>& /*buffer*/) final { return true; }
    // impl. of mkvparser::IMkvReader
    int Read(long long pos, long len, unsigned char* buf) final;
    int Length(long long* total, long long* available) final;
private:
    const std::unique_ptr<mkvparser::MkvReader> _fileReader;
};

}
#endif

namespace RTC
{

std::unique_ptr<MediaFrameSerializer> WebMMediaFrameSerializationFactory::CreateSerializer()
{
    return std::make_unique<WebMSerializer>();
}

std::unique_ptr<MediaFrameDeserializer> WebMMediaFrameSerializationFactory::CreateDeserializer()
{
#ifdef USE_TEST_FILE_FOR_DESERIALIZATION
    auto reader = FileReader::Create("/Users/user/Downloads/1b0cefc4-abdb-48d0-9c50-f5050755be94.webm");
    if (!reader) {
        reader = std::make_unique<WebMBuffersReader>();
    }
#else
    auto reader = std::make_unique<WebMBuffersReader>();
#endif
    return std::make_unique<WebMDeserializer>(std::move(reader));
}

} // namespace RTC

#ifdef USE_TEST_FILE_FOR_DESERIALIZATION

namespace {

FileReader::FileReader(std::unique_ptr<mkvparser::MkvReader> fileReader)
    : _fileReader(std::move(fileReader))
{
}

std::unique_ptr<RTC::MkvReader> FileReader::Create(const char* filename)
{
    if (filename) {
        auto fileReader = std::make_unique<mkvparser::MkvReader>();
        if (0L == fileReader->Open(filename)) {
            return std::make_unique<FileReader>(std::move(fileReader));
        }
    }
    return nullptr;
}

int FileReader::Read(long long pos, long len, unsigned char* buf)
{
    return _fileReader->Read(pos, len, buf);
}

int FileReader::Length(long long* total, long long* available)
{
    return _fileReader->Length(total, available);
}

}

#endif
