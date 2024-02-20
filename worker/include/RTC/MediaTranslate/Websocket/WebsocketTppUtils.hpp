#pragma once
#include <asio/ssl/context.hpp>

namespace RTC
{

struct WebsocketTls;

class WebsocketTppUtils
{
public:
	static std::shared_ptr<asio::ssl::context> CreateSSLContext(const WebsocketTls& tls) noexcept(false);
};

} // namespace RTC