#ifndef ESPCD_H
#define ESPCD_H

#include "Requests.h"
#include "Response.h"

#include "Arduino.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <HTTPUpdate.h>
#include <WebServer.h>
#define HttpUpdateClass httpUpdate
#define WebServerClass WebServer
#elif defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266httpUpdate.h>
#include <ESP8266WebServer.h>
#define HttpUpdateClass ESPhttpUpdate
#define WebServerClass ESP8266WebServer
#endif

#include <AutoConnect.h>


#if defined(ARDUINO_ARCH_ESP32)
#include <Preferences.h>
#define IDENTIFIER "ESPCD"
#define DEVICE_KEY "deviceId"
#define FIRMWARE_KEY "firmwareId"
#elif defined(ARDUINO_ARCH_ESP8266)
#include <EEPROM.h>
typedef struct {
  char deviceId[36+1];  // hexadecimal uuid is 36 digits long + null character as string terminator
  char firmwareId[36+1];
} EEPROM_CONFIG_t;
#endif

#define VERSION_CHECK_INTERVAL 5000
#define DEFAULT_VALUE "null"


class ESPCD {
  public:
    ESPCD(String baseUrl);
    void setup();
    void loop();
    WebServerClass& getServer();
  private:
    String baseUrl;
    bool secure;
    long previousMillis;
    AutoConnect portal;
    Requests requests;
#if defined(ARDUINO_ARCH_ESP32)
    Preferences pref;
#endif
    void syncTime();
    String getModel();
    void update(String firmwareId);

    Response getDevice(String id);
    Response getOrCreateDevice();
    Response createDevice();
    Response getProduct(String id);

#if defined(ARDUINO_ARCH_ESP32)
    String getNvsValue(const char* key);
    void setNvsValue(const char* key, String value);
#elif defined(ARDUINO_ARCH_ESP8266)
    String getEepromValue(String key);
    void setEepromValue(String key, String value, size_t size);
#endif

    String getFirmwareId();
    void setFirmwareId(String id);

    String getDeviceId();
    void setDeviceId(String id);
};

#endif
