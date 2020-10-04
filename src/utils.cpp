#include "utils.h"
#include "config.h"

const char* TEXT_PLAIN PROGMEM = "text/plain";
const char* STATE_PARAMETER PROGMEM = "state";
const char* TEMPERATURE_PARAMETER PROGMEM = "temp";
const char* WEBUI_PARAMETER PROGMEM = "web";
const char* MALFORMED_REQUEST PROGMEM = "Malformed request";
const char* SUCCESS PROGMEM = "Ok";
const char* HOMEBRIDGE_UPDATE_URLS[] = {
    "/targetHeatingCoolingState/",
    "/targetTemperature/",
    "/coolingThresholdTemperature/",
    "/heatingThresholdTemperature/"
};

bool OneParamRewrite::match(AsyncWebServerRequest *request) {
    if(request->url().startsWith(_urlPrefix)) {
        if(_paramIndex >= 0) {
            _params = _paramsBackup + request->url().substring(_paramIndex);
        } else {
            _params = _paramsBackup;
        }
        return true;
    } else {
        return false;
    }
}
OneParamRewrite::OneParamRewrite(const char* from, const char* to) : AsyncWebRewrite(from, to) {
    _paramIndex = _from.indexOf('{');

    if( _paramIndex >=0 && _from.endsWith("}")) {
        _urlPrefix = _from.substring(0, _paramIndex);
        int index = _params.indexOf('{');
        if(index >= 0) {
            _params = _params.substring(0, index);
        }
    } else {
        _urlPrefix = _from;
    }
    _paramsBackup = _params;
}

DynamicJsonDocument generateJsonFromStatus(const Status& status){
    DynamicJsonDocument json(1024);
    json["targetHeatingCoolingState"] = (int) status.targetHeatingCoolingState;
    json["currentHeatingCoolingState"] = (int) status.currentHeatingCoolingState;
    json["currentTemperature"] = status.currentTemperature;
    json["targetTemperature"] = status.targetTemperature;
    json["currentRelativeHumidity"] = status.currentRelativeHumidity;
    json["coolingThresholdTemperature"] = status.coolingThresholdTemperature;
    json["heatingThresholdTemperature"] = status.heatingThresholdTemperature;
    return json;
}


int getStatusValueByIndex(const Status& status, const StatusIndex idx){
    switch (idx){
        case StatusIndex::TargetHeatingCoolingState:
            return (int) status.targetHeatingCoolingState;
        case StatusIndex::TargetTemperature:
            return status.targetTemperature;
        case StatusIndex::CoolingThresholdTemperature:
            return status.coolingThresholdTemperature;
        case StatusIndex::HeatingThresholdTemperature:
            return status.heatingThresholdTemperature;
        default:
            return -1;
    }
}
void debugPrintRequest(AsyncWebServerRequest* request){
    #if DEBUG
    DEBUG_PRINTF("Handling request %s %s from %s\n", request->methodToString(), request->url().c_str(), request->client()->remoteIP().toString().c_str());

    const int headers = request->headers();
    for(int i=0;i<headers;i++){
        const AsyncWebHeader* h = request->getHeader(i);
        DEBUG_PRINTF("HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
    }

    int params = request->params();
    for(int i=0;i<params;i++){
        const AsyncWebParameter* p = request->getParam(i);
        if(p->isFile()){ //p->isPost() is also true
            DEBUG_PRINTF("FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
        } else if(p->isPost()){
            DEBUG_PRINTF("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
        } else {
            DEBUG_PRINTF("GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
        }
    }
    #endif
}