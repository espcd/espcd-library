#ifndef ESPCD_H
#define ESPCD_H

#include "Arduino.h"
#if defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#include <HTTPUpdate.h>
#include <HTTPClient.h>
#include <WebServer.h>
#define HttpUpdateClass httpUpdate
#define WebServerClass WebServer
#elif defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#define HttpUpdateClass ESPhttpUpdate
#define WebServerClass ESP8266WebServer
#endif
#include <AutoConnect.h>
#include <WiFiClientSecure.h>

#if defined(ARDUINO_ARCH_ESP32)
#include <Preferences.h>
#define IDENTIFIER "ESPCD"
#define VERSION_KEY "version"
#elif defined(ARDUINO_ARCH_ESP8266)
#include <EEPROM.h>
typedef struct {
  char  version[40+1];  // hexadecimal SHA-1 number is 40 digits long + null character as string terminator
} EEPROM_CONFIG_t;
#endif

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
    long previousMillis;
    AutoConnect portal;
#if defined(ARDUINO_ARCH_ESP32)
    Preferences pref;
#endif
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
