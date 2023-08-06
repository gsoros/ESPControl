#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESPAsyncWebServer.h>
#include <Scheduler.h>  // https://github.com/nrwiersma/ESP8266Scheduler
#include <ESP8266mDNS.h>
#include <Arduino_JSON.h>

#include "ui.html.h"
#include "config.h"
#include "credentials.h"

#define API_PORT 50123  // https://www.iana.org/assignments/service-names-port-numbers/service-names-port-numbers.txt
#define JSON_LENGTH 512
#define HTML_LENGTH 8192

Config config;
Stepper stepper1;

AsyncWebServer server(API_PORT);
char configJson[JSON_LENGTH];
char uiHtml[HTML_LENGTH];

String htmlProcessor(const String& var) {
    if (var == "FQDN_OR_IP")
        return WIFI_STA == WiFi.getMode()       //
                   ? WiFi.localIP().toString()  //
                   : WiFi.softAPIP().toString();
    if (var == "PORT")
        return String(API_PORT);
    return String();
}

void handleWebUI(AsyncWebServerRequest* request) {
    Serial.println("[HTTP] handleWebUI()");
    AsyncWebServerResponse* response = request->beginResponse_P(200, "text/html", indexHtmlTemplate, htmlProcessor);
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
}

void handleApiControl(AsyncWebServerRequest* request) {
    // Serial.println("[HTTP] handleApiControl()");
    config.handleApiControl(request);
}

void handleApiConfig(AsyncWebServerRequest* request) {
    // Serial.println("[HTTP] handleApiConfig()");
    AsyncWebServerResponse* response = request->beginResponse(200, "application/json", configJson);
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
}

void handleNotFound(AsyncWebServerRequest* request) {
    Serial.printf("[HTTP] not found: %s\n", request->url().c_str());
    if (0 == strcmp("/favicon.ico", request->url().c_str())) {
        request->send(404, "text/plain", "404 Not found");
        return;
    }
    AsyncWebServerResponse* response = request->beginResponse(302, "text/plain", "Moved");
    response->addHeader("Location", "/ui");
    request->send(response);
}

WiFiEventHandler connectedHandler;
WiFiEventHandler disconnectedHandler;
WiFiEventHandler softAPStationConnectedHandler;
WiFiEventHandler softAPStationDisconnectedHandler;

void onConnected(const WiFiEventStationModeConnected& evt) {
    Serial.println("WiFi connected");
}
void onDisconnected(const WiFiEventStationModeDisconnected& evt) {
    Serial.println("WiFi disconnected");
}
void onStationConnected(const WiFiEventSoftAPModeStationConnected& evt) {
    Serial.println("Station connected");
}
void onStationDisconnected(const WiFiEventSoftAPModeStationDisconnected& evt) {
    Serial.println("Station disconnected");
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
        MDNS.update();
    }
} serverTask;

class MonitorTask : public Task {
   public:
    const unsigned long wdTimeout = 3600000;  // 1h

    void loop() {
        // Watchdog: stop stepper [wdTimeout] milliseconds after the last command received
        unsigned int t = millis();
        if (0 < stepper1.lastCommandTime &&
            wdTimeout < t &&
            stepper1.lastCommandTime < t - wdTimeout &&
            stepper1.setPoint != 0) {
            Serial.printf("[Watchdog] Remote timed out, stopping the stepper\n");
            stepper1.setPoint = 0;
            stepper1.lastCommandTime = t;
        }

        // Log monitor messages to the serial console
        IPAddress ip = WiFi.getMode() == WIFI_AP ? WiFi.softAPIP() : WiFi.localIP();
        Serial.printf(
            "[Monitor] IP: %s  WD: %ld EN: %d  DI: %d  SP: %d(%d)\n",
            ip.toString().c_str(),
            0 == stepper1.setPoint ? 0 : (wdTimeout - (millis() - stepper1.lastCommandTime)) / 1000,
            stepper1.command == 0 ? 0 : 1,
            stepper1.command > 0 ? 1 : 0,
            abs(stepper1.command),
            abs(stepper1.setPoint));

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
    stepper1.pulseMin = 200;    // minimum pause between pulses in microsecs (fastest speed)
    stepper1.pulseMax = 15000;  // maximum pause between pulses in microsecs (slowest speed)
    stepper1.pulseWidth = 1;    // pulse width in microsecs
    stepper1.changeMax = 100;   // maximum step of speed change per cycle
    stepper1.commandMin = -1024;
    stepper1.commandMax = 1024;

    config.addDevice(&stepper1);

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    Serial.begin(115200);

    connectedHandler = WiFi.onStationModeConnected(&onConnected);
    disconnectedHandler = WiFi.onStationModeDisconnected(&onDisconnected);
    softAPStationConnectedHandler = WiFi.onSoftAPModeStationConnected(&onStationConnected);
    softAPStationDisconnectedHandler = WiFi.onSoftAPModeStationDisconnected(&onStationDisconnected);

    // ... create an AP ...
    Serial.print("[WiFi AP] Setting up acces point");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
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

    Scheduler.start(&serverTask);
    config.startControlTasks();
    Scheduler.start(&monitorTask);
    Scheduler.begin();
}

void loop() {}
