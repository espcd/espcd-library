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
    void setApiKey(String apiKey);
    void setCert(char* cert);

    void setup();
    std::unique_ptr<WiFiClient> getClient();
    String getRedirectedUrl(String url);
    String getUpdateUrl(String firmwareId);

    Response sendRequest(String method, String url, DynamicJsonDocument &responseJson);
    Response sendRequest(String method, String url, DynamicJsonDocument &requestJson, DynamicJsonDocument &responseJson);

    Response getRequest(String url, DynamicJsonDocument &responseJson);
    Response postRequest(String url, DynamicJsonDocument &requestJson, DynamicJsonDocument &responseJson);
    Response patchRequest(String url, DynamicJsonDocument &requestJson, DynamicJsonDocument &responseJson);

    Response getDevice(String id);
    Response getProduct(String id);
    Response getProductFirmware(String id, String fqbn);
    Response createDevice(DynamicJsonDocument &requestJson);
    Response patchDevice(String deviceId, DynamicJsonDocument &requestJson);
private:
    String url;
    String apiKey;
    bool secure;
    char* cert;

    void syncTime();
};

#endif
