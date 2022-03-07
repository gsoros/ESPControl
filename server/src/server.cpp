#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <Scheduler.h>    // https://github.com/nrwiersma/ESP8266Scheduler
#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager
#include <ESP8266mDNS.h>
#include <Arduino_JSON.h>
#include "ui.html.h"
#include "config.h"

#define NAME "Controller1"
#define AP_PASSWORD "espControlServer001"
#define MDNS_SERVICE "ESPControl"
#define API_PORT 50123  // https://www.iana.org/assignments/service-names-port-numbers/service-names-port-numbers.txt
//#define API_PORT 80
#define JSON_LENGTH 1024
#define HTML_LENGTH 8192

Config config;
Stepper stepper1;
// Stepper stepper2;
// Led led1;

WiFiManager wifiManager;
ESP8266WebServer server(API_PORT);
char configJson[JSON_LENGTH];
char uiHtml[HTML_LENGTH];

void handleWebUI() {
    server.send(200, "text/html", uiHtml);
    Serial.println("[HTTP] WebUI");
}

void handleApiControl() {
    // Serial.println("server.handleApiControl()");
    config.handleApiControl();
}

void handleApiConfig() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", configJson);
    Serial.println("[HTTP] config");
}

void handleNotFound() {
    Serial.printf("[HTTP] not found: %s\n", server.uri().c_str());
    if (0 == strcmp("/favicon.ico", server.uri().c_str())) {
        server.send(404, "text/plain", "404 Not found");
        return;
    }
    server.sendHeader("Location", "/ui", true);
    server.send(302, "text/plain", "302 Moved");
}

class ServerTask : public Task {
   protected:
    void setup() {
        config.setServer(&server);
        strncpy(configJson, config.toJsonString(CONFIG_MODE_PUBLIC).c_str(), JSON_LENGTH);
        server.on("/ui", handleWebUI);
        server.on("/api/control", handleApiControl);
        server.on("/api/config", handleApiConfig);
        server.onNotFound(handleNotFound);
        server.begin();
        if (MDNS.begin(
                config.name,
                WiFi.getMode() == WIFI_AP ? WiFi.softAPIP() : WiFi.localIP(),
                1)) {  // TTL is ignored
            Serial.println("mDNS responder started");
        } else {
            Serial.println("Error setting up MDNS responder");
        }
        MDNS.addService(config.mdnsService, config.mdnsProtocol, config.apiPort);
    }
    void loop() {
        server.handleClient();
        MDNS.update();
    }
} serverTask;

class MonitorTask : public Task {
   protected:
    void loop() {
        IPAddress ip = WiFi.getMode() == WIFI_AP ? WiFi.softAPIP() : WiFi.localIP();
        Serial.printf(
            "IP: %s  enable: %d  direction: %d  speed: %d\n",
            ip.toString().c_str(),
            stepper1.enabled,
            stepper1.direction,
            stepper1.speed);
        delay(5000);
    }
} monitorTask;

void setup() {
    config.name = NAME;                 // server name
    config.rate = 200;                  // minimum number of milliseconds between commands sent by the client
    config.mdnsService = MDNS_SERVICE;  // clients look for this service when discovering
    config.apiPort = API_PORT;

    stepper1.name = "Stepper1";
    stepper1.pin_enable = D1;
    stepper1.pin_direction = D2;
    stepper1.pin_pulse = D3;
    stepper1.min = 1;    // (ms) minimum delay between pulses: fastest speed
    stepper1.max = 200;  // (ms) maximum delay between pulses: slowest speed
    stepper1.pulse = 0;  // (ms) pulse width

    // stepper2.name = "Stepper2";
    // stepper2.pin_enable = D5;
    // stepper2.pin_direction = D6;
    // stepper2.pin_pulse = D7;

    // led1.name = "Led1";
    // led1.pin_enable = D4;
    // led1.invert = true;

    config.addDevice(&stepper1);
    // config.addDevice(&led1);
    // config.addDevice(&stepper2);

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    Serial.begin(115200);
    Serial.println("-------------------------------------------------");
    delay(1000);

    // Use wifimanager...
    // WiFiManagerParameter custom_text("<br /><a href=\"/ui\">ESPControlServer UI</a>");
    // wifiManager.addParameter(&custom_text);
    // wifiManager.autoConnect(config.name, AP_PASSWORD);

    // ... OR create an AP ...
    Serial.print("[WiFi AP] Setting up acces point");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(config.name, AP_PASSWORD);
    Serial.print(" done, IP: ");
    Serial.println(WiFi.softAPIP().toString().c_str());

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
                  snprintf_P(
                      uiHtml,
                      HTML_LENGTH,
                      indexHtmlTemplate,
                      WIFI_STA == WiFi.getMode()               //
                          ? WiFi.localIP().toString().c_str()  //
                          : WiFi.softAPIP().toString().c_str(),
                      API_PORT));

    Scheduler.start(&serverTask);
    config.startControlTasks();
    Scheduler.start(&monitorTask);
    Scheduler.begin();
}

void loop() {}
