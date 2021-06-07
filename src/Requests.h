#ifndef REQUESTS_H
#define REQUESTS_H

#include "Response.h"

#include "Arduino.h"
#include <ArduinoJson.h>
#include <memory>

#if defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#elif defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#endif


class Requests {
public:
    Requests() {}

    void setUrl(String url);
    void setCert(char* cert);

    void setup();
    std::unique_ptr<WiFiClient> getClient();
    String getRedirectedUrl(String url);
    String getUpdateUrl(String firmwareId);

    Response sendRequest(String method, String url);
    Response sendRequest(String method, String url, DynamicJsonDocument payload);

    Response getRequest(String url);
    Response postRequest(String url, DynamicJsonDocument payload);
    Response patchRequest(String url, DynamicJsonDocument payload);

    Response getDevice(String id);
    Response getProduct(String id);
    Response getProductFirmware(String id, String fqbn);
    Response createDevice(DynamicJsonDocument payload);
    Response patchDevice(String deviceId, DynamicJsonDocument payload);
private:
    String url;
    bool secure;
    char* cert;

    void syncTime();
};

#endif
