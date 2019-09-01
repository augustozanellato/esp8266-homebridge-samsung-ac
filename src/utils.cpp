#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <utils.h>

const char* TEXT_PLAIN PROGMEM = "text/plain";
const char* STATE_PARAMETER PROGMEM = "state";
const char* TEMPERATURE_PARAMETER PROGMEM = "temp";
const char* MALFORMED_REQUEST PROGMEM = "Malformed request";
const char* SUCCESS PROGMEM = "Ok";

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