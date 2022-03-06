#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino_JSON.h>
#include <Scheduler.h>                  // https://github.com/nrwiersma/ESP8266Scheduler
#include <ESP8266WebServer.h>
#include "devices.h"

#define MAX_DEVICES 32
#define CONFIG_MODE_PRIVATE 0
#define CONFIG_MODE_PUBLIC 1

class Config {
    public:
    const char *name;
    int rate;
    const char *mdnsService;
    const char *mdnsProtocol;
    int apiPort;
    Device *devices[MAX_DEVICES];
    int deviceCount;
    ESP8266WebServer *server;

    Config(
        const char *name = "Controller",
        int rate = 10,
        const char *mdnsService = "http",
        const char *mdnsProtocol = "tcp",
        int apiPort = 50123
        ) {
        this->deviceCount = 0;
        this->name = name;
        this->rate = rate;
        this->mdnsService = mdnsService;
        this->mdnsProtocol = mdnsProtocol;
        this->apiPort = apiPort;
        this->server = nullptr;
    }

    void setServer(ESP8266WebServer *server) {
        this->server = server;
        for (int i = 0; i < this->deviceCount; i++) {
                this->devices[i]->server = server;
        }
    }

    bool addDevice(Device *device) {
        if (this->hasDevice(device->name)) {
            Serial.printf("[Error] Device name \"%s\" already exists.", device->name);
            return false;
        }
        device->server = this->server;
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
            return nullptr;
        }
        return this->devices[i];
    }

    Device *device(const char *name) {
        for (int i = 0; i < this->deviceCount; i++) {
            //Serial.printf("Checking \"%s\"...\n", this->devices[i]->name);
            if (0 == strcmp(name, this->devices[i]->name)) {
                return this->devices[i];
            }
        }
        Serial.printf("[Error] Device \"%s\" not found.\n", name);
        return nullptr;
    }

    //void handleApiControl(ESP8266WebServer *server) {
    void handleApiControl() {
        const char *deviceName = const_cast<char *>(this->server->arg("device").c_str());
        //Serial.printf("Received control request for \"%s\"\n", deviceName);
        Device *device = this->device(deviceName);
        if (nullptr == device) {
            Serial.printf("[Error] control request received for non-existent device \"%s\"\n", deviceName);
            this->server->send(500, "text/plain", "Device does not exist");
            return;
        }
        device->handleApiControl();
    }

    void startControlTasks() {
        for (int i = 0; i < this->deviceCount; i++) {
            Serial.printf("Starting control task for device %i\n", i);
            Scheduler.start(this->devices[i]);
        }
    }

    JSONVar toJSONVar(int mode = CONFIG_MODE_PRIVATE) {
        JSONVar j;
        j["name"] = this->name;
        j["rate"] = this->rate;
        if (CONFIG_MODE_PRIVATE == mode) {
            j["mdnsService"] = this->mdnsService;
            j["apiPort"] = this->apiPort;
        }
        JSONVar devices;
        for (int i = 0; i < this->deviceCount; i++) {
            JSONVar device = this->devices[i]->toJSONVar(mode);
            devices[i] = device;
        }
        j["devices"] = devices;
        return j;
    }

    String toJsonString(int mode = CONFIG_MODE_PRIVATE) {
        return JSON.stringify(this->toJSONVar(mode));
    }
};

#endif
