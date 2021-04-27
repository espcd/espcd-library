#include "ESPCD.h"
#include "Arduino.h"
#if defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#include <HTTPUpdate.h>
#include <HTTPClient.h>
#include <Preferences.h>
#elif defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#include <ESP8266HTTPClient.h>
#endif
#include <AutoConnect.h>

AutoConnect portal;
AutoConnectConfig config;

#if defined(ARDUINO_ARCH_ESP32)
Preferences pref;
#elif defined(ARDUINO_ARCH_ESP8266)

#endif

long previousMillis = 0;

ESPCD::ESPCD(String baseUrl) {
    if (baseUrl.endsWith("/")) {
        baseUrl = baseUrl.substring(0, baseUrl.length()-1);
    }
    this->baseUrl = baseUrl;
    generateId();
}

void ESPCD::generateId() {
    uint64_t chipid = ESP.getEfuseMac();
    uint16_t chip = (uint16_t)(chipid >> 32);
    snprintf(this->id, 13, "%04X%08X", chip, (uint32_t)chipid);
}

void ESPCD::setLocalVersion(String version) {
    #if defined(ARDUINO_ARCH_ESP32)
    pref.begin(IDENTIFIER, false);
    pref.putString(VERSION_KEY, version);
    pref.end();
    #elif defined(ARDUINO_ARCH_ESP8266)

    #endif
}

String ESPCD::getLocalVersion() {
    #if defined(ARDUINO_ARCH_ESP32)
    pref.begin(IDENTIFIER, false);
    String version = pref.getString(VERSION_KEY, DEFAULT_VERSION);
    pref.end();
    #elif defined(ARDUINO_ARCH_ESP8266)

    #endif
    return version;
}

String ESPCD::getRemoteVersion() {
    String url = this->baseUrl + "/version?device=" + this->id;

    String version = DEFAULT_VERSION;
    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        version = http.getString();
    }
    return version;
}

void ESPCD::update() {
    String url = this->baseUrl + "/firmware?device=" + this->id;

    WiFiClient client;
    client.setTimeout(12000 / 1000);
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

void ESPCD::setup() {
    Serial.println("Hello from setup");

    config.autoReconnect = true;
    config.reconnectInterval = 6;
    portal.config(config);
    
    if (portal.begin()) {
        Serial.printf("WiFi connected: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("Connection failed.");
    }
}

#if defined(ARDUINO_ARCH_ESP32)
WebServer& ESPCD::get_server() {
    return portal.host();
}
#elif defined(ARDUINO_ARCH_ESP8266)
ESP8266WebServer& ESPCD::get_server() {
    return portal.host();
}
#endif

void ESPCD::loop() {
    if (WiFi.status() == WL_CONNECTED) {

        long currentMillis = millis();
        if (currentMillis - previousMillis > VERSION_CHECK_INTERVAL) {
            previousMillis = currentMillis;

            String localVersion = this->getLocalVersion();
            String remoteVersion = this->getRemoteVersion();
            Serial.printf("Local version: %s\n", localVersion);
            Serial.printf("Remote version: %s\n", remoteVersion);

            if (remoteVersion != DEFAULT_VERSION && localVersion != remoteVersion) {
                Serial.printf("Updating to version %s\n", remoteVersion);
                this->setLocalVersion(remoteVersion);
                this->update();
            }
        }
    }

    portal.handleClient();
}
