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

    void setSecure(bool secure);
    void setCert(unsigned char* certs_ca_pem, unsigned int certs_ca_pem_len);

    void syncTime();
    std::unique_ptr<WiFiClient> getClient();
    String getRedirectedUrl(String url);

    Response sendRequest(String method, String url);
    Response sendRequest(String method, String url, DynamicJsonDocument payload);

    Response getRequest(String url);
    Response postRequest(String url, DynamicJsonDocument payload);
    Response patchRequest(String url, DynamicJsonDocument payload);
private:
    bool secure;
    unsigned char* certs_ca_pem;
    unsigned int certs_ca_pem_len;
};

#endif
