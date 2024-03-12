#pragma once // RTC/MediaTranslate/TranslatorDefines.hpp

// TODO: remove it for production
#define MEDIA_TRANSLATIONS_TEST

// 10 sec max
#define POOL_MEMORY_ALLOCATOR_HEAP_CHUNKS_LIFETIME_MS 200

#ifdef MEDIA_TRANSLATIONS_TEST
#define USE_MAIN_THREAD_FOR_CALLBACKS_RETRANSMISSION

#define PRODUCER_LANGUAGE_ID "ru"
#define CONSUMER_LANGUAGE_ID "en"
#define CONSUMER_VOICE_ID    "Male"

#define NO_TRANSLATION_SERVICE
#define SINGLE_TRANSLATION_POINT_CONNECTION
//#define WRITE_TRANSLATION_TO_FILE
//#define WRITE_PRODUCER_RECV_TO_FILE // add MEDIASOUP_DEPACKETIZER_PATH env variable for reference to output folder

#ifdef NO_TRANSLATION_SERVICE
#define MOCK_CONNECTION_DELAY_MS 500U
#else
//#define LOCAL_WEBSOCKET_TEST_SERVER
#endif

#define MOCK_WEBM_INPUT_FILE "/Users/user/Documents/Sources/mediasoup_rtp_packets/received_translation_mono_example.webm"
// in seconds
#define MOCK_WEBM_INPUT_FILE_LEN_SECS 4U

#if defined(LOCAL_WEBSOCKET_TEST_SERVER) || defined(NO_TRANSLATION_SERVICE)
//#define MOCK_DISCONNECT_AFTER_MS 10000U
//#define MOCK_DISCONNECT_STUB_END_POINTS
#endif

#endif // MEDIA_TRANSLATIONS_TEST
