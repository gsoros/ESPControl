#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <Scheduler.h>                      // https://github.com/nrwiersma/ESP8266Scheduler
#include <WiFiManager.h>                    // https://github.com/tzapu/WiFiManager
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>                    // https://github.com/bblanchon/ArduinoJson
#include "config.h"
#include "devices.h"

#define NAME "Remote1"

Config config(NAME, "ESPControl");
Pot pot1("Pot1", A0, "Controller1", "Stepper1");
PotMeasureTask pot1MeasureTask(&pot1);
PotCommandTask pot1CommandTask(&pot1);

WiFiManager wifiManager;

void setup() {

    pot1.addTask(&pot1MeasureTask);
    pot1.addTask(&pot1CommandTask);
    config.addDevice(&pot1);
    
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    Serial.begin(115200);
    for (int x=0; x<5; x++) {
        Serial.println("-----------------------------------------");
    }
    delay(1000);

    wifiManager.autoConnect(NAME);

    Scheduler.start(&config);
    Scheduler.begin();
}

void loop() {}
