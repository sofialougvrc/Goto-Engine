#include "gte/mount.hpp"

namespace gte {

void VirtualMount::slewTo(double alt_deg, double az_deg) {
    current_ = {alt_deg, az_deg};
}

HorizontalCoord VirtualMount::currentPosition() const {
    return current_;
}

} // namespace gte
