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
#include <WiFiClientSecure.h>

AutoConnect portal;
AutoConnectConfig config;

#if defined(ARDUINO_ARCH_ESP32)
Preferences pref;
#elif defined(ARDUINO_ARCH_ESP8266)

#endif

long previousMillis = 0;

const char* rootCACert = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDBTCCAe2gAwIBAgIUUBrzGjoZ09d5JV1rpVhfAxUE3/MwDQYJKoZIhvcNAQEL\n" \
"BQAwEjEQMA4GA1UEAwwHdGVzdC1jYTAeFw0yMTA0MjMxMDUyMTdaFw0yMTA2MjIx\n" \
"MDUyMTdaMBIxEDAOBgNVBAMMB3Rlc3QtY2EwggEiMA0GCSqGSIb3DQEBAQUAA4IB\n" \
"DwAwggEKAoIBAQDnN55s+wkKuEENn89QBEo5YJW8DhH6bBQRDYcARlvEKcrdAJwY\n" \
"vJvqFRYiewFAdUDRIy4DdhfCxI6Mhh2hYAeS4Dqvc7ZEuEacwkAf5s/cE/PoYOk0\n" \
"z6KO9IDCdJGy14Nz2ft0cSmYtojhawSdLzNmPJ4r4x+Wi2+aZScBzQhnOZltGsNC\n" \
"Sk+zNQbrpXxCp+qAkMcTdSDzF3SrZ9viodpZoHIjKnqu8wVlpKLXILODFFYxByr6\n" \
"0eNNXK0UPjYfKv7R7B8niSGTPL//9prAKYDkOY0cZQoDBJWV9Uk/J1cuz0Pt8Y/4\n" \
"ioIcnDxwuGhHdWDEFgKN0qwmATIESdSJ55GPAgMBAAGjUzBRMB0GA1UdDgQWBBQW\n" \
"Ye4fGdrVQq1bejZ6/Skvk2d3mDAfBgNVHSMEGDAWgBQWYe4fGdrVQq1bejZ6/Skv\n" \
"k2d3mDAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3DQEBCwUAA4IBAQDPiVC96x3h\n" \
"jiuxYJI5ai1KzE4SFSZO2/UACxkeUPq6zzx1KICPiqz82hcqpiEuvSsf/tZ9vX9p\n" \
"/e89zF8pSxiO31xDOIr24+ZDXHIU1SwKP0y0vhwT1AI9B4W7aXxwu7FXcDK2v7Gn\n" \
"VkSKl9TPLMHOCHohITfSVZgofLxietXQu6h1a3eNayvhMEEN9G/lFfaThUrThyPo\n" \
"T2WtTs1lTW3b/xkwe4ZTIBlV4JmEm1Ym8XdzBlQWJL66bOP55oGvC198Mv+IxXws\n" \
"0wqpXvwX9JmjcTh5umhVMHMYt4sGXnKqvbP8WTIxg0kK0ym61NDeSb+VNspvNg93\n" \
"5pB9PJVvLFuv\n" \
"-----END CERTIFICATE-----\n";

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
    
    HTTPClient http;
    if (this->baseUrl.startsWith("https")) {
        WiFiClientSecure *client = new WiFiClientSecure;
        client->setCACert(rootCACert);
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

        if (this->baseUrl.startsWith("https")) {
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
