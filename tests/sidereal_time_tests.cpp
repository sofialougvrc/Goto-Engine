#include "gte/time_utils.hpp"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

std::chrono::system_clock::time_point makeUtc(
    int year,
    unsigned month,
    unsigned day,
    int hour,
    int minute,
    int second) {
    using namespace std::chrono;

    const auto date = sys_days{std::chrono::year{year} / month / day};
    const auto timestamp = date + hours{hour} + minutes{minute} + seconds{second};
    return std::chrono::time_point_cast<std::chrono::system_clock::duration>(timestamp);
}

bool expectNear(
    const std::string& name,
    double actual,
    double expected,
    double tolerance_deg) {
    const double error = std::abs(actual - expected);
    if (error <= tolerance_deg) {
        return true;
    }

    std::cerr << name << " failed: expected " << expected
              << " deg, got " << actual
              << " deg, error " << error
              << " deg\n";
    return false;
}

bool expectInNormalizedRange(const std::string& name, double value) {
    if (value >= 0.0 && value < 360.0) {
        return true;
    }

    std::cerr << name << " failed: value " << value
              << " is outside [0, 360)\n";
    return false;
}

} // namespace

int main() {
    constexpr double kToleranceDeg = 0.01;
    bool ok = true;

    // Reference values from the U.S. Naval Observatory Sidereal Time API.
    ok &= expectNear(
        "GMST at J2000.0",
        gte::greenwichMeanSiderealTime(makeUtc(2000, 1, 1, 12, 0, 0)),
        280.4606225,
        kToleranceDeg);

    ok &= expectNear(
        "GMST on 2024-04-08 18:18:00 UTC",
        gte::greenwichMeanSiderealTime(makeUtc(2024, 4, 8, 18, 18, 0)),
        111.99761375,
        kToleranceDeg);

    ok &= expectNear(
        "LST for Washington DC on 2024-04-08 18:18:00 UTC",
        gte::localSiderealTime(
            makeUtc(2024, 4, 8, 18, 18, 0),
            -77.0369),
        34.96071375,
        kToleranceDeg);

    ok &= expectNear(
        "GMST on 2026-07-04 00:00:00 UTC",
        gte::greenwichMeanSiderealTime(makeUtc(2026, 7, 4, 0, 0, 0)),
        282.01995875,
        kToleranceDeg);

    ok &= expectNear(
        "LST for Manila on 2026-07-04 00:00:00 UTC",
        gte::localSiderealTime(
            makeUtc(2026, 7, 4, 0, 0, 0),
            120.9842),
        43.00415875,
        kToleranceDeg);

    ok &= expectInNormalizedRange(
        "Negative longitude normalization",
        gte::localSiderealTime(makeUtc(2024, 4, 8, 18, 18, 0), -400.0));

    if (ok) {
        std::cout << "All sidereal time tests passed\n";
        return EXIT_SUCCESS;
    }

    return EXIT_FAILURE;
}
