#pragma once
#include "RTC/MediaTranslate/Websocket/WebsocketOptions.hpp"
#include <memory>
#include <string>

namespace RTC
{

class Websocket;

class WebsocketFactory
{
public:
    virtual ~WebsocketFactory() = default;
	virtual std::unique_ptr<Websocket> Create() const = 0;
    virtual std::string GetUri() const { return _tsUri; }
    virtual std::string GetUser() const { return _tsUser; }
    virtual std::string GetUserPassword() const { return _tsUserPassword; }
protected:
    WebsocketOptions CreateOptions() const;
private:
	static inline const std::string _tsUri = "wss://20.218.159.203:8080/record";
    static inline const std::string _tsUser = "user";
    static inline const std::string _tsUserPassword = "password";
};

} // namespace RTC
