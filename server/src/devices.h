#ifndef DEVICES_H
#define DEVICES_H

#include <Arduino_JSON.h>
#include <ESPAsyncWebServer.h>
#include <Scheduler.h>  // https://github.com/nrwiersma/ESP8266Scheduler

#define JSON_MODE_PRIVATE 0
#define JSON_MODE_PUBLIC 1

class Device : public Task {
   public:
    const char *name = "";
    const char *type = "";
    bool enabled = false;
    AsyncWebServer *server;

    virtual void handleApiControl(AsyncWebServerRequest *request, AsyncWebServerResponse *response = nullptr) {
        // Serial.printf("[Device %s] handleApiControl()\n", name);
        if (nullptr != response)
            response->addHeader("Access-Control-Allow-Origin", "*");
    };

    virtual JSONVar toJSONVar(int mode = JSON_MODE_PRIVATE) {
        JSONVar j;
        j["name"] = name;
        j["type"] = type;
        return j;
    }
};

class Stepper : public Device {
   public:
    int pinEnable;                      // enable pin
    int pinDirection;                   // direction pin
    int pinPulse;                       // pulse pin
    unsigned long pulseMin;             // minimum pause between pulses in microsecs (fastest speed)
    unsigned long pulseMax;             // maximum pause between pulses in microsecs (slowest speed)
    unsigned int pulseWidth;            // pulse width in microsecs
    int commandMin;                     // command minimum, negative for left
    int commandMax;                     // command maximum
    int changeMax;                      // maximum step of speed change per cycle
    int command = 0;                    // command being executed
    int setPoint = 0;                   // command target
    unsigned long lastCommandTime = 0;  // time of last command received, can be used for a watchdog

    Stepper(
        const char *name = "Stepper",
        int pinEnable = 0,
        int pinDirection = 0,
        int pinPulse = 0,
        unsigned long pulseMin = 2000,
        unsigned long pulseMax = 2000000,
        unsigned int pulseWidth = 1,
        int commandMin = -511,
        int commandMax = 512,
        int changeMax = 10) {
        this->name = name;
        type = "stepper";
        this->pinEnable = pinEnable;
        this->pinDirection = pinDirection;
        this->pinPulse = pinPulse;
        this->pulseMin = pulseMin;
        this->pulseMax = pulseMax;
        this->pulseWidth = pulseWidth;
        this->commandMin = commandMin;
        this->commandMax = commandMax;
        this->changeMax = changeMax;
    }

    void handleApiControl(AsyncWebServerRequest *request, AsyncWebServerResponse *response = nullptr) {
        // Serial.printf("[Stepper %s] handleApiControl()\n", name);
        if (!request->hasArg("command")) {
            request->send(400, "text/plain", "missing command");
            return;
        }
        int command = request->arg("command").toInt();
        if (command < commandMin)
            command = commandMin;
        else if (command > commandMax)
            command = commandMax;
        this->setPoint = command;
        lastCommandTime = millis();
        char message[100];
        sprintf(message, "[%s] command enable: %d  direction: %d  speed: %d",
                name, command == 0 ? 0 : 1, command > 0 ? 1 : 0, abs(command));
        if (nullptr == response) {
            // Serial.printf("[Stepper %s] handleApiControl() creating response\n", name);
            response = request->beginResponse(200, "text/plain", message);
        }
        Device::handleApiControl(request, response);
        // Serial.printf("[Stepper %s] handleApiControl() success, sending response\n", name);
        request->send(response);
    }

    JSONVar toJSONVar(int mode = JSON_MODE_PRIVATE) {
        Serial.printf("[Stepper %s] toJSONVar\n", name);
        JSONVar j = Device::toJSONVar(mode);
        j["commandMin"] = commandMin;
        j["commandMax"] = commandMax;
        if (JSON_MODE_PRIVATE == mode) {
            j["pinEnable"] = pinEnable;
            j["pinDirection"] = pinDirection;
            j["pinPulse"] = pinPulse;
            j["pulseMin"] = (long)pulseMin;
            j["pulseMax"] = (long)pulseMax;
            j["pulseWidth"] = (int)pulseWidth;
        }
        return j;
    }

