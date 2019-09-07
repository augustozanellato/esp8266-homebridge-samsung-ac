#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <DHTesp.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Samsung.h>
#include <FS.h>

#include "config.h"
#include "utils.h"

AsyncWebServer server(SERVER_PORT);
AsyncEventSource events("/events");
WiFiClient wifiClient;
IRSamsungAc samsungAC(D2, true);
DHTesp espDHT;
Status status;
bool shouldUpdateStatus = false;
bool shouldPushUpdateToHomebridge[4];

void sendEvent(const char* eventName, const int eventValue){
    static char conversionBuffer[4];
    itoa(eventValue, conversionBuffer, 10);
    events.send(conversionBuffer, eventName, millis(), 1000);
}

void sendHomebridgeUpdate(const char* url, const int value){
    static char conversionBuffer[4];
    static char URLBuffer[50];
    HTTPClient client;
    itoa(value, conversionBuffer, 10);
    strcpy(URLBuffer, url);
    strcat(URLBuffer, conversionBuffer);
    client.begin(wifiClient, serverIP.toString().c_str(), 2000, URLBuffer);

    int httpCode = client.GET();

    if(httpCode > 0) {
        DEBUG_PRINTF("GET %s: OK!\n", URLBuffer);
    } else {
        DEBUG_PRINTF("GET %s: FAIL (%s)\n", URLBuffer, client.errorToString(httpCode).c_str());
    }

    client.end(); 
}

void updateStatus(){
    float temperature = espDHT.getTemperature();
    float humidity = espDHT.getHumidity();

    if ((isnan(status.currentTemperature) || isnan(status.currentRelativeHumidity))){
        digitalWrite(16, LOW);
        return;
    } else {
        status.currentTemperature = temperature;
        status.currentRelativeHumidity = humidity;
        sendEvent(PSTR("currTemp"), status.currentTemperature);
        sendEvent(PSTR("currHum"), status.currentRelativeHumidity);
    }
    DEBUG_PRINTF("Temperature: %d, Humidity: %d, targetTemperature: %d, coolingThresholdTemperature: %d, heatingThresholdTemperature: %d", status.currentTemperature, status.currentRelativeHumidity, status.targetTemperature, status.coolingThresholdTemperature, status.heatingThresholdTemperature);
    if (status.targetHeatingCoolingState == STATE_AUTO){
        DEBUG_PRINTF(", target state is AUTO ");
        if (status.currentTemperature > status.coolingThresholdTemperature){
            DEBUG_PRINTF("so current state is now COOL\n");
            if (status.currentHeatingCoolingState != STATE_COOL){
                status.currentHeatingCoolingState = STATE_COOL;
                status.dirty = true;
            }
        } else if (status.currentTemperature < status.heatingThresholdTemperature){
            DEBUG_PRINTF("so current state is now HEAT\n");
            if (status.currentHeatingCoolingState != STATE_HEAT){
                status.currentHeatingCoolingState = STATE_HEAT;
                status.dirty = true;
            }
        } else {
            DEBUG_PRINTF("so current state is now OFF\n");
            if(status.currentHeatingCoolingState != STATE_OFF){
                status.currentHeatingCoolingState = STATE_OFF;
                status.dirty = true;
            }
        }
    } else {
        DEBUG_PRINTF("\n");
    }
}

void initializeStatus(){
    samsungAC.off();
    samsungAC.setFan(kSamsungAcFanLow);
    samsungAC.setMode(kSamsungAcCool);
    samsungAC.setTemp(20);
    samsungAC.setSwing(false);

    status.targetHeatingCoolingState = STATE_OFF;
    status.currentHeatingCoolingState = STATE_OFF;
    status.targetTemperature = 20;
    status.coolingThresholdTemperature = 25;
    status.heatingThresholdTemperature = 20;
    status.fanState = FAN_LOW;

    updateStatus();
}

