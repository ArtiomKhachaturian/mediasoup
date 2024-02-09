#define MS_CLASS "Websocket"
#include "RTC/MediaTranslate/Websocket.hpp"
#include "RTC/MediaTranslate/WebsocketListener.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/close.hpp>
#include <thread>
#include <atomic>

namespace {

class StringMemoryBuffer : public RTC::MemoryBuffer
{
public:
    StringMemoryBuffer(std::string payload);
    size_t GetSize() const final { return _payload.size(); }
    uint8_t* GetData() final;
    const uint8_t* GetData() const final;
private:
    std::string _payload;
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

inline std::string ToString(RTC::WebsocketListener::FailureType failure) {
    switch (failure) {
        case RTC::WebsocketListener::FailureType::General:
            return "general";
        case RTC::WebsocketListener::FailureType::NoConnection:
            return "no connection";
        case RTC::WebsocketListener::FailureType::CustomHeader:
            return "custom header";
        case RTC::WebsocketListener::FailureType::WriteText:
            return "write text";
        case RTC::WebsocketListener::FailureType::WriteBinary:
            return "write binary";
        case RTC::WebsocketListener::FailureType::TlsOptions:
            return "TLS options";
        default:
            break;
    }
    return "unknown";
}

inline std::string ToString(RTC::WebsocketState state) {
    switch (state) {
        case RTC::WebsocketState::Invalid:
            return "invalid";
        case RTC::WebsocketState::Connecting:
            return "connecting";
        case RTC::WebsocketState::Connected:
            return "connected";
        case RTC::WebsocketState::Disconnected:
            return "disconnected";
        default:
            break;
    }
    return "unknown";
}

}

namespace RTC
{

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

class Websocket::Config
{
public:
    static std::shared_ptr<const Config> VerifyAndParse(const std::string& uri,
                                                        const std::string& user,
                                                        const std::string& password,
                                                        std::unordered_map<std::string, std::string> headers,
                                                        std::string tlsTrustStore,
                                                        std::string tlsKeyStore,
                                                        std::string tlsPrivateKey,
                                                        std::string tlsPrivateKeyPassword);
    Config(const std::shared_ptr<websocketpp::uri>& uri,
           std::unordered_map<std::string, std::string> headers,
           std::string tlsTrustStore,
           std::string tlsKeyStore,
           std::string tlsPrivateKey,
           std::string tlsPrivateKeyPassword);
    bool IsSecure() const { return _uri->get_secure(); }
    const std::shared_ptr<websocketpp::uri>& GetUri() const { return _uri; }
    const std::unordered_map<std::string, std::string>& GetHeaders() const { return _headers; }
    const std::string& GetTlsTrustStore() const { return _tlsTrustStore; }
    const std::string& GetTlsKeyStore() const { return _tlsKeyStore; }
    const std::string& GetTlsPrivateKey() const { return _tlsPrivateKey; }
    const std::string& GetTlsPrivateKeyPassword() const { return _tlsPrivateKeyPassword; }
private:
    const std::shared_ptr<websocketpp::uri> _uri;
    const std::unordered_map<std::string, std::string> _headers;
    const std::string _tlsTrustStore;
    const std::string _tlsKeyStore;
    const std::string _tlsPrivateKey;
    const std::string _tlsPrivateKeyPassword;
};

class Websocket::Socket
{
public:
    virtual ~Socket() = default;
    virtual WebsocketState GetState() = 0;
    virtual void Run() = 0;
    virtual bool Open(const std::string& userAgent) = 0;
    virtual void Close() = 0;
    virtual bool WriteBinary(const MemoryBuffer& buffer) = 0;
    virtual bool WriteText(const std::string& text) = 0;
    static std::shared_ptr<Socket> Create(uint64_t id, const std::shared_ptr<const Config>& config,
                                          const std::shared_ptr<SocketListeners>& listeners);
};

template<class TConfig>
class Websocket::SocketImpl : public Socket
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
    bool Open(const std::string& userAgent) final;
    void Close() final;
    bool WriteBinary(const MemoryBuffer& buffer) final;
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
    static std::string ToText(const MessagePtr& message);
    static std::shared_ptr<MemoryBuffer> ToBinary(const MessagePtr& message);
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

class Websocket::SocketTls : public SocketImpl<websocketpp::config::asio_tls_client>
{
    using SslContextPtr = websocketpp::lib::shared_ptr<asio::ssl::context>;
public:
    SocketTls(uint64_t id, const std::shared_ptr<const Config>& config,
              const std::shared_ptr<SocketListeners>& listeners);
private:
    SslContextPtr OnTlsInit(websocketpp::connection_hdl);
};

class Websocket::SocketNoTls : public SocketImpl<websocketpp::config::asio_client>
{
public:
    SocketNoTls(uint64_t id, const std::shared_ptr<const Config>& config,
                const std::shared_ptr<SocketListeners>& listeners);
};

class Websocket::SocketWrapper : public Socket
{
public:
    SocketWrapper(std::shared_ptr<Socket> impl);
    ~SocketWrapper() final;
    static std::unique_ptr<Socket> Create(uint64_t id, const std::shared_ptr<const Config>& config,
                                          const std::shared_ptr<SocketListeners>& listeners);
    // impl. of Socket
    WebsocketState GetState() final;
    void Run() final;
    bool Open(const std::string& userAgent) final;
    void Close() final;
    bool WriteBinary(const MemoryBuffer& buffer) final;
    bool WriteText(const std::string& text) final;
private:
    std::shared_ptr<Socket> _impl;
    std::thread _asioThread;
};

Websocket::Websocket(const std::string& uri,
                     const std::string& user,
                     const std::string& password,
                     std::unordered_map<std::string, std::string> headers,
                     std::string tlsTrustStore,
                     std::string tlsKeyStore,
                     std::string tlsPrivateKey,
                     std::string tlsPrivateKeyPassword)
    : _config(Config::VerifyAndParse(uri, user, password,
                                     std::move(headers),
                                     std::move(tlsTrustStore),
                                     std::move(tlsKeyStore),
                                     std::move(tlsPrivateKey),
                                     std::move(tlsPrivateKeyPassword)))
    , _listeners(std::make_shared<SocketListeners>())
{
}

Websocket::~Websocket()
{
    Close();
}

bool Websocket::Open(const std::string& userAgent)
{
    bool result = false;
    if (_config) {
        LOCK_WRITE_PROTECTED_OBJ(_socket);
        if (!_socket.ConstRef()) {
            auto socket = SocketWrapper::Create(GetId(), _config, _listeners);
            if (socket) {
                result = socket->Open(userAgent);
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

void Websocket::Close()
{
    LOCK_WRITE_PROTECTED_OBJ(_socket);
    _socket.Take().reset();
}

WebsocketState Websocket::GetState() const
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

uint64_t Websocket::GetId() const
{
    return reinterpret_cast<uint64_t>(this);
}

bool Websocket::WriteBinary(const MemoryBuffer& buffer)
{
    LOCK_READ_PROTECTED_OBJ(_socket);
    if (const auto& socket = _socket.ConstRef()) {
        return socket->WriteBinary(buffer);
    }
    return false;
}

bool Websocket::WriteText(const std::string& text)
{
    LOCK_READ_PROTECTED_OBJ(_socket);
    if (const auto& socket = _socket.ConstRef()) {
        return socket->WriteText(text);
    }
    return false;
}

void Websocket::AddListener(WebsocketListener* listener)
{
    _listeners->Add(listener);
}

void Websocket::RemoveListener(WebsocketListener* listener)
{
    _listeners->Remove(listener);
}

Websocket::Config::Config(const std::shared_ptr<websocketpp::uri>& uri,
                          std::unordered_map<std::string, std::string> headers,
                          std::string tlsTrustStore,
                          std::string tlsKeyStore,
                          std::string tlsPrivateKey,
                          std::string tlsPrivateKeyPassword)
    : _uri(uri)
    , _headers(std::move(headers))
    , _tlsTrustStore(std::move(tlsTrustStore))
    , _tlsKeyStore(std::move(tlsKeyStore))
    , _tlsPrivateKey(std::move(tlsPrivateKey))
    , _tlsPrivateKeyPassword(std::move(tlsPrivateKeyPassword))
{
}

std::shared_ptr<const Websocket::Config> Websocket::Config::VerifyAndParse(const std::string& uri,
                                                                           const std::string& user,
                                                                           const std::string& password,
                                                                           std::unordered_map<std::string, std::string> headers,
                                                                           std::string tlsTrustStore,
                                                                           std::string tlsKeyStore,
                                                                           std::string tlsPrivateKey,
                                                                           std::string tlsPrivateKeyPassword)
{
    if (!uri.empty()) {
        auto validUri = std::make_shared<websocketpp::uri>(uri);
        if (validUri->get_valid()) {
            if (!user.empty() || !password.empty()) {
                auto auth = Utils::String::Base64Encode(user + ":" + password);
                headers["Authorization"] = "Basic " + auth;
            }
            return std::make_shared<Config>(validUri, std::move(headers),
                                            std::move(tlsTrustStore),
                                            std::move(tlsKeyStore),
                                            std::move(tlsPrivateKey),
                                            std::move(tlsPrivateKeyPassword));
        }
        MS_WARN_TAG(rtp, "invalid web socket URI");
    }
    return nullptr;
}

std::shared_ptr<Websocket::Socket> Websocket::Socket::Create(uint64_t id,
                                                             const std::shared_ptr<const Config>& config,
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
Websocket::SocketImpl<TConfig>::SocketImpl(uint64_t id, const std::shared_ptr<const Config>& config,
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
Websocket::SocketImpl<TConfig>::~SocketImpl()
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
WebsocketState Websocket::SocketImpl<TConfig>::GetState()
{
    if (!IsOpened()) {
        LOCK_READ_PROTECTED_OBJ(_hdl);
        return _hdl->expired() ? WebsocketState::Disconnected : WebsocketState::Connecting;
    }
    return WebsocketState::Connected;
}

template<class TConfig>
void Websocket::SocketImpl<TConfig>::Run()
{
    _client.run();
    SetOpened(false);
}

template<class TConfig>
bool Websocket::SocketImpl<TConfig>::Open(const std::string& userAgent)
{
    websocketpp::lib::error_code ec;
    const auto connection = _client.get_connection(GetConfig()->GetUri(), ec);
    if (ec) {
        InvokeListenersMethod(&WebsocketListener::OnFailed,
                              WebsocketListener::FailureType::NoConnection,
                              ec.message());
    }
    else {
        const auto& headers = GetConfig()->GetHeaders();
        for (auto it = headers.begin(); it != headers.end(); ++it) {
            try {
                connection->append_header(it->first, it->second);
            }
            catch(const std::exception& e) {
                InvokeListenersMethod(&WebsocketListener::OnFailed,
                                      WebsocketListener::FailureType::CustomHeader,
                                      e.what());
                return false;
            }
        }
        if (!userAgent.empty()) {
            _client.set_user_agent(userAgent);
        }
        _client.connect(connection);
    }
    return !ec;
}

template<class TConfig>
void Websocket::SocketImpl<TConfig>::Close()
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
bool Websocket::SocketImpl<TConfig>::WriteBinary(const MemoryBuffer& buffer)
{
    bool ok = false;
    if (IsOpened()) {
        LOCK_READ_PROTECTED_OBJ(_hdl);
        if (!_hdl->expired()) {
            websocketpp::lib::error_code ec;
            // overhead - deep copy of input buffer,
            // Websocketpp doesn't supports of buffers abstraction
            _client.send(_hdl, buffer.GetData(), buffer.GetSize(), websocketpp::frame::opcode::binary, ec);
            if (ec) {
                InvokeListenersMethod(&WebsocketListener::OnFailed,
                                      WebsocketListener::FailureType::WriteBinary,
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
bool Websocket::SocketImpl<TConfig>::WriteText(const std::string& text)
{
    bool ok = false;
    if (IsOpened()) {
        LOCK_READ_PROTECTED_OBJ(_hdl);
        if (!_hdl->expired()) {
            websocketpp::lib::error_code ec;
            _client.send(_hdl, text, websocketpp::frame::opcode::text, ec);
            if (ec) {
                InvokeListenersMethod(&WebsocketListener::OnFailed,
                                      WebsocketListener::FailureType::WriteText,
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
void Websocket::SocketImpl<TConfig>::InvokeListenersMethod(const Method& method,
                                                           Args&&... args) const
{
    _listeners->InvokeMethod(method, GetId(), std::forward<Args>(args)...);
}

template<class TConfig>
void Websocket::SocketImpl<TConfig>::OnSocketInit(websocketpp::connection_hdl hdl)
{
    {
        LOCK_WRITE_PROTECTED_OBJ(_hdl);
        _hdl = std::move(hdl);
    }
    InvokeListenersMethod(&WebsocketListener::OnStateChanged, GetState());
}

template<class TConfig>
void Websocket::SocketImpl<TConfig>::OnFail(websocketpp::connection_hdl hdl)
{
    auto droppedGuard = std::make_unique<HdlWriteGuard>(_hdl.GetWriteGuard());
    if (hdl.lock() == _hdl->lock()) {
        std::string error;
        if (const auto connection = _client.get_con_from_hdl(hdl)) {
            error = connection->get_ec().message();
        }
        else {
            error = "unknown error";
        }
        _client.stop();
        // report error & reset state
        DropHdl(std::move(droppedGuard));
        InvokeListenersMethod(&WebsocketListener::OnFailed,
                              WebsocketListener::FailureType::General,
                              error);
    }
}

template<class TConfig>
void Websocket::SocketImpl<TConfig>::OnOpen(websocketpp::connection_hdl hdl)
{
    auto droppedGuard = std::make_unique<HdlReadGuard>(_hdl.GetReadGuard());
    if (hdl.lock() == _hdl->lock()) {
        // update state
        SetOpened(true, std::move(droppedGuard));
    }
}

template<class TConfig>
void Websocket::SocketImpl<TConfig>::OnMessage(websocketpp::connection_hdl hdl,
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
                    InvokeListenersMethod(&WebsocketListener::OnTextMessageReceived, ToText(message));
                    break;
                case websocketpp::frame::opcode::binary:
                    InvokeListenersMethod(&WebsocketListener::OnBinaryMessageReceved, ToBinary(message));
                    break;
                default:
                    break;
            }
        }
    }
}

template<class TConfig>
void Websocket::SocketImpl<TConfig>::OnClose(websocketpp::connection_hdl hdl)
{
    auto droppedGuard = std::make_unique<HdlWriteGuard>(_hdl.GetWriteGuard());
    if (hdl.lock() == _hdl->lock()) {
        // report about close & reset state
        DropHdl(std::move(droppedGuard));
    }
}

template<class TConfig> template<class Guard>
void Websocket::SocketImpl<TConfig>::DropHdl(std::unique_ptr<Guard> droppedGuard)
{
    _hdl = websocketpp::connection_hdl();
    SetOpened(false, std::move(droppedGuard));
}

template<class TConfig> template<class Guard>
bool Websocket::SocketImpl<TConfig>::SetOpened(bool opened, std::unique_ptr<Guard> droppedGuard)
{
    if (opened != _opened.exchange(opened)) {
        droppedGuard.reset();
        InvokeListenersMethod(&WebsocketListener::OnStateChanged, GetState());
        return true;
    }
    return false;
}

template<class TConfig>
std::string Websocket::SocketImpl<TConfig>::ToText(const MessagePtr& message)
{
    if (message) {
        return std::move(message->get_raw_payload());
    }
    return std::string();
}

template<class TConfig>
std::shared_ptr<MemoryBuffer> Websocket::SocketImpl<TConfig>::ToBinary(const MessagePtr& message)
{
    if (message) {
        return std::make_shared<StringMemoryBuffer>(std::move(message->get_raw_payload()));
    }
    return nullptr;
}

Websocket::SocketTls::SocketTls(uint64_t id, const std::shared_ptr<const Config>& config,
                                const std::shared_ptr<SocketListeners>& listeners)
    : SocketImpl<websocketpp::config::asio_tls_client>(id, config, listeners)
{
    GetClient().set_tls_init_handler(bind(&SocketTls::OnTlsInit, this, _1));
}

Websocket::SocketTls::SslContextPtr Websocket::SocketTls::OnTlsInit(websocketpp::connection_hdl)
{
    SslContextPtr ctx = websocketpp::lib::make_shared<asio::ssl::context>(asio::ssl::context::tlsv12_client);
    try {
        const auto& tlsTrustStore = GetConfig()->GetTlsTrustStore();
        if (!tlsTrustStore.empty()) {
            ctx->add_certificate_authority(asio::buffer(tlsTrustStore.data(), tlsTrustStore.size()));
        }
        const auto& tlsKeyStore = GetConfig()->GetTlsTrustStore();
        if (!tlsKeyStore.empty()) {
            ctx->use_certificate_chain(asio::buffer(tlsKeyStore.data(), tlsKeyStore.size()));
        }
        const auto& tlsPrivateKey = GetConfig()->GetTlsPrivateKey();
        if (!tlsPrivateKey.empty()) {
            ctx->set_password_callback([config = GetConfig()](std::size_t /*size*/,
                                                              asio::ssl::context_base::password_purpose /*purpose*/) {
                return config->GetTlsPrivateKeyPassword();
            });
            ctx->use_private_key(asio::buffer(tlsPrivateKey.data(), tlsPrivateKey.size()),
                                 asio::ssl::context::file_format::pem);
        }
        if (!tlsTrustStore.empty() || !tlsKeyStore.empty()) { // maybe 'and' (&&) ?
            // Activates verification mode and rejects unverified peers
            ctx->set_verify_mode(asio::ssl::context::verify_peer | asio::ssl::context::verify_fail_if_no_peer_cert);
        }
        ctx->set_options(asio::ssl::context::default_workarounds |
                         asio::ssl::context::no_sslv2 |
                         asio::ssl::context::no_sslv3 |
                         asio::ssl::context::no_tlsv1 |
                         asio::ssl::context::no_tlsv1_1 |
                         asio::ssl::context::single_dh_use);
    } catch (const std::exception& e) {
        InvokeListenersMethod(&WebsocketListener::OnFailed,
                              WebsocketListener::FailureType::TlsOptions, e.what());
    }
    return ctx;
}

Websocket::SocketNoTls::SocketNoTls(uint64_t id, const std::shared_ptr<const Config>& config,
                                    const std::shared_ptr<SocketListeners>& listeners)
    : SocketImpl<websocketpp::config::asio_client>(id, config, listeners)
{
}

Websocket::SocketWrapper::SocketWrapper(std::shared_ptr<Socket> impl)
    : _impl(std::move(impl))
{
}

Websocket::SocketWrapper::~SocketWrapper()
{
    if (auto socket = std::atomic_exchange(&_impl, std::shared_ptr<Socket>())) {
        socket->Close();
        _asioThread.detach();
    }
}

std::unique_ptr<Websocket::Socket> Websocket::SocketWrapper::Create(uint64_t id,
                                                                    const std::shared_ptr<const Config>& config,
                                                                    const std::shared_ptr<SocketListeners>& listeners)
{
    if (auto impl = Socket::Create(id, config, listeners)) {
        return std::make_unique<SocketWrapper>(std::move(impl));
    }
    return nullptr;
}

WebsocketState Websocket::SocketWrapper::GetState()
{
    if (const auto impl = std::atomic_load(&_impl)) {
        return impl->GetState();
    }
    return WebsocketState::Disconnected;
}

void Websocket::SocketWrapper::Run()
{
    if (const auto impl = std::atomic_load(&_impl)) {
        _asioThread = std::thread([implRef = std::weak_ptr<Socket>(impl)]() {
            if (const auto impl = implRef.lock()) {
                impl->Run();
            }
        });
    }
}

bool Websocket::SocketWrapper::Open(const std::string& userAgent)
{
    if (const auto impl = std::atomic_load(&_impl)) {
        return impl->Open(userAgent);
    }
    return false;
}

void Websocket::SocketWrapper::Close()
{
    if (const auto impl = std::atomic_load(&_impl)) {
        impl->Close();
    }
}

bool Websocket::SocketWrapper::WriteBinary(const MemoryBuffer& buffer)
{
    if (const auto impl = std::atomic_load(&_impl)) {
        return impl->WriteBinary(buffer);
    }
    return false;
}

bool Websocket::SocketWrapper::WriteText(const std::string& text)
{
    if (const auto impl = std::atomic_load(&_impl)) {
        return impl->WriteText(text);
    }
    return false;
}

void WebsocketListener::OnStateChanged(uint64_t socketId, WebsocketState state)
{
    if (LogStreamBuf::IsAccepted(LogLevel::LOG_DEBUG)) {
        LogStreamBuf::Write(LogLevel::LOG_DEBUG, socketId, "state changed to " + ToString(state));
    }
}

void WebsocketListener::OnFailed(uint64_t socketId, FailureType type, const std::string& what)
{
    if (LogStreamBuf::IsAccepted(LogLevel::LOG_ERROR)) {
        LogStreamBuf::Write(LogLevel::LOG_ERROR, socketId, ToString(type) + " - " + what);
    }
}

} // namespace RTC

namespace {

StringMemoryBuffer::StringMemoryBuffer(std::string payload)
    : _payload(std::move(payload))
{
}

uint8_t* StringMemoryBuffer::GetData()
{
    return reinterpret_cast<uint8_t*>(_payload.data());
}

const uint8_t* StringMemoryBuffer::GetData() const
{
    return reinterpret_cast<const uint8_t*>(_payload.data());
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
