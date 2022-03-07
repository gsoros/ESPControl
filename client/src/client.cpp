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

#define AP_PASSWORD "Yoh9Ge2goucoo2la"

Config config("Remote1", "Controller1", AP_PASSWORD, "ESPControl");

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

PotWithDirectionAndEnableCommandTask commandTask(&speedPot, &enableSwitch, &directionSwitch);

Adafruit_SSD1306 display(128, 32, &Wire, -1);
OledWithPotAndWifi oled(&display, &speedPot);

// WiFiManager wifiManager;

WiFiEventHandler connectedHandler;
WiFiEventHandler disconnectedHandler;
WiFiEventHandler softAPStationConnectedHandler;
WiFiEventHandler softAPStationDisconnectedHandler;

void onConnected(const WiFiEventStationModeConnected& evt) {
    Serial.println("WiFi connected");
    oled.wifiConnected = 1;
}
void onDisconnected(const WiFiEventStationModeDisconnected& evt) {
    Serial.println("WiFi disconnected");
    oled.wifiConnected = 0;
}
void onStationConnected(const WiFiEventSoftAPModeStationConnected& evt) {
    Serial.println("Station connected");
    oled.wifiConnected++;
}
void onStationDisconnected(const WiFiEventSoftAPModeStationDisconnected& evt) {
    Serial.println("Station disconnected");
    oled.wifiConnected--;
}

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    Serial.begin(115200);

    Wire.begin(D2, D1);  // oled uses I2C

    connectedHandler = WiFi.onStationModeConnected(&onConnected);
    disconnectedHandler = WiFi.onStationModeDisconnected(&onDisconnected);
    softAPStationConnectedHandler = WiFi.onSoftAPModeStationConnected(&onStationConnected);
    softAPStationDisconnectedHandler = WiFi.onSoftAPModeStationDisconnected(&onStationDisconnected);

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
