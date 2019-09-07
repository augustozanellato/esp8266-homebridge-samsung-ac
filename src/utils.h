#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

#define DEBUG false

#if DEBUG
#define DEBUG_PRINTF(x, ...) Serial.printf(PSTR(x), ##__VA_ARGS__)
#else
#define DEBUG_PRINTF(...)
#endif

extern const char* TEXT_PLAIN;
extern const char* STATE_PARAMETER;
extern const char* TEMPERATURE_PARAMETER;
extern const char* WEBUI_PARAMETER;
extern const char* MALFORMED_REQUEST;
extern const char* SUCCESS;
extern const char* HOMEBRIDGE_UPDATE_URLS[];

enum HeatingCoolingState {
  STATE_OFF = 0,
  STATE_HEAT = 1,
  STATE_COOL = 2,
  STATE_AUTO = 3
};

enum FanState {
  FAN_QUIET = 0,
  FAN_LOW = 1,
  FAN_MEDIUM = 2,
  FAN_HIGH = 3,
  FAN_AUTO = 4
};

enum StatusIndex{
  TARGET_HEATING_COOLING_STATE_INDEX = 0,
  TARGET_TEMPERATURE_INDEX = 1,
  COOLING_THRESHOLD_TEMPERATURE_INDEX = 2,
  HEATING_THRESHOLD_TEMPERATURE_INDEX = 3
};

struct Status {
  HeatingCoolingState currentHeatingCoolingState, targetHeatingCoolingState;
  int targetTemperature;
  int currentTemperature, currentRelativeHumidity;
  int coolingThresholdTemperature, heatingThresholdTemperature;
  bool dirty;
  FanState fanState;
};

class OneParamRewrite : public AsyncWebRewrite{
  protected:
    String _urlPrefix;
    int _paramIndex;
    String _paramsBackup;
  public:
    OneParamRewrite(const char*, const char*);
    bool match(AsyncWebServerRequest*) override;
};

DynamicJsonDocument generateJsonFromStatus(const Status&);
int getStatusValueByIndex(const Status&, const StatusIndex);
void debugPrintRequest(AsyncWebServerRequest*);