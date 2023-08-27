#define MS_CLASS "OutputWebSocketDevice"
#include "RTC/OutputWebSocketDevice.hpp"
#include "Logger.hpp"
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/close.hpp>
#include <mutex>
#include <atomic>

namespace RTC
{

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

class OutputWebSocketDevice::UriParts
{
public:
    UriParts(bool secure, std::string uri);
    bool IsSecure() const { return _secure; }
    const std::string& GetUri() const { return _uri; }
    static std::shared_ptr<UriParts> VerifyAndParse(std::string uri);
private:
    static inline const std::string _wssScheme = "wss";
    static inline const std::string _wsScheme = "ws";
    const bool _secure = false;
    const std::string _uri;
};

class OutputWebSocketDevice::Socket
{
public:
    virtual ~Socket() = default;
    virtual WebSocketState GetState() = 0;
    virtual void Run() = 0;
    virtual bool Open(const std::string& uri) = 0;
    virtual void Close() = 0;
    virtual bool WriteBinary(const void* buf, uint32_t len) = 0;
    virtual bool WriteText(const std::string& text) = 0;
    virtual void SetListener(const std::shared_ptr<WebSocketListener>& listener) = 0;
    static std::shared_ptr<Socket> Create(const std::shared_ptr<UriParts>& uri,
                                          std::unordered_map<std::string, std::string> headers,
                                          std::string tlsTrustStore,
                                          std::string tlsKeyStore,
                                          std::string tlsPrivateKey,
                                          std::string tlsPrivateKeyPassword);
};

template<class TConfig>
class OutputWebSocketDevice::SocketImpl : public Socket
{
    using Client = websocketpp::client<TConfig>;
    using MessagePtr = typename TConfig::message_type::ptr;
    using Mutex = std::recursive_mutex;
    using ReadLock = std::lock_guard<Mutex>;
    using WriteLock = ReadLock;
public:
    ~SocketImpl() override { Close(); }
    // impl. of Socket
    WebSocketState GetState() final;
    void Run() final;
    bool Open(const std::string& uri) final;
    void Close() final;
    bool WriteBinary(const void* buf, uint32_t len) final;
    bool WriteText(const std::string& text) final;
    void SetListener(const std::shared_ptr<WebSocketListener>& listener) final;
protected:
    SocketImpl(std::unordered_map<std::string, std::string> headers = {});
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
    static std::vector<uint8_t> ToBinary(std::string rawPayload);
private:
    const std::unordered_map<std::string, std::string> _headers;
    std::shared_ptr<WebSocketListener> _listener;
    Client _client;
    websocketpp::connection_hdl _hdl;
    Mutex _hdlMutex;
    std::atomic_bool _opened = false;
};

class OutputWebSocketDevice::SocketTls : public SocketImpl<websocketpp::config::asio_tls_client>
{
    using SslContextPtr = websocketpp::lib::shared_ptr<asio::ssl::context>;
public:
    SocketTls(std::unordered_map<std::string, std::string> headers,
              std::string tlsTrustStore,
              std::string tlsKeyStore,
              std::string tlsPrivateKey,
              std::string tlsPrivateKeyPassword);
private:
    SslContextPtr OnTlsInit(websocketpp::connection_hdl);
private:
    const std::string _tlsTrustStore;
    const std::string _tlsKeyStore;
    const std::string _tlsPrivateKey;
    const std::string _tlsPrivateKeyPassword;
};

class OutputWebSocketDevice::SocketNoTls : public SocketImpl<websocketpp::config::asio_client>
{
public:
    SocketNoTls(std::unordered_map<std::string, std::string> headers);
};


OutputWebSocketDevice::OutputWebSocketDevice(std::string uri,
                                             std::unordered_map<std::string, std::string> headers,
                                             std::string tlsTrustStore,
                                             std::string tlsKeyStore,
                                             std::string tlsPrivateKey,
                                             std::string tlsPrivateKeyPassword)
    : _uri(UriParts::VerifyAndParse(std::move(uri)))
    , _socket(Socket::Create(_uri, std::move(headers),
                             std::move(tlsTrustStore),
                             std::move(tlsKeyStore),
                             std::move(tlsPrivateKey),
                             std::move(tlsPrivateKeyPassword)))
{
}

OutputWebSocketDevice::~OutputWebSocketDevice()
{
    Close();
}

bool OutputWebSocketDevice::Open()
{
    bool result = false;
    if (_socket && _uri) {
        if (WebSocketState::Disconnected == _socket->GetState()) {
            result = _socket->Open(_uri->GetUri());
            if (result) {
                _asioThread = std::thread([socketRef = std::weak_ptr<Socket>(_socket)]() {
                    if (const auto socket = socketRef.lock()) {
                        socket->Run();
                    }
                });
            }
        }
        else { // connected or connecting now
            result = true;
        }
    }
    return result;
}

void OutputWebSocketDevice::Close()
{
    if (_socket) {
        _socket->Close();
        if (_asioThread.joinable()) {
            _asioThread.join();
        }
    }
}

WebSocketState OutputWebSocketDevice::GetState() const
{
    return _socket ? _socket->GetState() : WebSocketState::Invalid;
}

bool OutputWebSocketDevice::WriteText(const std::string& text)
{
    return _socket->WriteText(text);
}

void OutputWebSocketDevice::SetListener(const std::shared_ptr<WebSocketListener>& listener)
{
    _socket->SetListener(listener);
}

bool OutputWebSocketDevice::Write(const void* buf, uint32_t len)
{
    return _socket->WriteBinary(buf, len);
}

OutputWebSocketDevice::UriParts::UriParts(bool secure, std::string uri)
    : _secure(secure)
    , _uri(std::move(uri))
{
}

std::shared_ptr<OutputWebSocketDevice::UriParts> OutputWebSocketDevice::UriParts::VerifyAndParse(std::string uri)
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
                return std::make_shared<UriParts>(_wssScheme == protocol, std::move(uri));
            }
        }
    }
    return nullptr;
}

