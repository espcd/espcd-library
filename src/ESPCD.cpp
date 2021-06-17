#include "ESPCD.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#include <HTTPClient.h>
#elif defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#endif

#include <ArduinoJson.h>


void ESPCD::setUrl(String url) {
    requests.setUrl(url);
}

void ESPCD::setApiKey(String apiKey) {
    requests.setApiKey(apiKey);
}

void ESPCD::setProductId(String productId) {
    this->productId = productId;
}

void ESPCD::setCert(char* cert) {
    requests.setCert(cert);
}

String ESPCD::getDefaultFqbn() {
    String fqbn = "unknown";
#if defined(ARDUINO_ARCH_ESP32)
    fqbn = "esp32:esp32:esp32";
#elif defined(ARDUINO_ARCH_ESP8266)
    fqbn = "esp8266:esp8266:generic";
#endif
    return fqbn;
}

Response ESPCD::getOrCreateDevice() {
    String deviceId = memory.getDeviceId();
    Serial.println("Local device: " + deviceId);

    Response res = requests.getDevice(deviceId);
    if (res.getStatusCode() == HTTP_CODE_NOT_FOUND) {
        DynamicJsonDocument newDevice(128);
        newDevice["fqbn"] = this->getDefaultFqbn();
        if (this->productId) {
            // check if product id is valid, if not remove it
            if (requests.getProduct(this->productId).getStatusCode() == HTTP_CODE_NOT_FOUND) {
                Serial.println("Product id \"" + this->productId + "\" is invalid and therefore deleted");
                this->productId = "";
            } else {
                newDevice["product_id"] = this->productId;
            }
        }
        res = requests.createDevice(newDevice);

        if (res.ok()) {
            DynamicJsonDocument device = res.getJson();
            deviceId = device["id"].as<String>();
            memory.setDeviceId(deviceId);
            memory.setFirmwareId("null");
        }
    }
    return res;
}

void ESPCD::update(String firmwareId) {
    String url = requests.getUpdateUrl(firmwareId);

    std::unique_ptr<WiFiClient> client = requests.getClient();
    // setFollowRedirects is supported in arduino-esp32 version 2.0.0 which is currently not released
#if defined(ARDUINO_ARCH_ESP32)
    url = requests.getRedirectedUrl(url);
#elif defined(ARDUINO_ARCH_ESP8266)
    HttpUpdateClass.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
#endif
    HttpUpdateClass.rebootOnUpdate(false);
    t_httpUpdate_return ret = HttpUpdateClass.update(*client, url);
    switch (ret) {
    case HTTP_UPDATE_FAILED:
        Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", HttpUpdateClass.getLastError(), HttpUpdateClass.getLastErrorString().c_str());
        break;
    case HTTP_UPDATE_OK:
        Serial.println("HTTP_UPDATE_OK");

        // change installed firmware in the local memory
        memory.setFirmwareId(firmwareId);

        // change installed firmware in the backend
        String deviceId = memory.getDeviceId();
        DynamicJsonDocument devicePatch(64);
        devicePatch["firmware_id"] = firmwareId;
        requests.patchDevice(deviceId, devicePatch);

        // restart
        ESP.restart();
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

    this->memory.setOffset(this->portal.getEEPROMUsedSize());

    if (this->portal.begin()) {
        Serial.printf("WiFi connected: %s\n", WiFi.localIP().toString().c_str());

        // stop access point after credential input
        if (WiFi.getMode() & WIFI_AP) {
            WiFi.softAPdisconnect(true);
            WiFi.enableAP(false);
        }

        requests.setup();
    } else {
        Serial.println("Connection failed.");
    }
    Serial.printf("Set default check interval to %d seconds\n", this->interval);
}

WebServerClass& ESPCD::getServer() {
    return this->portal.host();
}

void ESPCD::loop() {
    if (WiFi.status() == WL_CONNECTED) {
        long currentMillis = millis();

        if (this->previousMillis == 0 || currentMillis - this->previousMillis >= this->interval * 1000) {
            this->previousMillis = currentMillis;

            Serial.println("");

            // get device from the backend or create a new one if it does not exist
            Response deviceResponse = this->getOrCreateDevice();
            if (!deviceResponse.ok()) {
                Serial.println("Device request failed");
                return;
            }
            DynamicJsonDocument device = deviceResponse.getJson();
            String fqbn = device["fqbn"].as<String>();
            String productId = device["product_id"].as<String>();
            if (productId == "null") {
                Serial.println("Device product not set");
                return;
            }

            // get product from the backend that is assiciated with this device
            Response productResponse = requests.getProduct(productId);
            if (!productResponse.ok()) {
                Serial.println("Product request failed");
                return;
            }
            DynamicJsonDocument product = productResponse.getJson();
            int checkInterval = product["check_interval"].as<int>();
            if (checkInterval != this->interval) {
                Serial.printf("Set check interval to %d seconds\n", checkInterval);
                this->interval = checkInterval;
            }
            bool autoUpdate = product["auto_update"].as<bool>();
            if (!autoUpdate) {
                Serial.println("Auto update disabled");
                return;
            }

            String firmwareId = memory.getFirmwareId();
            Serial.println("Local firmware: " + firmwareId);

            Response productFirmwareResponse = requests.getProductFirmware(productId, fqbn);
            if (!productFirmwareResponse.ok()) {
                Serial.println("Product firmware request failed");
                return;
            }
            DynamicJsonDocument firmware = productFirmwareResponse.getJson();
            String remoteFirmware = firmware["id"].as<String>();
            Serial.println("Remote firmware: " + remoteFirmware);
            if (remoteFirmware == "null") {
                Serial.println("No firmware available");
                return;
            }

            if (firmwareId == remoteFirmware) {
                Serial.println("Latest firmware already installed");
                return;
            }

            Serial.println("Update in progress...");
            this->update(remoteFirmware);
        }
    }
    this->portal.handleClient();
}
