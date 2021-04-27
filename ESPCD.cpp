#include "ESPCD.h"
#include "Arduino.h"
#include <WiFi.h>
#include <HTTPUpdate.h>
#include <AutoConnect.h>

AutoConnect Portal;
AutoConnectConfig Config;

ESPCD::ESPCD(char* url) {
    this->url = url;
}

void ESPCD::setup() {
    Serial.println("Hello from setup");

    Config.autoReconnect = true;
    Config.reconnectInterval = 6;
    Portal.config(Config);
    
    if (Portal.begin()) {
        Serial.println("WiFi connected: " + WiFi.localIP().toString());
    } else {
        Serial.println("Connection failed.");
    }
}

void ESPCD::loop() {
    // Serial.println("Hello from loop");

    if (WiFi.status() == WL_CONNECTED) {
        WiFiClient client;
        client.setTimeout(12000 / 1000);
    
        t_httpUpdate_return ret = httpUpdate.update(client, this->url);
        switch (ret) {
        case HTTP_UPDATE_FAILED:
            Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
            break;
        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("HTTP_UPDATE_NO_UPDATES");
            break;
        case HTTP_UPDATE_OK:
            Serial.println("HTTP_UPDATE_OK");
            break;
        }
    }

    Portal.handleClient();
}
