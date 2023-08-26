#ifndef MS_RTC_OUTPUT_WEBSOCKET_DEVICE_HPP
#define MS_RTC_OUTPUT_WEBSOCKET_DEVICE_HPP

#include "RTC/OutputDevice.hpp"
#include <string>
#include <memory>
#include <thread>
#include <unordered_map>

namespace RTC
{

class OutputWebSocketDevice : public OutputDevice
{
    enum class State;
    class UriParts;
    class Socket;
    template<class TConfig> class SocketImpl;
    class SocketTls;
    class SocketNoTls;
public:
    enum class SocketState
    {
        Invalid, // wrong URI
        Connecting,
        Connected,
        Disconnected,
    };
public:
    // Supported URI schemes: WS or WSS
    OutputWebSocketDevice(std::string uri, std::unordered_map<std::string, std::string> headers = {});
    ~OutputWebSocketDevice() final;
    bool Open();
    void Close();
    SocketState GetState() const;
    bool WriteText(const std::string& text);
    // impl. of OutputDevice
    bool Write(const void* buf, uint32_t len) final;
    int64_t GetPosition() const final { return _position; }
    bool SetPosition(int64_t /*position*/) final { return false; }
    bool Seekable() const final { return false; }
    bool IsFileDevice() const final { return false; }
private:
    const std::shared_ptr<UriParts> _uri;
    const std::shared_ptr<Socket> _socket;
    int64_t _position = 0LL;
    std::thread _asioThread;
};

} // namespace RTC

#endif
