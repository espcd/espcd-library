#include "Requests.h"
#include "Response.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <HTTPClient.h>
#elif defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266HTTPClient.h>
#endif


void Requests::setUrl(String url) {
    if (url.endsWith("/")) {
        url = url.substring(0, url.length()-1);
    }
    this->url = url;
    this->secure = this->url.startsWith("https") ? true : false;
}

void Requests::setApiKey(String apiKey) {
    this->apiKey = apiKey;
}

void Requests::setCert(char* cert) {
    this->cert = cert;
}

void Requests::setup() {
    if (this->secure) {
        syncTime();
    }
}

void Requests::syncTime() {
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

std::unique_ptr<WiFiClient> Requests::getClient() {
    std::unique_ptr<WiFiClient> client;
    if (this->secure) {
        std::unique_ptr<WiFiClientSecure> secureClient(new WiFiClientSecure);
#if defined(ARDUINO_ARCH_ESP32)
        secureClient->setCACert((const char *) this->cert);
#elif defined(ARDUINO_ARCH_ESP8266)
        X509List cert(this->cert);
        secureClient->setTrustAnchors(&cert);
#endif
        secureClient->setTimeout(12);
        client = std::move(secureClient);
    } else {
        client = std::unique_ptr<WiFiClient>(new WiFiClient);
    }
    return client;
}

String Requests::getRedirectedUrl(String url) {
    const char * headerKeys[] = {"Location"};
    const size_t numberOfHeaders = 1;

    HTTPClient http;
    int httpCode = HTTP_CODE_FOUND;
    std::unique_ptr<WiFiClient> client = this->getClient();

    while (httpCode == HTTP_CODE_FOUND) {
        if (http.begin(*client, url)) {
            http.collectHeaders(headerKeys, numberOfHeaders);
            httpCode = http.sendRequest("HEAD");
            if (httpCode > 0) {
                if (httpCode >= 400) {
                    Serial.printf("HTTP code: %d\n", httpCode);
                }
                if (httpCode == HTTP_CODE_FOUND) {
                    url = http.header("Location");
                    url += "?api_key=" + this->apiKey;
                }
            } else {
                Serial.printf("HTTP failed, error: %s\n", http.errorToString(httpCode).c_str());
            }
        } else {
            Serial.printf("HTTP failed, error: unable to connect\n");
            break;
        }
    }
    return url;
}

String Requests::getUpdateUrl(String firmwareId) {
    return this->url + "/firmwares/" + firmwareId + "/content?api_key=" + this->apiKey;
}

Response Requests::sendRequest(String method, String url) {
    DynamicJsonDocument payload(2048);
    return this->sendRequest(method, url, payload);
}

Response Requests::sendRequest(String method, String url, DynamicJsonDocument payload) {
    DynamicJsonDocument json(2048);

    HTTPClient http;
    http.useHTTP10(true);

    url += "?api_key=" + this->apiKey;

    int httpCode = -1;
    std::unique_ptr<WiFiClient> client = this->getClient();
    if (http.begin(*client, url)) {
        String payloadStr;
        if (method == "POST" || method == "PATCH") {
            http.addHeader("Content-Type", "application/json");
            serializeJson(payload, payloadStr);
        }
        
        if (method == "GET") {
            httpCode = http.GET();
        } else if (method == "POST") {
            httpCode = http.POST(payloadStr);
        } else if (method == "PATCH") {
            httpCode = http.PATCH(payloadStr);
        }

        if (httpCode > 0) {
            if (httpCode >= 400) {
                Serial.printf("HTTP code: %d\n", httpCode);
            }
            if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
                deserializeJson(json, http.getStream());
            }
        } else {
            Serial.printf("HTTP failed, error: %s\n", http.errorToString(httpCode).c_str());
        }
        http.end();
    } else {
      Serial.printf("HTTP failed, error: unable to connect\n");
    }

    Response r;
    r.setStatusCode(httpCode);
    r.setJson(json);
    return r;
}

Response Requests::getRequest(String url) {
    return this->sendRequest("GET", url);
}

Response Requests::postRequest(String url, DynamicJsonDocument payload) {
    return this->sendRequest("POST", url, payload);
}

Response Requests::patchRequest(String url, DynamicJsonDocument payload) {
    return this->sendRequest("PATCH", url, payload);
}

Response Requests::getDevice(String id) {
    String url = this->url + "/devices/" + id;
    Response response = this->getRequest(url);
    return response;
}

Response Requests::getProduct(String id) {
    String url = this->url + "/products/" + id;
    Response response = this->getRequest(url);
    return response;
}

Response Requests::getProductFirmware(String id, String fqbn) {
    String url = this->url + "/products/" + id + "/firmware/" + fqbn;
    Response response = this->getRequest(url);
    return response;
}

Response Requests::createDevice(DynamicJsonDocument payload) {
    String url = this->url + "/devices";
    Response response = this->postRequest(url, payload);
    return response;
}

Response Requests::patchDevice(String deviceId, DynamicJsonDocument payload) {
    String url = this->url + "/devices/" + deviceId;
    Response response = this->patchRequest(url, payload);
    return response;
}
