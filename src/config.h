#pragma once
#include <Arduino.h>
#include <ESP8266WiFi.h>

//can't use a const and a constexpr if because toolchain doesn't support c++17
#define DEBUG false

extern const unsigned long UPDATE_INTERVAL;
extern const int SERVER_PORT;

extern const IPAddress deviceIP;
extern const IPAddress serverIP;
extern const IPAddress gatewayIP;
extern const IPAddress netMask;
extern const IPAddress dnsIP;

extern const char* SSID PROGMEM;
extern const char* PASSWORD PROGMEM;
