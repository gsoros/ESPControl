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
#define JSON_LENGTH 512
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
    // Serial.println("[HTTP] handleApiControl()");
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
        strncpy(configJson, config.toJsonString(JSON_MODE_PUBLIC).c_str(), JSON_LENGTH);
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
        // Watchdog: stop stepper 15 seconds after the last command received
        unsigned long timeout = 15000;
        unsigned long t = millis();
        if (0 < stepper1.lastCommandTime &&
            timeout < t &&
            stepper1.lastCommandTime < t - timeout &&
            stepper1.setPoint != 0) {
            Serial.printf("[Watchdog] Remote timed out, stopping the stepper\n");
            stepper1.setPoint = 0;
        }

        // Log monitor messages to the serial console
        IPAddress ip = WiFi.getMode() == WIFI_AP ? WiFi.softAPIP() : WiFi.localIP();
        Serial.printf(
            "[Monitor] IP: %s  enable: %d  direction: %d  speed: %d\n",
            ip.toString().c_str(),
            stepper1.command == 0 ? 0 : 1,
            stepper1.command > 0 ? 1 : 0,
            abs(stepper1.command));
        delay(5000);
    }
} monitorTask;

void setup() {
    config.name = NAME;                 // server name
    config.rate = 200;                  // minimum number of milliseconds between commands sent by the client
    config.mdnsService = MDNS_SERVICE;  // clients look for this service when discovering
    config.apiPort = API_PORT;

    stepper1.name = "Stepper1";
    stepper1.pinEnable = D1;
    stepper1.pinDirection = D2;
    stepper1.pinPulse = D3;
    stepper1.pulseMin = 1500;   // minimum pause between pulses in microsecs (fastest speed)
    stepper1.pulseMax = 15000;  // maximum pause between pulses in microsecs (slowest speed)
    stepper1.pulseWidth = 1;    // pulse width in microsecs
    stepper1.changeMax = 1;     // maximum step of speed change per cycle
    stepper1.commandMin = -1024;
    stepper1.commandMax = 1024;

    config.addDevice(&stepper1);

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

    Serial.printf("[Setup] Processing index.html template: %i\n",
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
