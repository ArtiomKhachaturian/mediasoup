#pragma once
#include <string>
#include <optional>
#include <unordered_map>

namespace RTC
{

struct WebsocketOptions
{
	WebsocketOptions() = default;
	WebsocketOptions(const WebsocketOptions&) = default;
	WebsocketOptions(WebsocketOptions&&) = default;
	WebsocketOptions& operator = (const WebsocketOptions&) = default;
	WebsocketOptions& operator = (WebsocketOptions&&) = default;
	std::unordered_map<std::string, std::string> _extraHeaders;
	std::string _userAgent;
	std::string _tlsTrustStore;
    std::string _tlsKeyStore;
    std::string _tlsPrivateKey;
    std::string _tlsPrivateKeyPassword;
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
};

} // namespace RTC
