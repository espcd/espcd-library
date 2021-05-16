#include "Response.h"


int Response::getStatusCode() {
    return this->statusCode;
}

void Response::setStatusCode(int statusCode) {
    this->statusCode = statusCode;
}

DynamicJsonDocument Response::getJson() {
    return this->json;
}

void Response::setJson(DynamicJsonDocument json) {
    this->json = json;
}
