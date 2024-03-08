#ifndef MS_RTC_RATE_CALCULATOR_HPP
#define MS_RTC_RATE_CALCULATOR_HPP

#include "common.hpp"
#include "DepLibUV.hpp"
#include <atomic>
#include <mutex>

namespace RTC
{
	// It is considered that the time source increases monotonically.
	// ie: the current timestamp can never be minor than a timestamp in the past.
	class RateCalculator
	{
	public:
		static constexpr size_t DefaultWindowSize{ 1000u };
		static constexpr float DefaultBpsScale{ 8000.0f };
		static constexpr uint16_t DefaultWindowItems{ 100u };

	public:
		explicit RateCalculator(
		  size_t windowSizeMs  = DefaultWindowSize,
		  float scale          = DefaultBpsScale,
          uint16_t windowItems = DefaultWindowItems);
        RateCalculator(RateCalculator&& other);
		void Update(size_t size, uint64_t nowMs);
		uint32_t GetRate(uint64_t nowMs);
        size_t GetBytes() const;

	private:
		void RemoveOldData(uint64_t nowMs);
        void Reset();

	private:
		struct BufferItem
		{
			size_t count{ 0u };
			uint64_t time{ 0u };
		};

	private:
		// Window Size (in milliseconds).
		const size_t windowSizeMs;
		// Scale in which the rate is represented.
		const float scale;
        // Window Size (number of items).
        const uint16_t windowItems;
        // Item Size (in milliseconds), calculated as: windowSizeMs / windowItems.
        const size_t itemSizeMs;
        // to protect below declared members
        mutable std::mutex mutex;
        // Total bytes transmitted.
        std::atomic<size_t> bytes{ 0u };
		// Buffer to keep data.
		std::vector<BufferItem> buffer;
		// Time (in milliseconds) for last item in the time window.
		uint64_t newestItemStartTime{ 0u };
		// Index for the last item in the time window.
		int32_t newestItemIndex{ -1 };
		// Time (in milliseconds) for oldest item in the time window.
		uint64_t oldestItemStartTime{ 0u };
		// Index for the oldest item in the time window.
		int32_t oldestItemIndex{ -1 };
		// Total count in the time window.
		size_t totalCount{ 0u };
		// Last value calculated by GetRate().
		uint32_t lastRate{ 0u };
		// Last time GetRate() was called.
		uint64_t lastTime{ 0u };
	};

	class RtpDataCounter
	{
	public:
		explicit RtpDataCounter(size_t windowSizeMs = 2500) : rate(windowSizeMs)
		{
		}
        RtpDataCounter(RtpDataCounter&& other);
	public:
		void Update(size_t packetSize);
		uint32_t GetBitrate(uint64_t nowMs)
		{
			return this->rate.GetRate(nowMs);
		}
		size_t GetPacketCount() const
		{
			return this->packets;
		}
		size_t GetBytes() const
		{
			return this->rate.GetBytes();
		}

	private:
		RateCalculator rate;
		std::atomic<size_t> packets{ 0u };
	};
} // namespace RTC

#endif
