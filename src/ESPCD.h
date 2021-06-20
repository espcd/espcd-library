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


class ESPCD {
  public:
    ESPCD() {}
    void setUrl(String url);
    void setApiKey(String apiKey);
    void setProductId(String productId);
    void setCert(const char* cert);
    void setup();
    void loop();
    WebServerClass& getServer();
  private:
    String url;
    int interval = 60;  // default check interval 60s, this is overwritten by product check_interval later
    bool secure;
    String productId;
    long previousMillis = 0;
    AutoConnect portal;
    Requests requests;
    Memory memory;
    
    String getDefaultFqbn();
    void update(String firmwareId);
    
    Response getOrCreateDevice();
};

#endif
