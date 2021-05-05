#include "ESPCD.h"
#include "cert.h"

ESPCD::ESPCD(String baseUrl) {
    if (baseUrl.endsWith("/")) {
        baseUrl = baseUrl.substring(0, baseUrl.length()-1);
    }
    this->baseUrl = baseUrl;
    this->secure = baseUrl.startsWith("https") ? true : false;
}

String ESPCD::getModel() {
    String model = DEFAULT_VALUE;
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

#if defined(ARDUINO_ARCH_ESP32)
String ESPCD::getNvsValue(const char* key) {
    this->pref.begin(IDENTIFIER, false);
    String value = this->pref.getString(key, DEFAULT_VALUE);
    this->pref.end();
    return value;
}
void ESPCD::setNvsValue(const char* key, String value) {
    this->pref.begin(IDENTIFIER, false);
    this->pref.putString(key, value.c_str());
    this->pref.end();
}
#elif defined(ARDUINO_ARCH_ESP8266)
String ESPCD::getEepromValue(String key) {
    EEPROM_CONFIG_t eepromConfig;
    EEPROM.begin(sizeof(eepromConfig));
    EEPROM.get<EEPROM_CONFIG_t>(0, eepromConfig);
    EEPROM.end();
    String value = String(key);
    return value;
}
void ESPCD::setEepromValue(String key, String value, size_t size) {
    EEPROM_CONFIG_t eepromConfig;
    strncpy(key, value.c_str(), size);
    EEPROM.begin(this->portal.getEEPROMUsedSize());
    EEPROM.put<EEPROM_CONFIG_t>(0, eepromConfig);
    EEPROM.commit();
    EEPROM.end();
}
#endif

String ESPCD::getFirmwareId() {
    String id = DEFAULT_VALUE;
#if defined(ARDUINO_ARCH_ESP32)
    id = this->getNvsValue(FIRMWARE_KEY);
#elif defined(ARDUINO_ARCH_ESP8266)
    id = this->getEepromValue(eepromConfig.firmwareId);
#endif
    return id;
}

void ESPCD::setFirmwareId(String id) {
#if defined(ARDUINO_ARCH_ESP32)
    this->setNvsValue(FIRMWARE_KEY, id);
#elif defined(ARDUINO_ARCH_ESP8266)
    this->setEepromValue(eepromConfig.firmwareId, id, sizeof(EEPROM_CONFIG_t::firmwareId));
#endif
}

String ESPCD::getDeviceId() {
    String id = DEFAULT_VALUE;
#if defined(ARDUINO_ARCH_ESP32)
    id = this->getNvsValue(DEVICE_KEY);
#elif defined(ARDUINO_ARCH_ESP8266)
    id = this->getEepromValue(eepromConfig.deviceId);
#endif
    return id;
}

void ESPCD::setDeviceId(String id) {
#if defined(ARDUINO_ARCH_ESP32)
    this->setNvsValue(DEVICE_KEY, id);
#elif defined(ARDUINO_ARCH_ESP8266)
    this->setEepromValue(eepromConfig.deviceId, id, sizeof(EEPROM_CONFIG_t::deviceId));
#endif
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

DynamicJsonDocument ESPCD::sendRequest(String method, String url) {
    DynamicJsonDocument body(2048);
    return this->sendRequest(method, url, body);
}

DynamicJsonDocument ESPCD::sendRequest(String method, String url, DynamicJsonDocument body) {
    DynamicJsonDocument response(2048);

    HTTPClient http;
    http.useHTTP10(true);

    std::unique_ptr<WiFiClient> client = this->getClient();
    if (http.begin(*client, url)) {
        String json;
        if (body.size() > 0) {
            http.addHeader("Content-Type", "application/json");
            serializeJson(body, json);
        }

        int httpCode;
        bool ok;
        if (method == "GET") {
            httpCode = http.GET();
            ok = httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY;
        } else if (method == "POST") {
            httpCode = http.POST(json);
            ok = httpCode == HTTP_CODE_CREATED;
        } else if (method == "PATCH") {
            httpCode = http.sendRequest("PATCH", json);
            ok = httpCode == HTTP_CODE_NO_CONTENT;
        }

        if (httpCode > 0) {
            Serial.printf("HTTP code: %d\n", httpCode);
            if (ok) {
                deserializeJson(response, http.getStream());
            }
        } else {
            Serial.printf("HTTP failed, error: %s\n", http.errorToString(httpCode).c_str());
        }
        http.end();
    } else {
      Serial.printf("HTTP failed, error: unable to connect\n");
    }

    return response;
}

DynamicJsonDocument ESPCD::getRequest(String url) {
    return this->sendRequest("GET", url);
}

DynamicJsonDocument ESPCD::postRequest(String url, DynamicJsonDocument request) {
    return this->sendRequest("POST", url, request);
}

DynamicJsonDocument ESPCD::patchRequest(String url, DynamicJsonDocument request) {
    return this->sendRequest("PATCH", url, request);
}

DynamicJsonDocument ESPCD::getDevice(String id) {
    String url = this->baseUrl + "/devices/" + id;
    DynamicJsonDocument response = getRequest(url);
    return response;
}

DynamicJsonDocument ESPCD::getFirmware(String id) {
    String url = this->baseUrl + "/firmwares/" + id;
    DynamicJsonDocument response = getRequest(url);
    return response;
}

DynamicJsonDocument ESPCD::createDevice() {
    String url = this->baseUrl + "/devices";

    DynamicJsonDocument request(2048);
    request["model"] = this->getModel();

    DynamicJsonDocument response = postRequest(url, request);
    return response;
}

DynamicJsonDocument ESPCD::getOrCreateDevice() {
    String deviceId = this->getDeviceId();
    Serial.print("local device id: ");
    Serial.println(deviceId);

    DynamicJsonDocument doc = this->getDevice(deviceId);
    if (doc.size() == 0) {
        doc = this->createDevice();

        String deviceId = doc["id"].as<String>();
        this->setDeviceId(deviceId);

        String firmwareId = DEFAULT_VALUE;
        this->setFirmwareId(firmwareId);
    }
    return doc;
}

String ESPCD::getRemoteVersion() {
    std::vector<std::pair<String, String>> params;
    String localVersion = this->getFirmwareId();
    params.push_back(std::make_pair("current", localVersion));
    String url = this->baseUrl + "/version";

    String version = DEFAULT_VALUE;
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
    String url = this->baseUrl + "/firmware";

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

            this->getOrCreateDevice();

            // String localVersion = this->getFirmwareId();
            // String remoteVersion = this->getRemoteVersion();
            // Serial.printf("Local version: %s\n", localVersion.c_str());
            // Serial.printf("Remote version: %s\n", remoteVersion.c_str());

            // if (remoteVersion != DEFAULT_VALUE && localVersion != remoteVersion) {
            //     Serial.printf("Updating to version %s\n", remoteVersion.c_str());
            //     this->setFirmwareId(remoteVersion);
            //     this->update();
            // }
        }
    }
    this->portal.handleClient();
}
