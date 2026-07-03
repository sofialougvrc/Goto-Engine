#include "gte/coord_transform.hpp"

#include <algorithm>
#include <cmath>

namespace gte {
namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kDegToRad = kPi / 180.0;
constexpr double kRadToDeg = 180.0 / kPi;
constexpr double kSingularityEpsilon = 1.0e-8;

double normalizeDegrees(double degrees) {
    double normalized = std::fmod(degrees, 360.0);
    if (normalized < 0.0) {
        normalized += 360.0;
    }
    return normalized;
}

double clampUnit(double value) {
    return std::clamp(value, -1.0, 1.0);
}

} // namespace

HorizontalCoord equatorialToHorizontal(
    EquatorialCoord target,
    double observer_lat_deg,
    double local_sidereal_time_deg) {
    const double hour_angle_rad =
        normalizeDegrees(local_sidereal_time_deg - target.ra_deg) * kDegToRad;
    const double dec_rad = target.dec_deg * kDegToRad;
    const double lat_rad = observer_lat_deg * kDegToRad;

    const double sin_alt =
        std::sin(dec_rad) * std::sin(lat_rad) +
        std::cos(dec_rad) * std::cos(lat_rad) * std::cos(hour_angle_rad);

    const double clamped_sin_alt = clampUnit(sin_alt);
    const double alt_rad = std::asin(clamped_sin_alt);
    if (std::abs(1.0 - std::abs(clamped_sin_alt)) < kSingularityEpsilon) {
        return {alt_rad * kRadToDeg, 0.0};
    }

    const double az_denominator = std::cos(alt_rad) * std::cos(lat_rad);

    if (std::abs(az_denominator) < kSingularityEpsilon) {
        return {alt_rad * kRadToDeg, 0.0};
    }

    const double cos_az =
        (std::sin(dec_rad) - std::sin(alt_rad) * std::sin(lat_rad)) /
        az_denominator;

    double az_rad = std::acos(clampUnit(cos_az));
    if (std::sin(hour_angle_rad) > 0.0) {
        az_rad = 2.0 * kPi - az_rad;
    }

    return {
        alt_rad * kRadToDeg,
        normalizeDegrees(az_rad * kRadToDeg),
    };
}

} // namespace gte
