#pragma once
#include <ArduinoJson.h>

class SugarCalc {
public:
    static float calculate(const JsonObject& spectrum) {
        // 与服务器保持一致的 MLR 离线算法公式
        float k = 5.2; 
        float a = 0.0012, b = 0.0005, c = 0.0021; 
        float ch415 = spectrum["ch415"] | 0.0;
        float ch445 = spectrum["ch445"] | 0.0;
        float ch480 = spectrum["ch480"] | 0.0;
        
        float brix = a * ch415 + b * ch445 + c * ch480 + k;
        if (brix < 0) return 0.0;
        if (brix > 20.0) return 20.0;
        return brix;
    }
};