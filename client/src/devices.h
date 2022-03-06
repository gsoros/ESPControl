#ifndef DEVICES_H
#define DEVICES_H

#include <ESP8266HTTPClient.h>
#include <Scheduler.h>    // https://github.com/nrwiersma/ESP8266Scheduler
#include <ArduinoJson.h>  // https://github.com/bblanchon/ArduinoJson
#include "request.h"

#define MAX_TASKS 8
#ifndef JSON_CONF_SIZE
#define JSON_CONF_SIZE 512
#endif

class Device : public Request {
   public:
    const char *name;
    const char *host;
    bool hostAvailable = false;
    int hostRate = 1000;
    IPAddress hostIp;
    int hostPort;
    const char *hostDevice;
    int pin;
    int lastCommand;
    int commandFailCount = 0;
    int commandFailMax = 5;
    int movementMin = 0;  // minimum difference between value and lastCommand to trigger sendCommand()

    Device() {
        this->name = "";
        this->host = "";
        this->hostIp = IPAddress();
        this->hostPort = 0;
        this->hostDevice = "";
        this->pin = 0;
        read();
    }

    bool sendCommand(int command) {
        if (!hostAvailable) return false;
        Serial.printf("[Device %s] Sending command: %d\n", name, command);
        char url[100];
        sprintf(url, "http://%s:%i/api/control?device=%s&command=%i",
                hostIp.toString().c_str(),
                hostPort,
                hostDevice,
                command);
        char response[this->responseBufSize];
        int statusCode = this->requestGet(url, response);
        if (statusCode == HTTP_CODE_OK) {
            lastCommand = command;
            commandFailCount = 0;
            return true;
        }
        commandFailCount++;
        Serial.printf("[%s] Command reply HTTP code %i, streak %i\n",
                      name,
                      statusCode,
                      commandFailCount);
        if (commandFailCount >= commandFailMax) {
            hostAvailable = false;
            commandFailCount = 0;
        }
        return false;
    }

    virtual int calculateCommand() {
        return getValue();
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

    virtual int read() {
        return getValue();
    };

    virtual int getValue() {
        return value;
    }

    virtual void setValue(int value) {
        this->value = value;
    }

   protected:
    int value;
};

class Pot : public Device, public Task {
   public:
    int min = 0;
    int max = 1024;
    int measurementsPerSec = 10;  // maximum number of pot measurements per second
    int numMeasurements = 64;     // number of analog measurements to average
    int measurementMax;
    int measurementDelay;
    int commandMin = -100;
    int commandMax = 100;

    Pot(
        const char *name = "Pot",
        const int pin = 0,
        const char *host = "",
        const char *hostDevice = "") {
        this->name = name;
        this->pin = pin;
        this->host = host;
        this->hostDevice = hostDevice;
        pinMode(pin, INPUT);
        measurementMax = max * numMeasurements;
        measurementDelay = 1000 / measurementsPerSec;
        read();
        lastCommand = getValue();
    }

    int calculateCommand() {
        int out = map(
            getValue(),
            min,
            max,
            commandMin,
            commandMax);
        Serial.printf(
            "[Pot %s] calculateCommand: %i (%i ... %i) => %i (%i ... %i)\n",
            name,
            getValue(),
            min,
            max,
            out,
            commandMin,
            commandMax);
        return out;
    }

    int read() {
        int total = 0;
        for (int x = 0; x < numMeasurements; x++) {
            total += analogRead(pin);
        }
        if (total > measurementMax) {
            Serial.printf("[POT %s] measurement overflow", name);
            setValue(max);
        } else {
            int measurement = total / numMeasurements;
            if (measurement < min) {
                setValue(min);
            } else if (measurement > max) {
                setValue(max);
            } else {
                setValue(measurement);
            }
        }
        total = 0;
        return getValue();
    }

    bool configFromJson(StaticJsonDocument<JSON_CONF_SIZE> conf) {
        bool ret = Device::configFromJson(conf);
        if (!ret) return false;
        if (conf.containsKey("devices")) {
            if (NULL != conf["devices"]) {
                for (unsigned int i = 0; i < conf["devices"].size(); i++) {
                    Serial.printf("Checking device %i to match %s\n", i, this->hostDevice);
                    if (conf["devices"][i]["name"] == this->hostDevice) {
                        Serial.printf("[Pot %s] Host device found\n", this->name);
                        if (conf["devices"][i]["command_min"].is<int>())
                            this->commandMin = conf["devices"][i]["command_min"];
                        if (conf["devices"][i]["command_max"].is<int>())
                            this->commandMax = conf["devices"][i]["command_max"];
                        validateMinMax();
                        Serial.printf("[Pot %s] Host device command min: %i, max: %i\n",
                                      this->name, this->commandMin, this->commandMax);
                        return ret;
                    }
                }
            }
        }
        return false;
    }

