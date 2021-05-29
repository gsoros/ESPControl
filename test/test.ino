#include <Arduino_JSON.h>
#include "config.h"

void setup() {
    Config config;
    config.name = "Controller1";
    
    Stepper stepper1;
    stepper1.name = "Stepper1";
    stepper1.pin_enable = D1;
    stepper1.pin_direction = D2;
    stepper1.pin_step = D3;
    stepper1.min = 10;              // minimum delay between pulses: fastest speed
    stepper1.max = 1000;            // maximum delay between pulses: slowest speed
    stepper1.pulse = 10;            // pulse width
    stepper1.command_min = -511;
    stepper1.command_max = 512;
    
    Led led1;
    led1.name = "Led1";
    led1.pin_enable = D4;
    
    config.addDevice(&stepper1);
    config.addDevice(&led1);

    Serial.begin(115200);
    while (!Serial);

    Serial.println("--------\n--------\n--------\n--------\n--------\n--BOOT--\n--------");
    Device *nonex = config.device(19);
    
    Serial.println("\n------\nCreating JSONVar Private");
    JSONVar j = config.toJSONVar();

    
    Serial.print("j['name'] = ");
    if (j.hasOwnProperty("name")) {
        Serial.println(j["name"]);
    } else {
        Serial.println("FAIL");
    }

    Serial.print("JSONVar dump:");
    Serial.println(j);
        
    String jsonString = JSON.stringify(j);
    Serial.print("JSON.stringify(j) = ");
    Serial.println(jsonString);
    
    Serial.println("parse");
    Serial.println("=====");
    
    j = JSON.parse(jsonString);
    
    if (JSON.typeof(j) == "undefined") {
        Serial.println("Parsing input failed!");
        return;
    }
    
    if (j.hasOwnProperty("name")) {
        Serial.print("j[\"name\"] = ");
        Serial.println(j["name"]);
    }
    
    Serial.println("\n------\nJSONVar Private");
    Serial.println(j);    

    Serial.println("\n------\nJSONVar Public");
    Serial.println(config.toJSONVar(CONFIG_MODE_PUBLIC)); 

    Serial.println("\n------\nJson Public");
    Serial.printf("%s\n", config.toJsonString(CONFIG_MODE_PUBLIC).c_str()); 
}

void loop() {
}
