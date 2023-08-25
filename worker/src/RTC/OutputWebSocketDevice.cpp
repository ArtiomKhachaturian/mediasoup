#define MS_CLASS "OutputWebSocketDevice"
#include "RTC/OutputWebSocketDevice.hpp"
#include <oatpp/network/Url.hpp>
#include <oatpp/core/async/Executor.hpp>
#include <oatpp/network/tcp/client/ConnectionProvider.hpp>
#include <oatpp-websocket/AsyncWebSocket.hpp>
#include <oatpp-websocket/Connector.hpp>
#include <algorithm>
#include <cctype>
#include <iterator>
#include <stdexcept>
#include "Logger.hpp"

namespace RTC
{

enum class OutputWebSocketDevice::State
{
    Disconnected,
    Connecting,
    Connected
};

class OutputWebSocketDevice::UriParts
{
public:
    UriParts(oatpp::network::Url url);
    oatpp::network::Address GetAddress() const;
    const oatpp::String& GetPath() const { return _url.path; }
    const oatpp::String& GetUserInfo() const { return _url.authority.userInfo; }
    static std::unique_ptr<UriParts> ParseAndCreate(const std::string& uri);
private:
    const oatpp::network::Url _url;
};

class OutputWebSocketDevice::Connector
{
public:
    Connector(std::shared_ptr<oatpp::network::tcp::client::ConnectionProvider> connectionProvider,
              oatpp::websocket::Connector::Headers headers);
    ~Connector();
    static std::unique_ptr<Connector> Create(const UriParts* uriParts,
                                             const std::unordered_map<std::string, std::string>& headers);
    bool Open(const oatpp::String& path = oatpp::String());
    void Close(bool wait);
private:
    const std::unique_ptr<oatpp::async::Executor> _executor;
    const std::shared_ptr<oatpp::network::tcp::client::ConnectionProvider> _connectionProvider;
    const std::shared_ptr<oatpp::websocket::Connector> _connector;
    const oatpp::websocket::Connector::Headers _headers;
    const std::shared_ptr<Listener> _listener;
};

class OutputWebSocketDevice::Listener : public oatpp::websocket::AsyncWebSocket::Listener
{
public:
    Listener() = default;
    bool setState(State state);
    State GetState() const { return _state.load(std::memory_order_relaxed); }
    void setSocketRef(const std::weak_ptr<oatpp::websocket::AsyncWebSocket>& socketRef);
    // impl. of oatpp::websocket::AsyncWebSocket::Listener
    oatpp::async::CoroutineStarter onPing(const std::shared_ptr<AsyncWebSocket>& socket,
                                          const oatpp::String& message) final;
    oatpp::async::CoroutineStarter onPong(const std::shared_ptr<AsyncWebSocket>& socket,
                                          const oatpp::String& message) final;
    oatpp::async::CoroutineStarter onClose(const std::shared_ptr<AsyncWebSocket>& socket,
                                           v_uint16 code, const oatpp::String& message) final;
    oatpp::async::CoroutineStarter readMessage(const std::shared_ptr<AsyncWebSocket>& socket,
                                               v_uint8 opcode, p_char8 data, oatpp::v_io_size size) final;
private:
    std::atomic<State> _state = State::Disconnected;
    std::mutex _socketRefMtx;
    std::weak_ptr<oatpp::websocket::AsyncWebSocket> _socketRef;
};

class OutputWebSocketDevice::ClientCoroutine : public oatpp::async::Coroutine<ClientCoroutine>
{
public:
    ClientCoroutine(const std::shared_ptr<oatpp::websocket::Connector>& connector,
                    const std::shared_ptr<Listener>& listener,
                    const std::string& domain = std::string(),
                    const oatpp::websocket::Connector::Headers& headers = {});
    // impl. of oatpp::async::Coroutine<>
    oatpp::async::Action act() final;
    oatpp::async::Action handleError(Error* error) final;
private:
    oatpp::async::Action OnConnected(const oatpp::provider::ResourceHandle<oatpp::data::stream::IOStream>& connection);
    oatpp::async::Action OnFinishListen();
private:
    const std::shared_ptr<oatpp::websocket::Connector> _connector;
    const std::shared_ptr<Listener> _listener;
    const std::string _domain;
    const oatpp::websocket::Connector::Headers _headers;
    // Established WebSocket connection
    std::shared_ptr<oatpp::websocket::AsyncWebSocket> _socket;
};

OutputWebSocketDevice::OutputWebSocketDevice(const std::string& uri,
                                             const std::unordered_map<std::string, std::string>& headers)
    : _uriParts(UriParts::ParseAndCreate(uri))
    , _connector(Connector::Create(_uriParts.get(), headers))
{
}

OutputWebSocketDevice::~OutputWebSocketDevice()
{
    _connector->Close(true);
}

bool OutputWebSocketDevice::IsValid() const
{
    return nullptr != _uriParts && nullptr != _connector;
}

bool OutputWebSocketDevice::Open()
{
    return IsValid() && _connector->Open(_uriParts->GetPath());
}

void OutputWebSocketDevice::Close()
{
    if (IsValid()) {
        _connector->Close(false);
    }
}

void OutputWebSocketDevice::ClassInit()
{
    if (!_oatInitialized.exchange(true)) {
        oatpp::base::Environment::init();
    }
}

void OutputWebSocketDevice::ClassDestroy()
{
    if (_oatInitialized.exchange(false)) {
        oatpp::base::Environment::destroy();
    }
}

bool OutputWebSocketDevice::Write(const void* buf, uint32_t len)
{
    return false;
}

OutputWebSocketDevice::UriParts::UriParts(oatpp::network::Url url)
    : _url(std::move(url))
{
}

oatpp::network::Address OutputWebSocketDevice::UriParts::GetAddress() const
{
    return oatpp::network::Address(_url.authority.host, _url.authority.port);
}

std::unique_ptr<OutputWebSocketDevice::UriParts> OutputWebSocketDevice::UriParts::ParseAndCreate(const std::string& uri)
{
    if (!uri.empty()) {
        auto url = oatpp::network::Url::Parser::parseUrl(uri);
        if (url.authority.host) {
            if (-1 == url.authority.port) {
                url.authority.port = 0;
            }
            return std::make_unique<UriParts>(std::move(url));
        }
    }
    return nullptr;
}

OutputWebSocketDevice::Connector::Connector(std::shared_ptr<oatpp::network::tcp::client::ConnectionProvider> connectionProvider,
                                            oatpp::websocket::Connector::Headers headers)
    : _executor(std::make_unique<oatpp::async::Executor>(1, 1))
    , _connectionProvider(std::move(connectionProvider))
    , _connector(oatpp::websocket::Connector::createShared(_connectionProvider))
    , _headers(std::move(headers))
    , _listener(std::make_shared<Listener>())
{
}

OutputWebSocketDevice::Connector::~Connector()
{
    _executor->waitTasksFinished();
    _executor->stop();
    _executor->join();
}

bool OutputWebSocketDevice::Connector::Open(const oatpp::String& path)
{
    if (_listener->setState(State::Connecting)) {
        _executor->execute<ClientCoroutine>(_connector, _listener, path, _headers);
        return true;
    }
    return false;
}

void OutputWebSocketDevice::Connector::Close(bool wait)
{
    if (_listener->setState(State::Disconnected)) {
        _connectionProvider->stop();
        if (wait) {
            _executor->waitTasksFinished();
        }
    }
}

std::unique_ptr<OutputWebSocketDevice::Connector> OutputWebSocketDevice::
    Connector::Create(const UriParts* uriParts, const std::unordered_map<std::string, std::string>& headers)
{
    if (uriParts) {
        using namespace oatpp::network;
        auto connectionProvider = tcp::client::ConnectionProvider::createShared(uriParts->GetAddress());
        if (connectionProvider) {
            oatpp::websocket::Connector::Headers oatppHeaders;
            {
                using namespace oatpp::data::share;
                for (auto it = headers.begin(); it != headers.end(); ++it) {
                    oatppHeaders.put(StringKeyLabelCI(it->first), StringKeyLabel(it->second));
                }
            }
            return std::make_unique<Connector>(std::move(connectionProvider),
                                               std::move(oatppHeaders));
        }
    }
    return nullptr;
}


bool OutputWebSocketDevice::Listener::setState(State state)
{
    bool changed = false;
    if (State::Disconnected == state) {
        changed = State::Disconnected != _state.exchange(state);
    }
    else {
        State expected;
        switch (state) {
            case State::Connecting:
                expected = State::Disconnected;
                break;
            case State::Connected:
                expected = State::Connecting;
                break;
            default:
                MS_ASSERT(false, "invalid websocket state");
                break;
        }
        changed = std::atomic_compare_exchange_strong(&_state, &expected, state);
    }
    return changed;
}

void OutputWebSocketDevice::Listener::setSocketRef(const std::weak_ptr<oatpp::websocket::AsyncWebSocket>& socketRef)
{
    if (!socketRef.expired()) {
        const std::lock_guard<std::mutex> guard(_socketRefMtx);
        _socketRef = socketRef;
    }
}

oatpp::async::CoroutineStarter OutputWebSocketDevice::Listener::onPing(const std::shared_ptr<AsyncWebSocket>& socket,
                                                                       const oatpp::String& message)
{
    return nullptr;
}

oatpp::async::CoroutineStarter OutputWebSocketDevice::Listener::onPong(const std::shared_ptr<AsyncWebSocket>& socket,
                                                                       const oatpp::String& message)
{
    return nullptr;
}

oatpp::async::CoroutineStarter OutputWebSocketDevice::Listener::onClose(const std::shared_ptr<AsyncWebSocket>& socket,
                                                                        v_uint16 code,
                                                                        const oatpp::String& message)
{
    setState(State::Disconnected);
    return nullptr;
}

oatpp::async::CoroutineStarter OutputWebSocketDevice::Listener::readMessage(const std::shared_ptr<AsyncWebSocket>& socket,
                                                                            v_uint8 opcode,
                                                                            p_char8 data,
                                                                            oatpp::v_io_size size)
{
    return nullptr;
}

OutputWebSocketDevice::ClientCoroutine::ClientCoroutine(const std::shared_ptr<oatpp::websocket::Connector>& connector,
                                                        const std::shared_ptr<Listener>& listener,
                                                        const std::string& domain,
                                                        const oatpp::websocket::Connector::Headers& headers)
    : _connector(connector)
    , _listener(listener)
    , _domain(domain)
    , _headers(headers)
{
}

oatpp::async::Action OutputWebSocketDevice::ClientCoroutine::act()
{
    // Establish WebSocket connection
    if (State::Connecting == _listener->GetState()) {
        return _connector->connectAsync(_domain, _headers).callbackTo(&ClientCoroutine::OnConnected);
    }
    return {};
}

oatpp::async::Action OutputWebSocketDevice::ClientCoroutine::handleError(Error* error)
{
    if (_listener->setState(State::Disconnected)) {
        const auto err = error ? error->what() : "unknown error";
        MS_WARN_TAG(rtx, "Failed to connect by websocket: %s", error ? error->what() : "unknown error");
    }
    return error;
}

oatpp::async::Action OutputWebSocketDevice::ClientCoroutine::OnConnected(const oatpp::provider::ResourceHandle<oatpp::data::stream::IOStream>& connection)
{
    /* maskOutgoingMessages for clients always true */
    if (_listener->setState(State::Connected)) {
        _socket = oatpp::websocket::AsyncWebSocket::createShared(connection, true);
        _socket->setListener(_listener);
        if (State::Connected == _listener->GetState()) {
            _listener->setSocketRef(_socket);
            // Listen on WebSocket, when WebSocket is closed - call onFinishListen()
            return _socket->listenAsync().next(yieldTo(&ClientCoroutine::OnFinishListen));
        }
        _socket.reset();
    }
    return {};
}

oatpp::async::Action OutputWebSocketDevice::ClientCoroutine::OnFinishListen()
{
    _listener->setState(State::Disconnected);
    _socket.reset();
    return finish();
}

} // namespace RTC
