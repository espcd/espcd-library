#include "Requests.h"

#include "Response.h"
#include "cert.h"
#if defined(ARDUINO_ARCH_ESP32)
#include <HTTPClient.h>
#elif defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266HTTPClient.h>
#endif


Requests::Requests(bool secure) {
    this->setSecure(secure);
}

void Requests::setSecure(bool secure) {
    this->secure = secure;
    if (secure) {
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
                Serial.printf("HTTP code: %d\n", httpCode);
                if (httpCode == HTTP_CODE_FOUND) {
                    url = http.header("Location");
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

Response Requests::sendRequest(String method, String url) {
    DynamicJsonDocument body(2048);
    return this->sendRequest(method, url, body);
}

Response Requests::sendRequest(String method, String url, DynamicJsonDocument body) {
    DynamicJsonDocument data(2048);

    HTTPClient http;
    http.useHTTP10(true);

    int httpCode = -1;

    std::unique_ptr<WiFiClient> client = this->getClient();
    if (http.begin(*client, url)) {
        String json;
        if (body.size() > 0) {
            http.addHeader("Content-Type", "application/json");
            serializeJson(body, json);
        }
        
        bool ok;
        bool hasBody;
        if (method == "GET") {
            httpCode = http.GET();
            ok = httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY;
            hasBody = ok;
        } else if (method == "POST") {
            httpCode = http.POST(json);
            ok = httpCode == HTTP_CODE_CREATED;
            hasBody = ok;
        } else if (method == "PATCH") {
            httpCode = http.PATCH(json);
            ok = httpCode == HTTP_CODE_NO_CONTENT;
            hasBody = false;
        }

        if (httpCode > 0) {
            Serial.printf("HTTP code: %d\n", httpCode);
            if (hasBody) {
                deserializeJson(data, http.getStream());
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
    r.setJson(data);
    return r;
}

Response Requests::getRequest(String url) {
    return this->sendRequest("GET", url);
}

Response Requests::postRequest(String url, DynamicJsonDocument body) {
    return this->sendRequest("POST", url, body);
}

Response Requests::patchRequest(String url, DynamicJsonDocument body) {
    return this->sendRequest("PATCH", url, body);
}
