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

#define NAME "ESPRemote001"
#define CONTROLLER_NAME "ESPController001"
#define AP_SSID CONTROLLER_NAME
#define AP_PASSWORD "Yoh9Ge2goucoo2la"
#define MDNS_SERVICE "ESPControl"

Config config(NAME, AP_SSID, AP_PASSWORD, MDNS_SERVICE);

Pot speedPot("Speed", A0, CONTROLLER_NAME, "Stepper1");

Adafruit_SSD1306 display(128, 32, &Wire, -1);
OledWithPotAndWifi oled(&display, &speedPot);

Switch directionSwitch("Direction", D6);
void IRAM_ATTR directionSwitchChanged() {
    directionSwitch.read();
    // Serial.printf("directionSwitch: %i\n", directionSwitch.getValue());
}

SwitchBlinker enableSwitch("Enable", D5);
void IRAM_ATTR enableSwitchChanged() {
    enableSwitch.read();
    // Serial.printf("enableSwitch: %i\n", enableSwitch.getValue());
}

PotWithDirectionAndEnableCommandTask commandTask(&speedPot, &enableSwitch, &directionSwitch);

// WiFiManager wifiManager;

WiFiEventHandler connectedHandler;
WiFiEventHandler disconnectedHandler;
WiFiEventHandler softAPStationConnectedHandler;
WiFiEventHandler softAPStationDisconnectedHandler;

void onConnected(const WiFiEventStationModeConnected& evt) {
    Serial.println("WiFi connected");
    oled.wifiConnected = 1;
    commandTask.lastCommandSent = 0;
}
void onDisconnected(const WiFiEventStationModeDisconnected& evt) {
    Serial.println("WiFi disconnected");
    oled.wifiConnected = 0;
}
void onStationConnected(const WiFiEventSoftAPModeStationConnected& evt) {
    Serial.println("Station connected");
    oled.wifiConnected++;
    commandTask.lastCommandSent = 0;
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
    speedPot.movementMin = 5;
    config.addDevice(&speedPot);

    commandTask.keepAliveSeconds = 1800;  // 30min, set watchdog timeout higher on server

    enableSwitch.setOled(&oled);
    enableSwitch.read();  // trigger blinking if disabled at boot

    config.setOled(&oled);

    Scheduler.start(&oled);
    Scheduler.start(&config);
    Scheduler.start(&speedPot);
    Scheduler.start(&commandTask);
    Scheduler.begin();
}

void loop() {}