void initializeAPI(){
    server.addRewrite(new OneParamRewrite(PSTR("/api/targetHeatingCoolingState/{state}"), PSTR("/api/targetHeatingCoolingState?state={state}")));
    server.addRewrite(new OneParamRewrite(PSTR("/api/targetTemperature/{temperature}"), PSTR("/api/targetTemperature?temp={temperature}")));
    server.addRewrite(new OneParamRewrite(PSTR("/api/coolingThresholdTemperature/{temperature}"), PSTR("/api/coolingThresholdTemperature?temp={temperature}")));
    server.addRewrite(new OneParamRewrite(PSTR("/api/heatingThresholdTemperature/{temperature}"), PSTR("/api/heatingThresholdTemperature?temp={temperature}")));

    server.on(PSTR("/api/targetHeatingCoolingState"), HTTP_GET, [] (AsyncWebServerRequest *request) {
        debugPrintRequest(request);
        if (request->hasParam(STATE_PARAMETER)) {
            HeatingCoolingState newState = static_cast<HeatingCoolingState>(request->getParam(STATE_PARAMETER)->value().toInt());
            if (newState != status.targetHeatingCoolingState){
                status.targetHeatingCoolingState = newState;
                status.currentHeatingCoolingState = status.targetHeatingCoolingState;
                shouldUpdateStatus = status.targetHeatingCoolingState;
                status.dirty = true;
                if (request->hasParam(WEBUI_PARAMETER)){
                    shouldPushUpdateToHomebridge[TARGET_HEATING_COOLING_STATE_INDEX] = true;
                }
                sendEvent(PSTR("targetState"), status.targetHeatingCoolingState);
            }
            request->send_P(200, TEXT_PLAIN, SUCCESS);
        } else {
            request->send_P(400, TEXT_PLAIN, MALFORMED_REQUEST);
        }
    });

    server.on(PSTR("/api/targetTemperature"), HTTP_GET, [] (AsyncWebServerRequest *request) {
        debugPrintRequest(request);
        if (request->hasParam(TEMPERATURE_PARAMETER)) {
            int newTargetTemperature = request->getParam(TEMPERATURE_PARAMETER)->value().toInt();
            if (newTargetTemperature != status.targetTemperature){
                status.targetTemperature = newTargetTemperature;
                status.dirty = true;
                if (request->hasParam(WEBUI_PARAMETER)){
                    shouldPushUpdateToHomebridge[TARGET_TEMPERATURE_INDEX] = true;
                }
                sendEvent(PSTR("targetTemp"), status.targetTemperature);
            }
            request->send_P(200, TEXT_PLAIN, SUCCESS);
        } else {
            request->send_P(400, TEXT_PLAIN, MALFORMED_REQUEST);
        }
    });

    server.on(PSTR("/api/coolingThresholdTemperature"), HTTP_GET, [] (AsyncWebServerRequest *request) {
        debugPrintRequest(request);
        if (request->hasParam(TEMPERATURE_PARAMETER)) {
            int newCoolingThresholdTemperature = request->getParam(TEMPERATURE_PARAMETER)->value().toInt();
            if (newCoolingThresholdTemperature != status.coolingThresholdTemperature){
                status.coolingThresholdTemperature = newCoolingThresholdTemperature;
                shouldUpdateStatus = true;
                sendEvent(PSTR("coolingTemp"), status.coolingThresholdTemperature);
                if (request->hasParam(WEBUI_PARAMETER)){
                    shouldPushUpdateToHomebridge[COOLING_THRESHOLD_TEMPERATURE_INDEX] = true;
                }
            }
            request->send_P(200, TEXT_PLAIN, SUCCESS);
        } else {
            request->send_P(400, TEXT_PLAIN, MALFORMED_REQUEST);
        }
    });

    server.on(PSTR("/api/heatingThresholdTemperature"), HTTP_GET, [] (AsyncWebServerRequest *request) {
        debugPrintRequest(request);
        if (request->hasParam(TEMPERATURE_PARAMETER)) {
            int newHeatingThresholdTemperature = request->getParam(TEMPERATURE_PARAMETER)->value().toInt();
            if (newHeatingThresholdTemperature != status.heatingThresholdTemperature){
                status.heatingThresholdTemperature = newHeatingThresholdTemperature;
                shouldUpdateStatus = true;
                if (request->hasParam(WEBUI_PARAMETER)){
                    shouldPushUpdateToHomebridge[HEATING_THRESHOLD_TEMPERATURE_INDEX] = true;
                }
                sendEvent(PSTR("heatingTemp"), status.heatingThresholdTemperature);
            }
            request->send_P(200, TEXT_PLAIN, SUCCESS);
        } else {
            request->send_P(400, TEXT_PLAIN, MALFORMED_REQUEST);
        }
    });

    server.on(PSTR("/api/status"), HTTP_GET, [] (AsyncWebServerRequest *request) {
        debugPrintRequest(request);
        AsyncResponseStream *response = request->beginResponseStream(PSTR("application/json"));
        serializeJsonPretty(generateJsonFromStatus(status), *response);
        request->send(response);
    });
}

