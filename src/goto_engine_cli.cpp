#include "gte/catalog.hpp"
#include "gte/tracking_loop.hpp"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <string>
#include <thread>

namespace {

struct Options {
    std::string object_id;
    std::string catalog_path = "data/messier.json";
    double lat_deg = 0.0;
    double lon_deg = 0.0;
    int ticks = -1;
};

void printUsage() {
    std::cerr
        << "Usage: goto-engine track <object-id> [--lat deg] [--lon deg] "
        << "[--catalog path] [--ticks n]\n";
}

double parseDouble(const std::string& value, const std::string& name) {
    try {
        size_t consumed = 0;
        const double parsed = std::stod(value, &consumed);
        if (consumed != value.size()) {
            throw std::invalid_argument("trailing input");
        }
        return parsed;
    } catch (const std::exception&) {
        throw std::runtime_error("Invalid " + name + ": " + value);
    }
}

int parseInt(const std::string& value, const std::string& name) {
    try {
        size_t consumed = 0;
        const int parsed = std::stoi(value, &consumed);
        if (consumed != value.size()) {
            throw std::invalid_argument("trailing input");
        }
        return parsed;
    } catch (const std::exception&) {
        throw std::runtime_error("Invalid " + name + ": " + value);
    }
}

Options parseOptions(int argc, char** argv) {
    if (argc < 3 || std::string(argv[1]) != "track") {
        throw std::runtime_error("Missing track command");
    }

    Options options;
    options.object_id = argv[2];

    for (int i = 3; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--lat" && i + 1 < argc) {
            options.lat_deg = parseDouble(argv[++i], "--lat");
        } else if (arg == "--lon" && i + 1 < argc) {
            options.lon_deg = parseDouble(argv[++i], "--lon");
        } else if (arg == "--catalog" && i + 1 < argc) {
            options.catalog_path = argv[++i];
        } else if (arg == "--ticks" && i + 1 < argc) {
            options.ticks = parseInt(argv[++i], "--ticks");
        } else {
            throw std::runtime_error("Unknown or incomplete option: " + arg);
        }
    }

    return options;
}

std::string localTimeOfDay(std::chrono::system_clock::time_point now) {
    const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &now_time);
#else
    localtime_r(&now_time, &tm);
#endif

    std::ostringstream out;
    out << std::put_time(&tm, "%H:%M:%S");
    return out.str();
}

} // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parseOptions(argc, argv);
        const gte::Catalog catalog = gte::Catalog::load(options.catalog_path);
        const gte::CatalogObject* object = catalog.find(options.object_id);
        if (object == nullptr) {
            throw std::runtime_error("Catalog object not found: " + options.object_id);
        }

        gte::VirtualMount mount;
        gte::TrackingLoop loop(mount, options.lat_deg, options.lon_deg);
        loop.trackObject(object->equatorial);

        int completed_ticks = 0;
        auto next_tick = std::chrono::steady_clock::now();
        while (options.ticks < 0 || completed_ticks < options.ticks) {
            const auto utc_now = std::chrono::system_clock::now();
            loop.tickAt(utc_now);
            const gte::HorizontalCoord position = mount.currentPosition();

            std::cout << '[' << localTimeOfDay(utc_now) << "] "
                      << object->id << " (" << object->name << ")"
                      << " -- Alt: " << std::fixed << std::setprecision(2)
                      << position.alt_deg << " deg  Az: "
                      << position.az_deg << " deg\n";

            ++completed_ticks;
            next_tick += std::chrono::seconds(1);
            if (options.ticks < 0 || completed_ticks < options.ticks) {
                std::this_thread::sleep_until(next_tick);
            }
        }
    } catch (const std::exception& e) {
        printUsage();
        std::cerr << "Error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
