#ifndef ESPCD_h
#define ESPCD_h

#if defined(ARDUINO_ARCH_ESP32)
#include <WebServer.h>
#elif defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WebServer.h>
#endif

class ESPCD {
  public:
    ESPCD(char* url);
    void setup();
    void loop();
#if defined(ARDUINO_ARCH_ESP32)
    WebServer& get_server();
#elif defined(ARDUINO_ARCH_ESP8266)
    ESP8266WebServer& get_server();
#endif
  private:
    char* url;
};

#endif
