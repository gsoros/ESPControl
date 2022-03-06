#ifndef CONFIG_H
#define CONFIG_H

#include <ESP8266HTTPClient.h>
#include <Scheduler.h>  // https://github.com/nrwiersma/ESP8266Scheduler
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>  // https://github.com/bblanchon/ArduinoJson
#include "devices.h"
#include "request.h"

#define MAX_DEVICES 32
#define JSON_CONF_SIZE 512

class Config : public Task, public Request {
   public:
    const char *name;
    const char *mdnsService;
    const char *mdnsProtocol;
    Device *devices[MAX_DEVICES];
    int deviceCount;
    int discoveryLoopDelay = 3000;

    Config(
        const char *name = "Remote",
        const char *mdnsService = "http",
        const char *mdnsProtocol = "tcp") {
        this->deviceCount = 0;
        this->name = name;
        this->mdnsService = mdnsService;
        this->mdnsProtocol = mdnsProtocol;
    }

    bool addDevice(Device *device) {
        if (this->hasDevice(device->name)) {
            Serial.printf("[Error] Device name \"%s\" already exists.", device->name);
            return false;
        }
        this->devices[this->deviceCount] = device;
        this->deviceCount++;
        return true;
    }

    bool hasDevice(const char *name) {
        for (int i = 0; i < this->deviceCount; i++) {
            if (name == this->devices[i]->name) {
                return true;
            }
        }
        return false;
    }

    Device *device(int i) {
        if (i > this->deviceCount) {
            Serial.printf("[Error] Device %i not found.\n", i);
            return NULL;
        }
        return this->devices[i];
    }

    Device *device(const char *name) {
        for (int i = 0; i < this->deviceCount; i++) {
            // Serial.printf("Checking \"%s\"...\n", this->devices[i]->name);
            if (0 == strcmp(name, this->devices[i]->name)) {
                return this->devices[i];
            }
        }
        Serial.printf("[Error] Device \"%s\" not found.\n", name);
        return NULL;
    }

   protected:
    void setup() {
        Serial.println("Config::setup");
    }

    void loop() {
        int hostsNotFound = 0;
        for (int i = 0; i < this->deviceCount; i++) {
            if (0 != strcmp(this->devices[i]->host, "") && !this->devices[i]->hostAvailable) {
                hostsNotFound++;
            }
        }
        if (0 == hostsNotFound) {
            // Serial.println("No discovery needed");
            delay(this->discoveryLoopDelay);
            return;
        }
        MDNS.begin(this->name);
        int hostsFound = 0;
        int tries = 0;
        while (hostsFound < hostsNotFound) {
            tries++;
            Serial.printf(
                "Sending mDNS query (try %i, not found %i, found %i)\n",
                tries,
                hostsNotFound,
                hostsFound);
            Serial.flush();
            int numServices = MDNS.queryService(this->mdnsService, this->mdnsProtocol);
            if (numServices == 0) {
                Serial.println("no services found");
            } else {
                Serial.printf("%i service%s found\n", numServices, numServices > 1 ? "s" : "");
                int deviceHostMap[this->deviceCount];
                for (int s = 0; s < numServices; ++s) {
                    Serial.printf("%i: ", s + 1);
                    for (int d = 0; d < this->deviceCount; d++) {
                        if (0 == strcmp(this->devices[d]->host, "")) continue;
                        char search[64];
                        snprintf(search, 64, "%s.local", this->devices[d]->host);
                        if (0 == strcmp(search, MDNS.answerHostname(s))) {
                            Serial.print("*");
                            this->devices[d]->hostIp = MDNS.answerIP(s);
                            this->devices[d]->hostPort = MDNS.answerPort(s);
                            deviceHostMap[d] = s;
                            hostsFound++;
                        }
                    }
                    Serial.printf("%s(%s:%i)\n",
                                  MDNS.answerHostname(s),
                                  MDNS.answerIP(s).toString().c_str(),
                                  MDNS.answerPort(s));
                    char url[100];
                    sprintf(url, "http://%s:%i/api/config",
                            MDNS.answerIP(s).toString().c_str(),
                            MDNS.answerPort(s));
                    char response[this->responseBufSize];
                    int http_code = this->requestGet(url, response);
                    if (http_code == HTTP_CODE_OK) {
                        Serial.print("[Config] code OK\n");
                        StaticJsonDocument<JSON_CONF_SIZE> conf;
                        deserializeJson(conf, response);
                        for (int d = 0; d < this->deviceCount; d++) {
                            if (deviceHostMap[d] == s) {
                                if (this->devices[d]->configFromJson(conf)) {
                                    this->devices[d]->hostAvailable = true;
                                }
                            }
                        }
                    }
                }
            }
            Serial.println();
            if (1 < tries) delay(this->discoveryLoopDelay);
        }
    }
};

#endif
