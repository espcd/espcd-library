#ifndef RESPONSE_H
#define RESPONSE_H

#include "Arduino.h"
#include <ArduinoJson.h>


class Response {
public:
    bool ok();

    int getStatusCode();
    void setStatusCode(int statusCode);

    String getBody();
    void setBody(String body);
private:
    int statusCode = -1;
    String body;
};

#endif
