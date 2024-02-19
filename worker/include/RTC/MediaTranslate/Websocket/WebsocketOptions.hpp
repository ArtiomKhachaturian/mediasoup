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
    std::optional<bool> _tcpNoDelay;
};

} // namespace RTC