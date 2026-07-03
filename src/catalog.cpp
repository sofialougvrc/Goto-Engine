#include "gte/catalog.hpp"

#include <fstream>
#include <iterator>
#include <regex>
#include <stdexcept>

namespace gte {
namespace {

std::string readFile(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Unable to open catalog: " + path);
    }

    return {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>(),
    };
}

std::string stringField(const std::string& object_text, const std::string& field) {
    const std::regex field_regex("\"" + field + "\"\\s*:\\s*\"([^\"]*)\"");
    std::smatch match;
    if (!std::regex_search(object_text, match, field_regex)) {
        throw std::runtime_error("Catalog object missing string field: " + field);
    }
    return match[1].str();
}

double numberField(const std::string& object_text, const std::string& field) {
    const std::regex field_regex("\"" + field + "\"\\s*:\\s*(-?[0-9]+(?:\\.[0-9]+)?)");
    std::smatch match;
    if (!std::regex_search(object_text, match, field_regex)) {
        throw std::runtime_error("Catalog object missing numeric field: " + field);
    }
    return std::stod(match[1].str());
}

} // namespace

Catalog Catalog::load(const std::string& path) {
    const std::string text = readFile(path);
    const std::regex object_regex("\\{([^{}]*)\\}");

    Catalog catalog;
    for (auto it = std::sregex_iterator(text.begin(), text.end(), object_regex);
         it != std::sregex_iterator();
         ++it) {
        const std::string object_text = (*it)[1].str();
        catalog.objects_.push_back({
            stringField(object_text, "id"),
            stringField(object_text, "name"),
            stringField(object_text, "type"),
            {
                numberField(object_text, "ra_deg"),
                numberField(object_text, "dec_deg"),
            },
        });
    }

    if (catalog.objects_.empty()) {
        throw std::runtime_error("Catalog contains no objects: " + path);
    }

    return catalog;
}

const CatalogObject* Catalog::find(const std::string& id) const {
    for (const auto& object : objects_) {
        if (object.id == id) {
            return &object;
        }
    }
    return nullptr;
}

const std::vector<CatalogObject>& Catalog::all() const {
    return objects_;
}

} // namespace gte
