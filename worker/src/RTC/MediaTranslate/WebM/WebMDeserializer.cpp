#include "RTC/MediaTranslate/WebM/WebMDeserializer.hpp"
#include "RTC/MediaTranslate/WebM/WebMDeserializedTrack.hpp"

namespace RTC
{

WebMDeserializer::WebMDeserializer(const std::shared_ptr<BufferAllocator>& allocator)
    : MediaFrameDeserializer(allocator)
    , _reader(allocator)
{
}

WebMDeserializer::~WebMDeserializer()
{
    WebMDeserializer::Clear();
}

void WebMDeserializer::Clear()
{
    MediaFrameDeserializer::Clear();
    _reader.ClearBuffers();
}

MediaFrameDeserializeResult WebMDeserializer::AddBuffer(std::shared_ptr<Buffer> buffer)
{
    return FromMkvReadResult(_reader.AddBuffer(std::move(buffer)));
}

void WebMDeserializer::ParseTracksInfo()
{
    if (const auto tracks = _reader.GetTracks()) {
        for (unsigned long i = 0UL, end = tracks->GetTracksCount(); i < end; ++i) {
            if (auto track = WebMDeserializedTrack::Create(tracks, i, GetAllocator())) {
                const auto& type = track->GetMime();
                AddTrack(type, std::move(track));
            }
        }
    }
}

} // namespace RTC
