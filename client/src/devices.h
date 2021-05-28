#ifndef DEVICES_H
#define DEVICES_H

#include <ESP8266HTTPClient.h>
#include <Scheduler.h>                  // https://github.com/nrwiersma/ESP8266Scheduler
#include <ArduinoJson.h>                // https://github.com/bblanchon/ArduinoJson
#include "request.h"

#define MAX_TASKS 8
#ifndef JSON_CONF_SIZE
#define JSON_CONF_SIZE 512
#endif

class Device {
    public:
    const char *name;
    const char *type;
    const char *host;
    bool hostAvailable = false;
    int hostRate = 1000;
    IPAddress hostIp;
    int hostPort;
    const char *hostDevice;
    int pin;
    Task *tasks[MAX_TASKS];
    int numTasks = 0;
    int commandFailCount = 0;
    int commandFailMax = 5;
   
    Device() {
        this->name = "";
        this->type = "";
        this->host = "";
        this->hostIp = IPAddress();
        this->hostPort = 0;
        this->hostDevice = "";
        this->pin = 0;
    }

    void addTask(Task *task) {
        this->tasks[this->numTasks] = task;
        this->numTasks++;
    }

    void startTasks() {
        for (int i=0; i<this->numTasks; i++) {
            Serial.printf("Starting task %i\n", i);
            Scheduler.start(this->tasks[i]);
        }
    }

    virtual bool configFromJson(StaticJsonDocument<JSON_CONF_SIZE> conf) {
        if (conf.containsKey("rate") && conf["rate"].is<int>()) {
            this->hostRate = conf["rate"];
            Serial.printf("[Pot %s] Host rate: %i\n", 
                this->name, this->hostRate);
            return true;
        }
        return false;
    }
};

class Pot : public Device {
    public:
    int min = 0;
    int max = 1024;
    int movementMin = 2;                // minimum movement of the pot
    int measurementsPerSec = 10;        // maximum number of pot measurements per second
    int numMeasurements = 64;           // number of analog measurements to average
    int measurementMax;
    int measurementDelay;
    int commandMin = -100;
    int commandMax = 100;
    int measurement;
    int lastCommand;

    Pot(
        const char *name = "Pot",
        const int pin = 0,
        const char *host = "",
        const char *hostDevice = ""
        ) {
        this->name = name;
        this->pin = pin;
        this->host = host;
        this->hostDevice = hostDevice;
    }

    bool configFromJson(StaticJsonDocument<JSON_CONF_SIZE> conf) {
        bool ret = Device::configFromJson(conf);
        if (!ret) return false;
        if (conf.containsKey("devices")) {
            if (NULL != conf["devices"]) {
                for (uint i=0; i<conf["devices"].size(); i++) {
                    Serial.printf("Checking device %i to match %s\n", i, this->hostDevice);
                    if (conf["devices"][i]["name"] == this->hostDevice) {
                        Serial.printf("[Pot %s] Host device found\n", this->name);
                        if (conf["devices"][i]["command_min"].is<int>()) 
                            this->commandMin = conf["devices"][i]["command_min"];
                        if (conf["devices"][i]["command_max"].is<int>()) 
                            this->commandMax = conf["devices"][i]["command_max"];
                        Serial.printf("[Pot %s] Host device command min: %i, max: %i\n", 
                            this->name, this->commandMin, this->commandMax);
                        return ret;
                    }
                }
            }
        }
        return false;
    }
};

class PotMeasureTask : public Task {
    public:
    Pot *pot;

    PotMeasureTask() {}
    PotMeasureTask(Pot *pot) {
        this->pot = pot;
    }

    protected:
    void setup() {
        pinMode(this->pot->pin, INPUT);
        this->pot->measurementMax = this->pot->max * this->pot->numMeasurements;
        this->pot->measurementDelay = 1000 / this->pot->measurementsPerSec;
        this->pot->lastCommand = (this->pot->min + this->pot->max) / 2;
    }

    void loop() {
        int measurementTotal = 0;
        for (int x = 0; x < this->pot->numMeasurements; x++) {
            measurementTotal += analogRead(this->pot->pin);
        }
        if (measurementTotal > this->pot->measurementMax) {
            Serial.printf("[POT %s] measurement overflow", this->pot->name);
            this->pot->measurement = this->pot->max;
        }
        else {
            int measurementTmp = measurementTotal / this->pot->numMeasurements;
            if (measurementTmp < this->pot->min) {
                this->pot->measurement = this->pot->min;
            }
            else if (measurementTmp > this->pot->max) {
                this->pot->measurement = this->pot->max;
            }
            else {
                this->pot->measurement = measurementTmp;
            }
        }
        measurementTotal = 0;
        delay(this->pot->measurementDelay);
    }
};

class PotCommandTask : public Task, public Request {
    public:
    Pot *pot;

    PotCommandTask() {}
    PotCommandTask(Pot *pot) {
        this->pot = pot;
    }

    protected:
    void setup() {
    }

    void loop() {
        if (!this->pot->hostAvailable) {
            delay(this->pot->hostRate);
            return;
        }
        int command = map(
            this->pot->measurement, 
            this->pot->min, 
            this->pot->max, 
            this->pot->commandMin, 
            this->pot->commandMax
        );
        int commandDiff = abs(this->pot->lastCommand - command);
        if (commandDiff > this->pot->movementMin) {
            this->pot->lastCommand = command;
            Serial.printf("Sending command: %d\n", command);
            char url[100];
            sprintf(url, "http://%s:%i/api/control?device=%s&command=%i", 
                this->pot->hostIp.toString().c_str(), 
                this->pot->hostPort, 
                this->pot->hostDevice, 
                command
            );
            char response[this->responseBufSize];
            int statusCode = this->requestGet(url, response);
            if (statusCode == HTTP_CODE_OK) {
                this->pot->commandFailCount = 0;
            } else {
                this->pot->commandFailCount++;
                Serial.printf("[POT %s] Command reply HTTP code %i, streak %i\n", 
                    this->pot->name, 
                    statusCode,
                    this->pot->commandFailCount
                );
                if (this->pot->commandFailCount >= this->pot->commandFailMax) {
                    this->pot->hostAvailable = false;
                    this->pot->commandFailCount = 0;
                }
            }
        }
        else if (commandDiff > 0) {
            //Serial.printf("%d movement too small\n", command_diff);
        }
        delay(this->pot->hostRate);
    }
};

class Switch : public Device { /* TODO */
    public:
    bool invert;

    Switch (
        const char *name = "Switch", 
        int pin = 0,         
        const char *host = "",
        const char *hostDevice = "", 
        bool invert = false) {
        this->name = name;
        this->type = "switch";
        this->pin = pin;
        this->host = host;
        this->hostDevice = hostDevice;
        this->invert = invert;
    }
};

#endif
