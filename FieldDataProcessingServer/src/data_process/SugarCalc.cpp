#include "SugarCalc.h"
#include <cmath>

double SugarCalc::calculate(const nlohmann::json& spectrum) {
    // Fixed-light raw-channel MLR. chClear is recorded for diagnostics only,
    // because the current optical structure can saturate the clear channel.
    double ch445 = spectrum.value("ch445", 0.0);
    double ch480 = spectrum.value("ch480", 0.0);
    double ch680 = spectrum.value("ch680", 0.0);
    double chNIR = spectrum.value("chNIR", 0.0);

    if (ch445 < 10.0 || ch480 < 10.0 || ch680 < 10.0 || chNIR < 10.0) {
        return 0.0;
    }

    const double scale = 10000.0;
    double x445 = ch445 / scale;
    double x480 = ch480 / scale;
    double x680 = ch680 / scale;
    double xNIR = chNIR / scale;

    const double k = 13.21941763;
    const double a = -23.50752069;
    const double b = 8.59055811;
    const double c = 5.98831600;
    const double d = -8.41861824;

    double raw_brix = (a * x445) + (b * x480) + (c * x680) + (d * xNIR) + k;

    if (std::isnan(raw_brix)) return 0.0;
    if (raw_brix < 0.0) return 0.0;
    if (raw_brix > 20.0) return 20.0;

    return raw_brix;
}
