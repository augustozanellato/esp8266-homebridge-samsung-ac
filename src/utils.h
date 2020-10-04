#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "config.h"

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

enum class HeatingCoolingState: int {
  Off = 0,
  Heat = 1,
  Cool = 2,
  Auto = 3
};
enum class FanState: int {
  Quiet = 0,
  Low = 1,
  Medium = 2,
  High = 3,
  Auto = 4
};

enum class StatusIndex: int {
  TargetHeatingCoolingState = 0,
  TargetTemperature = 1,
  CoolingThresholdTemperature = 2,
  HeatingThresholdTemperature = 3
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