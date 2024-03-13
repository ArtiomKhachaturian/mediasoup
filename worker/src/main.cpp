#define MS_CLASS "mediasoup-worker"
// #define MS_LOG_DEV_LEVEL 3

#include "MediaSoupErrors.hpp"
#include "lib.hpp"
#include <cstdlib> // std::_Exit()
#include <string>

static constexpr int ConsumerChannelFd{ 3 };
static constexpr int ProducerChannelFd{ 4 };

#include "RTC/Buffers/PoolAllocator.hpp"
#include <iostream>
#include <chrono>
#include <cstdlib> // Для генерации случайных чисел
#include <ctime>   // Для инициализации генератора случайных чисел

int main(int argc, char* argv[])
{
    RTC::PoolAllocator manager;

    std::srand(static_cast<unsigned>(std::time(nullptr)));

    const size_t numAllocations = 1000000;
    auto start = std::chrono::high_resolution_clock::now();
    size_t summarySize = 0U;
    for (size_t i = 0; i < numAllocations; ++i) {
        const size_t size = std::rand() % 28000 + 1;
        auto ptr = manager.Allocate(size);
        summarySize += size;
    }
    auto end = std::chrono::high_resolution_clock::now();

    // Вычисляем время выполнения в микросекундах
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "Allocated " << summarySize << " bytes: " << duration << " milliseconds" << std::endl;
    
    start = std::chrono::high_resolution_clock::now();
    manager.PurgeGarbage();
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "Purge time: " << duration << " milliseconds" << std::endl;


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
