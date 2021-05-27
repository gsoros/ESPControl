#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <Scheduler.h>                      // https://github.com/nrwiersma/ESP8266Scheduler
#include <WiFiManager.h>                    // https://github.com/tzapu/WiFiManager
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>                    // https://github.com/bblanchon/ArduinoJson

#define RESPONSE_BUF_SIZE 1024

const char *NAME = "Remote1";               // Our name
const char *MDNS_SERVICE = "ESPControl";    // Look for this service
const char *MDNS_PROTOCOL = "tcp";        
const char *HOST = "Controller1";           // Look for this host to control
const char *DEVICE = "Stepper1";            // Look for this device to control

const int POT_PIN = A0;
const int POT_MIN = 0;
const int POT_MAX = 1024;
const int MOVEMENT_MIN = 2;                 // minimum movement of the pot
const int MEASUREMENTS_PER_SEC = 10;        // maximum number of pot measurements per second
const int NUM_MEASUREMENTS = 64;            // number of analog measurements to average
const int MEASUREMENT_MAX = 1024 * NUM_MEASUREMENTS;
const int MEASUREMENT_DELAY = 1000 / MEASUREMENTS_PER_SEC;

int command_rate = 1000;
int command_min = -100;
int command_max = 100;
int measurement_total;
int measurement = (POT_MIN + POT_MAX) / 2;
int measurement_tmp = measurement;
int last_command = (command_min + command_max) / 2;
int command = last_command;
int command_diff = 0;
bool monitor_enable = false;
IPAddress host_ip;
int host_port;

WiFiManager wifiManager;

int httpRequest(char *url, char *response) {
    int http_code = 0;
    WiFiClient client;
    HTTPClient http;
    if (http.begin(client, url)) {
        Serial.printf("[HTTP] GET %s\n", url);
        http_code = http.GET();
        if (http_code <= 0) {
            Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(http_code).c_str());
        } else {
            Serial.printf("[HTTP] GET... code: %d\n", http_code);
            if (http_code == HTTP_CODE_OK || http_code == HTTP_CODE_MOVED_PERMANENTLY) {
                snprintf(response, RESPONSE_BUF_SIZE, http.getString().c_str());
                Serial.printf("[HTTP] Response: %s\n", response);
            }
        }
        http.end();
    } else {
        Serial.println("[HTTP] Unable to connect");
    }
    return http_code;
}

class MeasureTask : public Task {
    protected:
    void setup() {
        pinMode(POT_PIN, INPUT);
    }
    void loop() {
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

        // Find host to control
        MDNS.begin(NAME);
        bool found = false;
        int tries = 0;
        while (!found) {
            tries++;
            Serial.printf("Sending mDNS query (try %i)\n", tries);
            int n = MDNS.queryService(MDNS_SERVICE, MDNS_PROTOCOL);
            if (n == 0) {
                Serial.println("no services found");
            } else {
                Serial.printf("%i service%s found\n", n, n > 1 ? "s" : "");
                for (int i = 0; i < n; ++i) {
                    Serial.print(i + 1);
                    Serial.print(": ");
                    char search[64];
                    snprintf(search, 64, "%s.local", HOST);
                    if (0 == strcmp(search, MDNS.answerHostname(i))) {
                        Serial.print("*****");
                        host_ip = MDNS.answerIP(i);
                        host_port = MDNS.answerPort(i);
                        found = true;
                    }
                    Serial.print(MDNS.answerHostname(i));
                    Serial.print(" (");
                    Serial.print(MDNS.answerIP(i));
                    Serial.print(":");
                    Serial.print(MDNS.answerPort(i));
                    Serial.println(")");
                }
            }
            Serial.println();
            if (1 < tries) delay(1000);
        }
        
        char url[100];
        sprintf(url, "http://%s:%i/api/config", host_ip.toString().c_str(), host_port);
        char response[RESPONSE_BUF_SIZE];
        int http_code = httpRequest(url, response);
        if (http_code == HTTP_CODE_OK) {
            Serial.print("[Config] code OK\n");
            StaticJsonDocument<512> conf;
            deserializeJson(conf, response);
            if (conf.containsKey("rate") && conf["rate"].is<int>()) {
                command_rate = conf["rate"];
                Serial.printf("Rate: %i\n", command_rate);
            }
            if (conf.containsKey("devices")) {
                if (NULL != conf["devices"]) {
                    for (uint i=0; i<conf["devices"].size(); i++) {
                        if (conf["devices"][i]["name"] == DEVICE) {
                            Serial.println("Device found");
                            if (conf["devices"][i]["command_min"].is<int>()) 
                                command_min = conf["devices"][i]["command_min"];
                            if (conf["devices"][i]["command_max"].is<int>()) 
                                command_max = conf["devices"][i]["command_max"];
                            Serial.printf("Device command min: %i, max: %i\n", command_min, command_max);
                            break;
                        }
                    }
                }
            }
        }
        monitor_enable = true;
    }
    void loop() {

        command = map(measurement, POT_MIN, POT_MAX, command_min, command_max);
        command_diff = abs(last_command - command);
        if (command_diff > MOVEMENT_MIN) {
            last_command = command;
            Serial.printf("Sending command: %d\n", command);
            char url[100];
            sprintf(url, "http://%s:%i/api/control?device=%s&command=%i", 
                host_ip.toString().c_str(), host_port, DEVICE, command);
            char response[RESPONSE_BUF_SIZE];
            httpRequest(url, response);
        }
        else if (command_diff > 0) {
            //Serial.printf("%d movement too small\n", command_diff);
        }
        delay(command_rate);
    }
} command_task;

class MonitorTask : public Task {
    protected:
    void loop() {
        while (!monitor_enable) {
            delay(100);
        }
        Serial.printf(
            "Server: %s, measurement: %d, command: %d\n",
            host_ip.toString().c_str(),
            measurement,
            command
        );
        delay(1000);
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

    wifiManager.autoConnect(NAME);

    Scheduler.start(&monitor_task);
    Scheduler.start(&measure_task);
    Scheduler.start(&command_task);
    Scheduler.begin();
}

void loop() {}
