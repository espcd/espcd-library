#ifndef ESPCD_H
#define ESPCD_H

#include "Requests.h"
#include "Response.h"
#include "Memory.h"

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

#define VERSION_CHECK_INTERVAL 5000


class ESPCD {
  public:
    ESPCD();
    ESPCD(String baseUrl);
    ESPCD(String baseUrl, unsigned char* certs_ca_pem, unsigned int certs_ca_pem_len);
    void setBaseUrl(String baseUrl);
    void setCert(unsigned char* certs_ca_pem, unsigned int certs_ca_pem_len);
    void setup();
    void loop();
    WebServerClass& getServer();
  private:
    String baseUrl;
    bool secure;
    long previousMillis = 0;
    AutoConnect portal;
    Requests requests;
    Memory memory;
    
    String getModel();
    void update(String firmwareId);
    
    Response getOrCreateDevice();
};

#endif
