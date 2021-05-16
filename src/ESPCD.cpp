#include "ESPCD.h"
#include "Requests.h"


ESPCD::ESPCD(String baseUrl) {
    if (baseUrl.endsWith("/")) {
        baseUrl = baseUrl.substring(0, baseUrl.length()-1);
    }
    this->baseUrl = baseUrl;
    this->previousMillis = 0;
}

String ESPCD::getModel() {
    String model = DEFAULT_VALUE;
#if defined(ARDUINO_ARCH_ESP32)
    model = "esp32";
#elif defined(ARDUINO_ARCH_ESP8266)
    model = "esp8266";
#endif
    return model;
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

Response ESPCD::getDevice(String id) {
    String url = this->baseUrl + "/devices/" + id;
    Response response = requests.getRequest(url);
    return response;
}

DynamicJsonDocument ESPCD::getProduct(String id) {
    String url = this->baseUrl + "/products/" + id;
    Response response = requests.getRequest(url);
    return response.getJson();
}

DynamicJsonDocument ESPCD::createDevice() {
    String url = this->baseUrl + "/devices";

    DynamicJsonDocument request(2048);
    request["model"] = this->getModel();

    Response response = requests.postRequest(url, request);
    return response.getJson();
}

DynamicJsonDocument ESPCD::getOrCreateDevice() {
    String deviceId = this->getDeviceId();
    Serial.print("local device id: ");
    Serial.println(deviceId);

    Response res = this->getDevice(deviceId);
    DynamicJsonDocument doc = res.getJson();
    if (res.getStatusCode() == HTTP_CODE_NOT_FOUND) {
        doc = this->createDevice();

        String deviceId = doc["id"].as<String>();
        this->setDeviceId(deviceId);

        String firmwareId = DEFAULT_VALUE;
        this->setFirmwareId(firmwareId);
    } else {
        Serial.println("Do not create new device, httpCode != HTTP_CODE_NOT_FOUND");
    }
    return doc;
}

void ESPCD::update(String firmwareId) {
    String url = this->baseUrl + "/firmwares/" + firmwareId + "/content";

    std::unique_ptr<WiFiClient> client = requests.getClient();
    // setFollowRedirects is supported in arduino-esp32 version 2.0.0 which is currently not released
#if defined(ARDUINO_ARCH_ESP32)
    url = requests.getRedirectedUrl(url);
#elif defined(ARDUINO_ARCH_ESP8266)
    HttpUpdateClass.setFollowRedirects(true);
#endif
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

    AutoConnectConfig config;
    config.autoReconnect = true;
    config.reconnectInterval = 6;
#if defined(ARDUINO_ARCH_ESP8266)
    config.boundaryOffset = sizeof(EEPROM_CONFIG_t);
#endif
    this->portal.config(config);

    if (this->portal.begin()) {
        Serial.printf("WiFi connected: %s\n", WiFi.localIP().toString().c_str());

        bool secure = baseUrl.startsWith("https") ? true : false;
        requests.setSecure(secure);
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

            DynamicJsonDocument device = this->getOrCreateDevice();
            String productId = device["product_id"].as<String>();

            if (productId == DEFAULT_VALUE) {
                Serial.println("Http Error or no product set");
                return;
            }

            DynamicJsonDocument product = this->getProduct(productId);
            String availableFirmware = product["firmware_id"].as<String>();
            bool autoUpdate = product["auto_update"].as<bool>();

            Serial.print("availableFirmware: ");
            Serial.println(availableFirmware);

            String firmwareId = this->getFirmwareId();
            Serial.print("localFirmware: ");
            Serial.println(firmwareId);

            if (autoUpdate && availableFirmware != DEFAULT_VALUE && firmwareId != availableFirmware) {
                Serial.println("do update");
                this->setFirmwareId(availableFirmware);

                String deviceId = this->getDeviceId();
                String url = this->baseUrl + "/devices/" + deviceId;
                DynamicJsonDocument request(2048);
                request["firmware_id"] = availableFirmware;
                requests.patchRequest(url, request);

                this->update(availableFirmware);
            }
        }
    }
    this->portal.handleClient();
}