std::shared_ptr<OutputWebSocketDevice::Socket> OutputWebSocketDevice::Socket::
    Create(const std::shared_ptr<UriParts>& uri,
           std::unordered_map<std::string, std::string> headers,
           std::string tlsTrustStore,
           std::string tlsKeyStore,
           std::string tlsPrivateKey,
           std::string tlsPrivateKeyPassword)
{
    if (uri) {
        if (uri->IsSecure()) {
            return std::make_shared<SocketTls>(std::move(headers),
                                               std::move(tlsTrustStore),
                                               std::move(tlsKeyStore),
                                               std::move(tlsPrivateKey),
                                               std::move(tlsPrivateKeyPassword));
        }
        return std::make_shared<SocketNoTls>(std::move(headers));
    }
    return nullptr;
}

template<class TConfig>
OutputWebSocketDevice::SocketImpl<TConfig>::SocketImpl(std::unordered_map<std::string, std::string> headers)
    : _headers(std::move(headers))
{
    // Initialize ASIO
    _client.init_asio();
    // Register our handlers
    _client.set_socket_init_handler(bind(&SocketImpl::OnSocketInit, this, _1));
    _client.set_message_handler(bind(&SocketImpl::OnMessage, this, _1, _2));
    _client.set_open_handler(bind(&SocketImpl::OnOpen, this, _1));
    _client.set_close_handler(bind(&SocketImpl::OnClose, this, _1));
    _client.set_fail_handler(bind(&SocketImpl::OnFail, this, _1));
}

template<class TConfig>
WebSocketState OutputWebSocketDevice::SocketImpl<TConfig>::GetState()
{
    if (IsOpened()) {
        return WebSocketState::Connected;
    }
    const ReadLock lock(_hdlMutex);
    return _hdl.expired() ? WebSocketState::Disconnected : WebSocketState::Connecting;
}

template<class TConfig>
void OutputWebSocketDevice::SocketImpl<TConfig>::Run()
{
    _client.run();
    SetOpened(false);
}

template<class TConfig>
bool OutputWebSocketDevice::SocketImpl<TConfig>::Open(const std::string& uri)
{
    if (!uri.empty()) {
        websocketpp::lib::error_code ec;
        const auto connection = _client.get_connection(uri, ec);
        if (ec) {
            // write error to log
            MS_WARN_TAG(rtp, "Websocket get connection failure: %s", ec.message().c_str());
        }
        else {
            for (auto it = _headers.begin(); it != _headers.end(); ++it) {
                connection->append_header(it->first, it->second);
            }
            _client.connect(connection);
            return true;
        }
    }
    return false;
}

template<class TConfig>
void OutputWebSocketDevice::SocketImpl<TConfig>::Close()
{
    auto lock = std::make_unique<WriteLock>(_hdlMutex);
    if (!_hdl.expired()) {
        _client.close(_hdl, websocketpp::close::status::going_away, std::string());
        DropHdl(std::move(lock));
    }
}

template<class TConfig>
bool OutputWebSocketDevice::SocketImpl<TConfig>::WriteBinary(const void* buf, uint32_t len)
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
bool OutputWebSocketDevice::SocketImpl<TConfig>::WriteText(const std::string& text)
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
void OutputWebSocketDevice::SocketImpl<TConfig>::SetListener(const std::shared_ptr<WebSocketListener>& listener)
{
    std::atomic_store(&_listener, listener);
}

template<class TConfig>
void OutputWebSocketDevice::SocketImpl<TConfig>::OnSocketInit(websocketpp::connection_hdl hdl)
{
    {
        const WriteLock lock(_hdlMutex);
        _hdl = std::move(hdl);
    }
    if (const auto listener = std::atomic_load(&_listener)) {
        listener->OnStateChanged(GetState());
    }
}

