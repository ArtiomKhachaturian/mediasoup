#define MS_CLASS "RTC::RateCalculator"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/RateCalculator.hpp"
#include "Logger.hpp"
#include <cmath> // std::trunc()

namespace RTC
{
    RateCalculator::RateCalculator(
      size_t windowSizeMs,
      float scale,
      uint16_t windowItems)
      : windowSizeMs(windowSizeMs), scale(scale), windowItems(std::max<size_t>(1, windowItems)),
        itemSizeMs(std::max<size_t>(windowSizeMs / this->windowItems, 1))
    {
        this->buffer.resize(this->windowItems);
    }

    RateCalculator::RateCalculator(RateCalculator&& other)
        : windowSizeMs(other.windowSizeMs)
        , scale(other.scale)
        , windowItems(other.windowItems)
        , itemSizeMs(other.itemSizeMs)
        , bytes(other.bytes.load())
    {
        this->buffer = std::move(other.buffer);
        this->newestItemStartTime = other.newestItemStartTime;
        this->newestItemIndex = other.newestItemIndex;
        this->oldestItemStartTime = other.oldestItemStartTime;
        this->oldestItemIndex = other.oldestItemIndex;
        this->totalCount = other.totalCount;
        this->lastRate = other.lastRate;
        this->lastTime = other.lastTime;
    }

	void RateCalculator::Update(size_t size, uint64_t nowMs)
	{
		MS_TRACE();

        {
            const std::lock_guard<std::mutex> guard(this->mutex);
            // Ignore too old data. Should never happen.
            if (nowMs < this->oldestItemStartTime)
            {
                return;
            }
        }

		// Increase bytes.
		this->bytes.fetch_add(size);

		RemoveOldData(nowMs);

        const std::lock_guard<std::mutex> guard(this->mutex);
		// If the elapsed time from the newest item start time is greater than the
		// item size (in milliseconds), increase the item index.
		if (this->newestItemIndex < 0 || nowMs - this->newestItemStartTime >= this->itemSizeMs)
		{
			this->newestItemIndex++;
			this->newestItemStartTime = nowMs;

			if (this->newestItemIndex >= this->windowItems)
			{
				this->newestItemIndex = 0;
			}

			MS_ASSERT(
			  this->newestItemIndex != this->oldestItemIndex || this->oldestItemIndex == -1,
			  "newest index overlaps with the oldest one");

			// Set the newest item.
			BufferItem& item = this->buffer[this->newestItemIndex];
			item.count       = size;
			item.time        = nowMs;
		}
		else
		{
			// Update the newest item.
			BufferItem& item = this->buffer[this->newestItemIndex];
			item.count += size;
		}

		// Set the oldest item index and time, if not set.
		if (this->oldestItemIndex < 0)
		{
			this->oldestItemIndex     = this->newestItemIndex;
			this->oldestItemStartTime = nowMs;
		}

		this->totalCount += size;

		// Reset lastRate and lastTime so GetRate() will calculate rate again even
		// if called with same now in the same loop iteration.
		this->lastRate = 0;
		this->lastTime = 0;
	}

	uint32_t RateCalculator::GetRate(uint64_t nowMs)
	{
		MS_TRACE();
        
        {
            const std::lock_guard<std::mutex> guard(this->mutex);
            
            if (nowMs == this->lastTime)
            {
                return this->lastRate;
            }
        }

		RemoveOldData(nowMs);

		const float scale = this->scale / this->windowSizeMs;

        const std::lock_guard<std::mutex> guard(this->mutex);
        
		this->lastTime = nowMs;
		this->lastRate = static_cast<uint32_t>(std::trunc(this->totalCount * scale + 0.5f));

		return this->lastRate;
	}

    size_t RateCalculator::GetBytes() const
    {
        return this->bytes;
    }

	inline void RateCalculator::RemoveOldData(uint64_t nowMs)
	{
		MS_TRACE();
        
        std::unique_lock<std::mutex> guard(this->mutex);
		// No item set.
		if (this->newestItemIndex < 0 || this->oldestItemIndex < 0)
		{
			return;
		}

		const uint64_t newOldestTime = nowMs - this->windowSizeMs;

		// Oldest item already removed.
		if (newOldestTime < this->oldestItemStartTime)
		{
			return;
		}

		// A whole window size time has elapsed since last entry. Reset the buffer.
		if (newOldestTime >= this->newestItemStartTime)
		{
            guard.unlock();
            
			Reset();

			return;
		}

		while (newOldestTime >= this->oldestItemStartTime)
		{
			BufferItem& oldestItem = this->buffer[this->oldestItemIndex];
			this->totalCount -= oldestItem.count;
			oldestItem.count = 0u;
			oldestItem.time  = 0u;

			if (++this->oldestItemIndex >= this->windowItems)
			{
				this->oldestItemIndex = 0;
			}

			const BufferItem& newOldestItem = this->buffer[this->oldestItemIndex];
			this->oldestItemStartTime       = newOldestItem.time;
		}
	}

    void RateCalculator::Reset()
    {
        const std::lock_guard<std::mutex> guard(this->mutex);
        
        std::memset(static_cast<void*>(&this->buffer.front()), 0, sizeof(BufferItem) * this->buffer.size());

        this->newestItemStartTime = 0u;
        this->newestItemIndex     = -1;
        this->oldestItemStartTime = 0u;
        this->oldestItemIndex     = -1;
        this->totalCount          = 0u;
        this->lastRate            = 0u;
        this->lastTime            = 0u;
    }


    RtpDataCounter::RtpDataCounter(RtpDataCounter&& other)
        : rate(std::move(other.rate))
        , packets(other.GetPacketCount())
    {
    }

	void RtpDataCounter::Update(size_t packetSize)
	{
		const uint64_t nowMs = DepLibUV::GetTimeMs();

		this->packets++;
		this->rate.Update(packetSize, nowMs);
	}
} // namespace RTC
