#pragma once

#include "gte/coord_transform.hpp"
#include "gte/mount.hpp"

#include <chrono>

namespace gte {

class TrackingLoop {
public:
    TrackingLoop(IMountDriver& mount, double observer_lat_deg, double observer_lon_deg);

    void trackObject(EquatorialCoord target);
    void tick();
    void tickAt(std::chrono::system_clock::time_point utc);

private:
    EquatorialCoord target_{0.0, 0.0};
    IMountDriver& mount_;
    double observer_lat_;
    double observer_lon_;
    bool has_target_ = false;
};

} // namespace gte
