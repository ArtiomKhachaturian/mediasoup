#define MS_CLASS "OutputWebSocketDevice"
#include "RTC/OutputWebSocketDevice.hpp"
#include <oatpp/core/async/Executor.hpp>
#include <oatpp/network/tcp/client/ConnectionProvider.hpp>
#include <oatpp-websocket/AsyncWebSocket.hpp>
#include <oatpp-websocket/Connector.hpp>
#include <algorithm>
#include <cctype>
#include <iterator>
#include <stdexcept>
#include "Logger.hpp"

namespace {

// split by port & domain
inline void SplitPathToDomainAndPort(std::string_view path, std::string& domain, std::string& port)
{
    if (path.size() > 1UL) {
        // split by port & domain
        const auto pathSplitter = path.find(':');
        if (std::string::npos != pathSplitter) {
            domain.insert(domain.begin(), path.begin(), path.begin() + pathSplitter);
            port.insert(port.begin(), path.begin() + pathSplitter + 1UL, path.end());
        } else {
            domain = std::move(path);
        }
    }
}

} // namespace

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
    UriParts(const std::string& uri);
    oatpp::network::Address GetAddress() const { return {_uri, _port}; }
    const std::string& GetDomain() const { return _domain; }
    static std::string CreateUriWithScheme(const std::string& baseUri, bool secure);
    static std::unique_ptr<UriParts> ParseAndCreate(const std::string& uri);
private:
    static const std::string _wssScheme;
    static const std::string _wsScheme;
    bool _isConnectionSecure = false;
    std::string _domain;
    v_uint16 _port = 0U;
    std::string _uri;
};

