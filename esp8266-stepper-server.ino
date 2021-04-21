#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <Scheduler.h>            // https://github.com/nrwiersma/ESP8266Scheduler
#include "html.h"

const char *AP_SSID = "StepperControl";
const char *AP_PASSWORD = NULL;
const int AP_CHANNEL = 1;
const bool AP_HIDDEN = false;
const int AP_MAX_CONNECTIONS = 1;

const int STEPPER_PIN_ENABLE = D1;
const int STEPPER_PIN_DIRECTION = D2;
const int STEPPER_PIN_STEP = D3;
const int STEPPER_MIN = 10;    // minimum delay between pulses: fastest speed
const int STEPPER_MAX = 1000;  // maximum delay between pulses: slowest speed
const int STEPPER_PULSE = 10;  // pulse width

const int SLIDER_MIN = -1000;
const int SLIDER_MAX = 1000;

bool stepper_enable = false;
int stepper_direction = 0;
int stepper_speed = 0;

ESP8266WebServer server(80);

void handleRoot() {
    server.send(200, "text/html", html);
    Serial.println("root");
}

void handleCommand() {
    int vector = server.arg("vector").toInt();
    stepper_direction = vector < 0 ? -1 : 1;
    stepper_speed = abs(vector);
    stepper_enable = stepper_speed > 0;
    char message[100];
    sprintf(message, "command enable: %d  direction: %d  speed: %d", stepper_enable, stepper_direction, stepper_speed);
    server.send(200, "text/plain", message);
    Serial.println(message);
}

class ServerTask : public Task {
    protected:
    void setup() {
        WiFi.mode(WIFI_AP);
        WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, AP_HIDDEN, AP_MAX_CONNECTIONS);
        Serial.print("IP Address: ");    
        Serial.println(WiFi.softAPIP());
        
        WiFi.onSoftAPModeStationConnected(&onConnected);
        WiFi.onSoftAPModeStationDisconnected(&onDisconnected);
        WiFi.onSoftAPModeProbeRequestReceived(&onProbe);
        
        server.on("/", handleRoot);
        server.on("/command", handleCommand);
        server.begin();
    }
    void loop()  {
        server.handleClient();
    }
} server_task;


class ControlTask : public Task {
    protected:
    void setup() {
        pinMode(STEPPER_PIN_ENABLE, OUTPUT);
        pinMode(STEPPER_PIN_DIRECTION, OUTPUT);
        pinMode(STEPPER_PIN_STEP, OUTPUT);
        digitalWrite(STEPPER_PIN_ENABLE, LOW);
        digitalWrite(STEPPER_PIN_DIRECTION, LOW);
        digitalWrite(STEPPER_PIN_STEP, LOW);
    }
    void loop() {
        while (stepper_enable && (0 < stepper_speed)) {
            digitalWrite(STEPPER_PIN_DIRECTION, (0 < stepper_direction) ? HIGH : LOW);
            digitalWrite(STEPPER_PIN_ENABLE, HIGH);
            digitalWrite(STEPPER_PIN_STEP, HIGH);
            delay(STEPPER_PULSE);
            digitalWrite(STEPPER_PIN_STEP, LOW);
            int pause = map(stepper_speed, SLIDER_MIN, SLIDER_MAX, STEPPER_MAX*2, STEPPER_MIN);
            delay(pause);
        }
        digitalWrite(STEPPER_PIN_ENABLE, LOW);
    }
} control_task;


class MonitorTask : public Task {
    protected:
    void loop() {
        Serial.printf("connected: %d  enable: %d  direction: %d  speed: %d\n", 
            WiFi.softAPgetStationNum(), stepper_enable, stepper_direction, stepper_speed);
        delay(1000);
    }
} monitor_task;


void onConnected(const WiFiEventSoftAPModeStationConnected& evt) {
    Serial.print("connected: ");
    Serial.println(macToString(evt.mac));
}

void onDisconnected(const WiFiEventSoftAPModeStationDisconnected& evt) {
    stepper_enable = false;
    Serial.print("disconnected: ");
    Serial.println(macToString(evt.mac));
}

void onProbe(const WiFiEventSoftAPModeProbeRequestReceived& evt) {
    Serial.print("probe from: ");
    Serial.print(macToString(evt.mac));
    Serial.print(" RSSI: ");
    Serial.println(evt.rssi);
}

String macToString(const unsigned char* mac) {
    char buf[20];
    snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    Serial.begin(115200);
    Serial.println("");
    delay(1000);
    Scheduler.start(&server_task);
    Scheduler.start(&control_task);
    Scheduler.start(&monitor_task);
    Scheduler.begin();
}

void loop() {}
