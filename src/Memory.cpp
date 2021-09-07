#include "Memory.h"

void Memory::setOffset(int offset) {
    this->offset = offset;
}

#if defined(ARDUINO_ARCH_ESP32)
String Memory::getNvsValue(const char* key) {
    this->pref.begin(IDENTIFIER, false);
    String value = this->pref.getString(key, "null");
    this->pref.end();
    return value;
}

void Memory::setNvsValue(const char* key, String value) {
    this->pref.begin(IDENTIFIER, false);
    this->pref.putString(key, value.c_str());
    this->pref.end();
}
#elif defined(ARDUINO_ARCH_ESP8266)

String removeUnsafeUrlCharacters(String str) {
  String out = "";
  for (int i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    if (isAlphaNumeric(c) || c == '-' || c == '.' || c == '_' || c == '~') {
      out += c;
    }
  }
  return out;
}

void Memory::readEepromConfig() {
    EEPROM.begin(sizeof(eepromConfig));
    EEPROM.get<EEPROM_CONFIG_t>(0, eepromConfig);
    EEPROM.end();
}

void Memory::writeEepromConfig() {
    EEPROM.begin(this->offset);
    EEPROM.put<EEPROM_CONFIG_t>(0, eepromConfig);
    EEPROM.commit();
    EEPROM.end();
}
#endif

String Memory::getFirmwareId() {
#if defined(ARDUINO_ARCH_ESP32)
    return this->getNvsValue(FIRMWARE_KEY);
#elif defined(ARDUINO_ARCH_ESP8266)
    this->readEepromConfig();
    String value = String(eepromConfig.firmwareId);
    value = removeUnsafeUrlCharacters(value);
    return value == "" ? "null" : value;
#endif
}

void Memory::setFirmwareId(String id) {
#if defined(ARDUINO_ARCH_ESP32)
    this->setNvsValue(FIRMWARE_KEY, id);
#elif defined(ARDUINO_ARCH_ESP8266)
    strncpy(eepromConfig.firmwareId, id.c_str(), sizeof(EEPROM_CONFIG_t::firmwareId));
    this->writeEepromConfig();
#endif
}

String Memory::getDeviceId() {
#if defined(ARDUINO_ARCH_ESP32)
    return this->getNvsValue(DEVICE_KEY);
#elif defined(ARDUINO_ARCH_ESP8266)
    this->readEepromConfig();
    String value = String(eepromConfig.deviceId);
    value = removeUnsafeUrlCharacters(value);
    return value == "" ? "null" : value;
#endif
}

void Memory::setDeviceId(String id) {
#if defined(ARDUINO_ARCH_ESP32)
    this->setNvsValue(DEVICE_KEY, id);
#elif defined(ARDUINO_ARCH_ESP8266)
    strncpy(eepromConfig.deviceId, id.c_str(), sizeof(EEPROM_CONFIG_t::deviceId));
    this->writeEepromConfig();
#endif
}
