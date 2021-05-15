#ifndef DEVICES_H
#define DEVICES_H

#include <Arduino_JSON.h>
#include <ESP8266WebServer.h>
#include <Scheduler.h>                  // https://github.com/nrwiersma/ESP8266Scheduler

#define DEVICES_MODE_PRIVATE 0
#define DEVICES_MODE_PUBLIC 1

class Device : public Task {
    public: 
    char *name;
    char *type;
    bool enabled;
    ESP8266WebServer *server;

    Device() {
        this->name = "";
        this->type = "";
        this->enabled = false;
        this->server = null;
    }

    void handleApiControl(){};
        
    JSONVar toJSONVar(int mode = DEVICES_MODE_PRIVATE) {
        JSONVar j;
        j["name"] = this->name;
        j["type"] = this->type;
        return j;
    }


};

class Stepper : public Device {
    public:
    int pin_enable;
    int pin_direction;
    int pin_step;
    int min;
    int max;
    int pulse;
    int command_min;
    int command_max;
    int direction;
    int speed;

    Stepper(
        char *name = "Stepper", 
        int pin_enable = 0,
        int pin_direction = 0,
        int pin_step = 0,
        int min = 0,
        int max = 1024,
        int pulse = 10,
        int command_min = -511,
        int command_max = 512
    ) {
        this->name = name;
        this->type = "stepper";
        this->pin_enable = pin_enable;
        this->pin_direction = pin_direction;
        this->pin_step = pin_step;
        this->min = min;
        this->max = max;
        this->pulse = pulse;
        this->command_min = command_min;
        this->command_max = command_max;
        this->direction = 0;
        this->speed = 0;
    }

    void handleApiControl() {
        int vector = this->server->arg("vector").toInt();
        if (vector < this->command_min) {
            vector = this->command_min;
        }
        else if (vector > this->command_max) {
            vector = this->command_max;
        }
        this->direction = vector < 0 ? -1 : 1;
        this->speed = abs(vector);
        this->enabled = this->speed > 0;
        char message[100];
        sprintf(message, "command enable: %d  direction: %d  speed: %d", 
            this->enabled, this->direction, this->speed);
        this->server->send(200, "text/plain", message);
        Serial.println(message);
    }

    JSONVar toJSONVar(int mode = DEVICES_MODE_PRIVATE) {
        JSONVar j = Device::toJSONVar(mode);
        j["command_min"] = this->command_min;
        j["command_max"] = this->command_max;
        if (DEVICES_MODE_PRIVATE == mode) {
            j["pin_enable"] = this->pin_enable;
            j["pin_direction"] = this->pin_direction;
            j["pin_step"] = this->pin_step;
            j["min"] = this->min;
            j["max"] = this->max;
            j["pulse"] = this->pulse;
        }
        return j;
    }

    protected:
    void setup() {
        pinMode(this->pin_enable, OUTPUT);
        pinMode(this->pin_direction, OUTPUT);
        pinMode(this->pin_step, OUTPUT);
        digitalWrite(this->pin_enable, LOW);
        digitalWrite(this->pin_direction, LOW);
        digitalWrite(this->pin_step, LOW);
    }

    void loop() {
       Serial.print("--StLoo--");
       while (this->enabled && (0 < this->speed)) {
            digitalWrite(this->pin_direction, (0 < this->direction) ? HIGH : LOW);
            digitalWrite(this->pin_enable, HIGH);
            digitalWrite(this->pin_step, HIGH);
            delay(this->pulse);
            digitalWrite(this->pin_step, LOW);
            int pause = map(this->speed, this->command_min, this->command_max, this->max*2, this->min);
            delay(pause);
        }
        digitalWrite(this->pin_enable, LOW);
    }
};

class Led : public Device {
    public:
    int pin_enable;

    Led (char *name = "Led", int pin_enable = 0) {
        this->name = name;
        this->type = "led";
        this->pin_enable = pin_enable;
    }

    void handleApiControl() {
        bool enabled = false;
        /*
        if (
            ("t" == this->server->arg("enable").c_str()) ||   // "true"
            (0 < this->server->arg("enable").toInt()))        // 1
            enabled = true;
        */
        this->enabled = enabled;
        char message[100];
        sprintf(message, "command enable: %b", this->enabled);
        this->server->send(200, "text/plain", message);
        Serial.println(message);
    }
    
    JSONVar toJSONVar(int mode = DEVICES_MODE_PRIVATE) {
        JSONVar j = Device::toJSONVar(mode);
        if (DEVICES_MODE_PRIVATE == mode) {
            j["pin_enable"] = this->pin_enable;
        }
        return j;
    }

    protected:
    void setup() {
        pinMode(this->pin_enable, OUTPUT);
        digitalWrite(this->pin_enable, LOW);
    }

    void loop() {
        Serial.print(ESP.getFreeHeap());
        digitalWrite(this->pin_enable, this->enabled ? HIGH : LOW);
    }
};

#endif
