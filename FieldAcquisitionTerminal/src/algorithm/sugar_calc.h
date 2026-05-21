#pragma once
#include <ArduinoJson.h>
#include <math.h>

class SugarCalc {
public:
    static float calculate(const JsonObject& spectrum) {
        // Fixed-light raw-channel MLR. chClear is recorded for diagnostics only,
        // because the current optical structure can saturate the clear channel.
        float ch445 = spectrum["ch445"] | 0.0f;
        float ch480 = spectrum["ch480"] | 0.0f;
        float ch680 = spectrum["ch680"] | 0.0f;
        float chNIR = spectrum["chNIR"] | 0.0f;

        if (ch445 < 10.0f || ch480 < 10.0f || ch680 < 10.0f || chNIR < 10.0f) {
            return 0.0f;
        }

        const float scale = 10000.0f;
        float x445 = ch445 / scale;
        float x480 = ch480 / scale;
        float x680 = ch680 / scale;
        float xNIR = chNIR / scale;

        const float k = 13.21941763f;
        const float a = -23.50752069f;
        const float b = 8.59055811f;
        const float c = 5.98831600f;
        const float d = -8.41861824f;

        float raw_brix = (a * x445) + (b * x480) + (c * x680) + (d * xNIR) + k;

        if (isnan(raw_brix) || raw_brix < 0.0f) return 0.0f;
        if (raw_brix > 20.0f) return 20.0f;

        return raw_brix;
    }
};
