#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino_JSON.h>
#include <Scheduler.h>  // https://github.com/nrwiersma/ESP8266Scheduler
#include <ESP8266WebServer.h>
#include "devices.h"

#define MAX_DEVICES 32
#define JSON_MODE_PRIVATE 0
#define JSON_MODE_PUBLIC 1

class Config {
   public:
    const char *name;
    int rate;
    const char *mdnsService;
    const char *mdnsProtocol;
    int apiPort;
    Device *devices[MAX_DEVICES];
    int deviceCount = 0;
    ESP8266WebServer *server;

    Config(
        const char *name = "Controller",
        int rate = 10,
        const char *mdnsService = "http",
        const char *mdnsProtocol = "tcp",
        int apiPort = 50123) {
        this->name = name;
        this->rate = rate;
        this->mdnsService = mdnsService;
        this->mdnsProtocol = mdnsProtocol;
        this->apiPort = apiPort;
    }

    void setServer(ESP8266WebServer *server) {
        this->server = server;
        for (int i = 0; i < deviceCount; i++) {
            devices[i]->server = server;
        }
    }

    bool addDevice(Device *device) {
        if (hasDevice(device->name)) {
            Serial.printf("[Config] Device name \"%s\" already exists.", device->name);
            return false;
        }
        device->server = server;
        devices[deviceCount] = device;
        deviceCount++;
        return true;
    }

    bool hasDevice(const char *name) {
        for (int i = 0; i < deviceCount; i++) {
            if (name == devices[i]->name) {
                return true;
            }
        }
        return false;
    }

    Device *device(int i) {
        if (i > deviceCount) {
            Serial.printf("[Config] Device %i not found.\n", i);
            return nullptr;
        }
        return devices[i];
    }

    Device *device(const char *name) {
        for (int i = 0; i < deviceCount; i++) {
            // Serial.printf("Checking \"%s\"...\n", devices[i]->name);
            if (0 == strcmp(name, devices[i]->name)) {
                return devices[i];
            }
        }
        Serial.printf("[Config] Device \"%s\" not found.\n", name);
        return nullptr;
    }

    void handleApiControl() {
        if (nullptr == server) {
            Serial.println("[Config] handleApiControl: error: server is null");
            return;
        }
        const char *deviceName = server->arg("device").c_str();
        // Serial.printf("[Config] Received control request for %s\n", deviceName);
        Device *device = this->device(deviceName);
        if (nullptr == device) {
            Serial.printf("[Config] Control request received for non-existent device \"%s\"\n", deviceName);
            server->send(500, "text/plain", "Device does not exist");
            return;
        }
        device->handleApiControl();
    }

    void startControlTasks() {
        for (int i = 0; i < deviceCount; i++) {
            Serial.printf("[Config] Starting control task for device %i\n", i);
            Scheduler.start(devices[i]);
        }
    }

    JSONVar toJSONVar(int mode = JSON_MODE_PRIVATE) {
        JSONVar j;
        j["name"] = name;
        j["rate"] = rate;
        if (JSON_MODE_PRIVATE == mode) {
            j["mdnsService"] = mdnsService;
            j["apiPort"] = apiPort;
        }
        JSONVar devices;
        for (int i = 0; i < deviceCount; i++) {
            JSONVar device = this->devices[i]->toJSONVar(mode);
            devices[i] = device;
        }
        j["devices"] = devices;
        return j;
    }

    String toJsonString(int mode = JSON_MODE_PRIVATE) {
        return JSON.stringify(toJSONVar(mode));
    }
};

#endif
