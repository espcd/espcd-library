#include "ESPCD.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#include <HTTPClient.h>
#elif defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#endif

#include <WiFiClientSecure.h>
#include <ArduinoJson.h>


ESPCD::ESPCD() {}

ESPCD::ESPCD(String baseUrl) {
    this->setBaseUrl(baseUrl);
}

ESPCD::ESPCD(String baseUrl, unsigned char* certs_ca_pem, unsigned int certs_ca_pem_len) {
    this->setBaseUrl(baseUrl);
    this->setCert(certs_ca_pem, certs_ca_pem_len);
}

void ESPCD::setBaseUrl(String baseUrl) {
    if (baseUrl.endsWith("/")) {
        baseUrl = baseUrl.substring(0, baseUrl.length()-1);
    }
    this->baseUrl = baseUrl;
}

void ESPCD::setCert(unsigned char* certs_ca_pem, unsigned int certs_ca_pem_len) {
    requests.setCert(certs_ca_pem, certs_ca_pem_len);
}

String ESPCD::getModel() {
    String model = "unknown";
#if defined(ARDUINO_ARCH_ESP32)
    model = "esp32";
#elif defined(ARDUINO_ARCH_ESP8266)
    model = "esp8266";
#endif
    return model;
}

Response ESPCD::getDevice(String id) {
    String url = this->baseUrl + "/devices/" + id;
    Response response = requests.getRequest(url);
    return response;
}

Response ESPCD::getProduct(String id) {
    String url = this->baseUrl + "/products/" + id;
    Response response = requests.getRequest(url);
    return response;
}

Response ESPCD::createDevice() {
    String url = this->baseUrl + "/devices";

    DynamicJsonDocument payload(2048);
    payload["model"] = this->getModel();

    Response response = requests.postRequest(url, payload);
    return response;
}

Response ESPCD::getOrCreateDevice() {
    String deviceId = memory.getDeviceId();
    Serial.println("local device id: " + deviceId);

    Response res = this->getDevice(deviceId);
    if (res.getStatusCode() == HTTP_CODE_NOT_FOUND) {
        res = this->createDevice();
        DynamicJsonDocument json = res.getJson();
        String deviceId = json["id"].as<String>();
        
        memory.setDeviceId(deviceId);
        memory.setFirmwareId("");
    }
    return res;
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

            Response deviceResponse = this->getOrCreateDevice();
            if (!deviceResponse.ok()) {
                Serial.println("Device request failed");
                return;
            }
            DynamicJsonDocument device = deviceResponse.getJson();
            String productId = device["product_id"].as<String>();

            if (productId == "null") {
                Serial.println("Device product not set");
                return;
            }

            Response productResponse = this->getProduct(productId);
            if (!deviceResponse.ok()) {
                Serial.println("Product request failed");
                return;
            }
            DynamicJsonDocument product = productResponse.getJson();
            String availableFirmware = product["firmware_id"].as<String>();
            bool autoUpdate = product["auto_update"].as<bool>();

            Serial.println("availableFirmware: " + availableFirmware);

            String firmwareId = memory.getFirmwareId();
            Serial.println("localFirmware: " + firmwareId);

            if (autoUpdate && availableFirmware != "null" && firmwareId != availableFirmware) {
                Serial.println("Do update...");
                memory.setFirmwareId(availableFirmware);

                String deviceId = memory.getDeviceId();
                String url = this->baseUrl + "/devices/" + deviceId;
                DynamicJsonDocument payload(2048);
                payload["firmware_id"] = availableFirmware;
                requests.patchRequest(url, payload);

                this->update(availableFirmware);
            }
        }
    }
    this->portal.handleClient();
}
