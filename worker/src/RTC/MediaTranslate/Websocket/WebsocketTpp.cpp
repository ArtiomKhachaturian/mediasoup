#define MS_CLASS "RTC::WebsocketTpp"
#include "RTC/MediaTranslate/Websocket/WebsocketTpp.hpp"
#include "RTC/MediaTranslate/Websocket/WebsocketListener.hpp"
#include "RTC/MediaTranslate/Websocket/WebsocketFailure.hpp"
#include "RTC/MediaTranslate/Websocket/WebsocketTppUtils.hpp"
#include "RTC/MediaTranslate/ThreadExecution.hpp"
#include "RTC/Buffers/Buffer.hpp"
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

template <class TConfig>
struct ExtendedConfig : public TConfig
{
    static const size_t connection_read_buffer_size = Websocket::GetConnectionReadBufferSize();
};

}

namespace RTC
{

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;
using namespace websocketpp::config;

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

class WebsocketTpp::Api
{
public:
    virtual ~Api() = default;
    virtual WebsocketState GetState() = 0;
    virtual void Run() = 0;
    virtual bool Open() = 0;
    virtual void Close() = 0;
    virtual bool WriteBinary(const std::shared_ptr<Buffer>& buffer) = 0;
    virtual bool WriteText(const std::string& text) = 0;
    static std::shared_ptr<Api> Create(uint64_t id, const std::shared_ptr<const Config>& config,
                                       const std::shared_ptr<SocketListeners>& listeners);
};

template<class TConfig>
class WebsocketTpp::Impl : public Api
{
    using Client = websocketpp::client<TConfig>;
    using MessagePtr = typename TConfig::message_type::ptr;
public:
    ~Impl() override;
    // impl. of Api
    WebsocketState GetState() final;
    void Run() final;
    bool Open() final;
    void Close() final;
    bool WriteBinary(const std::shared_ptr<Buffer>& buffer) final;
    bool WriteText(const std::string& text) final;
protected:
    Impl(uint64_t id, const std::shared_ptr<const Config>& config,
         const std::shared_ptr<SocketListeners>& listeners);
    uint64_t GetId() const { return _id; }
    const std::shared_ptr<websocketpp::uri>& GetUri() const { return _config->GetUri(); }
    const WebsocketOptions& GetOptions() const { return _config->GetOptions(); }
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
    // return true if state changed
    bool SetOpened(bool opened);
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

class WebsocketTpp::TlsOn : public Impl<ExtendedConfig<asio_tls_client>>
{
    using SslContextPtr = websocketpp::lib::shared_ptr<asio::ssl::context>;
public:
    TlsOn(uint64_t id, const std::shared_ptr<const Config>& config,
          const std::shared_ptr<SocketListeners>& listeners);
private:
    SslContextPtr OnTlsInit(websocketpp::connection_hdl);
};

class WebsocketTpp::TlsOff : public Impl<ExtendedConfig<asio_client>>
{
public:
    TlsOff(uint64_t id, const std::shared_ptr<const Config>& config,
           const std::shared_ptr<SocketListeners>& listeners);
};

class WebsocketTpp::Wrapper : public Api, private ThreadExecution
{
public:
    Wrapper(std::string socketName, std::shared_ptr<Api> api);
    ~Wrapper() final;
    static std::unique_ptr<Api> Create(uint64_t id,
                                       const std::shared_ptr<const Config>& config,
                                       const std::shared_ptr<SocketListeners>& listeners);
    // impl. of Api
    WebsocketState GetState() final;
    void Run() final { StartExecution(); }
    bool Open() final;
    void Close() final;
    bool WriteBinary(const std::shared_ptr<Buffer>& buffer) final;
    bool WriteText(const std::string& text) final;
protected:
    // impl. of ThreadExecution
    void DoExecuteInThread() final;
    void DoStopThread() final;
private:
    std::shared_ptr<Api> _api;
};

WebsocketTpp::WebsocketTpp(const std::string& uri, WebsocketOptions options)
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
        LOCK_WRITE_PROTECTED_OBJ(_api);
        if (!_api.ConstRef()) {
            auto api = Wrapper::Create(GetId(), _config, _listeners);
            if (api) {
                result = api->Open();
                if (result) {
                    api->Run();
                    _api = std::move(api);
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
    LOCK_WRITE_PROTECTED_OBJ(_api);
    _api.Take().reset();
}

WebsocketState WebsocketTpp::GetState() const
{
    if (_config) {
        LOCK_READ_PROTECTED_OBJ(_api);
        if (const auto& api = _api.ConstRef()) {
            return api->GetState();
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
    LOCK_READ_PROTECTED_OBJ(_api);
    if (const auto& api = _api.ConstRef()) {
        return api->WriteText(text);
    }
    return false;
}

bool WebsocketTpp::WriteBinary(const std::shared_ptr<Buffer>& buffer)
{
    if (buffer) {
        LOCK_READ_PROTECTED_OBJ(_api);
        if (const auto& api = _api.ConstRef()) {
            return api->WriteBinary(buffer);
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
            MS_ERROR("invalid web socket URI %s", uri.c_str());
        }
    }
    return config;
}

std::shared_ptr<WebsocketTpp::Api> WebsocketTpp::Api::Create(uint64_t id,
                                                             const std::shared_ptr<const Config>& config,
                                                             const std::shared_ptr<SocketListeners>& listeners)
{
    if (config) {
        if (config->IsSecure()) {
            return std::make_shared<TlsOn>(id, config, listeners);
        }
        return std::make_shared<TlsOff>(id, config, listeners);
    }
    return nullptr;
}

template<class TConfig>
WebsocketTpp::Impl<TConfig>::Impl(uint64_t id, const std::shared_ptr<const Config>& config,
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
    _client.set_user_agent(GetOptions()._userAgent);
    _client.get_alog().set_ostream(&_debugStream);
    _client.get_elog().set_ostream(&_errorStream);
    _client.init_asio();
    _client.start_perpetual();
    // Register our handlers
    _client.set_socket_init_handler(bind(&Impl::OnSocketInit, this, _1));
    _client.set_message_handler(bind(&Impl::OnMessage, this, _1, _2));
    _client.set_open_handler(bind(&Impl::OnOpen, this, _1));
    _client.set_close_handler(bind(&Impl::OnClose, this, _1));
    _client.set_fail_handler(bind(&Impl::OnFail, this, _1));
}

template<class TConfig>
WebsocketTpp::Impl<TConfig>::~Impl()
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
WebsocketState WebsocketTpp::Impl<TConfig>::GetState()
{
    if (!IsOpened()) {
        LOCK_READ_PROTECTED_OBJ(_hdl);
        return _hdl->expired() ? WebsocketState::Disconnected : WebsocketState::Connecting;
    }
    return WebsocketState::Connected;
}

template<class TConfig>
void WebsocketTpp::Impl<TConfig>::Run()
{
    _client.run();
    SetOpened(false);
}

template<class TConfig>
bool WebsocketTpp::Impl<TConfig>::Open()
{
    websocketpp::lib::error_code ec;
    const auto connection = _client.get_connection(GetUri(), ec);
    if (ec) {
        InvokeListenersMethod(&WebsocketListener::OnFailed,
                              WebsocketFailure::NoConnection,
                              ec.message());
    }
    else {
        const auto& extraHeaders = GetOptions()._extraHeaders;
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
        if (!GetOptions()._userAgent.empty()) {
            _client.set_user_agent(GetOptions()._userAgent);
        }
        _client.connect(connection);
    }
    return !ec;
}

template<class TConfig>
void WebsocketTpp::Impl<TConfig>::Close()
{
    websocketpp::connection_hdl hdl;
    {
        LOCK_WRITE_PROTECTED_OBJ(_hdl);
        hdl = _hdl.Take();
    }
    if (!hdl.expired()) {
        websocketpp::lib::error_code ec;
        _client.close(hdl, _closeCode, websocketpp::close::status::get_string(_closeCode), ec);
        _client.stop();
        if (ec) { // ignore of failures during closing
            _errorStreamBuf.Write(ec.message());
        }
        SetOpened(false);
    }
}

template<class TConfig>
bool WebsocketTpp::Impl<TConfig>::WriteBinary(const std::shared_ptr<Buffer>& buffer)
{
    bool ok = false;
    if (buffer && IsOpened()) {
        websocketpp::lib::error_code ec;
        {
            LOCK_READ_PROTECTED_OBJ(_hdl);
            if (!_hdl->expired()) {
                // overhead - deep copy of input buffer,
                // Websocketpp doesn't supports of buffers abstraction
                _client.send(_hdl, buffer->GetData(), buffer->GetSize(),
                             websocketpp::frame::opcode::binary, ec);
                if (!ec) {
                    ok = true;
                }
            }
        }
        if (!ok && ec) {
            InvokeListenersMethod(&WebsocketListener::OnFailed,
                                  WebsocketFailure::WriteBinary,
                                  ec.message());
        }
    }
    return ok;
}

template<class TConfig>
bool WebsocketTpp::Impl<TConfig>::WriteText(const std::string& text)
{
    bool ok = false;
    if (IsOpened()) {
        websocketpp::lib::error_code ec;
        {
            LOCK_READ_PROTECTED_OBJ(_hdl);
            if (!_hdl->expired()) {
                _client.send(_hdl, text, websocketpp::frame::opcode::text, ec);
                if (!ec) {
                    ok = true;
                }
            }
        }
        if (!ok && ec) {
            InvokeListenersMethod(&WebsocketListener::OnFailed,
                                  WebsocketFailure::WriteText,
                                  ec.message());
        }
    }
    return ok;
}

template<class TConfig>
template <class Method, typename... Args>
void WebsocketTpp::Impl<TConfig>::InvokeListenersMethod(const Method& method, 
                                                        Args&&... args) const
{
    _listeners->InvokeMethod(method, GetId(), std::forward<Args>(args)...);
}

template<class TConfig>
template<class TOption>
void WebsocketTpp::Impl<TConfig>::SetTcpSocketOption(const TOption& option, 
                                                     const char* optionName)
{
    LOCK_READ_PROTECTED_OBJ(_hdl);
    SetTcpSocketOption(option, _hdl, optionName);
}

template<class TConfig>
template<class TOption>
void WebsocketTpp::Impl<TConfig>::SetTcpSocketOption(const TOption& option,
                                                     websocketpp::connection_hdl hdl,
                                                     const char* optionName)
{
    if (!hdl.expired()) {
        websocketpp::lib::error_code ec;
        if (const auto connection = _client.get_con_from_hdl(hdl, ec)) {
            connection->get_socket().lowest_layer().set_option(option, ec);
            if (ec) {
                MS_ERROR("failed to set %s option: %s", optionName, ec.message().c_str());
            }
        }
        else {
            MS_ERROR("failed to set %s option because no client connection: %s",
                     optionName, ec.message().c_str());
        }
    }
}

template<class TConfig>
void WebsocketTpp::Impl<TConfig>::OnSocketInit(websocketpp::connection_hdl hdl)
{
    {
        LOCK_WRITE_PROTECTED_OBJ(_hdl);
        _hdl = std::move(hdl);
    }
    InvokeListenersMethod(&WebsocketListener::OnStateChanged, GetState());
}

template<class TConfig>
void WebsocketTpp::Impl<TConfig>::OnFail(websocketpp::connection_hdl hdl)
{
    bool report = false;
    websocketpp::lib::error_code ec;
    {
        LOCK_READ_PROTECTED_OBJ(_hdl);
        report = _hdl->lock() == hdl.lock();
        if (report) {
            if (const auto connection = _client.get_con_from_hdl(hdl, ec)) {
                ec = connection->get_ec();
            }
        }
    }
    if (report) {
        // report error
        InvokeListenersMethod(&WebsocketListener::OnFailed, WebsocketFailure::General, ec.message());
    }
}

template<class TConfig>
void WebsocketTpp::Impl<TConfig>::OnOpen(websocketpp::connection_hdl hdl)
{
    bool opened = false;
    {
        LOCK_READ_PROTECTED_OBJ(_hdl);
        if (_hdl->lock() == hdl.lock()) {
            opened = true;
            if (const auto& tcpNoDelay = GetOptions()._tcpNoDelay) {
                const websocketpp::lib::asio::ip::tcp::no_delay option(tcpNoDelay.value());
                SetTcpSocketOption(option, hdl, "TCP_NO_DELAY");
            }
        }
    }
    if (opened) {
        // update state
        SetOpened(true);
    }
}

template<class TConfig>
void WebsocketTpp::Impl<TConfig>::OnMessage(websocketpp::connection_hdl hdl,
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
void WebsocketTpp::Impl<TConfig>::OnClose(websocketpp::connection_hdl hdl)
{
    bool closed = false;
    {
        LOCK_WRITE_PROTECTED_OBJ(_hdl);
        if (hdl.lock() == _hdl->lock()) {
            _hdl = websocketpp::connection_hdl();
            closed = true;
        }
    }
    if (closed) {
        SetOpened(false);
    }
}

template<class TConfig>
bool WebsocketTpp::Impl<TConfig>::SetOpened(bool opened)
{
    const auto oldState = GetState();
    if (opened != _opened.exchange(opened)) {
        const auto newState = GetState();
        if (oldState != newState) {
            InvokeListenersMethod(&WebsocketListener::OnStateChanged, newState);
        }
        return true;
    }
    return false;
}

template<class TConfig>
std::string WebsocketTpp::Impl<TConfig>::ToText(MessagePtr message)
{
    if (message) {
        auto text = std::move(message->get_raw_payload());
        message->recycle();
        return text;
    }
    return std::string();
}

template<class TConfig>
std::shared_ptr<Buffer> WebsocketTpp::Impl<TConfig>::ToBinary(MessagePtr message)
{
    if (message) {
        return std::make_shared<MessageBuffer<MessagePtr>>(std::move(message));
    }
    return nullptr;
}

WebsocketTpp::TlsOn::TlsOn(uint64_t id, const std::shared_ptr<const Config>& config,
                           const std::shared_ptr<SocketListeners>& listeners)
    : Impl<ExtendedConfig<asio_tls_client>>(id, config, listeners)
{
    GetClient().set_tls_init_handler(bind(&TlsOn::OnTlsInit, this, _1));
}

WebsocketTpp::TlsOn::SslContextPtr WebsocketTpp::TlsOn::OnTlsInit(websocketpp::connection_hdl)
{
    try {
        return WebsocketTppUtils::CreateSSLContext(GetOptions()._tls);
    } catch (const std::exception& e) {
        InvokeListenersMethod(&WebsocketListener::OnFailed, WebsocketFailure::TlsOptions, e.what());
    }
    return nullptr;
}

WebsocketTpp::TlsOff::TlsOff(uint64_t id, const std::shared_ptr<const Config>& config,
                             const std::shared_ptr<SocketListeners>& listeners)
    : Impl<ExtendedConfig<asio_client>>(id, config, listeners)
{
}

WebsocketTpp::Wrapper::Wrapper(std::string socketName, std::shared_ptr<Api> api)
    : ThreadExecution(std::move(socketName), ThreadPriority::Highest)
    , _api(std::move(api))
{
}

WebsocketTpp::Wrapper::~Wrapper()
{
    StopExecution();
}

std::unique_ptr<WebsocketTpp::Api> WebsocketTpp::Wrapper::Create(uint64_t id,
                                                                 const std::shared_ptr<const Config>& config,
                                                                 const std::shared_ptr<SocketListeners>& listeners)
{
    if (auto api = Api::Create(id, config, listeners)) {
        return std::make_unique<Wrapper>(config->GetUri()->str(), std::move(api));
    }
    return nullptr;
}

WebsocketState WebsocketTpp::Wrapper::GetState()
{
    if (const auto api = std::atomic_load(&_api)) {
        return api->GetState();
    }
    return WebsocketState::Disconnected;
}

bool WebsocketTpp::Wrapper::Open()
{
    if (const auto api = std::atomic_load(&_api)) {
        return api->Open();
    }
    return false;
}

void WebsocketTpp::Wrapper::Close()
{
    if (const auto api = std::atomic_load(&_api)) {
        api->Close();
    }
}

bool WebsocketTpp::Wrapper::WriteBinary(const std::shared_ptr<Buffer>& buffer)
{
    if (buffer) {
        if (const auto api = std::atomic_load(&_api)) {
            return api->WriteBinary(buffer);
        }
    }
    return false;
}

bool WebsocketTpp::Wrapper::WriteText(const std::string& text)
{
    if (const auto api = std::atomic_load(&_api)) {
        return api->WriteText(text);
    }
    return false;
}

void WebsocketTpp::Wrapper::DoExecuteInThread()
{
    if (const auto api = std::atomic_load(&_api)) {
        api->Run();
    }
}

void WebsocketTpp::Wrapper::DoStopThread()
{
    if (auto api = std::atomic_exchange(&_api, std::shared_ptr<Api>())) {
        api->Close();
    }
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
                MS_DEBUG_DEV("WebsocketTpp debug: %s", message.c_str());
                break;
            case LogLevel::LOG_WARN:
                MS_WARN_DEV("WebsocketTpp warning: %s", message.c_str());
                break;
            case LogLevel::LOG_ERROR:
                MS_ERROR("WebsocketTpp error: %s", message.c_str());
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
