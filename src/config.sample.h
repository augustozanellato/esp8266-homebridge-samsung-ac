#include <Arduino.h>
#include <ESP8266WiFi.h>

//set DEBUG to true in order to get debug messages on uart
#define DEBUG false
//controls the update interval for the dht11 sensor
#define UPDATE_DELAY 5000
//the port on which the server will run
#define SERVER_PORT 80

//your network configuration
const IPAddress deviceIP(192, 168, 1, 3);
const IPAddress serverIP(192, 168, 1, 2);
const IPAddress gatewayIP(192, 168, 1, 1);
const IPAddress netMask(255, 255, 255, 0);
const IPAddress dnsIP(8, 8, 8, 8);

//Your wifi ssid
const char* SSID PROGMEM = "YourNetworkSSID";
//your wifi password
const char* PASSWORD PROGMEM = "YourNetworkPassword";