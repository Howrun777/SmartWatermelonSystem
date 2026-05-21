#pragma once
#include <json.hpp>

class SugarCalc {
public:
    // Calculates Brix with a fixed-light raw-channel MLR model.
    static double calculate(const nlohmann::json& spectrum);
};
