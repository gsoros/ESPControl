#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <Scheduler.h>                  // https://github.com/nrwiersma/ESP8266Scheduler
#include <WiFiManager.h>                // https://github.com/tzapu/WiFiManager
#include <ESP8266mDNS.h>
#include <Arduino_JSON.h>
#include "index.html.h"
#include "config.h"

//#define API_PORT 50123                  // https://www.iana.org/assignments/service-names-port-numbers/service-names-port-numbers.txt
#define API_PORT 80
#define JSON_LENGTH 1024
#define HTML_LENGTH 4096

Config config;
Stepper stepper1;
Stepper stepper2;
Led led1;

WiFiManager wifiManager;
ESP8266WebServer server(API_PORT);
char configJson[JSON_LENGTH];
char indexHtml[HTML_LENGTH];

void handleRoot() {
    server.send(200, "text/html", indexHtml);
    Serial.println("[HTTP] root");
}

void handleApiControl() {
    //Serial.println("server.handleApiControl()");
    config.handleApiControl();
}

void handleApiConfig() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", configJson);
    Serial.println("[HTTP] config");
}

void handleNotFound() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "302 Moved");
    Serial.printf("[HTTP] not found: %s\n", server.uri().c_str());
}

class ServerTask : public Task {
    protected:
    void setup() {
        config.setServer(&server);
        if (strlcpy(configJson, config.toJsonString(CONFIG_MODE_PUBLIC).c_str(), JSON_LENGTH) > JSON_LENGTH)
            Serial.println("[ERROR] Json buffer too small");
        server.on("/", handleRoot);
        server.on("/api/control", handleApiControl);
        server.on("/api/config", handleApiConfig);
        server.onNotFound(handleNotFound);
        server.begin();
        if (MDNS.begin(config.name, WiFi.localIP(), 1)) { // TTL is ignored
            Serial.println("mDNS responder started");
        } else {
            Serial.println("Error setting up MDNS responder");
        }
        MDNS.addService(config.mdnsService, config.mdnsProtocol, config.apiPort);
    }
    void loop()  {
        server.handleClient();
        MDNS.update();
    }
} server_task;


class MonitorTask : public Task {
    protected:
    void loop() {
        //Serial.printf("IP: %s  enable: %d  direction: %d  speed: %d\n",
        //    WiFi.localIP().toString().c_str(), stepper_enable, stepper_direction, stepper_speed);
        delay(5000);
    }
} monitor_task;


void setup() {
    config.name = "Controller1";
    config.rate = 500;                      // minimum number of milliseconds between commands sent by the client
    config.mdnsService = "ESPControl";      // clients look for this service when discovering
    config.apiPort = API_PORT;

    stepper1.name = "Stepper1";
    stepper1.pin_enable = D1;
    stepper1.pin_direction = D2;
    stepper1.pin_step = D3;
    stepper1.min = 10;              // minimum delay between pulses: fastest speed
    stepper1.max = 1000;            // maximum delay between pulses: slowest speed
    stepper1.pulse = 10;            // pulse width
    stepper1.command_min = -511;
    stepper1.command_max = 512;

    stepper2.name = "Stepper2";
    stepper2.pin_enable = D5;
    stepper2.pin_direction = D6;
    stepper2.pin_step = D7;
    
    led1.name = "Led1";
    led1.pin_enable = D4;
    led1.invert = true;

    config.addDevice(&stepper1);
    config.addDevice(&led1);
    config.addDevice(&stepper2);
    
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.begin(115200);
    Serial.println("-------------------------------------------------");
    delay(1000);

    // Use wifimanager...
    wifiManager.autoConnect(config.name);

    // ... OR create an AP ...
    // Serial.print("[WiFi AP] Setting up acces point");
    // WiFi.mode(WIFI_AP);
    // WiFi.softAP(config.name);
    // Serial.print(" done, IP: ");
    // Serial.println(WiFi.softAPIP().toString().c_str());

    // ... OR connect to an AP
    // Serial.print("[WiFi] Connecting");
    // WiFi.mode(WIFI_STA);
    // WiFi.begin("AP_SSID", "AP_PASSWORD");
    // while (WiFi.status() != WL_CONNECTED) {
    //     delay(100);
    //     Serial.print(".");
    // }
    // Serial.print(" connected, IP: ");
    // Serial.println(WiFi.localIP());

    Serial.printf("Processing index.html template: %i\n",
        snprintf(
            indexHtml, 
            HTML_LENGTH, 
            indexHtmlTemplate, 
            WiFi.localIP().toString().c_str(), 
            API_PORT
        )
    );

    Scheduler.start(&server_task);
    config.startControlTasks();
    Scheduler.start(&monitor_task);
    Scheduler.begin();
}

void loop() {}
