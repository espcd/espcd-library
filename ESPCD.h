#ifndef ESPCD_H
#define ESPCD_H

#if defined(ARDUINO_ARCH_ESP32)
#include <WebServer.h>
#elif defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WebServer.h>
#endif

#define IDENTIFIER "ESPCD"
#define VERSION_KEY "version"
#define VERSION_LEN 40
#define VERSION_CHECK_INTERVAL 5000
#define DEFAULT_VERSION ""

class ESPCD {
  public:
    ESPCD(String baseUrl);
    void setup();
    void loop();
#if defined(ARDUINO_ARCH_ESP32)
    WebServer& getServer();
#elif defined(ARDUINO_ARCH_ESP8266)
    ESP8266WebServer& getServer();
#endif
  private:
    String baseUrl;
    bool secure;
    char id[9];
    void generateId();
    void syncTime();
    void setLocalVersion(String version);
    std::unique_ptr<WiFiClient> getClient();
    String getLocalVersion();
    String getRemoteVersion();
    void update();
};

#endif
