#include "ESPCD.h"
#include "cert.h"

ESPCD::ESPCD(String baseUrl) {
    if (baseUrl.endsWith("/")) {
        baseUrl = baseUrl.substring(0, baseUrl.length()-1);
    }
    this->baseUrl = baseUrl;
    this->secure = baseUrl.startsWith("https") ? true : false;
    this->deviceId = this->generateDeviceId();
    this->model = this->generateModel();
}

String ESPCD::generateDeviceId() {
#if defined(ARDUINO_ARCH_ESP32)
    uint64_t chipid = ESP.getEfuseMac();
    uint16_t chip = (uint16_t)(chipid >> 32);
    char deviceId[13];
    snprintf(deviceId, 13, "%04X%08X", chip, (uint32_t)chipid);
    return String(deviceId);
#elif defined(ARDUINO_ARCH_ESP8266)
    uint32_t chipid = ESP.getChipId();
    char deviceId[9];
    snprintf(deviceId, 9, "%08X", chipid);
    return String(deviceId);
#endif
}

String ESPCD::generateModel() {
    String model = "unknown";
#if defined(ARDUINO_ARCH_ESP32)
    model = "ESP32";
#elif defined(ARDUINO_ARCH_ESP8266)
    model = "ESP8266";
#endif
    return model;
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
    this->pref.begin(IDENTIFIER, false);
    this->pref.putString(VERSION_KEY, version);
    this->pref.end();
#elif defined(ARDUINO_ARCH_ESP8266)
    EEPROM_CONFIG_t eepromConfig;
    strncpy(eepromConfig.version, version.c_str(), sizeof(EEPROM_CONFIG_t::version));
    EEPROM.begin(this->portal.getEEPROMUsedSize());
    EEPROM.put<EEPROM_CONFIG_t>(0, eepromConfig);
    EEPROM.commit();
    EEPROM.end();
#endif
}

String ESPCD::getLocalVersion() {
    String version = DEFAULT_VERSION;
#if defined(ARDUINO_ARCH_ESP32)
    this->pref.begin(IDENTIFIER, false);
    version = this->pref.getString(VERSION_KEY, DEFAULT_VERSION);
    this->pref.end();
#elif defined(ARDUINO_ARCH_ESP8266)
    EEPROM_CONFIG_t eepromConfig;
    EEPROM.begin(sizeof(eepromConfig));
    EEPROM.get<EEPROM_CONFIG_t>(0, eepromConfig);
    EEPROM.end();
    version = String(eepromConfig.version);
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
    std::vector<std::pair<String, String>> params;
    return this->buildUrl(path, params);
}

String ESPCD::buildUrl(String path, std::vector<std::pair<String, String>> params) {
    if (path.startsWith("/")) {
        path = path.substring(1, path.length());
    }
    String url = this->baseUrl + "/" + path;
    params.push_back(std::make_pair("device", this->deviceId));
    params.push_back(std::make_pair("model", this->model));
    for (int i = 0; i < params.size(); i++) {
        String separator = i == 0 ? "?" : "&";
        url += separator + params[i].first + "=" + params[i].second;
    }
    return url;
}

String ESPCD::getRemoteVersion() {
    std::vector<std::pair<String, String>> params;
    String localVersion = this->getLocalVersion();
    params.push_back(std::make_pair("current", localVersion));
    String url = this->buildUrl("/version", params);

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
    String url = this->buildUrl("/firmware");

    std::unique_ptr<WiFiClient> client = this->getClient();
    t_httpUpdate_return ret = HttpUpdateClass.update(*client, url);
    switch (ret) {
    case HTTP_UPDATE_FAILED:
        Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", HttpUpdateClass.getLastError(), HttpUpdateClass.getLastErrorString().c_str());
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

    this->previousMillis = 0;

    AutoConnectConfig config;
    config.autoReconnect = true;
    config.reconnectInterval = 6;
#if defined(ARDUINO_ARCH_ESP8266)
    config.boundaryOffset = sizeof(EEPROM_CONFIG_t);
#endif
    this->portal.config(config);

    if (this->portal.begin()) {
        Serial.printf("WiFi connected: %s\n", WiFi.localIP().toString().c_str());

        if (this->secure) {
            this->syncTime();
        }
    } else {
        Serial.println("Connection failed.");
    }
}

WebServerClass& ESPCD::getServer() {
    return this->portal.host();
}

void ESPCD::loop() {
    if (WiFi.status() == WL_CONNECTED) {
        long currentMillis = millis();
        if (currentMillis - this->previousMillis > VERSION_CHECK_INTERVAL) {
            this->previousMillis = currentMillis;

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
    this->portal.handleClient();
}
