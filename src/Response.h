#ifndef RESPONSE_H
#define RESPONSE_H

#include "Arduino.h"
#include <ArduinoJson.h>


class Response {
public:
    bool ok();

    int getStatusCode();
    void setStatusCode(int statusCode);

    DynamicJsonDocument getJson();
    void setJson(DynamicJsonDocument json);
private:
    int statusCode = -1;
    DynamicJsonDocument json = DynamicJsonDocument(2048);
};

#endif