    void validateMinMax() {
        if (commandMax < commandMin) {
            Serial.printf("[Pot] validateMinMax Warning: max < min, swapping\n");
            int tmp = commandMax;
            commandMax = commandMin;
            commandMin = tmp;
        }
        if (0 < commandMin) {
            Serial.printf("[Pot] validateMinMax Warning: 0 < min, zeroing\n");
            commandMin = 0;
        }
        if (commandMax < 0) {
            Serial.printf("[Pot] validateMinMax Warning: max < 0, zeroing\n");
            commandMax = 0;
        }
    }

   protected:
    void setup() {
        // Serial.println("Setting up pot measurement task");
    }

    void loop() {
        read();
        delay(measurementDelay);
    }
};

class Switch : public Device {
   public:
    bool invert;

    Switch(
        const char *name = "Switch",
        int pin = 0,
        const char *host = "",
        const char *hostDevice = "",
        bool invert = false) {
        this->name = name;
        this->pin = pin;
        this->host = host;
        this->hostDevice = hostDevice;
        this->invert = invert,
        pinMode(pin, INPUT_PULLUP);
        lastCommand = read();
    }

    int read() {
        setValue(digitalRead(pin));
        return getValue();
    }

    virtual int getValue() {
        return valueVolatile;
    }

    virtual void setValue(int value) {
        this->valueVolatile = value;
    }

   protected:
    volatile int valueVolatile;
};

class DeviceCommandTask : public Task, public Request {
   public:
    Device *device;

    DeviceCommandTask(Device *device) {
        this->device = device;
    }

   protected:
    virtual void setup() {
    }

    virtual void loop() {
        if (!device->hostAvailable) {
            delay(device->hostRate);
            return;
        }
        int command = device->calculateCommand();
        int commandDiff = abs(device->lastCommand - command);
        if (commandDiff > device->movementMin) {
            device->sendCommand(command);
        } else if (commandDiff > 0) {
            Serial.printf("[%s] %d movement too small\n", device->name, commandDiff);
        }
        delay(device->hostRate);
    }
};

class PotWithDirectionAndEnableCommandTask : public DeviceCommandTask {
   public:
    Pot *pot;
    Switch *direction;
    Switch *enable;

    PotWithDirectionAndEnableCommandTask(Pot *pot, Switch *enable, Switch *direction) : DeviceCommandTask(pot) {
        this->pot = pot;
        this->enable = enable;
        this->direction = direction;
    }

    int calculateCommand() {
        int out;
        if (enable->getValue() != HIGH)
            out = 0;
        else {
            int tmpMin;
            int tmpMax;
            if (direction->getValue() == HIGH) {
                tmpMin = 0;
                tmpMax = pot->commandMin;
            } else {
                tmpMin = 0;
                tmpMax = pot->commandMax;
            }
            out = map(
                pot->getValue(),
                pot->min,
                pot->max,
                tmpMin,
                tmpMax);
        }
        Serial.printf(
            "[PotWithDirectionAndEnableCommandTask] calculateCommand: %s %s %i (%i ... %i) => %i (%i ... %i)\n",
            enable->getValue() == HIGH ? "Enable" : "Disable",
            direction->getValue() == HIGH ? "Left" : "Right",
            pot->getValue(),
            pot->min,
            pot->max,
            out,
            pot->commandMin,
            pot->commandMax);
        return out;
    }

   protected:
    virtual void loop() {
        if (!device->hostAvailable) {
            delay(device->hostRate);
            return;
        }
        int command = calculateCommand();
        int commandDiff = abs(device->lastCommand - command);
        if (commandDiff > device->movementMin) {
            device->sendCommand(command);
        } else if (commandDiff > 0) {
            Serial.printf("[%s] %d movement too small\n", device->name, commandDiff);
        }
        delay(device->hostRate);
    }
};

#endif
