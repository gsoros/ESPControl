#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <Scheduler.h>                  // https://github.com/nrwiersma/ESP8266Scheduler
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

const char *AP_SSID = "StepperControl";
const char *AP_PASSWORD = NULL;
IPAddress host(192, 168, 4, 1);
const uint16_t PORT = 80;

const int POT_PIN = A0;
const int POT_MIN = 0;
const int POT_MAX = 1024;
const int MOVEMENT_MIN = 2;             // minimum movement of the pot percentage
const int MEASUREMENTS_PER_SEC = 10;    // maximum number of pot measurements per second
const int COMMANDS_PER_SEC = 10;        // maximum number of commands per second
const int COMMAND_MIN = -511;           // TODO get these params from the server
const int COMMAND_MAX = 512;
const int NUM_MEASUREMENTS = 64;        // number of analog measurements to average
const int MEASUREMENT_MAX = 1024 * NUM_MEASUREMENTS;
const int MEASUREMENT_DELAY = 1000 / MEASUREMENTS_PER_SEC;
const int COMMAND_DELAY = 1000 / COMMANDS_PER_SEC;

const char *wl_status[] = {
    /*0*/ "idle", 
    /*1*/ "no SSID available", 
    /*2*/ "scan completed", 
    /*3*/ "connected", 
    /*4*/ "connection failed", 
    /*5*/ "connection lost", 
    /*6*/ "disconnected"
};

int measurement_total;
int measurement = (POT_MIN + POT_MAX) / 2;
int measurement_tmp = measurement;
int last_command = (COMMAND_MIN + COMMAND_MAX) / 2;
int command = last_command;
int command_diff = 0;
bool monitor = false;

void waitForWifi(int pause = 100) {
    while (WiFi.status() != WL_CONNECTED) {
        delay(pause);
    }
}

class MeasureTask : public Task {
    protected:
    void setup() {
        pinMode(POT_PIN, INPUT);
    }
    void loop() {
        waitForWifi();
        for (int x = 0; x < NUM_MEASUREMENTS; x++) {
            measurement_total += analogRead(POT_PIN);
        }
        if (measurement_total > MEASUREMENT_MAX) {
            Serial.println("measurement overflow");
            measurement = POT_MAX;
        }
        else {
            measurement_tmp = measurement_total / NUM_MEASUREMENTS;
            if (measurement_tmp < POT_MIN) {
                measurement = POT_MIN;
            }
            else if (measurement_tmp > POT_MAX) {
                measurement = POT_MAX;
            }
            else {
                measurement = measurement_tmp;
            }
        }
        measurement_total = 0;
        delay(MEASUREMENT_DELAY);
    }
} measure_task;


class CommandTask : public Task {
    protected:
    void setup() {
        Serial.printf("Connecting to %s\n", AP_SSID);
        WiFi.mode(WIFI_STA);
        WiFi.setAutoConnect(true);
        WiFi.setAutoReconnect(true);
        WiFi.begin(AP_SSID, AP_PASSWORD);
        waitForWifi();
        host = WiFi.gatewayIP();
        Serial.printf(
            "WiFi connected, IP: %s  Gateway: %s\n", 
            WiFi.localIP().toString().c_str(), 
            host.toString().c_str()
        );
        monitor = true;
    }
    void loop() {
        waitForWifi();
        command = map(measurement, POT_MIN, POT_MAX, COMMAND_MIN, COMMAND_MAX);
        command_diff = abs(last_command - command);
        if (command_diff > MOVEMENT_MIN) {
            last_command = command;
            Serial.printf("Sending command: %d\n", command);
            WiFiClient client;
            HTTPClient http;
            char url[100];
            sprintf(url, "http://%s/command?vector=%d", host.toString().c_str(), command);
            if (http.begin(client, url)) {  // HTTP       
                Serial.printf("[HTTP] GET %s\n", url);
                int httpCode = http.GET();
                if (httpCode <= 0) {
                    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
                } else {
                    Serial.printf("[HTTP] GET... code: %d\n", httpCode);
                    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
                        Serial.println(http.getString());
                    }      
                }         
                http.end();
            } else {
                Serial.print("[HTTP] Unable to connect\n");
            }         
        } 
        else if (command_diff > 0) {
            //Serial.printf("%d movement too small\n", command_diff); 
        }
        delay(COMMAND_DELAY);
    }
} command_task;


class MonitorTask : public Task {
    protected:
    void loop() {
        while (!monitor) {
            delay(100); 
        }
        Serial.printf(
            "WiFi: %s  Host: %s  measurement: %d  command: %d\n", 
            wl_status[WiFi.status()], 
            host.toString().c_str(), 
            measurement, 
            command
        );
        delay(500);
    }
} monitor_task;


void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    Serial.begin(115200);
    for (int x=0; x<5; x++) {
        Serial.println("-----------------------------------------");
    }
    delay(1000);
    Scheduler.start(&monitor_task);
    Scheduler.start(&measure_task);
    Scheduler.start(&command_task);
    Scheduler.begin();
}

void loop() {}
