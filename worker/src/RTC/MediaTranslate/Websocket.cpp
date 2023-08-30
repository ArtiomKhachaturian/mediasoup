#define MS_CLASS "Websocket"
#include "RTC/MediaTranslate/Websocket.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/close.hpp>
#include <atomic>

namespace {

class StringMemoryBuffer : public MemoryBuffer
{
public:
    StringMemoryBuffer(std::string payload);
    size_t GetSize() const final { return _payload.size(); }
    uint8_t* GetData() final;
    const uint8_t* GetData() const final;
private:
    std::string _payload;
};

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
    virtual bool Open() = 0;
    virtual void Close() = 0;
    virtual bool WriteBinary(const void* buf, size_t len) = 0;
    virtual bool WriteText(const std::string& text) = 0;
    virtual void SetListener(const std::weak_ptr<WebsocketListener>& listener) = 0;
    static std::shared_ptr<Socket> Create(uint64_t id, const std::shared_ptr<const Config>& config);
};

template<class TConfig>
class Websocket::SocketImpl : public Socket
{
    using Client = websocketpp::client<TConfig>;
    using MessagePtr = typename TConfig::message_type::ptr;
public:
    ~SocketImpl() override { Close(); }
    // impl. of Socket
    WebsocketState GetState() final;
    void Run() final;
    bool Open() final;
    void Close() final;
    bool WriteBinary(const void* buf, size_t len) final;
    bool WriteText(const std::string& text) final;
    void SetListener(const std::weak_ptr<WebsocketListener>& listener) final;
protected:
    SocketImpl(uint64_t id, const std::shared_ptr<const Config>& config);
    uint64_t GetId() const { return _id; }
    const std::shared_ptr<const Config>& GetConfig() const { return _config; }
    const Client& GetClient() const { return _client; }
    Client& GetClient() { return _client; }
private:
    void OnSocketInit(websocketpp::connection_hdl hdl);
    void OnFail(websocketpp::connection_hdl hdl);
    void OnOpen(websocketpp::connection_hdl hdl);
    void OnMessage(websocketpp::connection_hdl hdl, MessagePtr message);
    void OnClose(websocketpp::connection_hdl hdl);
    template<class Lock>
    void DropHdl(std::unique_ptr<Lock> droppedLock);
    // return true if state changed
    template<class Lock = WriteLock>
    bool SetOpened(bool opened, std::unique_ptr<Lock> droppedLock = nullptr);
    bool IsOpened() const { return _opened.load(std::memory_order_relaxed); }
    std::shared_ptr<WebsocketListener> GetListener();
    static std::string ToText(const MessagePtr& message);
    static std::shared_ptr<MemoryBuffer> ToBinary(const MessagePtr& message);
private:
    static inline constexpr uint16_t _closeCode = websocketpp::close::status::going_away;
    const uint64_t _id;
    const std::shared_ptr<const Config> _config;
    Client _client;
    websocketpp::connection_hdl _hdl;
    Mutex _hdlMutex;
    std::weak_ptr<WebsocketListener> _listener;
    Mutex _listenerMutex;
    std::atomic_bool _opened = false;
};

class Websocket::SocketTls : public SocketImpl<websocketpp::config::asio_tls_client>
{
    using SslContextPtr = websocketpp::lib::shared_ptr<asio::ssl::context>;
public:
    SocketTls(uint64_t id, const std::shared_ptr<const Config>& config);
private:
    SslContextPtr OnTlsInit(websocketpp::connection_hdl);
};

