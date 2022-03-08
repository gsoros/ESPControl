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

class OledWithPotAndWifi;

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
    OledWithPotAndWifi *oled;

    Device() {
        this->name = "";
        this->host = "";
        this->hostIp = IPAddress();
        this->hostPort = 0;
        this->hostDevice = "";
        this->pin = 0;
        read();
    }

    void blinkOledPercent(int speed);
    void blinkOledWifi(int speed);
    bool sendCommand(int command);

    void setOled(OledWithPotAndWifi *oled) {
        this->oled = oled;
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
                        if (conf["devices"][i]["commandMin"].is<int>())
                            this->commandMin = conf["devices"][i]["commandMin"];
                        if (conf["devices"][i]["commandMax"].is<int>())
                            this->commandMax = conf["devices"][i]["commandMax"];
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

    virtual void loop() {
        read();
        delay(measurementDelay);
    }
};

class Oled : public Task {
   public:
    int reset = -1;
    int width = 128;
    int height = 32;
    int address = 0x3C;
    Adafruit_SSD1306 *display;

    Oled(Adafruit_SSD1306 *display) {
        this->display = display;
    }

    void writeText(const char *text, int cursorX = 0, int cursorY = 0, bool clear = true) {
        if (clear)
            display->fillRect(cursorX, cursorY, width - cursorX, height - cursorY, SSD1306_BLACK);
        display->setTextSize(4);
        display->setTextColor(SSD1306_WHITE);
        display->setCursor(cursorX, cursorY);
        display->cp437(true);
        display->print(text);
        display->display();
    }

   protected:
    virtual void setup() {
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
    }
};

class OledWithPotAndWifi : public Oled {
   public:
    Pot *pot;
    int wifiConnected = 0;
    int wifiBlinkSpeed = 0;     // blinks/s
    int percentBlinkSpeed = 0;  // blinks/s
    int potLastValue = 0;

    OledWithPotAndWifi(Adafruit_SSD1306 *display, Pot *pot) : Oled(display) {
        this->pot = pot;
        pot->setOled(this);
    }

    void drawWifi(bool visible = true) {
        if (visible)
            display->drawBitmap(0, 0, wifiIcon, wifiIconWidth, wifiIconHeight, SSD1306_WHITE);
        else
            display->fillRect(0, 0, wifiIconWidth, wifiIconHeight, SSD1306_BLACK);
        display->display();
    }

    void writePercent(uint8_t percent, bool visible = true) {
        if (percent < 0 || 100 < percent) {
            Serial.printf("Percent out ouf range: %d\n", percent);
            return;
        }
        if (visible) {
            char text[5];
            snprintf(text, 5, "%3d%%", percent);
            writeText(text, 32);
            return;
        }
        writeText("", 32);
    }

   protected:
    uint8_t potPercent = 0;
    int wifiConnectedLastValue = 0;
    bool wifiIconVisible = false;
    unsigned long wifiIconLastChange = 0;
    bool percentVisible = false;
    unsigned long percentVisibleLastChange = 0;
    const uint8_t wifiIconWidth = 32;
    const uint8_t wifiIconHeight = 32;
    const unsigned char wifiIcon[32 * 32] = {
        0b00000000, 0b00001110, 0b11110000, 0b00000000,
        0b00000000, 0b01111110, 0b11111110, 0b00000000,
        0b00000000, 0b11111110, 0b11111110, 0b10000000,
        0b00000010, 0b11111110, 0b11111110, 0b11000000,
        0b00001110, 0b11111100, 0b00111110, 0b11110000,
        0b00011110, 0b11000000, 0b00000010, 0b11111000,
        0b00111110, 0b00000000, 0b00000000, 0b11111100,
        0b01111110, 0b00000000, 0b00000000, 0b01111110,
        0b01111100, 0b00000110, 0b11100000, 0b00111110,
        0b11111000, 0b00111110, 0b11111100, 0b00011110,
        0b01110000, 0b01111110, 0b11111110, 0b00001110,
        0b00100000, 0b11111110, 0b11111110, 0b10000100,
        0b00000010, 0b11111100, 0b00111110, 0b11000000,
        0b00000110, 0b11100000, 0b00000110, 0b11100000,
        0b00001110, 0b11000000, 0b00000010, 0b11110000,
        0b00001110, 0b00000000, 0b00000000, 0b11110000,
        0b00000110, 0b00000110, 0b11100000, 0b01100000,
        0b00000010, 0b00011110, 0b11111000, 0b01000000,
        0b00000000, 0b00111110, 0b11111100, 0b00000000,
        0b00000000, 0b01111110, 0b11111110, 0b00000000,
        0b00000000, 0b11111110, 0b01111110, 0b00000000,
        0b00000000, 0b01110000, 0b00001110, 0b00000000,
        0b00000000, 0b01100000, 0b00000110, 0b00000000,
        0b00000000, 0b00000000, 0b00000000, 0b00000000,
        0b00000000, 0b00000010, 0b11000000, 0b00000000,
        0b00000000, 0b00000010, 0b11100000, 0b00000000,
        0b00000000, 0b00000110, 0b11100000, 0b00000000,
        0b00000000, 0b00000110, 0b11100000, 0b00000000,
        0b00000000, 0b00000110, 0b11100000, 0b00000000,
        0b00000000, 0b00000110, 0b11100000, 0b00000000,
        0b00000000, 0b00000010, 0b11000000, 0b00000000,
        0b00000000, 0b00000000, 0b10000000, 0b00000000};

