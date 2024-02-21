#define MS_CLASS "mediasoup-worker"
// #define MS_LOG_DEV_LEVEL 3

#include "MediaSoupErrors.hpp"
#include "lib.hpp"
#include <cstdlib> // std::_Exit()
#include <string>

/*#include "RTC/MediaTranslate/Websocket/WebsocketTppFactory.hpp"
#include "RTC/MediaTranslate/Websocket/Websocket.hpp"
#include "RTC/MediaTranslate/Websocket/WebsocketListener.hpp"
#include "RTC/MediaTranslate/Websocket/WebsocketState.hpp"
#include "RTC/MediaTranslate/Websocket/WebsocketFailure.hpp"*/

static constexpr int ConsumerChannelFd{ 3 };
static constexpr int ProducerChannelFd{ 4 };

int main(int argc, char* argv[])
{
    /*RTC::WebsocketTppTestFactory factory;
    if (auto socket = factory.Create()) {
        auto listener = RTC::WebsocketListener::Create([&socket](uint64_t, RTC::WebsocketState state) {
            MS_ERROR_STD("state changed to %s", RTC::ToString(state));
            if (RTC::WebsocketState::Connected == state) {
                socket->WriteText("test");
                socket->WriteBinary({'s', 'h', 'a', '-', '2', '5', '6'});
            }
            
        }, [](uint64_t, RTC::WebsocketFailure failure, const std::string& what) {
            MS_ERROR_STD("failure %s: %s", RTC::ToString(failure), what.c_str());
        });
        socket->AddListener(listener);
        if (!socket->Open()) {
            MS_ERROR_STD("Failed to open websocket");
        }
        else {
            sleep(200);
        }
        socket->RemoveListener(listener);
        delete listener;
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
