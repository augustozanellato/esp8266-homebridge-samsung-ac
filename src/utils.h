#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#pragma once

extern const char* TEXT_PLAIN;
extern const char* STATE_PARAMETER;
extern const char* TEMPERATURE_PARAMETER;
extern const char* MALFORMED_REQUEST;
extern const char* SUCCESS;

enum HeatingCoolingState {
  OFF = 0,
  HEAT = 1,
  COOL = 2,
  AUTO = 3
};

struct Status{
  HeatingCoolingState currentHeatingCoolingState, targetHeatingCoolingState;
  float targetTemperature;
  float currentTemperature, currentRelativeHumidity;
  bool dirty;
};

class OneParamRewrite : public AsyncWebRewrite{
  protected:
    String _urlPrefix;
    int _paramIndex;
    String _paramsBackup;
  public:
    OneParamRewrite(const char* from, const char* to);
    bool match(AsyncWebServerRequest *request) override;
};