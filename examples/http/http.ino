#include <ESPCD.h>

ESPCD espcd;

void setup() {
  delay(1000);
  Serial.begin(115200);
  Serial.println();

  espcd.setUrl("http://api.espcd.duckdns.org/");
  espcd.setApiKey("563ba65f-d4fd-474a-9d51-7d751aa96bb5");
  espcd.setProductId("3c6b01ec-d94a-4c23-9f25-543ff8457f39");  // if a new device needs to be created, associate this product automatically
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
