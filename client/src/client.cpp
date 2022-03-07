#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <Scheduler.h>    // https://github.com/nrwiersma/ESP8266Scheduler
#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>  // https://github.com/bblanchon/ArduinoJson
#include "config.h"
#include "devices.h"

Config config("Remote1", "Controller1", "espControlServer001", "ESPControl");

Switch enableSwitch("Enable", D5);
void IRAM_ATTR enableSwitchChanged() {
    enableSwitch.read();
    Serial.printf("enableSwitch: %i\n", enableSwitch.getValue());
}

Switch directionSwitch("Direction", D6);
void IRAM_ATTR directionSwitchChanged() {
    directionSwitch.read();
    Serial.printf("directionSwitch: %i\n", directionSwitch.getValue());
}

Pot speedPot("Speed", A0, "Controller1", "Stepper1");

// DeviceCommandTask commandTask(&speedPot);
PotWithDirectionAndEnableCommandTask commandTask(&speedPot, &enableSwitch, &directionSwitch);

Adafruit_SSD1306 display(128, 32, &Wire, -1);
Oled oled(D2, D1, &display);

WiFiManager wifiManager;

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    Serial.begin(115200);
    for (int x = 0; x < 5; x++) {
        Serial.println("-----------------------------------------");
    }
    delay(1000);

    config.addDevice(&enableSwitch);
    attachInterrupt(digitalPinToInterrupt(enableSwitch.pin), enableSwitchChanged, CHANGE);

    config.addDevice(&directionSwitch);
    attachInterrupt(digitalPinToInterrupt(directionSwitch.pin), directionSwitchChanged, CHANGE);

    speedPot.invert = true;
    speedPot.min = 9;
    speedPot.max = 950;
    config.addDevice(&speedPot);

    Scheduler.start(&oled);
    Scheduler.start(&config);
    Scheduler.start(&speedPot);
    Scheduler.start(&commandTask);
    Scheduler.begin();
}

void loop() {}
