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
    char *name;
    int rate;
    char *mdnsService;
    int apiPort;
    Device *devices[MAX_DEVICES];
    int deviceCount;
    ESP8266WebServer *server;

    Config(
        char *name = "Controller",
        int rate = 10,
        char *mdnsService = "http",
        int apiPort = 50123
    ) {
        this->deviceCount = 0;
        this->name = name;
        this->rate = rate;
        this->mdnsService = mdnsService;
        this->apiPort = apiPort;
        this->server = null;
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
            Serial.printf("[Error] Device %i not found.", i);
            return null;
        }
        return this->devices[i];
    }

    Device *device(const char *name) {
        for (int i = 0; i < this->deviceCount; i++) {
            if (name == this->devices[i]->name) {
                return this->devices[i];
            }
        }
        Serial.printf("[Error] Device \"%s\" not found.", name);
        return null;
    }

    //void handleApiControl(ESP8266WebServer *server) {
    void handleApiControl() {
        const char *deviceName = this->server->arg("device").c_str();
        //char *deviceName = "Stepper1";
        Device *device = this->device(deviceName);
        if (null == device) {
            Serial.printf("[Error] control request received for non-existent device \"%s\"", deviceName);
            return;
        }
        device->handleApiControl();
    }

    //void setup() {
    //    for (int i = 0; i < this->deviceCount; i++) {
    //        this->devices[i]->setup();
    //    }
    //}

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