    virtual void setup() {
        Oled::setup();
        potLastValue = pot->getValue() + 1;  // make sure value gets displayed on oled
    }

    virtual void loop() {
        int potValue = pot->getValue();
        if (potValue != potLastValue) {
            potPercent = pot->invert                                       //
                             ? map(potValue, pot->max, pot->min, 1, 100)   //
                             : map(potValue, pot->min, pot->max, 1, 100);  //
            writePercent(potPercent);
            potLastValue = potValue;
        }
        if (0 < percentBlinkSpeed &&
            percentVisibleLastChange < millis() - 1000 / percentBlinkSpeed) {
            percentVisible = !percentVisible;
            writePercent(potPercent, percentVisible);
            percentVisibleLastChange = millis();
        }

        if (0 < wifiBlinkSpeed) {
            if (wifiIconLastChange < millis() - 1000 / wifiBlinkSpeed) {
                wifiIconVisible = !wifiIconVisible;
                drawWifi(wifiIconVisible);
                wifiIconLastChange = millis();
            }
        } else if (wifiConnected != wifiConnectedLastValue) {
            drawWifi(0 < wifiConnected);
            wifiConnectedLastValue = wifiConnected;
        }
        delay(pot->measurementDelay);
    }
};

void Device::blinkOledWifi(int speed) {
    if (nullptr == oled) return;
    oled->wifiBlinkSpeed = speed;
}

void Device::blinkOledPercent(int speed) {
    if (nullptr == oled) return;
    oled->percentBlinkSpeed = speed;
}

bool Device::sendCommand(int command) {
    if (!hostAvailable) return false;
    Serial.printf("[Device %s] Sending command: %d\n", name, command);
    char url[100];
    sprintf(url, "http://%s:%i/api/control?device=%s&command=%i",
            hostIp.toString().c_str(),
            hostPort,
            hostDevice,
            command);
    char response[this->responseBufSize];
    blinkOledWifi(10);
    int statusCode = this->requestGet(url, response);
    blinkOledWifi(0);
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
        this->invert = invert;
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

class SwitchBlinker : public Switch {
   public:
    SwitchBlinker(
        const char *name = "Switch",
        int pin = 0,
        const char *host = "",
        const char *hostDevice = "",
        bool invert = false) : Switch(name, pin, host, hostDevice, invert){};

    int read() {
        int value = Switch::read();
        blinkOledPercent(value ? 0 : 300);
        if (nullptr != oled) oled->potLastValue += 1;  // trigger refresh
        return value;
    }
};

class DeviceCommandTask : public Task, public Request {
   public:
    Device *device;
    int keepAliveSeconds = 5;  // send command to keep connection alive
    unsigned long lastCommandSent = 0;

    DeviceCommandTask(Device *device) {
        this->device = device;
    }

   protected:
    virtual void loop() {
        if (!device->hostAvailable) {
            delay(device->hostRate);
            return;
        }
        int command = calculateCommand();
        int commandDiff = abs(device->lastCommand - command);
        long cutoff = millis() - keepAliveSeconds * 1000;
        if (commandDiff > device->movementMin                           //
            || (lastCommandSent < (unsigned long)cutoff && 0 < cutoff)  //
            || 0 == lastCommandSent) {
            if (device->sendCommand(command))
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
        /*
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
        */
        return out;
    }
};

#endif
