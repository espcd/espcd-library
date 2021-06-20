#include "Response.h"


int Response::getStatusCode() {
    return this->statusCode;
}

void Response::setStatusCode(int statusCode) {
    this->statusCode = statusCode;
}

String Response::getBody() {
    return this->body;
}

void Response::setBody(String body) {
    this->body = body;
}

bool Response::ok() {
    return this->statusCode > 0 && this->statusCode < 400;
}
