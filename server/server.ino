#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <Scheduler.h>                  // https://github.com/nrwiersma/ESP8266Scheduler
#include <WiFiManager.h>                // https://github.com/tzapu/WiFiManager
#include <ESP8266mDNS.h> 
#include <Arduino_JSON.h>
#include "html.h"
#include "config.h"

WiFiManager wifiManager;
ESP8266WebServer *server;
Stepper stepper1;
Led led1;
Config config;
char *configJson;

void handleRoot() {
    server->send(200, "text/html", html);
    Serial.println("root");
}

void handleApiControl() {
    config.handleApiControl();
}

void handleApiConfig() {
    server->send(200, "application/json", configJson);
    Serial.println("config");
}

class ServerTask : public Task {
    protected:
    void setup() {
        configJson = const_cast<char*>(config.toJsonString().c_str());
        ESP8266WebServer webServer(config.apiPort);
        server = &webServer;
        config.setServer(server);
        server->on("/", handleRoot);
        server->on("/api/control", handleApiControl);
        server->on("/api/config", handleApiConfig);
        //server->begin();
        if (MDNS.begin(config.name, WiFi.localIP(), 1)) { // TTL is ignored
            Serial.println("mDNS responder started");
        } else {
            Serial.println("Error setting up MDNS responder");
        }
        MDNS.addService(config.mdnsService, "tcp", config.apiPort);
    }
    void loop()  {
        server->handleClient();
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
    config.rate = 100;                      // minimum number of milliseconds between commands sent by the client
    config.mdnsService = "steppercontrol";  // clients look for this service when discovering
    config.apiPort = 50123;                 // https://www.iana.org/assignments/service-names-port-numbers/service-names-port-numbers.txt

    stepper1.name = "Stepper1";
    stepper1.pin_enable = D1;
    stepper1.pin_direction = D2;
    stepper1.pin_step = D3;
    stepper1.min = 10;              // minimum delay between pulses: fastest speed
    stepper1.max = 1000;            // maximum delay between pulses: slowest speed
    stepper1.pulse = 10;            // pulse width
    stepper1.command_min = -511;
    stepper1.command_max = 512;
    
    led1.name = "Led1";
    led1.pin_enable = D4;

    
    config.addDevice(&stepper1);
    config.addDevice(&led1);

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    Serial.begin(115200);
    Serial.println("-------------------------------------------------");
    delay(1000);
    
    //wifiManager.autoConnect(config.name);
    
    Scheduler.start(&server_task);
    config.startControlTasks();
    Scheduler.start(&monitor_task);
    Scheduler.begin();
}

void loop() {}
