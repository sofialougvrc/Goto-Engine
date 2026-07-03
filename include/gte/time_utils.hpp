#pragma once

#include <chrono>

namespace gte {

// Greenwich Mean Sidereal Time, in degrees [0, 360).
double greenwichMeanSiderealTime(std::chrono::system_clock::time_point utc);

// Local Sidereal Time = GMST + observer longitude, in degrees [0, 360).
// Longitude uses the east-positive convention: east of Greenwich is positive,
// west of Greenwich is negative.
double localSiderealTime(std::chrono::system_clock::time_point utc, double longitude_deg);

} // namespace gte
