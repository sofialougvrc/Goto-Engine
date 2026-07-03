#pragma once

#include "gte/coord_transform.hpp"

#include <string>
#include <vector>

namespace gte {

struct CatalogObject {
    std::string id;
    std::string name;
    std::string type;
    EquatorialCoord equatorial;
};

class Catalog {
public:
    static Catalog load(const std::string& path);

    const CatalogObject* find(const std::string& id) const;
    const std::vector<CatalogObject>& all() const;

private:
    std::vector<CatalogObject> objects_;
};

} // namespace gte
