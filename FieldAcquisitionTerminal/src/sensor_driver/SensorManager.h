#pragma once
#include <Arduino.h>
#include <Adafruit_AS7341.h>
#include <Adafruit_SHT31.h>
#include <BH1750.h>
#include <ArduinoJson.h>

class SensorManager {
public:
    void begin();
    void readEnvironment(float &temp, float &hum, int &light);
    bool readSpectrum(JsonObject& doc);
    //void controlLight(bool state);

private:
    Adafruit_AS7341 as7341;
    Adafruit_SHT31 sht31;
    BH1750 bh1750;
    bool has_as7341 = false;
    bool has_sht31 = false;
    bool has_bh1750 = false;
};