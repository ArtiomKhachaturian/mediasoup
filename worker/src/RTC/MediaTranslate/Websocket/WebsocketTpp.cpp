#define MS_CLASS "RTC::WebsocketTpp"
#include "RTC/MediaTranslate/Websocket/WebsocketTpp.hpp"
#include "RTC/MediaTranslate/Websocket/WebsocketListener.hpp"
#include "RTC/MediaTranslate/Websocket/WebsocketFailure.hpp"
#include "RTC/MediaTranslate/Websocket/WebsocketTppUtils.hpp"
#include "RTC/MediaTranslate/Buffers/Buffer.hpp"
#include "RTC/MediaTranslate/ThreadUtils.hpp"
#include "Logger.hpp"
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/close.hpp>
#include <thread>
#include <atomic>

namespace {

using namespace RTC;

template<class MessagePtr>
class MessageBuffer : public Buffer
{
public:
    MessageBuffer(MessagePtr message);
    ~MessageBuffer() final;
    // impl. of Buffer
    size_t GetSize() const final;
    uint8_t* GetData() final;
    const uint8_t* GetData() const final;
private:
    const MessagePtr _message;
};

class LogStreamBuf : public std::streambuf
{
public:
    LogStreamBuf(uint64_t socketId, LogLevel level);
    static bool IsAccepted(LogLevel level);
    static void Write(LogLevel level, const std::string& message);
    static void Write(LogLevel level, uint64_t socketId, const std::string& message);
    void Write(const std::string& message) const;
protected:
    // overrides of std::streambuf
    std::streamsize xsputn(const char* s, std::streamsize count) final;
    int sync() final;
private:
    const uint64_t _socketId;
    const LogLevel _level;
    std::string _buffer;
};

}

