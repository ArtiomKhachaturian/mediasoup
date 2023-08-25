#ifndef MS_RTC_OUTPUT_WEBSOCKET_DEVICE_HPP
#define MS_RTC_OUTPUT_WEBSOCKET_DEVICE_HPP

#include "RTC/OutputDevice.hpp"
#include <atomic>
#include <string>
#include <memory>
#include <unordered_map>

namespace RTC
{

class OutputWebSocketDevice : public OutputDevice
{
    enum class State;
    class UriParts;
    class Connector;
    class ClientCoroutine;
    class Listener;
    class SocketImpl;
public:
    // Supported URI schemes: HTTP/HTTPS or WS/WSS
    OutputWebSocketDevice(const std::string& uri,
                          const std::unordered_map<std::string, std::string>& headers = {});
    ~OutputWebSocketDevice() final;
    bool Open();
    void Close();
    bool IsValid() const;
    static void ClassInit();
    static void ClassDestroy();
    // impl. of OutputDevice
    bool Write(const void* buf, uint32_t len) final;
    int64_t GetPosition() const final { return _position; }
    bool SetPosition(int64_t /*position*/) final { return false; }
    bool Seekable() const final { return false; }
    bool IsFileDevice() const final { return false; }
private:
    static inline std::atomic_bool _oatInitialized = false;
    const std::unique_ptr<UriParts> _uriParts;
    const std::unique_ptr<Connector> _connector;
    //const ActiveFlag _active;
    int64_t _position = 0LL;
};

} // namespace RTC

#endif
