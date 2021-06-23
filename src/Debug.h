// comment ESPCD_DEBUG to disable debug output
#define ESPCD_DEBUG

#ifdef ESPCD_DEBUG
#define DEBUG_MSG(...) Serial.printf( __VA_ARGS__ )
#else
#define DEBUG_MSG(...)
#endif
