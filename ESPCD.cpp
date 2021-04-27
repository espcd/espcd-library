#include "ESPCD.h"
#include "Arduino.h"
#if defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#include <HTTPUpdate.h>
#elif defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#endif
#include <AutoConnect.h>

AutoConnect Portal;
AutoConnectConfig Config;

ESPCD::ESPCD(String url) {
    this->url = url;
    generate_id();
}

void ESPCD::generate_id() {
    uint64_t chipid = ESP.getEfuseMac();
    uint16_t chip = (uint16_t)(chipid >> 32);
    snprintf(this->id, 13, "%04X%08X", chip, (uint32_t)chipid);
}

void ESPCD::setup() {
    Serial.println("Hello from setup");

    Config.autoReconnect = true;
    Config.reconnectInterval = 6;
    Portal.config(Config);
    
    if (Portal.begin()) {
        Serial.println("WiFi connected: " + WiFi.localIP().toString());
    } else {
        Serial.println("Connection failed.");
    }
}

#if defined(ARDUINO_ARCH_ESP32)
WebServer& ESPCD::get_server() {
    return Portal.host();
}
#elif defined(ARDUINO_ARCH_ESP8266)
ESP8266WebServer& ESPCD::get_server() {
    return Portal.host();
}
#endif

void ESPCD::loop() {
    if (WiFi.status() == WL_CONNECTED) {
        WiFiClient client;
        client.setTimeout(12000 / 1000);
        
        String url = this->url + "?device=" + this->id;
#if defined(ARDUINO_ARCH_ESP32)
        t_httpUpdate_return ret = httpUpdate.update(client, url);
#elif defined(ARDUINO_ARCH_ESP8266)
        t_httpUpdate_return ret = ESPhttpUpdate.update(client, url);
#endif
        switch (ret) {
        case HTTP_UPDATE_FAILED:
#if defined(ARDUINO_ARCH_ESP32)
            Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
#elif defined(ARDUINO_ARCH_ESP8266)
            Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
#endif
            break;
        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("HTTP_UPDATE_NO_UPDATES");
            break;
        case HTTP_UPDATE_OK:
            Serial.println("HTTP_UPDATE_OK");
            break;
        }
    }

    Portal.handleClient();
}
