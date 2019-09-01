#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <DHTesp.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Samsung.h>

#include "utils.h"
#include "config.h"

AsyncWebServer server(SERVER_PORT);
IRSamsungAc samsungAC(D2, true);
DHTesp espDHT;
Status status;

void updateStatus(){
    status.currentTemperature = espDHT.getTemperature();
    status.currentRelativeHumidity = espDHT.getHumidity();
    digitalWrite(16, !(isnan(status.currentTemperature) || isnan(status.currentRelativeHumidity)));
    int tempDiff = status.currentTemperature - status.targetTemperature;
    #if DEBUG
    Serial.printf(PSTR("Temperature: %d, Humidity: %d, DeltaTemp: %d, coolingThresholdTemperature: %d, heatingThresholdTemperature: %d"), status.currentTemperature, status.currentRelativeHumidity, tempDiff, status.coolingThresholdTemperature, status.heatingThresholdTemperature);
    #endif
    if (status.targetHeatingCoolingState == AUTO){
        #if DEBUG
        Serial.printf(PSTR(", target state is AUTO "));
        #endif
        if (status.currentTemperature > status.coolingThresholdTemperature){
            #if DEBUG
            Serial.printf(PSTR("so current state is now COOL\n"));
            #endif
            status.currentHeatingCoolingState = COOL;
            status.dirty = true;
        } else if (status.currentTemperature < status.heatingThresholdTemperature){
            #if DEBUG
            Serial.printf(PSTR("so current state is now HEAT\n"));
            #endif
            status.currentHeatingCoolingState = HEAT;
            status.dirty = true;
        }
        #if DEBUG
        else{
            Serial.printf(PSTR("\n"));
        }
        #endif
    }
    #if DEBUG
    else{
        Serial.printf(PSTR("\n"));
    }
    #endif
}

void initializeStatus(){
    samsungAC.off();
    samsungAC.setFan(kSamsungAcFanLow);
    samsungAC.setMode(kSamsungAcCool);
    samsungAC.setTemp(20);
    samsungAC.setSwing(false);

    status.targetHeatingCoolingState = OFF;
    status.targetTemperature = 20;
    status.coolingThresholdTemperature = 30;
    status.heatingThresholdTemperature = 20;

    updateStatus();
}

void notFound(AsyncWebServerRequest *request) {
    request->send_P(404, TEXT_PLAIN, PSTR("Not found"));
}

void setup() {
    espDHT.setup(D3, DHTesp::DHT11);
    Serial.begin(115200);
    samsungAC.begin();
    pinMode(16, OUTPUT);
    WiFi.mode(WIFI_STA);
    WiFi.config(deviceIP, gatewayIP, netMask, dnsIP);
    WiFi.begin(SSID, PASSWORD);
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
        Serial.printf(PSTR("WiFi Failed!\n"));
        return;
    }

    initializeStatus();
    
    server.addRewrite(new OneParamRewrite(PSTR("/targetHeatingCoolingState/{state}"), PSTR("/targetHeatingCoolingState?state={state}")));
    server.addRewrite(new OneParamRewrite(PSTR("/targetTemperature/{temperature}"), PSTR("/targetTemperature?temp={temperature}")));
    server.addRewrite(new OneParamRewrite(PSTR("/coolingThresholdTemperature/{temperature}"), PSTR("/coolingThresholdTemperature?temp={temperature}")));
    server.addRewrite(new OneParamRewrite(PSTR("/heatingThresholdTemperature/{temperature}"), PSTR("/heatingThresholdTemperature?temp={temperature}")));

    server.on(PSTR("/targetHeatingCoolingState"), HTTP_GET, [] (AsyncWebServerRequest *request) {
        if (request->hasParam(STATE_PARAMETER)) {
            status.targetHeatingCoolingState = static_cast<HeatingCoolingState>(request->getParam(STATE_PARAMETER)->value().toInt());
            status.dirty = true;
            request->send_P(200, TEXT_PLAIN, SUCCESS);
        } else {
            request->send_P(400, TEXT_PLAIN, MALFORMED_REQUEST);
        }
    });

    server.on(PSTR("/targetTemperature"), HTTP_GET, [] (AsyncWebServerRequest *request) {
        if (request->hasParam(TEMPERATURE_PARAMETER)) {
            status.targetTemperature = request->getParam(TEMPERATURE_PARAMETER)->value().toInt();
            status.dirty = true;
            request->send_P(200, TEXT_PLAIN, SUCCESS);
        } else {
            request->send_P(400, TEXT_PLAIN, MALFORMED_REQUEST);
        }
    });

    server.on(PSTR("/coolingThresholdTemperature"), HTTP_GET, [] (AsyncWebServerRequest *request) {
        if (request->hasParam(TEMPERATURE_PARAMETER)) {
            status.coolingThresholdTemperature = request->getParam(TEMPERATURE_PARAMETER)->value().toInt();
            request->send_P(200, TEXT_PLAIN, SUCCESS);
        } else {
            request->send_P(400, TEXT_PLAIN, MALFORMED_REQUEST);
        }
    });

    server.on(PSTR("/heatingThresholdTemperature"), HTTP_GET, [] (AsyncWebServerRequest *request) {
        if (request->hasParam(TEMPERATURE_PARAMETER)) {
            status.heatingThresholdTemperature = request->getParam(TEMPERATURE_PARAMETER)->value().toInt();
            request->send_P(200, TEXT_PLAIN, SUCCESS);
        } else {
            request->send_P(400, TEXT_PLAIN, MALFORMED_REQUEST);
        }
    });

    // Send a GET request to <IP>/get?message=<message>
    server.on(PSTR("/status"), HTTP_GET, [] (AsyncWebServerRequest *request) {
        AsyncResponseStream *response = request->beginResponseStream(PSTR("application/json"));
        DynamicJsonDocument json(1024);
        json["targetHeatingCoolingState"] = (int) status.targetHeatingCoolingState;
        json["currentHeatingCoolingState"] = (int) status.currentHeatingCoolingState;
        json["currentTemperature"] = status.currentTemperature;
        json["targetTemperature"] = status.targetTemperature;
        json["currentRelativeHumidity"] = status.currentRelativeHumidity;
        json["coolingThresholdTemperature"] = status.coolingThresholdTemperature;
        json["heatingThresholdTemperature"] = status.heatingThresholdTemperature;
        serializeJsonPretty(json, *response);
        request->send(response);
    });

    server.onNotFound(notFound);

    server.begin();

    Serial.printf(PSTR("READY!\n"));
}

void loop() {
    static long last_update = millis();
    if (millis() - last_update > UPDATE_INTERVAL){
        last_update = millis();
        updateStatus();
    }
    if (status.dirty){
        status.dirty = false;
        switch (status.currentHeatingCoolingState){
            case OFF:
                if (samsungAC.getPower()){
                    samsungAC.off();
                    samsungAC.send();
                }
                break;
            case HEAT:
                if (!samsungAC.getPower()){
                    samsungAC.on();
                }
                samsungAC.setMode(kSamsungAcHeat);
                samsungAC.setTemp(status.targetTemperature);
                samsungAC.send();
                break;
            case COOL:
                if (!samsungAC.getPower()){
                    samsungAC.on();
                }
                samsungAC.setMode(kSamsungAcCool);
                samsungAC.setTemp(status.targetTemperature);
                samsungAC.send();
                break;
            case AUTO:
                if (!samsungAC.getPower()){
                    samsungAC.on();
                    samsungAC.send();
                }
                break;
        }
        #if DEBUG
            Serial.printf(samsungAC.toString().c_str());
            Serial.printf("\n");
        #endif
    }
}