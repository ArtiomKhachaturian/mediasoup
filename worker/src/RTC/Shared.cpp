#define MS_CLASS "Shared"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/Shared.hpp"
#include "Logger.hpp"

namespace RTC
{
	Shared::Shared(const std::weak_ptr<BufferAllocator>& allocator,
                   ChannelMessageRegistrator* channelMessageRegistrator,
                   Channel::ChannelNotifier* channelNotifier)
        : BufferAllocations<void>(allocator)
        , channelMessageRegistrator(channelMessageRegistrator)
        , channelNotifier(channelNotifier)
	{
		MS_TRACE();
	}

	Shared::~Shared()
	{
		MS_TRACE();

		delete this->channelMessageRegistrator;
		delete this->channelNotifier;
	}
} // namespace RTC
