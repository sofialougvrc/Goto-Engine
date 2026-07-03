#include "gte/catalog.hpp"
#include "gte/coord_transform.hpp"
#include "gte/mount.hpp"
#include "gte/time_utils.hpp"
#include "gte/tracking_loop.hpp"

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

double normalizeDegrees(double degrees) {
    double normalized = std::fmod(degrees, 360.0);
    if (normalized < 0.0) {
        normalized += 360.0;
    }
    return normalized;
}

double angleError(double actual, double expected) {
    double error = std::abs(normalizeDegrees(actual) - normalizeDegrees(expected));
    if (error > 180.0) {
        error = 360.0 - error;
    }
    return error;
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

bool expectAngleNear(
    const std::string& name,
    double actual,
    double expected,
    double tolerance_deg) {
    const double error = angleError(actual, expected);
    if (error <= tolerance_deg) {
        return true;
    }

    std::cerr << name << " failed: expected " << expected
              << " deg, got " << actual
              << " deg, angular error " << error
              << " deg\n";
    return false;
}

bool expectTrue(const std::string& name, bool condition) {
    if (condition) {
        return true;
    }

    std::cerr << name << " failed\n";
    return false;
}

bool expectHorizontalNear(
    const std::string& name,
    gte::HorizontalCoord actual,
    gte::HorizontalCoord expected,
    double tolerance_deg) {
    bool ok = true;
    ok &= expectNear(name + " altitude", actual.alt_deg, expected.alt_deg, tolerance_deg);
    ok &= expectAngleNear(name + " azimuth", actual.az_deg, expected.az_deg, tolerance_deg);
    return ok;
}

} // namespace

int main() {
    bool ok = true;

    // References from the U.S. Naval Observatory Celestial Navigation API for
    // 2024-04-08 18:18 UT1 at Washington, DC. The USNO declination and local
    // hour angle are represented here as equivalent RA/LST inputs.
    ok &= expectHorizontalNear(
        "USNO Alpheratz reference",
        gte::equatorialToHorizontal(
            {327.443943, 29.221197},
            38.9072,
            0.0),
        {61.525426, 260.082043},
        0.1);

    ok &= expectHorizontalNear(
        "USNO Hamal reference",
        gte::equatorialToHorizontal(
            {357.169416, 23.575408},
            38.9072,
            0.0),
        {74.480763, 189.739066},
        0.1);

    ok &= expectHorizontalNear(
        "Zenith convention",
        gte::equatorialToHorizontal({120.0, 35.0}, 35.0, 120.0),
        {90.0, 0.0},
        1.0e-6);

    ok &= expectHorizontalNear(
        "North celestial pole convention",
        gte::equatorialToHorizontal({15.0, 90.0}, 40.0, 250.0),
        {40.0, 0.0},
        1.0e-9);

    ok &= expectHorizontalNear(
        "Below horizon is valid",
        gte::equatorialToHorizontal({180.0, 0.0}, 45.0, 0.0),
        {-45.0, 0.0},
        1.0e-9);

    const gte::Catalog catalog = gte::Catalog::load("data/messier.json");
    ok &= expectTrue("Messier catalog has 110 objects", catalog.all().size() == 110);

    const gte::CatalogObject* m42 = catalog.find("M42");
    ok &= expectTrue("M42 exists", m42 != nullptr);
    if (m42 != nullptr) {
        ok &= expectNear("M42 RA", m42->equatorial.ra_deg, 83.82208333, 1.0e-6);
        ok &= expectNear("M42 Dec", m42->equatorial.dec_deg, -5.39111111, 1.0e-6);
        ok &= expectTrue("M42 name", m42->name == "Great Orion Nebula");

        const auto utc = makeUtc(2026, 1, 15, 4, 0, 0);
        const double lst = gte::localSiderealTime(utc, -77.0369);
        ok &= expectHorizontalNear(
            "M42 end-to-end position",
            gte::equatorialToHorizontal(m42->equatorial, 38.9072, lst),
            {43.90482229, 199.19538759},
            0.01);
    }

    gte::VirtualMount mount;
    mount.slewTo(12.5, 181.25);
    ok &= expectHorizontalNear(
        "Virtual mount stores current position",
        mount.currentPosition(),
        {12.5, 181.25},
        1.0e-12);

    const auto base = makeUtc(2026, 1, 1, 0, 0, 0);
    const double base_lst = gte::greenwichMeanSiderealTime(base);
    const gte::EquatorialCoord rising_target{normalizeDegrees(base_lst + 30.0), 0.0};

    gte::VirtualMount tracking_mount;
    gte::TrackingLoop tracking(tracking_mount, 0.0, 0.0);
    tracking.trackObject(rising_target);
    tracking.tickAt(base);
    const double alt0 = tracking_mount.currentPosition().alt_deg;
    tracking.tickAt(base + std::chrono::seconds(60));
    const double alt1 = tracking_mount.currentPosition().alt_deg;
    tracking.tickAt(base + std::chrono::seconds(600));
    const double alt2 = tracking_mount.currentPosition().alt_deg;

    ok &= expectTrue("Tracking loop rising target altitude increases", alt0 < alt1 && alt1 < alt2);
    ok &= expectNear("Tracking loop first altitude", alt0, 60.0, 0.01);
    ok &= expectNear("Tracking loop later altitude", alt2, 62.50684479, 0.01);

    if (ok) {
        std::cout << "All engine tests passed\n";
        return EXIT_SUCCESS;
    }

    return EXIT_FAILURE;
}
