#define MS_CLASS "mediasoup-worker"
// #define MS_LOG_DEV_LEVEL 3

#include "MediaSoupErrors.hpp"
#include "lib.hpp"
#include <cstdlib> // std::_Exit()
#include <string>

#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerMediaFragment.hpp"
#include "RTC/MediaTranslate/RtpPacketsPlayer/RtpPacketsPlayerCallback.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimer.hpp"
#include "RTC/MediaTranslate/FileReader.hpp"
#include "RTC/MediaTranslate/TranslatorDefines.hpp"
#include "RTC/MediaTranslate/TranslatorUtils.hpp"
#include "RTC/Timestamp.hpp"
#include <chrono>
#include <list>

using namespace RTC;

class FakeMediaSource : public MediaSourceImpl
{
public:
    FakeMediaSource() = default;
};

class TestMediaSink : public RtpPacketsPlayerCallback
{
public:
    TestMediaSink() = default;
    virtual void OnPlayStarted(uint32_t /*ssrc*/, uint64_t /*mediaId*/,
                               uint64_t /*mediaSourceId*/) {
        MS_ERROR_STD("OnPlayStarted at %s", GetCurrentTime().c_str());
        _current = GetCurrentTs();
    }
    void OnPlay(const Timestamp& timestampOffset, RtpPacket* packet,
                        uint64_t mediaId, uint64_t mediaSourceId) {
        const auto now = GetCurrentTs();
        MS_ERROR_STD("OnPlay %u - %u, delta is %lld ms",
                    timestampOffset.GetRtpTime(),
                    timestampOffset.GetTime().ms<uint32_t>(),
                    (now - _current).count());
        _current = now;
    }
    virtual void OnPlayFinished(uint32_t /*ssrc*/, uint64_t /*mediaId*/,
                                uint64_t /*mediaSourceId*/) {
        MS_ERROR_STD("OnPlayFinished at %s", GetCurrentTime().c_str());
    }
private:
    static std::chrono::milliseconds GetCurrentTs() {
        using namespace std::chrono;
        return duration_cast<milliseconds>(system_clock::now().time_since_epoch());
    }
private:
    std::chrono::milliseconds _current;
};

static constexpr int ConsumerChannelFd{ 3 };
static constexpr int ProducerChannelFd{ 4 };

int main(int argc, char* argv[])
{
    if (const auto buffer = FileReader::ReadAll(MOCK_WEBM_INPUT_FILE)) {
        auto timer = std::make_shared<MediaTimer>();
        TestMediaSink sink;
        if (auto fragment = RtpPacketsPlayerMediaFragment::Parse(buffer, timer, &sink)) {
            fragment->Start(0, 0, 48000, 100, 0, 0);
            sleep(1100);
        }
    }
	// Ensure we are called by our Node library.
	if (!std::getenv("MEDIASOUP_VERSION"))
	{
		MS_ERROR_STD("you don't seem to be my real father!");

		// 41 is a custom exit code to notify about "missing MEDIASOUP_VERSION" env.
		std::_Exit(41);
	}

	const std::string version = std::getenv("MEDIASOUP_VERSION");

	auto statusCode = mediasoup_worker_run(
		argc, argv, version.c_str(), ConsumerChannelFd, ProducerChannelFd, nullptr, nullptr, nullptr, nullptr);

	std::_Exit(statusCode);
}
