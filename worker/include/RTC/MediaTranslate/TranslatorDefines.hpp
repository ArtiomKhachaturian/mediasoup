#pragma once // RTC/MediaTranslate/TranslatorDefines.hpp

// TODO: remove it for production
#define MEDIA_TRANSLATIONS_TEST

// TODO: add timer-based garbage collector for cleanup of unused heap chunks
#define ENABLE_HEAP_CHUNKS_IN_POOL_MEMORY_ALLOCATOR

#ifdef ENABLE_HEAP_CHUNKS_IN_POOL_MEMORY_ALLOCATOR
// 10 sec max
#define POOL_MEMORY_ALLOCATOR_HEAP_CHUNKS_LIFETIME_MS 200
#endif

#ifdef MEDIA_TRANSLATIONS_TEST
#define USE_MAIN_THREAD_FOR_CALLBACKS_RETRANSMISSION

//#define NO_TRANSLATION_SERVICE
#define SINGLE_TRANSLATION_POINT_CONNECTION
#define WRITE_TRANSLATION_TO_FILE
//#define WRITE_PRODUCER_RECV_TO_FILE // add MEDIASOUP_DEPACKETIZER_PATH env variable for reference to output folder

#ifdef NO_TRANSLATION_SERVICE
#define MOCK_CONNECTION_DELAY_MS 500U
#else
//#define LOCAL_WEBSOCKET_TEST_SERVER
#endif

#define MOCK_WEBM_INPUT_FILE "/Users/user/Documents/Sources/mediasoup_rtp_packets/speakshift_test2_3-59.webm"
// in seconds
#define MOCK_WEBM_INPUT_FILE_LEN_SECS 240U

#if defined(LOCAL_WEBSOCKET_TEST_SERVER) || defined(NO_TRANSLATION_SERVICE)
#define MOCK_DISCONNECT_AFTER_MS 10000U
//#define MOCK_DISCONNECT_STUB_END_POINTS
#endif

#endif // MEDIA_TRANSLATIONS_TEST