template<class TConfig>
void OutputWebSocketDevice::SocketImpl<TConfig>::OnFail(websocketpp::connection_hdl hdl)
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
        if (const auto listener = std::atomic_load(&_listener)) {
            listener->OnFailed(error);
        }
    }
}

template<class TConfig>
void OutputWebSocketDevice::SocketImpl<TConfig>::OnOpen(websocketpp::connection_hdl hdl)
{
    auto lock = std::make_unique<ReadLock>(_hdlMutex);
    if (hdl.lock() == _hdl.lock()) {
        // update state
        SetOpened(true, std::move(lock));
    }
}

template<class TConfig>
void OutputWebSocketDevice::SocketImpl<TConfig>::OnMessage(websocketpp::connection_hdl hdl,
                                                           MessagePtr message)
{
    if (message) {
        bool accepted = false;
        {
            const ReadLock lock(_hdlMutex);
            accepted = hdl.lock() == _hdl.lock();
        }
        if (accepted) {
            if (const auto listener = std::atomic_load(&_listener)) {
                switch (message->get_opcode()) {
                    case websocketpp::frame::opcode::text:
                        listener->OnTextMessageReceived(std::move(message->get_raw_payload()));
                        break;
                    case websocketpp::frame::opcode::binary:
                        listener->OnBinaryMessageReceved(ToBinary(std::move(message->get_raw_payload())));
                        break;
                    default:
                        break;
                }
            }
        }
    }
}

template<class TConfig>
void OutputWebSocketDevice::SocketImpl<TConfig>::OnClose(websocketpp::connection_hdl hdl)
{
    auto lock = std::make_unique<WriteLock>(_hdlMutex);
    if (hdl.lock() == _hdl.lock()) {
        // report about close & reset state
        DropHdl(std::move(lock));
    }
}

template<class TConfig> template<class Lock>
void OutputWebSocketDevice::SocketImpl<TConfig>::DropHdl(std::unique_ptr<Lock> droppedLock)
{
    _hdl = websocketpp::connection_hdl();
    SetOpened(false, std::move(droppedLock));
}

template<class TConfig> template<class Lock>
bool OutputWebSocketDevice::SocketImpl<TConfig>::SetOpened(bool opened,
                                                           std::unique_ptr<Lock> droppedLock)
{
    if (opened != _opened.exchange(opened)) {
        if (const auto listener = std::atomic_load(&_listener)) {
            droppedLock.reset();
            listener->OnStateChanged(GetState());
        }
        return true;
    }
    return false;
}

template<class TConfig>
std::vector<uint8_t> OutputWebSocketDevice::SocketImpl<TConfig>::ToBinary(std::string rawPayload)
{
    if (!rawPayload.empty()) {
        
    }
    return {};
}

OutputWebSocketDevice::SocketTls::SocketTls(std::unordered_map<std::string, std::string> headers,
                                            std::string tlsTrustStore,
                                            std::string tlsKeyStore,
                                            std::string tlsPrivateKey,
                                            std::string tlsPrivateKeyPassword)
    : SocketImpl<websocketpp::config::asio_tls_client>(std::move(headers))
    , _tlsTrustStore(std::move(tlsTrustStore))
    , _tlsKeyStore(std::move(tlsKeyStore))
    , _tlsPrivateKey(std::move(tlsPrivateKey))
    , _tlsPrivateKeyPassword(std::move(tlsPrivateKeyPassword))
{
    GetClient().set_tls_init_handler(bind(&SocketTls::OnTlsInit, this, _1));
}

OutputWebSocketDevice::SocketTls::SslContextPtr OutputWebSocketDevice::SocketTls::OnTlsInit(websocketpp::connection_hdl)
{
    SslContextPtr ctx = websocketpp::lib::make_shared<asio::ssl::context>(asio::ssl::context::tlsv12_client);
    bool checkPeersVerification = false;
    if (!_tlsTrustStore.empty()) {
        ctx->add_certificate_authority(asio::buffer(_tlsTrustStore.data(), _tlsTrustStore.size()));
        checkPeersVerification = true;
    }
    if (!_tlsKeyStore.empty()) {
        ctx->use_certificate_chain(asio::buffer(_tlsKeyStore.data(), _tlsKeyStore.size()));
        checkPeersVerification = true;
    }
    if (!_tlsPrivateKey.empty()) {
        ctx->set_password_callback([this](std::size_t /*size*/, asio::ssl::context_base::password_purpose /*purpose*/) {
            return _tlsPrivateKeyPassword;
        });
        ctx->use_private_key(asio::buffer(_tlsPrivateKey.data(),_tlsPrivateKey.size()), asio::ssl::context::file_format::pem);
    }
    if (checkPeersVerification) {
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

OutputWebSocketDevice::SocketNoTls::SocketNoTls(std::unordered_map<std::string, std::string> headers)
    : SocketImpl<websocketpp::config::asio_client>(std::move(headers))
{
}

} // namespace RTC
