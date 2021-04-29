#include "ESPCD.h"
#include "cert.h"
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
#include <EEPROM.h>
#endif
#include <AutoConnect.h>

AutoConnect portal;
AutoConnectConfig config;

#if defined(ARDUINO_ARCH_ESP32)
Preferences pref;
#elif defined(ARDUINO_ARCH_ESP8266)
typedef struct {
  char  version[40];
} EEPROM_CONFIG_t;
#endif

long previousMillis = 0;

ESPCD::ESPCD(String baseUrl) {
    if (baseUrl.endsWith("/")) {
        baseUrl = baseUrl.substring(0, baseUrl.length()-1);
    }
    this->baseUrl = baseUrl;
    this->secure = this->baseUrl.startsWith("https") ? true : false;
    generateId();
}

void ESPCD::generateId() {
#ifdef ESP8266
    sprintf_P(this->id, PSTR("%08x"), ESP.getChipId());
#else
    sprintf_P(this->id, PSTR("%08x"), (uint32_t)(ESP.getEfuseMac() << 40 >> 40));
#endif
}

void ESPCD::syncTime() {
    // trigger NTP time sync
    Serial.println("Syncing NTP time...");
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");  // UTC

    // waiting for finished sync
    time_t now = time(nullptr);
    while (now < 8 * 3600 * 2) {
        delay(500);
        now = time(nullptr);
    }

    // print NTP time
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    Serial.print(F("Current time: "));
    Serial.print(asctime(&timeinfo));
}

void ESPCD::setLocalVersion(String version) {
    #if defined(ARDUINO_ARCH_ESP32)
    pref.begin(IDENTIFIER, false);
    pref.putString(VERSION_KEY, version);
    pref.end();
    #elif defined(ARDUINO_ARCH_ESP8266)
    EEPROM_CONFIG_t eepromConfig;
    strncpy(eepromConfig.version, version.c_str(), sizeof(EEPROM_CONFIG_t::version));
    EEPROM.begin(portal.getEEPROMUsedSize());
    EEPROM.put<EEPROM_CONFIG_t>(0, eepromConfig);
    EEPROM.commit();
    EEPROM.end();
    #endif
}

String toString(char* c, uint8_t length) {
  String r;
  while (length-- && *c) {
    r += (isAlphaNumeric(*c) ? String(*c) : String(*c, HEX));
    c++;
  }
  return r;
}

String ESPCD::getLocalVersion() {
    String version = DEFAULT_VERSION;
    #if defined(ARDUINO_ARCH_ESP32)
    pref.begin(IDENTIFIER, false);
    version = pref.getString(VERSION_KEY, DEFAULT_VERSION);
    pref.end();
    #elif defined(ARDUINO_ARCH_ESP8266)
    EEPROM_CONFIG_t eepromConfig;
    EEPROM.begin(sizeof(eepromConfig));
    EEPROM.get<EEPROM_CONFIG_t>(0, eepromConfig);
    EEPROM.end();
    version = toString(eepromConfig.version, sizeof(EEPROM_CONFIG_t::version));
    #endif
    return version;
}

WiFiClientSecure* ESPCD::getSecureClient() {
    WiFiClientSecure* client = new WiFiClientSecure();
#if defined(ARDUINO_ARCH_ESP32)
    client->setCACert((const char *) certs_ca_pem);
#elif defined(ARDUINO_ARCH_ESP8266)
    client->setCACert(certs_ca_pem, certs_ca_pem_len);
#endif
    client->setTimeout(12);
    return client;
}

String ESPCD::getRemoteVersion() {
    String url = this->baseUrl + "/version?device=" + this->id;
    
    HTTPClient http;
    if (this->secure) {
        WiFiClientSecure* client = this->getSecureClient();
        http.begin(*client, url);
    } else {
        http.begin(url);
    }

    int httpCode = http.GET();
    String version = DEFAULT_VERSION;
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        version = http.getString();
    }
    return version;
}

void ESPCD::update() {
    String url = this->baseUrl + "/firmware?device=" + this->id;

    WiFiClient* client;
    if (this->secure) {
        client = this->getSecureClient();
    } else {
        client = new WiFiClient();
    }

#if defined(ARDUINO_ARCH_ESP32)
    t_httpUpdate_return ret = httpUpdate.update(*client, url);
#elif defined(ARDUINO_ARCH_ESP8266)
    t_httpUpdate_return ret = ESPhttpUpdate.update(*client, url);
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
#if defined(ARDUINO_ARCH_ESP8266)
    config.boundaryOffset = sizeof(EEPROM_CONFIG_t);
#endif
    portal.config(config);

    if (portal.begin()) {
        Serial.printf("WiFi connected: %s\n", WiFi.localIP().toString().c_str());

        if (this->secure) {
            syncTime();
        }
    } else {
        Serial.println("Connection failed.");
    }
}

#if defined(ARDUINO_ARCH_ESP32)
WebServer& ESPCD::getServer() {
    return portal.host();
}
#elif defined(ARDUINO_ARCH_ESP8266)
ESP8266WebServer& ESPCD::getServer() {
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
            Serial.printf("Local version: %s\n", localVersion.c_str());
            Serial.printf("Remote version: %s\n", remoteVersion.c_str());

            if (remoteVersion != DEFAULT_VERSION && localVersion != remoteVersion) {
                Serial.printf("Updating to version %s\n", remoteVersion.c_str());
                this->setLocalVersion(remoteVersion);
                this->update();
            }
        }
    }

    portal.handleClient();
}
