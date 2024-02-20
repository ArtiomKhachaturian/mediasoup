#pragma once
#include "RTC/MediaTranslate/Websocket/WebsocketTls.hpp"
#include <optional>
#include <unordered_map>

namespace RTC
{

struct WebsocketOptions
{
public:
	WebsocketOptions() = default;
	WebsocketOptions(const WebsocketOptions&) = default;
	WebsocketOptions(WebsocketOptions&&) = default;
	WebsocketOptions& operator = (const WebsocketOptions&) = default;
	WebsocketOptions& operator = (WebsocketOptions&&) = default;
    void AddAuthorizationHeader(const std::string& user, const std::string& password);
    static WebsocketOptions CreateWithAuthorization(const std::string& user = std::string(),
                                                    const std::string& password = std::string());
    // agent
    std::string _userAgent;
    // headers
	std::unordered_map<std::string, std::string> _extraHeaders;
    // options
    /* list of possible options:
     * asio::socket_base::broadcast @n
     * asio::socket_base::do_not_route @n
     * asio::socket_base::keep_alive @n
     * asio::socket_base::linger @n
     * asio::socket_base::receive_buffer_size @n
     * asio::socket_base::receive_low_watermark @n
     * asio::socket_base::reuse_address @n
     * asio::socket_base::send_buffer_size @n
     * asio::socket_base::send_low_watermark @n
     * asio::ip::multicast::join_group @n
     * asio::ip::multicast::leave_group @n
     * asio::ip::multicast::enable_loopback @n
     * asio::ip::multicast::outbound_interface @n
     * asio::ip::multicast::hops @n
     * asio::ip::tcp::no_delay
    */
    std::optional<bool> _tcpNoDelay;
    // SSL/TLS
    WebsocketTls _tls;
};

} // namespace RTC
