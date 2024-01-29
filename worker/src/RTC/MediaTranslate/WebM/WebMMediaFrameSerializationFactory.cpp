#include "RTC/MediaTranslate/WebM/WebMMediaFrameSerializationFactory.hpp"
#include "RTC/MediaTranslate/WebM/WebMSerializer.hpp"
#include "RTC/MediaTranslate/WebM/WebMDeserializer.hpp"
#include "RTC/MediaTranslate/WebM/WebMBuffersReader.hpp"
#ifdef USE_TEST_FILE_FOR_DESERIALIZATION
#include "Logger.hpp"
#include <mkvparser/mkvreader.h>
#endif

#ifdef USE_TEST_FILE_FOR_DESERIALIZATION

#define MS_CLASS "RTC::WebMMediaFrameSerializationFactory"

namespace {

using namespace RTC;

class FileReader : public MkvReader
{
public:
    FileReader(std::unique_ptr<mkvparser::MkvReader> fileReader);
    ~FileReader() final = default;
    static std::unique_ptr<MkvReader> Create(const char* filename);
    // impl. of MkvReader
    MediaFrameDeserializeResult AddBuffer(const std::shared_ptr<const MemoryBuffer>& buffer) final;
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
    bool loopback = false;
#ifdef USE_TEST_FILE_FOR_DESERIALIZATION
    auto reader = FileReader::Create(_testFileName);
    if (!reader) {
        MS_ERROR("failed to load %s test file for WebM deserializer", _testFileName);
        reader = std::make_unique<WebMBuffersReader>();
    }
    else {
        MS_DEBUG_TAG(info, "test file %s for WebM deserializer loaded successfully", _testFileName);
        loopback = true;
    }
#else
    auto reader = std::make_unique<WebMBuffersReader>();
#endif
    return std::make_unique<WebMDeserializer>(std::move(reader), loopback);
}

} // namespace RTC

#ifdef USE_TEST_FILE_FOR_DESERIALIZATION

namespace {

FileReader::FileReader(std::unique_ptr<mkvparser::MkvReader> fileReader)
    : _fileReader(std::move(fileReader))
{
}

std::unique_ptr<MkvReader> FileReader::Create(const char* filename)
{
    if (filename) {
        auto fileReader = std::make_unique<mkvparser::MkvReader>();
        if (0L == fileReader->Open(filename)) {
            return std::make_unique<FileReader>(std::move(fileReader));
        }
    }
    return nullptr;
}

MediaFrameDeserializeResult FileReader::AddBuffer(const std::shared_ptr<const MemoryBuffer>& /*buffer*/)
{
    return MediaFrameDeserializeResult::Success;
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