   protected:
    void setup() {
        Serial.printf("[Stepper %s] setup\n", name);
        pinMode(pinEnable, OUTPUT);
        pinMode(pinDirection, OUTPUT);
        pinMode(pinPulse, OUTPUT);
        digitalWrite(pinEnable, LOW);
        digitalWrite(pinDirection, HIGH);
        digitalWrite(pinPulse, LOW);
    }

    void loop() {
        easeCommandToSetPoint();
        int command = this->command;
        unsigned long pause = calculatePause();
        if (0 != this->command) {
            digitalWrite(pinEnable, HIGH);
            digitalWrite(pinDirection, 0 < command ? LOW : HIGH);
        }
        while (0 != this->command) {
            easeCommandToSetPoint();
            digitalWrite(pinPulse, HIGH);
            microDelay(pulseWidth);
            digitalWrite(pinPulse, LOW);
            pulseEndTime = micros64();
            if (command != this->command) {
                digitalWrite(pinDirection, 0 < command ? LOW : HIGH);
                command = this->command;
                pause = calculatePause();
            }
            microDelay(pause - (micros64() - pulseEndTime));
        }
        digitalWrite(pinEnable, LOW);
    }

    void easeCommandToSetPoint() {
        if (command == setPoint) return;
        if (command < setPoint) {
            command += changeMax;
            if (setPoint < command)  // overshoot
                command = setPoint;
        } else {  // setPoint < command
            command -= changeMax;
            if (command < setPoint)  // overshoot
                command = setPoint;
        }
    }

    unsigned long calculatePause() {
        if (0 == command) {
            return 0;
        }
        int min, max;
        if (command < 0) {
            min = commandMax < 0 ? abs(commandMax) : 0;
            max = abs(commandMin);
        } else {  // 0 < command
            min = 0 < commandMin ? commandMin : 0;
            max = commandMax;
        }
        unsigned long pause = map(
            abs(command),
            min,
            max,
            pulseMax,
            pulseMin);
        // Serial.printf(
        //    "[Stepper %s] calculatePause command: %d (%d ... %d) => pause: %ld (%ld ... %ld)\n",
        //    name, command, min, max, pause, pulseMin, pulseMax);
        return pause;
    }

    void microDelay(unsigned long us) {
        if (0 == us) return;
        unsigned long end = micros() + us;
        while (micros() < end)
            yield();
    }

   private:
    uint64_t pulseEndTime = 0;
};

class Led : public Device {
   public:
    int pin_enable;
    bool invert;

    Led(const char *name = "Led", int pin_enable = 0, bool invert = false) {
        this->name = name;
        type = "led";
        this->pin_enable = pin_enable;
        this->invert = invert;
    }

    void handleApiControl(AsyncWebServerRequest *request, AsyncWebServerResponse *response = nullptr) {
        Device::handleApiControl(request, response);
        bool enabled = false;
        if (
            ('t' == request->arg("enable").c_str()[0]) ||  // "true"
            (0 < request->arg("enable").toInt()))          // 1
            enabled = true;

        this->enabled = enabled;
        char message[100];
        sprintf(message, "[%s] command enable: %s", name,
                enabled ? "true" : "false");
        request->send(200, "text/plain", message);
        Serial.println(message);
    }

    JSONVar toJSONVar(int mode = JSON_MODE_PRIVATE) {
        JSONVar j = Device::toJSONVar(mode);
        if (JSON_MODE_PRIVATE == mode) {
            j["pin_enable"] = pin_enable;
        }
        return j;
    }

   protected:
    void setup() {
        pinMode(pin_enable, OUTPUT);
    }

    void loop() {
        digitalWrite(pin_enable, invert ? !enabled : enabled ? HIGH
                                                             : LOW);
    }
};

#endif