class Websocket::SocketNoTls : public SocketImpl<websocketpp::config::asio_client>
{
public:
    SocketNoTls(uint64_t id, const std::shared_ptr<const Config>& config);
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
{
}

Websocket::~Websocket()
{
    Close();
}

bool Websocket::Open()
{
    bool result = false;
    if (_config) {
        const WriteLock lock(_socketMutex);
        if (!_socket) {
            _socket = Socket::Create(GetId(), _config);
            if (_socket) {
                _socket->SetListener(std::atomic_load(&_listener));
                result = _socket->Open();
                if (result) {
                    _socketAsioThread = std::thread([socketRef = std::weak_ptr<Socket>(_socket)]() {
                        if (const auto socket = socketRef.lock()) {
                            socket->Run();
                        }
                    });
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
    const WriteLock lock(_socketMutex);
    if (auto socket = std::move(_socket)) {
        const auto wasActive = WebsocketState::Disconnected != socket->GetState();
        socket->SetListener(std::weak_ptr<WebsocketListener>());
        socket->Close();
        _socketAsioThread.detach();
        if (wasActive) {
            if (const auto listener = std::atomic_load(&_listener)) {
                listener->OnStateChanged(GetId(), WebsocketState::Disconnected);
            }
        }
    }
}

WebsocketState Websocket::GetState() const
{
    if (_config) {
        const ReadLock lock(_socketMutex);
        if (_socket) {
            return _socket->GetState();
        }
        return WebsocketState::Disconnected;
    }
    return WebsocketState::Invalid;
}

uint64_t Websocket::GetId() const
{
    return reinterpret_cast<uint64_t>(this);
}

bool Websocket::WriteText(const std::string& text)
{
    const ReadLock lock(_socketMutex);
    return _socket && _socket->WriteText(text);
}

void Websocket::SetListener(const std::shared_ptr<WebsocketListener>& listener)
{
    if (_config) {
        if (listener != std::atomic_exchange(&_listener, listener)) {
            const ReadLock lock(_socketMutex);
            if (_socket) {
                _socket->SetListener(listener);
            }
        }
    }
}

bool Websocket::Write(const void* buf, size_t len)
{
    if (buf && len) {
        const ReadLock lock(_socketMutex);
        return _socket && _socket->WriteBinary(buf, len);
    }
    return false;
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
                                                             const std::shared_ptr<const Config>& config)
{
    if (config) {
        if (config->IsSecure()) {
            return std::make_shared<SocketTls>(id, config);
        }
        return std::make_shared<SocketNoTls>(id, config);
    }
    return nullptr;
}

template<class TConfig>
Websocket::SocketImpl<TConfig>::SocketImpl(uint64_t id, const std::shared_ptr<const Config>& config)
    : _id(id)
    , _config(config)
{
    // Initialize ASIO
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
WebsocketState Websocket::SocketImpl<TConfig>::GetState()
{
    if (IsOpened()) {
        return WebsocketState::Connected;
    }
    const ReadLock lock(_hdlMutex);
    return _hdl.expired() ? WebsocketState::Disconnected : WebsocketState::Connecting;
}

template<class TConfig>
void Websocket::SocketImpl<TConfig>::Run()
{
    _client.run();
    SetOpened(false);
}

template<class TConfig>
bool Websocket::SocketImpl<TConfig>::Open()
{
    websocketpp::lib::error_code ec;
    const auto connection = _client.get_connection(GetConfig()->GetUri(), ec);
    if (ec) {
        // write error to log
        MS_WARN_TAG(rtp, "Websocket get connection failure: %s", ec.message().c_str());
    }
    else {
        const auto& headers = GetConfig()->GetHeaders();
        for (auto it = headers.begin(); it != headers.end(); ++it) {
            connection->append_header(it->first, it->second);
        }
        _client.connect(connection);
    }
    return 0 == ec.value();
}

template<class TConfig>
void Websocket::SocketImpl<TConfig>::Close()
{
    auto lock = std::make_unique<WriteLock>(_hdlMutex);
    if (!_hdl.expired()) {
        _client.stop();
        _client.close(_hdl, _closeCode, websocketpp::close::status::get_string(_closeCode));
        DropHdl(std::move(lock));
    }
}

template<class TConfig>
bool Websocket::SocketImpl<TConfig>::WriteBinary(const void* buf, size_t len)
{
    bool ok = false;
    if (buf && len && IsOpened()) {
        const ReadLock lock(_hdlMutex);
        if (!_hdl.expired()) {
            websocketpp::lib::error_code ec;
            _client.send(_hdl, buf, len, websocketpp::frame::opcode::binary, ec);
            if (ec) {
                MS_WARN_TAG(rtp, "Websocket send binary failure: %s", ec.message().c_str());
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
        const ReadLock lock(_hdlMutex);
        if (!_hdl.expired()) {
            websocketpp::lib::error_code ec;
            _client.send(_hdl, text, websocketpp::frame::opcode::text, ec);
            if (ec) {
                MS_WARN_TAG(rtp, "Websocket send text failure: %s", ec.message().c_str());
            }
            else {
                ok = true;
            }
        }
    }
    return ok;
}

template<class TConfig>
void Websocket::SocketImpl<TConfig>::SetListener(const std::weak_ptr<WebsocketListener>& listener)
{
    const WriteLock lock(_listenerMutex);
    _listener = listener;
}

template<class TConfig>
void Websocket::SocketImpl<TConfig>::OnSocketInit(websocketpp::connection_hdl hdl)
{
    {
        const WriteLock lock(_hdlMutex);
        _hdl = std::move(hdl);
    }
    if (const auto listener = GetListener()) {
        listener->OnStateChanged(GetId(), GetState());
    }
}

template<class TConfig>
void Websocket::SocketImpl<TConfig>::OnFail(websocketpp::connection_hdl hdl)
{
    auto lock = std::make_unique<WriteLock>(_hdlMutex);
    if (hdl.lock() == _hdl.lock()) {
        std::string error;
        if (const auto connection = _client.get_con_from_hdl(hdl)) {
            error = connection->get_ec().message();
        }
        else {
            error = "unknown error";
        }
        MS_WARN_TAG(rtp, "Websocket general failure: %s", error.c_str());
        // report error & reset state
        DropHdl(std::move(lock));
        if (const auto listener = GetListener()) {
            listener->OnFailed(GetId(), error);
        }
    }
}

template<class TConfig>
void Websocket::SocketImpl<TConfig>::OnOpen(websocketpp::connection_hdl hdl)
{
    auto lock = std::make_unique<ReadLock>(_hdlMutex);
    if (hdl.lock() == _hdl.lock()) {
        // update state
        SetOpened(true, std::move(lock));
    }
}

template<class TConfig>
void Websocket::SocketImpl<TConfig>::OnMessage(websocketpp::connection_hdl hdl,
                                               MessagePtr message)
{
    if (message) {
        bool accepted = false;
        {
            const ReadLock lock(_hdlMutex);
            accepted = hdl.lock() == _hdl.lock();
        }
        if (accepted) {
            if (const auto listener = GetListener()) {
                switch (message->get_opcode()) {
                    case websocketpp::frame::opcode::text:
                        listener->OnTextMessageReceived(GetId(), ToText(message));
                        break;
                    case websocketpp::frame::opcode::binary:
                        listener->OnBinaryMessageReceved(GetId(), ToBinary(message));
                        break;
                    default:
                        break;
                }
            }
        }
    }
}

template<class TConfig>
void Websocket::SocketImpl<TConfig>::OnClose(websocketpp::connection_hdl hdl)
{
    auto lock = std::make_unique<WriteLock>(_hdlMutex);
    if (hdl.lock() == _hdl.lock()) {
        // report about close & reset state
        DropHdl(std::move(lock));
    }
}

template<class TConfig> template<class Lock>
void Websocket::SocketImpl<TConfig>::DropHdl(std::unique_ptr<Lock> droppedLock)
{
    _hdl = websocketpp::connection_hdl();
    SetOpened(false, std::move(droppedLock));
}

template<class TConfig> template<class Lock>
bool Websocket::SocketImpl<TConfig>::SetOpened(bool opened, std::unique_ptr<Lock> droppedLock)
{
    if (opened != _opened.exchange(opened)) {
        if (const auto listener = GetListener()) {
            droppedLock.reset();
            listener->OnStateChanged(GetId(), GetState());
        }
        return true;
    }
    return false;
}

template<class TConfig>
std::shared_ptr<WebsocketListener> Websocket::SocketImpl<TConfig>::GetListener()
{
    const ReadLock lock(_listenerMutex);
    return _listener.lock();
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

Websocket::SocketTls::SocketTls(uint64_t id, const std::shared_ptr<const Config>& config)
    : SocketImpl<websocketpp::config::asio_tls_client>(id, config)
{
    GetClient().set_tls_init_handler(bind(&SocketTls::OnTlsInit, this, _1));
}

Websocket::SocketTls::SslContextPtr Websocket::SocketTls::OnTlsInit(websocketpp::connection_hdl)
{
    SslContextPtr ctx = websocketpp::lib::make_shared<asio::ssl::context>(asio::ssl::context::tlsv12_client);
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
    try {
        ctx->set_options(asio::ssl::context::default_workarounds |
                         asio::ssl::context::no_sslv2 |
                         asio::ssl::context::no_sslv3 |
                         asio::ssl::context::no_tlsv1 |
                         asio::ssl::context::no_tlsv1_1 |
                         asio::ssl::context::single_dh_use);
    } catch (const std::exception& e) {
        MS_WARN_TAG(rtp, "Websocket SSL set options failure: %s", e.what());
    }
    return ctx;
}

Websocket::SocketNoTls::SocketNoTls(uint64_t id, const std::shared_ptr<const Config>& config)
    : SocketImpl<websocketpp::config::asio_client>(id, config)
{
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

}
