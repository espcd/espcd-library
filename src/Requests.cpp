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

void Requests::setCert(const char* cert) {
    this->cert = cert;
}

void Requests::setup() {
    if (this->secure) {
        syncTime();
    }
    this->initClient();
}

void Requests::syncTime() {
    // trigger NTP time sync
    DEBUG_MSG("Syncing NTP time...\n");
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
    DEBUG_MSG("Current time: %s\n", asctime(&timeinfo));
}

void Requests::initClient() {
    if (this->secure) {
        WiFiClientSecure* secureClient = new WiFiClientSecure();
#if defined(ARDUINO_ARCH_ESP32)
        secureClient->setCACert(this->cert);
#elif defined(ARDUINO_ARCH_ESP8266)
        secureClient->setTrustAnchors(new X509List(this->cert));
#endif
        this->client = dynamic_cast<WiFiClient*>(secureClient);
    } else {
        this->client = new WiFiClient();
    }
    this->client->setTimeout(12);
}

WiFiClient* Requests::getClient() {
    return this->client;
}

String Requests::getRedirectedUrl(String url) {
    const char * headerKeys[] = {"Location"};
    const size_t numberOfHeaders = 1;

    HTTPClient http;
    int httpCode = HTTP_CODE_FOUND;
    while (httpCode == HTTP_CODE_FOUND) {
        if (http.begin(*this->client, url)) {
            http.collectHeaders(headerKeys, numberOfHeaders);
            httpCode = http.sendRequest("HEAD");
            if (httpCode > 0) {
                if (httpCode >= 400) {
                    DEBUG_MSG("HTTP code: %d\n", httpCode);
                }
                if (httpCode == HTTP_CODE_FOUND) {
                    url = http.header("Location");
                }
            } else {
                DEBUG_MSG("HTTP failed, error: %s\n", http.errorToString(httpCode).c_str());
            }
        } else {
            DEBUG_MSG("HTTP failed, error: unable to connect\n");
            break;
        }
    }
    http.end();
    return url;
}

String Requests::getUpdateUrl(String firmwareId) {
    return this->url + "/firmwares/" + firmwareId + "/content?api_key=" + this->apiKey;
}

Response Requests::sendRequest(String method, String url) {
    DynamicJsonDocument requestJson(0);
    return this->sendRequest(method, url, requestJson);
}

Response Requests::sendRequest(String method, String url, DynamicJsonDocument &requestJson) {
    url += "?api_key=" + this->apiKey;

    HTTPClient http;
    int httpCode = -1;
    String body = "";
    if (http.begin(*this->client, url)) {
        String requestStr;
        if (method == "POST" || method == "PATCH") {
            http.addHeader("Content-Type", "application/json");
            serializeJson(requestJson, requestStr);
        }
        
        if (method == "GET") {
            httpCode = http.GET();
        } else if (method == "POST") {
            httpCode = http.POST(requestStr);
        } else if (method == "PATCH") {
            httpCode = http.PATCH(requestStr);
        }

        if (httpCode > 0) {
            if (httpCode >= 400) {
                DEBUG_MSG("HTTP code: %d\n", httpCode);
            }
            if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
                body = http.getString();
            }
        } else {
            DEBUG_MSG("HTTP failed, error: %s\n", http.errorToString(httpCode).c_str());
        }
    } else {
      DEBUG_MSG("HTTP failed, error: unable to connect\n");
    }
    http.end();

    Response r;
    r.setStatusCode(httpCode);
    r.setBody(body);
    return r;
}

Response Requests::getRequest(String url) {
    return this->sendRequest("GET", url);
}

Response Requests::postRequest(String url, DynamicJsonDocument &requestJson) {
    return this->sendRequest("POST", url, requestJson);
}

Response Requests::patchRequest(String url, DynamicJsonDocument &requestJson) {
    return this->sendRequest("PATCH", url, requestJson);
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

Response Requests::createDevice(DynamicJsonDocument &requestJson) {
    String url = this->url + "/devices";
    Response response = this->postRequest(url, requestJson);
    return response;
}

Response Requests::patchDevice(String deviceId, DynamicJsonDocument &requestJson) {
    String url = this->url + "/devices/" + deviceId;
    Response response = this->patchRequest(url, requestJson);
    return response;
}
