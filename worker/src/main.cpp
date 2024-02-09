#define MS_CLASS "mediasoup-worker"
// #define MS_LOG_DEV_LEVEL 3

#include "MediaSoupErrors.hpp"
#include "lib.hpp"
#include <cstdlib> // std::_Exit()
#include <string>
#include <unistd.h>

//#include "RTC/MediaTranslate/RtpPacketsPlayer/MediaTimer.hpp"
//#include "RTC/MediaTranslate/RtpPacketsPlayer/MediaTimerCallback.hpp"

static constexpr int ConsumerChannelFd{ 3 };
static constexpr int ProducerChannelFd{ 4 };

/*class FakeCallBack : public RTC::MediaTimerCallback
{
public:
    FakeCallBack(RTC::MediaTimer* timer) : _timer(timer) {}
    void SetTimerId(uint64_t timerId) { _timerId = timerId; }
    void OnEvent() final {
        MS_ERROR_STD("FakeCallBack::OnEvent: %zu", _counter++);
        _timer->Schedule(_timerId, 20, true);
    }
private:
    RTC::MediaTimer* _timer;
    uint64_t _timerId = 0UL;
    size_t _counter = 1UL;
};*/

int main(int argc, char* argv[])
{
    /*RTC::MediaTimer timer;
    auto callback = std::make_shared<FakeCallBack>(&timer);
    if (const auto timerId = timer.RegisterTimer(callback)) {
        callback->SetTimerId(timerId);
        timer.Schedule(timerId, 0, true);
        sleep(100); // TODO: remove this sleep for production
        timer.UnregisterTimer(timerId);
    }*/
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
