#ifndef ESPCD_H
#define ESPCD_H

#if defined(ARDUINO_ARCH_ESP32)
#include <WebServer.h>
#define WebServerClass WebServer
#elif defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WebServer.h>
#define WebServerClass ESP8266WebServer
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
    WebServerClass& getServer();
  private:
    String baseUrl;
    bool secure;
    String id;
    String generateId();
    void syncTime();
    String buildUrl(String path);
    void setLocalVersion(String version);
    std::unique_ptr<WiFiClient> getClient();
    String getLocalVersion();
    String getRemoteVersion();
    void update();
};

#endif