namespace RTC
{

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

class WebsocketTpp::Config
{
public:
    static std::shared_ptr<const Config> VerifyAndParse(const std::string& uri,
                                                        WebsocketOptions options);
    bool IsSecure() const { return _uri->get_secure(); }
    const std::shared_ptr<websocketpp::uri>& GetUri() const { return _uri; }
    const WebsocketOptions& GetOptions() const { return _options; }
private:
    Config(std::shared_ptr<websocketpp::uri> uri, WebsocketOptions options);
private:
    const std::shared_ptr<websocketpp::uri> _uri;
    const WebsocketOptions _options;
};

class WebsocketTpp::Socket
{
public:
    virtual ~Socket() = default;
    virtual WebsocketState GetState() = 0;
    virtual void Run() = 0;
    virtual bool Open() = 0;
    virtual void Close() = 0;
    virtual bool WriteBinary(const std::shared_ptr<Buffer>& buffer) = 0;
    virtual bool WriteText(const std::string& text) = 0;
    static std::shared_ptr<Socket> Create(uint64_t id, const std::shared_ptr<const Config>& config,
                                          const std::shared_ptr<SocketListeners>& listeners);
};

template <class TConfig>
struct WebsocketTpp::SocketConfig : public TConfig
{
    static const size_t connection_read_buffer_size = Websocket::_connectionReadBufferSize;
};

template<class TConfig>
class WebsocketTpp::SocketImpl : public Socket
{
    using Client = websocketpp::client<TConfig>;
    using MessagePtr = typename TConfig::message_type::ptr;
    using HdlWriteGuard = typename ProtectedObj<websocketpp::connection_hdl>::GuardTraits::MutexWriteGuard;
    using HdlReadGuard = typename ProtectedObj<websocketpp::connection_hdl>::GuardTraits::MutexReadGuard;
public:
    ~SocketImpl() override;
    // impl. of Socket
    WebsocketState GetState() final;
    void Run() final;
    bool Open() final;
    void Close() final;
    bool WriteBinary(const std::shared_ptr<Buffer>& buffer) final;
    bool WriteText(const std::string& text) final;
protected:
    SocketImpl(uint64_t id, const std::shared_ptr<const Config>& config,
               const std::shared_ptr<SocketListeners>& listeners);
    uint64_t GetId() const { return _id; }
    const std::shared_ptr<const Config>& GetConfig() const { return _config; }
    const Client& GetClient() const { return _client; }
    Client& GetClient() { return _client; }
    template <class Method, typename... Args>
    void InvokeListenersMethod(const Method& method, Args&&... args) const;
    template<class TOption>
    void SetTcpSocketOption(const TOption& option, websocketpp::connection_hdl hdl, const char* optionName);
    template<class TOption>
    void SetTcpSocketOption(const TOption& option, const char* optionName);
private:
    void OnSocketInit(websocketpp::connection_hdl hdl);
    void OnFail(websocketpp::connection_hdl hdl);
    void OnOpen(websocketpp::connection_hdl hdl);
    void OnMessage(websocketpp::connection_hdl hdl, MessagePtr message);
    void OnClose(websocketpp::connection_hdl hdl);
    template<class Guard>
    void DropHdl(std::unique_ptr<Guard> droppedGuard);
    // return true if state changed
    template<class Guard = HdlWriteGuard>
    bool SetOpened(bool opened, std::unique_ptr<Guard> droppedGuard = nullptr);
    bool IsOpened() const { return _opened.load(std::memory_order_relaxed); }
    static std::string ToText(MessagePtr message);
    static std::shared_ptr<Buffer> ToBinary(MessagePtr message);
private:
    static inline constexpr uint16_t _closeCode = websocketpp::close::status::going_away;
    const uint64_t _id;
    const std::shared_ptr<const Config> _config;
    const std::shared_ptr<SocketListeners> _listeners;
    LogStreamBuf _debugStreamBuf;
    LogStreamBuf _errorStreamBuf;
    std::ostream _debugStream;
    std::ostream _errorStream;
    Client _client;
    ProtectedObj<websocketpp::connection_hdl> _hdl;
    std::atomic_bool _opened = false;
};

class WebsocketTpp::SocketTls : public SocketImpl<SocketConfig<websocketpp::config::asio_tls_client>>
{
    using SslContextPtr = websocketpp::lib::shared_ptr<asio::ssl::context>;
public:
    SocketTls(uint64_t id, const std::shared_ptr<const Config>& config,
              const std::shared_ptr<SocketListeners>& listeners);
private:
    SslContextPtr OnTlsInit(websocketpp::connection_hdl);
};

class WebsocketTpp::SocketNoTls : public SocketImpl<SocketConfig<websocketpp::config::asio_client>>
{
public:
    SocketNoTls(uint64_t id, const std::shared_ptr<const Config>& config,
                const std::shared_ptr<SocketListeners>& listeners);
};

class WebsocketTpp::SocketWrapper : public Socket
{
public:
    SocketWrapper(std::shared_ptr<Socket> impl);
    ~SocketWrapper() final;
    static std::unique_ptr<Socket> Create(uint64_t id, const std::shared_ptr<const Config>& config,
                                          const std::shared_ptr<SocketListeners>& listeners);
    // impl. of Socket
    WebsocketState GetState() final;
    void Run() final;
    bool Open() final;
    void Close() final;
    bool WriteBinary(const std::shared_ptr<Buffer>& buffer) final;
    bool WriteText(const std::string& text) final;
private:
    std::shared_ptr<Socket> _impl;
    std::thread _asioThread;
};

WebsocketTpp::WebsocketTpp(const std::string& uri,
                           WebsocketOptions options)
    : _config(Config::VerifyAndParse(uri, std::move(options)))
{
}

WebsocketTpp::~WebsocketTpp()
{
    Close();
}

bool WebsocketTpp::Open()
{
    bool result = false;
    if (_config) {
        LOCK_WRITE_PROTECTED_OBJ(_socket);
        if (!_socket.ConstRef()) {
            auto socket = SocketWrapper::Create(GetId(), _config, _listeners);
            if (socket) {
                result = socket->Open();
                if (result) {
                    socket->Run();
                    _socket = std::move(socket);
                }
            }
        }
        else { // connected or connecting now
            result = true;
        }
    }
    return result;
}

void WebsocketTpp::Close()
{
    LOCK_WRITE_PROTECTED_OBJ(_socket);
    _socket.Take().reset();
}

WebsocketState WebsocketTpp::GetState() const
{
    if (_config) {
        LOCK_READ_PROTECTED_OBJ(_socket);
        if (const auto& socket = _socket.ConstRef()) {
            return socket->GetState();
        }
        return WebsocketState::Disconnected;
    }
    return WebsocketState::Invalid;
}

std::string WebsocketTpp::GetUrl() const
{
    return _config ? _config->GetUri()->str() : std::string();
}

bool WebsocketTpp::WriteText(const std::string& text)
{
    LOCK_READ_PROTECTED_OBJ(_socket);
    if (const auto& socket = _socket.ConstRef()) {
        return socket->WriteText(text);
    }
    return false;
}

bool WebsocketTpp::WriteBinary(const std::shared_ptr<Buffer>& buffer)
{
    if (buffer) {
        LOCK_READ_PROTECTED_OBJ(_socket);
        if (const auto& socket = _socket.ConstRef()) {
            return socket->WriteBinary(buffer);
        }
    }
    return false;
}

WebsocketTpp::Config::Config(std::shared_ptr<websocketpp::uri> uri, WebsocketOptions options)
    : _uri(std::move(uri))
    , _options(std::move(options))
{
}

std::shared_ptr<const WebsocketTpp::Config> WebsocketTpp::Config::
    VerifyAndParse(const std::string& uri, WebsocketOptions options)
{
    std::shared_ptr<const Config> config;
    if (!uri.empty()) {
        auto validUri = std::make_shared<websocketpp::uri>(uri);
        if (validUri->get_valid()) {
            config.reset(new Config(std::move(validUri), std::move(options)));
        }
        else {
            MS_WARN_DEV_STD("invalid web socket URI %s", uri.c_str());
        }
    }
    return config;
}

std::shared_ptr<WebsocketTpp::Socket> WebsocketTpp::Socket::
    Create(uint64_t id, const std::shared_ptr<const Config>& config,
           const std::shared_ptr<SocketListeners>& listeners)
{
    if (config) {
        if (config->IsSecure()) {
            return std::make_shared<SocketTls>(id, config, listeners);
        }
        return std::make_shared<SocketNoTls>(id, config, listeners);
    }
    return nullptr;
}

template<class TConfig>
WebsocketTpp::SocketImpl<TConfig>::SocketImpl(uint64_t id, const std::shared_ptr<const Config>& config,
                                              const std::shared_ptr<SocketListeners>& listeners)
    : _id(id)
    , _config(config)
    , _listeners(listeners)
    , _debugStreamBuf(id, LogLevel::LOG_DEBUG)
    , _errorStreamBuf(id, LogLevel::LOG_ERROR)
    , _debugStream(&_debugStreamBuf)
    , _errorStream(&_errorStreamBuf)
{
    // Initialize ASIO
    _client.set_user_agent(_config->GetOptions()._userAgent);
    _client.get_alog().set_ostream(&_debugStream);
    _client.get_elog().set_ostream(&_errorStream);
    _client.init_asio();
    _client.start_perpetual();
    // Register our handlers
    _client.set_socket_init_handler(bind(&SocketImpl::OnSocketInit, this, _1));
    _client.set_message_handler(bind(&SocketImpl::OnMessage, this, _1, _2));
    _client.set_open_handler(bind(&SocketImpl::OnOpen, this, _1));
    _client.set_close_handler(bind(&SocketImpl::OnClose, this, _1));
    _client.set_fail_handler(bind(&SocketImpl::OnFail, this, _1));
}

template<class TConfig>
WebsocketTpp::SocketImpl<TConfig>::~SocketImpl()
{
    const auto inActiveState = WebsocketState::Disconnected != GetState();
    if (inActiveState) {
        _listeners->BlockInvokes(true);
    }
    Close();
    if (inActiveState) {
        _listeners->BlockInvokes(false);
        InvokeListenersMethod(&WebsocketListener::OnStateChanged, WebsocketState::Disconnected);
    }
}

template<class TConfig>
WebsocketState WebsocketTpp::SocketImpl<TConfig>::GetState()
{
    if (!IsOpened()) {
        LOCK_READ_PROTECTED_OBJ(_hdl);
        return _hdl->expired() ? WebsocketState::Disconnected : WebsocketState::Connecting;
    }
    return WebsocketState::Connected;
}

template<class TConfig>
void WebsocketTpp::SocketImpl<TConfig>::Run()
{
    SetCurrentThreadName(_config->GetUri()->str());
    SetCurrentThreadPriority(ThreadPriority::Highest);
    _client.run();
    SetOpened(false);
}

template<class TConfig>
bool WebsocketTpp::SocketImpl<TConfig>::Open()
{
    websocketpp::lib::error_code ec;
    const auto connection = _client.get_connection(GetConfig()->GetUri(), ec);
    if (ec) {
        InvokeListenersMethod(&WebsocketListener::OnFailed,
                              WebsocketFailure::NoConnection,
                              ec.message());
    }
    else {
        const auto& extraHeaders = GetConfig()->GetOptions()._extraHeaders;
        for (auto it = extraHeaders.begin(); it != extraHeaders.end(); ++it) {
            try {
                connection->append_header(it->first, it->second);
            }
            catch(const std::exception& e) {
                InvokeListenersMethod(&WebsocketListener::OnFailed,
                                      WebsocketFailure::CustomHeader,
                                      e.what());
                return false;
            }
        }
        if (!_config->GetOptions()._userAgent.empty()) {
            _client.set_user_agent(_config->GetOptions()._userAgent);
        }
        _client.connect(connection);
    }
    return !ec;
}

template<class TConfig>
void WebsocketTpp::SocketImpl<TConfig>::Close()
{
    auto droppedGuard = std::make_unique<HdlWriteGuard>(_hdl.GetWriteGuard());
    if (!_hdl->expired()) {
        websocketpp::lib::error_code ec;
        _client.close(_hdl, _closeCode, websocketpp::close::status::get_string(_closeCode), ec);
        _client.stop();
        if (ec) { // ignore of failures during closing
            _errorStreamBuf.Write(ec.message());
        }
        DropHdl(std::move(droppedGuard));
    }
}

template<class TConfig>
bool WebsocketTpp::SocketImpl<TConfig>::WriteBinary(const std::shared_ptr<Buffer>& buffer)
{
    bool ok = false;
    if (buffer && IsOpened()) {
        LOCK_READ_PROTECTED_OBJ(_hdl);
        if (!_hdl->expired()) {
            websocketpp::lib::error_code ec;
            // overhead - deep copy of input buffer,
            // Websocketpp doesn't supports of buffers abstraction
            _client.send(_hdl, buffer->GetData(), buffer->GetSize(),
                         websocketpp::frame::opcode::binary, ec);
            if (ec) {
                InvokeListenersMethod(&WebsocketListener::OnFailed,
                                      WebsocketFailure::WriteBinary,
                                      ec.message());
            }
            else {
                ok = true;
            }
        }
    }
    return ok;
}

template<class TConfig>
bool WebsocketTpp::SocketImpl<TConfig>::WriteText(const std::string& text)
{
    bool ok = false;
    if (IsOpened()) {
        LOCK_READ_PROTECTED_OBJ(_hdl);
        if (!_hdl->expired()) {
            websocketpp::lib::error_code ec;
            _client.send(_hdl, text, websocketpp::frame::opcode::text, ec);
            if (ec) {
                InvokeListenersMethod(&WebsocketListener::OnFailed,
                                      WebsocketFailure::WriteText,
                                      ec.message());
            }
            else {
                ok = true;
            }
        }
    }
    return ok;
}

template<class TConfig>
template <class Method, typename... Args>
void WebsocketTpp::SocketImpl<TConfig>::InvokeListenersMethod(const Method& method,
                                                              Args&&... args) const
{
    _listeners->InvokeMethod(method, GetId(), std::forward<Args>(args)...);
}

template<class TConfig>
template<class TOption>
void WebsocketTpp::SocketImpl<TConfig>::SetTcpSocketOption(const TOption& option,
                                                           const char* optionName)
{
    LOCK_READ_PROTECTED_OBJ(_hdl);
    SetTcpSocketOption(option, _hdl, optionName);
}

template<class TConfig>
template<class TOption>
void WebsocketTpp::SocketImpl<TConfig>::SetTcpSocketOption(const TOption& option,
                                                           websocketpp::connection_hdl hdl,
                                                           const char* optionName)
{
    if (!hdl.expired()) {
        websocketpp::lib::error_code ec;
        if (const auto connection = _client.get_con_from_hdl(hdl, ec)) {
            connection->get_socket().lowest_layer().set_option(option, ec);
            if (ec) {
                MS_ERROR_STD("failed to set %s option: %s",
                             optionName, ec.message().c_str());
            }
        }
        else {
            MS_ERROR_STD("failed to set %s option because no client connection: %s",
                         optionName, ec.message().c_str());
        }
    }
}

template<class TConfig>
void WebsocketTpp::SocketImpl<TConfig>::OnSocketInit(websocketpp::connection_hdl hdl)
{
    {
        LOCK_WRITE_PROTECTED_OBJ(_hdl);
        _hdl = std::move(hdl);
    }
    InvokeListenersMethod(&WebsocketListener::OnStateChanged, GetState());
}

template<class TConfig>
void WebsocketTpp::SocketImpl<TConfig>::OnFail(websocketpp::connection_hdl hdl)
{
    auto droppedGuard = std::make_unique<HdlWriteGuard>(_hdl.GetWriteGuard());
    if (hdl.lock() == _hdl->lock()) {
        websocketpp::lib::error_code ec;
        if (const auto connection = _client.get_con_from_hdl(hdl, ec)) {
            ec = connection->get_ec();
        }
        _client.stop();
        // report error & reset state
        DropHdl(std::move(droppedGuard));
        InvokeListenersMethod(&WebsocketListener::OnFailed, WebsocketFailure::General, ec.message());
    }
}

template<class TConfig>
void WebsocketTpp::SocketImpl<TConfig>::OnOpen(websocketpp::connection_hdl hdl)
{
    auto droppedGuard = std::make_unique<HdlReadGuard>(_hdl.GetReadGuard());
    if (hdl.lock() == _hdl->lock()) {
        if (const auto& tcpNoDelay = GetConfig()->GetOptions()._tcpNoDelay) {
            const websocketpp::lib::asio::ip::tcp::no_delay option(tcpNoDelay.value());
            SetTcpSocketOption(option, hdl, "TCP_NO_DELAY");
        }
        // update state
        SetOpened(true, std::move(droppedGuard));
    }
}

template<class TConfig>
void WebsocketTpp::SocketImpl<TConfig>::OnMessage(websocketpp::connection_hdl hdl,
                                                  MessagePtr message)
{
    if (message) {
        bool accepted = false;
        {
            LOCK_READ_PROTECTED_OBJ(_hdl);
            accepted = hdl.lock() == _hdl->lock();
        }
        if (accepted) {
            switch (message->get_opcode()) {
                case websocketpp::frame::opcode::text:
                    InvokeListenersMethod(&WebsocketListener::OnTextMessageReceived,
                                          ToText(std::move(message)));
                    break;
                case websocketpp::frame::opcode::binary:
                    InvokeListenersMethod(&WebsocketListener::OnBinaryMessageReceved,
                                          ToBinary(std::move(message)));
                    break;
                default:
                    break;
            }
        }
    }
}

template<class TConfig>
void WebsocketTpp::SocketImpl<TConfig>::OnClose(websocketpp::connection_hdl hdl)
{
    auto droppedGuard = std::make_unique<HdlWriteGuard>(_hdl.GetWriteGuard());
    if (hdl.lock() == _hdl->lock()) {
        // report about close & reset state
        DropHdl(std::move(droppedGuard));
    }
}

template<class TConfig> template<class Guard>
void WebsocketTpp::SocketImpl<TConfig>::DropHdl(std::unique_ptr<Guard> droppedGuard)
{
    _hdl = websocketpp::connection_hdl();
    SetOpened(false, std::move(droppedGuard));
}

template<class TConfig> template<class Guard>
bool WebsocketTpp::SocketImpl<TConfig>::SetOpened(bool opened, std::unique_ptr<Guard> droppedGuard)
{
    if (opened != _opened.exchange(opened)) {
        droppedGuard.reset();
        InvokeListenersMethod(&WebsocketListener::OnStateChanged, GetState());
        return true;
    }
    return false;
}

template<class TConfig>
std::string WebsocketTpp::SocketImpl<TConfig>::ToText(MessagePtr message)
{
    if (message) {
        auto text = std::move(message->get_raw_payload());
        message->recycle();
        return text;
    }
    return std::string();
}

template<class TConfig>
std::shared_ptr<Buffer> WebsocketTpp::SocketImpl<TConfig>::ToBinary(MessagePtr message)
{
    if (message) {
        return std::make_shared<MessageBuffer<MessagePtr>>(std::move(message));
    }
    return nullptr;
}

WebsocketTpp::SocketTls::SocketTls(uint64_t id, const std::shared_ptr<const Config>& config,
                                   const std::shared_ptr<SocketListeners>& listeners)
    : SocketImpl<SocketConfig<websocketpp::config::asio_tls_client>>(id, config, listeners)
{
    GetClient().set_tls_init_handler(bind(&SocketTls::OnTlsInit, this, _1));
}

WebsocketTpp::SocketTls::SslContextPtr WebsocketTpp::SocketTls::OnTlsInit(websocketpp::connection_hdl)
{
    try {
        return WebsocketTppUtils::CreateSSLContext(GetConfig()->GetOptions()._tls);
    } catch (const std::exception& e) {
        InvokeListenersMethod(&WebsocketListener::OnFailed, WebsocketFailure::TlsOptions, e.what());
    }
    return nullptr;
}

WebsocketTpp::SocketNoTls::SocketNoTls(uint64_t id, const std::shared_ptr<const Config>& config,
                                       const std::shared_ptr<SocketListeners>& listeners)
    : SocketImpl<SocketConfig<websocketpp::config::asio_client>>(id, config, listeners)
{
}

WebsocketTpp::SocketWrapper::SocketWrapper(std::shared_ptr<Socket> impl)
    : _impl(std::move(impl))
{
}

WebsocketTpp::SocketWrapper::~SocketWrapper()
{
    if (auto socket = std::atomic_exchange(&_impl, std::shared_ptr<Socket>())) {
        socket->Close();
        _asioThread.detach();
    }
}

std::unique_ptr<WebsocketTpp::Socket> WebsocketTpp::SocketWrapper::
    Create(uint64_t id, const std::shared_ptr<const Config>& config,
           const std::shared_ptr<SocketListeners>& listeners)
{
    if (auto impl = Socket::Create(id, config, listeners)) {
        return std::make_unique<SocketWrapper>(std::move(impl));
    }
    return nullptr;
}

WebsocketState WebsocketTpp::SocketWrapper::GetState()
{
    if (const auto impl = std::atomic_load(&_impl)) {
        return impl->GetState();
    }
    return WebsocketState::Disconnected;
}

void WebsocketTpp::SocketWrapper::Run()
{
    if (const auto impl = std::atomic_load(&_impl)) {
        _asioThread = std::thread([implRef = std::weak_ptr<Socket>(impl)]() {
            if (const auto impl = implRef.lock()) {
                impl->Run();
            }
        });
    }
}

bool WebsocketTpp::SocketWrapper::Open()
{
    if (const auto impl = std::atomic_load(&_impl)) {
        return impl->Open();
    }
    return false;
}

void WebsocketTpp::SocketWrapper::Close()
{
    if (const auto impl = std::atomic_load(&_impl)) {
        impl->Close();
    }
}

bool WebsocketTpp::SocketWrapper::WriteBinary(const std::shared_ptr<Buffer>& buffer)
{
    if (buffer) {
        if (const auto impl = std::atomic_load(&_impl)) {
            return impl->WriteBinary(buffer);
        }
    }
    return false;
}

bool WebsocketTpp::SocketWrapper::WriteText(const std::string& text)
{
    if (const auto impl = std::atomic_load(&_impl)) {
        return impl->WriteText(text);
    }
    return false;
}

} // namespace RTC