void notFound(AsyncWebServerRequest *request) {
    debugPrintRequest(request);
    request->send_P(404, TEXT_PLAIN, PSTR("Not found"));
}

String indexTemplater(const String& var) {
    if(var == "TARGET_TEMPERATURE")
        return String(status.targetTemperature);
    if(var == "CURRENT_TEMPERATURE")
        return String(status.currentTemperature);
    if(var == "CURRENT_HUMIDITY")
        return String(status.currentRelativeHumidity);
    if(var == "HEATING_THRESHOLD_TEMPERATURE")
        return String(status.heatingThresholdTemperature);
    if(var == "COOLING_THRESHOLD_TEMPERATURE")
        return String(status.coolingThresholdTemperature);
    if(var == "ACTIVE_MODE")
        return String((int) status.targetHeatingCoolingState);
    if(var == "ACTIVE_FAN")
        return String((int) status.fanState);

    return String("");
}

void initializeWebUI(){
    server.addHandler(&events);
    server.serveStatic(PSTR("/assets/"), SPIFFS, PSTR("/assets/"));
    server.on(PSTR("/"), [] (AsyncWebServerRequest *request){
        debugPrintRequest(request);
        request->send(SPIFFS, PSTR("/index.html"), String(), false, indexTemplater);
    });

    server.on(PSTR("/api/targetFanState"), HTTP_GET, [] (AsyncWebServerRequest *request) {
        debugPrintRequest(request);
        if (request->hasParam(STATE_PARAMETER)) {
            status.fanState = static_cast<FanState>(request->getParam(STATE_PARAMETER)->value().toInt());
            status.dirty = true;
            sendEvent(PSTR("fanState"), status.fanState);
            request->send_P(200, TEXT_PLAIN, SUCCESS);
        } else {
            request->send_P(400, TEXT_PLAIN, MALFORMED_REQUEST);
        }
    });
}

void setup() {
    espDHT.setup(D3, DHTesp::DHT11);
    Serial.begin(115200);
    samsungAC.begin();
    WiFi.mode(WIFI_STA);
    WiFi.config(deviceIP, gatewayIP, netMask, dnsIP);
    WiFi.begin(SSID, PASSWORD);
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
        Serial.printf(PSTR("WiFi Failed!\n"));
        return;
    }
    SPIFFS.begin();

    pinMode(16, OUTPUT);
    digitalWrite(16, HIGH);

    initializeStatus();
    initializeAPI();
    initializeWebUI();


    server.onNotFound(notFound);

    server.begin();

    Serial.printf(PSTR("READY!\n"));
}

void loop() {
    static long last_update = millis();
    if ((millis() - last_update > UPDATE_INTERVAL) || shouldUpdateStatus){
        last_update = millis();
        shouldUpdateStatus = false;
        updateStatus();
    }

    for (int i = 0; i < 4; i++){
        if (shouldPushUpdateToHomebridge[i]){
            shouldPushUpdateToHomebridge[i] = false;
            sendHomebridgeUpdate(HOMEBRIDGE_UPDATE_URLS[i], getStatusValueByIndex(status, static_cast<StatusIndex>(i)));
        }
    }

    if (status.dirty){
        status.dirty = false;
        samsungAC.setTemp(status.targetHeatingCoolingState == STATE_AUTO ? (status.currentHeatingCoolingState == STATE_HEAT ? status.coolingThresholdTemperature : status.heatingThresholdTemperature) : status.targetTemperature);
        bool targetPowerState = status.currentHeatingCoolingState != STATE_OFF;
        if (targetPowerState != samsungAC.getPower())
            samsungAC.setPower(targetPowerState);
        switch (status.currentHeatingCoolingState){
            case STATE_HEAT:
                samsungAC.setMode(kSamsungAcHeat); 
                break;
            case STATE_COOL:
                samsungAC.setMode(kSamsungAcCool);
                break;
            default:
                break;
        }
        samsungAC.setQuiet(status.fanState == FAN_QUIET);
        switch (status.fanState){
            case FAN_LOW:
                samsungAC.setFan(kSamsungAcFanLow);
                break;
            case FAN_MEDIUM:
                samsungAC.setFan(kSamsungAcFanMed);
                break;
            case FAN_HIGH:
                samsungAC.setFan(kSamsungAcFanHigh);
                break;
            case FAN_AUTO:
                samsungAC.setFan(kSamsungAcFanAuto);
                break;
            default:
                break;
        }
        DEBUG_PRINTF("%s\n", samsungAC.toString().c_str());
        samsungAC.send();
    }
}