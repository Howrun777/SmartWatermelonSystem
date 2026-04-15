#include "SensorManager.h"
#include "../config.h"

void SensorManager::begin() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    
    if (sht31.begin(0x44)) { has_sht31 = true; } else { Serial.println("SHT31 Error"); }
    if (bh1750.begin()) { has_bh1750 = true; } else { Serial.println("BH1750 Error"); }
    
    if (as7341.begin()) {
        has_as7341 = true;
        as7341.setATIME(50);
        as7341.setASTEP(999);
        as7341.setGain(AS7341_GAIN_256X);
    } else {
        Serial.println("AS7341 Error");
    }
}


void SensorManager::readEnvironment(float &temp, float &hum, int &light) {
    if (has_sht31) {
        temp = sht31.readTemperature();
        hum = sht31.readHumidity();
        if (isnan(temp)) temp = 99.0;
        if (isnan(hum)) hum = 99.0;
    } else { temp = 99.0; hum = 99.0; }

    if (has_bh1750) {
        light = bh1750.readLightLevel();
        if (light < 0) light = 99;
    } else { light = 99; }
}


bool SensorManager::readSpectrum(JsonObject& doc) {
    if (!has_as7341) return false;
    if (!as7341.readAllChannels()) return false;
    
    doc["ch415"] = as7341.getChannel(AS7341_CHANNEL_415nm_F1);
    doc["ch445"] = as7341.getChannel(AS7341_CHANNEL_445nm_F2);
    doc["ch480"] = as7341.getChannel(AS7341_CHANNEL_480nm_F3);
    doc["ch515"] = as7341.getChannel(AS7341_CHANNEL_515nm_F4);
    doc["ch555"] = as7341.getChannel(AS7341_CHANNEL_555nm_F5);
    doc["ch595"] = as7341.getChannel(AS7341_CHANNEL_590nm_F6);
    doc["ch640"] = as7341.getChannel(AS7341_CHANNEL_630nm_F7);
    doc["ch680"] = as7341.getChannel(AS7341_CHANNEL_680nm_F8);
    doc["chClear"] = as7341.getChannel(AS7341_CHANNEL_CLEAR);
    doc["chNIR"] = as7341.getChannel(AS7341_CHANNEL_NIR);
    return true;
}