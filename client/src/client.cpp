#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <Scheduler.h>                  // https://github.com/nrwiersma/ESP8266Scheduler
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>                // https://github.com/bblanchon/ArduinoJson

const char *AP_SSID = "StepperControl";
const char *AP_PASSWORD = NULL;
const char *HOSTNAME = "steppercontrol.local";

const int POT_PIN = A0;
const int POT_MIN = 0;
const int POT_MAX = 1024;
const int MOVEMENT_MIN = 2;             // minimum movement of the pot
const int MEASUREMENTS_PER_SEC = 10;    // maximum number of pot measurements per second
const int NUM_MEASUREMENTS = 64;        // number of analog measurements to average
const int MEASUREMENT_MAX = 1024 * NUM_MEASUREMENTS;
const int MEASUREMENT_DELAY = 1000 / MEASUREMENTS_PER_SEC;

const char *wl_status[] = {
    /*0*/ "idle",
    /*1*/ "no SSID available",
    /*2*/ "scan completed",
    /*3*/ "connected",
    /*4*/ "connection failed",
    /*5*/ "connection lost",
    /*6*/ "disconnected"
};

int command_rate = 500;
int command_min = -100;
int command_max = 100;
int measurement_total;
int measurement = (POT_MIN + POT_MAX) / 2;
int measurement_tmp = measurement;
int last_command = (command_min + command_max) / 2;
int command = last_command;
int command_diff = 0;
bool monitor_enable = false;
int responseBufferLength = 255;
IPAddress host_ip;

void waitForWifi(int pause = 100) {
    while (WiFi.status() != WL_CONNECTED) {
        delay(pause);
    }
}

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
                snprintf(response, responseBufferLength, http.getString().c_str());
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
        Serial.flush();
        WiFi.mode(WIFI_STA);
        WiFi.setAutoConnect(true);
        WiFi.setAutoReconnect(true);
        WiFi.begin(AP_SSID, AP_PASSWORD);
        waitForWifi();
        int err = 0;
        while (err != 1) {
            err = WiFi.hostByName(HOSTNAME, host_ip);
            if(err != 1) {
                Serial.printf("Error getting server IP address, code: %i\n", err);
                delay(500);
            }
        }
        Serial.printf(
            "WiFi connected, our IP: %s, server IP: %s\n",
            WiFi.localIP().toString().c_str(),
            host_ip.toString().c_str()
        );
        char url[100];
        sprintf(url, "http://%s/config", HOSTNAME);
        char response[responseBufferLength];
        int http_code = httpRequest(url, response);
        if (http_code == HTTP_CODE_OK) {
            Serial.print("[Config] code OK\n");
            StaticJsonDocument<200> conf;
            deserializeJson(conf, response);
            const char *confName = conf["name"];
            Serial.printf("[Config] host is %s\n", confName);
            Serial.printf("[Config] old values: min %i, max %i, rate %i\n", command_min, command_max, command_rate);
            command_min = conf["min"];
            command_max = conf["max"];
            command_rate = conf["rate"];
            Serial.printf("[Config] new values: min %i, max %i, rate %i\n", command_min, command_max, command_rate);
        }
        monitor_enable = true;
    }
    void loop() {
        waitForWifi();
        command = map(measurement, POT_MIN, POT_MAX, command_min, command_max);
        command_diff = abs(last_command - command);
        if (command_diff > MOVEMENT_MIN) {
            last_command = command;
            Serial.printf("Sending command: %d\n", command);
            char url[100];
            sprintf(url, "http://%s/command?vector=%d", HOSTNAME, command);
            char response[responseBufferLength];
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
            "WiFi: %s  Server: %s, measurement: %d, command: %d\n",
            wl_status[WiFi.status()],
            host_ip.toString().c_str(),
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
