#ifndef DEVICES_H
#define DEVICES_H

#include <ESP8266HTTPClient.h>
#include <Scheduler.h>    // https://github.com/nrwiersma/ESP8266Scheduler
#include <ArduinoJson.h>  // https://github.com/bblanchon/ArduinoJson
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "request.h"

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
    bool invert = false;

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
        if (-1 < commandMin) {
            Serial.printf("[Pot] validateMinMax Warning: -1 < min\n");
            commandMin = -1;
        }
        if (commandMax < 1) {
            Serial.printf("[Pot] validateMinMax Warning: max < 1\n");
            commandMax = 1;
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
    int keepAliveSeconds = 5;  // send command to keep connection alive

    DeviceCommandTask(Device *device) {
        this->device = device;
    }

   protected:
    unsigned long lastCommandSent = 0;

    virtual void loop() {
        if (!device->hostAvailable) {
            delay(device->hostRate);
            return;
        }
        int command = calculateCommand();
        int commandDiff = abs(device->lastCommand - command);
        if (commandDiff > device->movementMin || lastCommandSent < millis() - keepAliveSeconds * 1000) {
            device->sendCommand(command);
            lastCommandSent = millis();
        } else if (commandDiff > 0) {
            Serial.printf("[%s] %d movement too small\n", device->name, commandDiff);
        }
        delay(device->hostRate);
    }

    virtual int calculateCommand() {
        return device->calculateCommand();
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
            int inMin;
            int inMax;
            if (pot->invert) {
                inMin = pot->max;
                inMax = pot->min;
            } else {
                inMin = pot->min;
                inMax = pot->max;
            }
            int outMin;
            int outMax;
            if (direction->getValue() == HIGH) {
                outMin = -1;
                outMax = pot->commandMin;
            } else {
                outMin = 1;
                outMax = pot->commandMax;
            }
            out = map(
                pot->getValue(),
                inMin,
                inMax,
                outMin,
                outMax);
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
};

class Oled : public Task {
   public:
    int sdaPin;
    int sclPin;
    int reset = -1;
    int width = 128;
    int height = 32;
    int address = 0x3C;
    Adafruit_SSD1306 *display;

    Oled(int sdaPin,
         int sclPin,
         Adafruit_SSD1306 *display) {
        this->sdaPin = sdaPin;
        this->sclPin = sclPin;
        this->display = display;
    }

   protected:
    virtual void setup() {
        Wire.begin(sdaPin, sclPin);
        // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
        if (!display->begin(SSD1306_SWITCHCAPVCC, address, false, false))
            Serial.println(F("SSD1306 allocation failed"));
        display->ssd1306_command(SSD1306_DISPLAYOFF);
        display->ssd1306_command(SSD1306_DISPLAYON);
        display->dim(true);
        display->clearDisplay();
        display->display();
    }

    virtual void loop() {
        testanimate();
    }

    void testanimate() {
        int numflakes = 10;
        int xpos = 0;
        int ypos = 1;
        int deltay = 2;
        int logoWidth = 16;
        int logoHeight = 16;

        static const unsigned char PROGMEM logo_bmp[] =
            {0b00000000, 0b11000000,
             0b00000001, 0b11000000,
             0b00000001, 0b11000000,
             0b00000011, 0b11100000,
             0b11110011, 0b11100000,
             0b11111110, 0b11111000,
             0b01111110, 0b11111111,
             0b00110011, 0b10011111,
             0b00011111, 0b11111100,
             0b00001101, 0b01110000,
             0b00011011, 0b10100000,
             0b00111111, 0b11100000,
             0b00111111, 0b11110000,
             0b01111100, 0b11110000,
             0b01110000, 0b01110000,
             0b00000000, 0b00110000};

        int8_t f, icons[numflakes][3];

        // Initialize 'snowflake' positions
        for (f = 0; f < numflakes; f++) {
            icons[f][xpos] = random(1 - logoWidth, display->width());
            icons[f][ypos] = -logoHeight;
            icons[f][deltay] = random(1, 6);
            Serial.print(F("x: "));
            Serial.print(icons[f][xpos], DEC);
            Serial.print(F(" y: "));
            Serial.print(icons[f][ypos], DEC);
            Serial.print(F(" dy: "));
            Serial.println(icons[f][deltay], DEC);
        }

        for (;;) {                    // Loop forever...
            display->clearDisplay();  // Clear the display buffer

            // Draw each snowflake:
            for (f = 0; f < numflakes; f++) {
                display->drawBitmap(icons[f][xpos], icons[f][ypos], logo_bmp, logoWidth, logoWidth, logoHeight, SSD1306_WHITE);
            }

            display->display();  // Show the display buffer on the screen
            // delay(10);          // Pause for 1/10 second

            // Then update coordinates of each flake...
            for (f = 0; f < numflakes; f++) {
                icons[f][ypos] += icons[f][deltay];
                // If snowflake is off the bottom of the screen...
                if (icons[f][ypos] >= display->height()) {
                    // Reinitialize to a random position, just off the top
                    icons[f][xpos] = random(1 - logoWidth, display->width());
                    icons[f][ypos] = -logoHeight;
                    icons[f][deltay] = random(1, 16);
                }
            }
            yield();
        }
    }
};

#endif
