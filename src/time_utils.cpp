#include "gte/time_utils.hpp"

#include <cmath>

namespace gte {
namespace {

constexpr long double kJulianDateAtUnixEpoch = 2440587.5L;
constexpr long double kSecondsPerDay = 86400.0L;
constexpr long double kJ2000JulianDate = 2451545.0L;
constexpr long double kDaysPerJulianCentury = 36525.0L;

double normalizeDegrees(long double degrees) {
    double normalized = std::fmod(static_cast<double>(degrees), 360.0);
    if (normalized < 0.0) {
        normalized += 360.0;
    }
    return normalized;
}

long double julianDate(std::chrono::system_clock::time_point utc) {
    const auto since_epoch = utc.time_since_epoch();
    const auto seconds =
        std::chrono::duration<long double>(since_epoch).count();

    return kJulianDateAtUnixEpoch + seconds / kSecondsPerDay;
}

} // namespace

double greenwichMeanSiderealTime(std::chrono::system_clock::time_point utc) {
    const long double jd = julianDate(utc);
    const long double days_since_j2000 = jd - kJ2000JulianDate;
    const long double t = days_since_j2000 / kDaysPerJulianCentury;

    const long double gmst =
        280.46061837L +
        360.98564736629L * days_since_j2000 +
        0.000387933L * t * t -
        (t * t * t) / 38710000.0L;

    return normalizeDegrees(gmst);
}

double localSiderealTime(std::chrono::system_clock::time_point utc, double longitude_deg) {
    return normalizeDegrees(greenwichMeanSiderealTime(utc) + longitude_deg);
}

} // namespace gte
