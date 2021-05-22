#include "Memory.h"

#if defined(ARDUINO_ARCH_ESP32)
String Memory::getNvsValue(const char* key) {
    this->pref.begin(IDENTIFIER, false);
    String value = this->pref.getString(key, "");
    this->pref.end();
    return value;
}

void Memory::setNvsValue(const char* key, String value) {
    this->pref.begin(IDENTIFIER, false);
    this->pref.putString(key, value.c_str());
    this->pref.end();
}
#elif defined(ARDUINO_ARCH_ESP8266)
String Memory::getEepromValue(String key) {
    EEPROM_CONFIG_t eepromConfig;
    EEPROM.begin(sizeof(eepromConfig));
    EEPROM.get<EEPROM_CONFIG_t>(0, eepromConfig);
    EEPROM.end();
    String value = String(key);
    return value;
}

void Memory::setEepromValue(String key, String value, size_t size) {
    EEPROM_CONFIG_t eepromConfig;
    strncpy(key, value.c_str(), size);
    EEPROM.begin(this->portal.getEEPROMUsedSize());
    EEPROM.put<EEPROM_CONFIG_t>(0, eepromConfig);
    EEPROM.commit();
    EEPROM.end();
}
#endif

String Memory::getFirmwareId() {
#if defined(ARDUINO_ARCH_ESP32)
    return this->getNvsValue(FIRMWARE_KEY);
#elif defined(ARDUINO_ARCH_ESP8266)
    return this->getEepromValue(eepromConfig.firmwareId);
#endif
}

void Memory::setFirmwareId(String id) {
#if defined(ARDUINO_ARCH_ESP32)
    this->setNvsValue(FIRMWARE_KEY, id);
#elif defined(ARDUINO_ARCH_ESP8266)
    this->setEepromValue(eepromConfig.firmwareId, id, sizeof(EEPROM_CONFIG_t::firmwareId));
#endif
}

String Memory::getDeviceId() {
#if defined(ARDUINO_ARCH_ESP32)
    return this->getNvsValue(DEVICE_KEY);
#elif defined(ARDUINO_ARCH_ESP8266)
    return this->getEepromValue(eepromConfig.deviceId);
#endif
}

void Memory::setDeviceId(String id) {
#if defined(ARDUINO_ARCH_ESP32)
    this->setNvsValue(DEVICE_KEY, id);
#elif defined(ARDUINO_ARCH_ESP8266)
    this->setEepromValue(eepromConfig.deviceId, id, sizeof(EEPROM_CONFIG_t::deviceId));
#endif
}
