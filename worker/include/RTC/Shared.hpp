#ifndef MS_RTC_SHARED_HPP
#define MS_RTC_SHARED_HPP

#include "ChannelMessageRegistrator.hpp"
#include "Channel/ChannelNotifier.hpp"
#include "RTC/Buffers/BufferAllocations.hpp"

namespace RTC
{
	class Shared : public BufferAllocations<void>
	{
	public:
		explicit Shared(
          ChannelMessageRegistrator* channelMessageRegistrator,
		  Channel::ChannelNotifier* channelNotifier,
          const std::shared_ptr<BufferAllocator>& allocator = nullptr);
		~Shared();

	public:
		ChannelMessageRegistrator* channelMessageRegistrator{ nullptr };
		Channel::ChannelNotifier* channelNotifier{ nullptr };
	};
} // namespace RTC

#endif