namespace {

template<class MessagePtr>
MessageBuffer<MessagePtr>::MessageBuffer(MessagePtr message)
    : _message(std::move(message))
{
}

template<class MessagePtr>
MessageBuffer<MessagePtr>::~MessageBuffer()
{
    _message->recycle();
}

template<class MessagePtr>
size_t MessageBuffer<MessagePtr>::GetSize() const
{
    return _message->get_payload().size();
}

template<class MessagePtr>
uint8_t* MessageBuffer<MessagePtr>::GetData()
{
    return reinterpret_cast<uint8_t*>(_message->get_raw_payload().data());
}

template<class MessagePtr>
const uint8_t* MessageBuffer<MessagePtr>::GetData() const
{
    return reinterpret_cast<const uint8_t*>(_message->get_payload().data());
}

LogStreamBuf::LogStreamBuf(uint64_t socketId, LogLevel level)
    : _socketId(socketId)
    , _level(level)
{
}

std::streamsize LogStreamBuf::xsputn(const char* s, std::streamsize count)
{
    if (s && count) {
        if (IsAccepted(_level)) {
            _buffer += std::string(s, count);
        }
        return count;
    }
    return 0;
}

int LogStreamBuf::sync()
{
    if (IsAccepted(_level)) {
        Write(std::move(_buffer));
        return 0;
    }
    return -1;
}

bool LogStreamBuf::IsAccepted(LogLevel level)
{
    return Settings::configuration.logLevel >= level;
}

void LogStreamBuf::Write(LogLevel level, const std::string& message)
{
    if (IsAccepted(level) && !message.empty()) {
        std::string levelDesc;
        switch (level) {
            case LogLevel::LOG_DEBUG:
                MS_DEBUG_DEV_STD("Websocket debug: %s", message.c_str());
                break;
            case LogLevel::LOG_WARN:
                MS_WARN_DEV_STD("Websocket warning: %s", message.c_str());
                break;
            case LogLevel::LOG_ERROR:
                MS_ERROR_STD("Websocket error: %s", message.c_str());
                break;
            default:
                break;
        }
    }
}

void LogStreamBuf::Write(LogLevel level, uint64_t socketId, const std::string& message)
{
    if (IsAccepted(level) && !message.empty()) {
        Write(level, message + " (id is " + std::to_string(socketId) + ")");
    }
}

void LogStreamBuf::Write(const std::string& message) const
{
    Write(_level, _socketId, message);
}

}
