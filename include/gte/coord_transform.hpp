#pragma once

namespace gte {

struct EquatorialCoord {
    double ra_deg;
    double dec_deg;
};

struct HorizontalCoord {
    double alt_deg;
    double az_deg;
};

// Converts right ascension/declination to altitude/azimuth.
// Azimuth is degrees clockwise from north in [0, 360). If azimuth is undefined
// at a singular point such as the zenith, az_deg is returned as 0 by convention.
HorizontalCoord equatorialToHorizontal(
    EquatorialCoord target,
    double observer_lat_deg,
    double local_sidereal_time_deg);

} // namespace gte
