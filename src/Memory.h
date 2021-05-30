#ifndef MEMORY_H
#define MEMORY_H

#include "Arduino.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <Preferences.h>
#define IDENTIFIER "ESPCD"
#define DEVICE_KEY "deviceId"
#define FIRMWARE_KEY "firmwareId"
#elif defined(ARDUINO_ARCH_ESP8266)
#include <EEPROM.h>
typedef struct {
  char deviceId[36+1];  // hexadecimal uuid is 36 digits long + null character as string terminator
  char firmwareId[36+1];
} EEPROM_CONFIG_t;
#endif

class Memory {
  public:
    void setOffset(int offset);
    String getFirmwareId();
    void setFirmwareId(String id);
    String getDeviceId();
    void setDeviceId(String id);
  private:
    int offset = 0;
#if defined(ARDUINO_ARCH_ESP32)
    Preferences pref;
#endif

#if defined(ARDUINO_ARCH_ESP32)
    String getNvsValue(const char* key);
    void setNvsValue(const char* key, String value);
#elif defined(ARDUINO_ARCH_ESP8266)
    String getEepromValue(String key);
    void setEepromValue(String key, String value, size_t size);
#endif
};

#endif