class OutputWebSocketDevice::Connector
{
public:
    Connector(std::shared_ptr<oatpp::network::tcp::client::ConnectionProvider> connectionProvider,
              std::shared_ptr<oatpp::websocket::Connector> connector,
              oatpp::websocket::Connector::Headers headers);
    ~Connector();
    static std::unique_ptr<Connector> Create(const UriParts* uriParts,
                                             const std::unordered_map<std::string, std::string>& headers);
    bool Open(const std::string& domain = std::string());
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

OutputWebSocketDevice::OutputWebSocketDevice(const std::string& baseUri, bool secure,
                                             const std::unordered_map<std::string, std::string>& headers)
    : OutputWebSocketDevice(UriParts::CreateUriWithScheme(baseUri, secure), headers)
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
    return IsValid() && _connector->Open();
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

const std::string OutputWebSocketDevice::UriParts::_wssScheme("wss");
const std::string OutputWebSocketDevice::UriParts::_wsScheme("ws");

OutputWebSocketDevice::UriParts::UriParts(const std::string& uri)
{
    if (!uri.empty()) {
        auto protocolEnd = uri.find("://");
        // extract protocol (URI scheme)
        if (std::string::npos != protocolEnd) {
            std::string protocol;
            protocol.reserve(protocolEnd);
            std::transform(uri.begin(), uri.begin() + protocolEnd, std::back_inserter(protocol),
                           [](unsigned char c) { return std::tolower(c); });
            // is websocket proto?
            if (_wssScheme == protocol || _wsScheme == protocol) {
                protocolEnd += 3;
                _isConnectionSecure = _wssScheme == protocol; // secure or not
                // extract URI part
                const auto pathEnd = uri.find_first_of('/', protocolEnd);
                std::string port;
                if (std::string::npos != pathEnd) {
                    SplitPathToDomainAndPort(std::string(uri.begin() + protocolEnd, uri.begin() + pathEnd), _domain, port);
                } else {
                    SplitPathToDomainAndPort(std::string(uri.begin() + protocolEnd, uri.end()), _domain, port);
                }
                _uri = uri;
                if (!port.empty()) {
                    try {
                        _port = std::stoi(port.c_str());
                    }
                    catch(const std::exception& e) {
                        MS_WARN_TAG(rtp, "Failed to extract port number: %s", e.what());
                    }
                }
            }
        }
    }
}

std::string OutputWebSocketDevice::UriParts::CreateUriWithScheme(const std::string& baseUri, bool secure)
{
    if (!baseUri.empty()) {
        return (secure ? UriParts::_wssScheme : UriParts::_wsScheme) + "://" + baseUri;
    }
    return baseUri;
}

std::unique_ptr<OutputWebSocketDevice::UriParts> OutputWebSocketDevice::UriParts::ParseAndCreate(const std::string& uri)
{
    if (!uri.empty()) {
        auto uriParts = std::make_unique<UriParts>(uri);
        if (!uriParts->_uri.empty()) {
            return uriParts;
        }
    }
    return nullptr;
}

OutputWebSocketDevice::Connector::Connector(std::shared_ptr<oatpp::network::tcp::client::ConnectionProvider> connectionProvider,
                                            std::shared_ptr<oatpp::websocket::Connector> connector,
                                            oatpp::websocket::Connector::Headers headers)
    : _executor(std::make_unique<oatpp::async::Executor>())
    , _connectionProvider(std::move(connectionProvider))
    , _connector(std::move(connector))
    , _headers(std::move(headers))
    , _listener(std::make_shared<Listener>())
{
}

OutputWebSocketDevice::Connector::~Connector()
{
    _executor->waitTasksFinished();
}

bool OutputWebSocketDevice::Connector::Open(const std::string& domain)
{
    if (_listener->setState(State::Connecting)) {
        _executor->execute<ClientCoroutine>(_connector, _listener, domain, _headers);
        return true;
    }
    return false;
}

void OutputWebSocketDevice::Connector::Close(bool wait)
{
    if (_listener->setState(State::Disconnected) && wait) {
        _executor->waitTasksFinished();
    }
}

std::unique_ptr<OutputWebSocketDevice::Connector> OutputWebSocketDevice::
    Connector::Create(const UriParts* uriParts, const std::unordered_map<std::string, std::string>& headers)
{
    if (uriParts) {
        using namespace oatpp::network;
        auto connectionProvider = tcp::client::ConnectionProvider::createShared(uriParts->GetAddress());
        if (connectionProvider) {
            auto connector = oatpp::websocket::Connector::createShared(connectionProvider);
            if (connector) {
                using namespace oatpp::data::share;
                oatpp::websocket::Connector::Headers oatppHeaders;
                for (auto it = headers.begin(); it != headers.end(); ++it) {
                    oatppHeaders.put(StringKeyLabelCI(it->first), StringKeyLabel(it->second));
                }
                return std::make_unique<Connector>(std::move(connectionProvider),
                                                   std::move(connector),
                                                   std::move(oatppHeaders));
            }
        }
    }
    return nullptr;
}


bool OutputWebSocketDevice::Listener::setState(State state)
{
    State expected[2] = {};
    switch (state) {
        case State::Disconnected:
            expected[0] = State::Connecting;
            expected[0] = State::Connected;
            break;
        case State::Connecting:
            expected[0] = expected[1] = State::Disconnected;
            break;
        case State::Connected:
            expected[0] = expected[1] = State::Connecting;
            break;
    }
    return std::atomic_compare_exchange_strong(&_state, &expected[0], state) ||
           std::atomic_compare_exchange_strong(&_state, &expected[1], state);
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
    if (_listener->setState(State::Connecting)) {
        return _connector->connectAsync(_domain, _headers).callbackTo(&ClientCoroutine::OnConnected);
    }
    return {};
}

oatpp::async::Action OutputWebSocketDevice::ClientCoroutine::handleError(Error* error)
{
    MS_ERROR("Failed to connect by websocket: %s", error ? error->what() : "unknown error");
    return error;
}

oatpp::async::Action OutputWebSocketDevice::ClientCoroutine::OnConnected(const oatpp::provider::ResourceHandle<oatpp::data::stream::IOStream>& connection)
{
    /* maskOutgoingMessages for clients always true */
    if (_listener->setState(State::Connected)) {
        _socket = oatpp::websocket::AsyncWebSocket::createShared(connection, true);
        _socket->setListener(_listener);
        if (State::Connected == _listener->GetState()) {
            // Listen on WebSocket, when WebSocket is closed - call onFinishListen()
            return _socket->listenAsync().next(yieldTo(&ClientCoroutine::OnFinishListen));
        }
        else {
            _socket.reset();
        }
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
