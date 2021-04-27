#ifndef ESPCD_h
#define ESPCD_h

#if defined(ARDUINO_ARCH_ESP32)
#include <WebServer.h>
#elif defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WebServer.h>
#endif

#define IDENTIFIER "ESPCD"
#define VERSION_KEY "version"
#define VERSION_CHECK_INTERVAL 5000
#define DEFAULT_VERSION ""

class ESPCD {
  public:
    ESPCD(String baseUrl);
    void setup();
    void loop();
#if defined(ARDUINO_ARCH_ESP32)
    WebServer& get_server();
#elif defined(ARDUINO_ARCH_ESP8266)
    ESP8266WebServer& get_server();
#endif
  private:
    String baseUrl;
    char id[13];
    void generateId();
    void setLocalVersion(String version);
    String getLocalVersion();
    String getRemoteVersion();
    void update();
};

#endif
