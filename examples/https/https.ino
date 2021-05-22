#include <ESPCD.h>
#include "cert.h"

ESPCD espcd("https://test.example.com:3000/", certs_ca_pem, certs_ca_pem_len);

void setup() {
  delay(1000);
  Serial.begin(115200);
  Serial.println();

  espcd.setup();

#if defined(ARDUINO_ARCH_ESP32)
  static WebServer& server = espcd.getServer();
#elif defined(ARDUINO_ARCH_ESP8266)
  static ESP8266WebServer& server = espcd.getServer();
#endif
  server.on("/", []() {
    server.send(200, "text/plain", "Hello World");
  });
  Serial.println("HTTP server started");
}

void loop() {
  espcd.loop();
}
