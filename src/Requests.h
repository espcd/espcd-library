#ifndef REQUESTS_H
#define REQUESTS_H

#include "Arduino.h"
#include "Response.h"
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
    Requests(bool secure);

    void setSecure(bool secure);
    
    void syncTime();
    std::unique_ptr<WiFiClient> getClient();
    String getRedirectedUrl(String url);

    Response sendRequest(String method, String url);
    Response sendRequest(String method, String url, DynamicJsonDocument body);

    Response getRequest(String url);
    Response postRequest(String url, DynamicJsonDocument body);
    Response patchRequest(String url, DynamicJsonDocument body);
private:
    bool secure;
};

#endif
