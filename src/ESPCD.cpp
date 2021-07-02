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
    this->requests.setUrl(url);
}

void ESPCD::setApiKey(String apiKey) {
    this->requests.setApiKey(apiKey);
}

void ESPCD::setProductId(String productId) {
    this->productId = productId;
}

void ESPCD::setCert(const char* cert) {
    this->requests.setCert(cert);
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
    String deviceId = this->memory.getDeviceId();
    DEBUG_MSG("Local device: %s\n", deviceId.c_str());

    Response res = this->requests.getDevice(deviceId);
    if (res.getStatusCode() == HTTP_CODE_NOT_FOUND) {
        DynamicJsonDocument newDevice(128);
        newDevice["fqbn"] = this->getDefaultFqbn();
        if (this->productId) {
            // check if product id is valid, if not remove it
            if (this->requests.getProduct(this->productId).getStatusCode() == HTTP_CODE_NOT_FOUND) {
                DEBUG_MSG("Product id %s is invalid and therefore deleted\n", this->productId.c_str());
                this->productId = "";
            } else {
                newDevice["product_id"] = this->productId;
            }
        }
        res = this->requests.createDevice(newDevice);

        if (res.ok()) {
            DynamicJsonDocument device(768);
            deserializeJson(device, res.getBody());

            deviceId = device["id"].as<String>();
            this->memory.setDeviceId(deviceId);
            this->memory.setFirmwareId("null");
        }
    }
    return res;
}

void ESPCD::update(String firmwareId) {
    String url = this->requests.getUpdateUrl(firmwareId);

    WiFiClient* client = this->requests.getClient();
#if defined(ARDUINO_ARCH_ESP32)
    // setFollowRedirects is supported in arduino-esp32 version 2.0.0 which is currently not released
    url = this->requests.getRedirectedUrl(url);
#elif defined(ARDUINO_ARCH_ESP8266)
    HttpUpdateClass.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
#endif
    HttpUpdateClass.rebootOnUpdate(false);
    t_httpUpdate_return ret = HttpUpdateClass.update(*client, url);
    switch (ret) {
    case HTTP_UPDATE_FAILED:
        DEBUG_MSG("HTTP_UPDATE_FAILED Error (%d): %s\n", HttpUpdateClass.getLastError(), HttpUpdateClass.getLastErrorString().c_str());
        break;
    case HTTP_UPDATE_OK:
        DEBUG_MSG("HTTP_UPDATE_OK\n");

        // change installed firmware in the local memory
        this->memory.setFirmwareId(firmwareId);

        // change installed firmware in the backend
        String deviceId = this->memory.getDeviceId();
        DynamicJsonDocument devicePatch(64);
        devicePatch["firmware_id"] = firmwareId;
        this->requests.patchDevice(deviceId, devicePatch);
    }
    ESP.restart();
}

long randomiseInterval(int interval) {
    return random(interval * 0.9, interval * 1.1);
}

void ESPCD::setup() {
    DEBUG_MSG("Hello from setup\n");

    randomSeed(analogRead(0));

    AutoConnectConfig config;
    config.autoReconnect = true;
    config.portalTimeout = 10000;
    config.retainPortal = true;

#if defined(ARDUINO_ARCH_ESP8266)
    config.boundaryOffset = sizeof(EEPROM_CONFIG_t);
#endif
    this->portal.config(config);

    this->memory.setOffset(this->portal.getEEPROMUsedSize());

    while (true) {
        if (this->portal.begin()) {
            DEBUG_MSG("WiFi connected: %s\n", WiFi.localIP().toString().c_str());

            // stop access point after credential input
            if (WiFi.getMode() & WIFI_AP) {
                WiFi.softAPdisconnect(true);
                WiFi.enableAP(false);
            }

            this->requests.setup();

            break;
        } else {
            DEBUG_MSG("Connection failed.\n");
        }
    }
    
    DEBUG_MSG("Set default check interval to %d seconds\n", this->interval);
}

WebServerClass& ESPCD::getServer() {
    return this->portal.host();
}

void ESPCD::loop() {
    if (WiFi.status() == WL_CONNECTED) {
        long currentMillis = millis();

        if (this->previousMillis == 0 || currentMillis - this->previousMillis >= this->randomisedInterval * 1000) {
            DEBUG_MSG("\n");

            this->previousMillis = currentMillis;
            this->randomisedInterval = randomiseInterval(this->interval);
            DEBUG_MSG("Set randomised check interval to %d seconds\n", this->randomisedInterval);

            // get device from the backend or create a new one if it does not exist
            Response deviceResponse = this->getOrCreateDevice();
            if (!deviceResponse.ok()) {
                DEBUG_MSG("Device request failed\n");
                return;
            }
            DynamicJsonDocument device(768);
            deserializeJson(device, deviceResponse.getBody());

            String fqbn = device["fqbn"].as<String>();
            String productId = device["product_id"].as<String>();
            if (productId == "null") {
                DEBUG_MSG("Device product not set\n");
                return;
            }

            // get product from the backend that is assiciated with this device
            Response productResponse = this->requests.getProduct(productId);
            if (!productResponse.ok()) {
                DEBUG_MSG("Product request failed\n");
                return;
            }
            DynamicJsonDocument product(768);
            deserializeJson(product, productResponse.getBody());
            int checkInterval = product["check_interval"].as<int>();
            if (checkInterval != this->interval) {
                DEBUG_MSG("Set check interval to %d seconds\n", checkInterval);
                this->interval = checkInterval;
                this->randomisedInterval = randomiseInterval(checkInterval);
                DEBUG_MSG("Set randomised check interval to %d seconds\n", this->randomisedInterval);
            }
            bool autoUpdate = product["auto_update"].as<bool>();
            if (!autoUpdate) {
                DEBUG_MSG("Auto update disabled\n");
                return;
            }

            String firmwareId = this->memory.getFirmwareId();
            DEBUG_MSG("Local firmware: %s\n", firmwareId.c_str());

            Response productFirmwareResponse = this->requests.getProductFirmware(productId, fqbn);
            if (!productFirmwareResponse.ok()) {
                DEBUG_MSG("Product firmware request failed");
                return;
            }
            DynamicJsonDocument firmware(768);
            deserializeJson(firmware, productFirmwareResponse.getBody());
            String remoteFirmware = firmware["id"].as<String>();
            DEBUG_MSG("Remote firmware: %s\n", remoteFirmware.c_str());
            if (remoteFirmware == "null") {
                DEBUG_MSG("No firmware available\n");
                return;
            }

            if (firmwareId == remoteFirmware) {
                DEBUG_MSG("Latest firmware already installed\n");
                return;
            }

            DEBUG_MSG("Update in progress...\n");
            this->update(remoteFirmware);
        }
    }
    this->portal.handleClient();
}
