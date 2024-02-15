#define MS_CLASS "mediasoup-worker"
// #define MS_LOG_DEV_LEVEL 3

#include "MediaSoupErrors.hpp"
#include "lib.hpp"
#include <cstdlib> // std::_Exit()
#include <string>
#include <unistd.h>

static constexpr int ConsumerChannelFd{ 3 };
static constexpr int ProducerChannelFd{ 4 };


/*#include "RTC/MediaTranslate/MediaTimer/MediaTimer.hpp"
#include "RTC/MediaTranslate/MediaTimer/MediaTimerCallback.hpp"
#include <thread>

class FakeCallBack : public RTC::MediaTimerCallback
{
public:
    FakeCallBack(RTC::MediaTimer* timer) : _timer(timer) {}
    void SetTimerId(uint64_t timerId) { _timerId = timerId; }
    void OnEvent() final {
        MS_ERROR_STD("FakeCallBack::OnEvent: %zu", _counter++);
        _timer->SetTimeout(_timerId, 2000);
    }
private:
    RTC::MediaTimer* _timer;
    uint64_t _timerId = 0UL;
    size_t _counter = 1UL;
};*/

int main(int argc, char* argv[])
{
    sleep(10); // TODO: remove this sleep for production
    /*{
        RTC::MediaTimer timer("testTimer");
        auto callback = std::make_shared<FakeCallBack>(&timer);
        if (const auto timerId = timer.RegisterTimer(callback)) {
            callback->SetTimerId(timerId);
            timer.SetTimeout(timerId, 0);
            MS_ERROR_STD("MediaTimer::Start");
            timer.Start(timerId, false);
            sleep(10); // TODO: remove this sleep for production
            timer.UnregisterTimer(timerId);
        }
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
