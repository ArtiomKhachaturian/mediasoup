#pragma once
#include "RTC/MediaTranslate/Websocket/WebsocketTlsPeerVerification.hpp"
#include "RTC/MediaTranslate/Websocket/WebsocketTlsMethod.hpp"
#include <string>

namespace RTC
{

struct WebsocketTls 
{
    WebsocketTlsMethod _method = WebsocketTlsMethod::sslv23;
    WebsocketTlsPeerVerification _peerVerification = WebsocketTlsPeerVerification::No;
    // PEM or ASN.1
    bool _certificateIsPem = true;
    // always create a new key when using _dh parameter
    bool _dhSingle = true;
    // disable SSL v2
    bool _sslv2No = true;
    // disable SSL v3
    bool _sslv3No = true;
    // disable TLS v1
    bool _tlsv1No = true;
    // disable TLS v1.1
    bool _tlsv1_1No = true;
    // disable TLS v1.2
    bool _tlsv1_2No = false;
    // disable TLS v1.3
    bool _tlsv1_3No = false;
    // don't use compression even if supported
    bool _sslNoCompression = false;
    std::string _certificate;
    std::string _certificatePrivateKey;
    std::string _certificatePrivateKeyPassword;
    std::string _trustStore;
    std::string _sslCiphers;
    std::string _dh; // Diffie-Hellman
};

} // namespace RTC
