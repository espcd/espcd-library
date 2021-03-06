#ifndef REQUESTS_H
#define REQUESTS_H

#include "Debug.h"
#include "Response.h"

#include "Arduino.h"
#include <ArduinoJson.h>

#if defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#elif defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#endif


class Requests {
public:
    Requests() {}

    void setUrl(String url);
    void setApiKey(String apiKey);
    void setCert(const char* cert);

    void setup();
    void initClient();
    WiFiClient* getClient();
    String getRedirectedUrl(String url);
    String getUpdateUrl(String firmwareId);

    Response sendRequest(String method, String url);
    Response sendRequest(String method, String url, DynamicJsonDocument &requestJson);

    Response getRequest(String url);
    Response postRequest(String url, DynamicJsonDocument &requestJson);
    Response patchRequest(String url, DynamicJsonDocument &requestJson);

    Response getDevice(String id);
    Response getProduct(String id);
    Response getProductFirmware(String id, String fqbn);
    Response createDevice(DynamicJsonDocument &requestJson);
    Response patchDevice(String deviceId, DynamicJsonDocument &requestJson);
private:
    String url;
    String apiKey;
    bool secure;
    const char* cert;
    WiFiClient* client;

    void syncTime();
};

#endif
