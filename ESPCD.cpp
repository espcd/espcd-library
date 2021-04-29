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
#include <WiFiClientSecure.h>

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
#if defined(ARDUINO_ARCH_ESP32)
    sprintf_P(this->id, PSTR("%08x"), (uint32_t)(ESP.getEfuseMac() << 40 >> 40));
#elif defined(ARDUINO_ARCH_ESP8266)
    sprintf_P(this->id, PSTR("%08x"), ESP.getChipId());
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

std::unique_ptr<WiFiClient> ESPCD::getClient() {
    std::unique_ptr<WiFiClient> client;
    if (this->secure) {
        std::unique_ptr<WiFiClientSecure> secureClient(new WiFiClientSecure);
#if defined(ARDUINO_ARCH_ESP32)
        secureClient->setCACert((const char *) certs_ca_pem);
#elif defined(ARDUINO_ARCH_ESP8266)
        secureClient->setCACert(certs_ca_pem, certs_ca_pem_len);
#endif
        secureClient->setTimeout(12);
        client = std::move(secureClient);
    } else {
        client = std::unique_ptr<WiFiClient>(new WiFiClient);
    }
    return client;
}

String ESPCD::buildUrl(String path) {
    if (path.startsWith("/")) {
        path = path.substring(1, path.length());
    }
    String url = this->baseUrl + "/" + path + "?device=" + this->id;
    return url;
}

String ESPCD::getRemoteVersion() {
    String url = buildUrl("/version");

    String version = DEFAULT_VERSION;
    HTTPClient http;
    std::unique_ptr<WiFiClient> client = this->getClient();
    if (http.begin(*client, url)) {
        int httpCode = http.GET();
        if (httpCode > 0) {
            Serial.printf("HTTP GET code: %d\n", httpCode);
            if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
                version = http.getString();
            }
        } else {
            Serial.printf("HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
        }
        http.end();
    } else {
      Serial.printf("HTTP GET failed, error: unable to connect\n");
    }
    return version;
}

void ESPCD::update() {
    String url = buildUrl("/firmware");

    std::unique_ptr<WiFiClient> client = this->getClient();
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
