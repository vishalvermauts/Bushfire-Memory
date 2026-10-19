#include "bushfire/GeoUtils.hpp"
#include "bushfire/CacheStore.hpp"
#include "bushfire/SimilarityEngine.hpp"
#include "bushfire/ExportService.hpp"
#include "bushfire/VerifiedSnapshots.hpp"
#include "bushfire/providers/HotspotAdapter.hpp"
#include "bushfire/providers/RfsIncidentAdapter.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

#define TEST_ASSERT(expr, msg) \
    do { \
        if (!(expr)) { \
            std::cerr << "FAILED: " << msg << " (" << #expr << ") at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

bool testHaversine() {
    // Sydney to Melbourne: ~713 km
    double d = bushfire::GeoUtils::haversineDistanceKm(-33.8688, 151.2093, -37.8136, 144.9631);
    TEST_ASSERT(d > 700.0 && d < 730.0, "Sydney to Melbourne distance should be ~713 km");

    // Self distance should be 0.0
    double dSelf = bushfire::GeoUtils::haversineDistanceKm(-33.8688, 151.2093, -33.8688, 151.2093);
    TEST_ASSERT(dSelf < 1e-4, "Self distance must be 0");

    return true;
}

bool testBbox() {
    std::array<double, 4> nswBbox = {140.0, -38.0, 154.0, -28.0};
    TEST_ASSERT(bushfire::GeoUtils::pointInBbox(151.2, -33.8, nswBbox), "Sydney is in NSW bbox");
    TEST_ASSERT(!bushfire::GeoUtils::pointInBbox(115.8, -31.9, nswBbox), "Perth is not in NSW bbox");

    std::array<double, 4> overlapBbox = {150.0, -35.0, 152.0, -32.0};
    TEST_ASSERT(bushfire::GeoUtils::bboxIntersects(nswBbox, overlapBbox), "Bboxes should intersect");

    return true;
}

bool testCentroidComputation() {
    // Polygon centroid
    nlohmann::json polyGeom = {
        {"type", "Polygon"},
        {"coordinates", {{
            {150.0, -34.0}, {152.0, -34.0}, {152.0, -32.0}, {150.0, -32.0}, {150.0, -34.0}
        }}}
    };
    auto c = bushfire::GeoUtils::computeCentroid(polyGeom);
    TEST_ASSERT(std::abs(c.first - 150.8) < 0.3, "Polygon centroid Lng should be ~151.0");
    TEST_ASSERT(std::abs(c.second - (-33.2)) < 0.3, "Polygon centroid Lat should be ~-33.0");

    return true;
}

bool testRfsDescriptionParser() {
    std::string html = "ALERT LEVEL: Emergency Warning <br /> LOCATION: Gospers Mountain <br /> COUNCIL AREA: Hawkesbury <br /> STATUS: Out of control <br /> TYPE: Bush Fire <br /> FIRE: Yes <br /> SIZE: 512000 ha <br /> RESPONSIBLE AGENCY: Rural Fire Service <br /> UPDATED: 26 Oct 2019 14:00";
    auto parsed = bushfire::GeoUtils::parseRfsDescription(html);

    TEST_ASSERT(parsed.alertLevel == "Emergency Warning", "Alert level parsed");
    TEST_ASSERT(parsed.location == "Gospers Mountain", "Location parsed");
    TEST_ASSERT(parsed.councilArea == "Hawkesbury", "Council area parsed");
    TEST_ASSERT(parsed.status == "Out of control", "Status parsed");
    TEST_ASSERT(parsed.type == "Bush Fire", "Type parsed");
    TEST_ASSERT(parsed.isFire == true, "isFire parsed");
    TEST_ASSERT(parsed.sizeHa.has_value() && *parsed.sizeHa == 512000.0, "Size in ha parsed");
    TEST_ASSERT(parsed.responsibleAgency == "Rural Fire Service", "Agency parsed");

    return true;
}

bool testCircularDoySeasonality() {
    // Dec 31 (DOY 365) to Jan 1 (DOY 1) should be 1 day apart, seasonal similarity close to 1.0
    double s = bushfire::SimilarityEngine::computeSeasonScore(365, 1);
    TEST_ASSERT(s > 0.99, "Dec 31 and Jan 1 must have circular season score > 0.99");

    // 6 months apart (Jan 1 vs July 2: DOY 1 vs DOY 183) should be close to 0.0
    double sOpposite = bushfire::SimilarityEngine::computeSeasonScore(1, 183);
    TEST_ASSERT(sOpposite < 0.02, "6 months apart must have season score near 0");

    return true;
}

bool testSimilarityEngineIdentity() {
    auto fires = bushfire::VerifiedSnapshots::getHistoricalFires();
    TEST_ASSERT(!fires.empty(), "Historical fires fixture must not be empty");

    const auto& f = fires[0];
    auto match = bushfire::SimilarityEngine::evaluate(f, f);

    TEST_ASSERT(std::abs(match.totalScorePct - 100.0) < 1e-4, "Self similarity must equal 100%");
    TEST_ASSERT(std::abs(match.geoScore - 1.0) < 1e-4, "Self geo score must equal 1.0");
    TEST_ASSERT(std::abs(match.sizeScore - 1.0) < 1e-4, "Self size score must equal 1.0");
    TEST_ASSERT(std::abs(match.seasonScore - 1.0) < 1e-4, "Self season score must equal 1.0");
    TEST_ASSERT(std::abs(match.typeScore - 1.0) < 1e-4, "Self type score must equal 1.0");

    return true;
}

bool testSimilarityMonotonicDecay() {
    // Proximity
    double s1 = bushfire::SimilarityEngine::computeGeoScore(10.0);
    double s2 = bushfire::SimilarityEngine::computeGeoScore(100.0);
    double s3 = bushfire::SimilarityEngine::computeGeoScore(500.0);
    TEST_ASSERT(s1 > s2 && s2 > s3, "Geo score must strictly decay with distance");

    // Size log-scale
    double sz1 = bushfire::SimilarityEngine::computeSizeScore(10000.0, 10000.0);
    double sz2 = bushfire::SimilarityEngine::computeSizeScore(10000.0, 20000.0);
    double sz3 = bushfire::SimilarityEngine::computeSizeScore(10000.0, 100000.0);
    TEST_ASSERT(sz1 > sz2 && sz2 > sz3, "Size score must decay with area divergence");

    return true;
}

bool testCacheStore() {
    bushfire::CacheStore cache(":memory:");

    // Cache set & get
    bool ok = cache.set("test:key", "payload-data", 300);
    TEST_ASSERT(ok, "Cache set should succeed");

    auto entry = cache.get("test:key");
    TEST_ASSERT(entry.has_value(), "Cache entry should exist");
    TEST_ASSERT(entry->payload == "payload-data", "Payload should match");
    TEST_ASSERT(entry->freshness == bushfire::FreshnessStatus::CachedFresh, "Should be CachedFresh");

    // Settings set & get
    cache.setSetting("NASA_FIRMS_MAP_KEY", "test-key-12345");
    auto keyVal = cache.getSetting("NASA_FIRMS_MAP_KEY");
    TEST_ASSERT(keyVal.has_value() && *keyVal == "test-key-12345", "Setting should be retrieved");

    return true;
}

bool testExportService() {
    auto incidents = bushfire::VerifiedSnapshots::getFallbackIncidents();
    auto history = bushfire::VerifiedSnapshots::getHistoricalFires();
    auto hotspots = bushfire::VerifiedSnapshots::getFallbackHotspots();

    std::string gj = bushfire::ExportService::exportGeoJson(incidents, history, hotspots);
    auto root = nlohmann::json::parse(gj, nullptr, false);
    TEST_ASSERT(!root.is_discarded(), "GeoJSON must be valid JSON");
    TEST_ASSERT(root["type"] == "FeatureCollection", "GeoJSON must be FeatureCollection");
    TEST_ASSERT(root["features"].size() == (incidents.size() + history.size() + hotspots.size()), "All features present in GeoJSON");

    std::string csv = bushfire::ExportService::exportIncidentsCsv(incidents);
    TEST_ASSERT(csv.find("id,original_id,title") != std::string::npos, "CSV header must be present");

    std::string md = bushfire::ExportService::exportMarkdownReport(incidents, history, hotspots, {});
    TEST_ASSERT(md.find("CRITICAL PUBLIC SAFETY NOTICE") != std::string::npos, "Markdown report must contain safety header");

    return true;
}

int main() {
    std::cout << "========================================\n"
              << "  BUSHFIRE MEMORY C++20 TEST RUNNER    \n"
              << "========================================\n";

    int testsPassed = 0;
    int testsFailed = 0;

    auto run = [&](const char* name, bool (*fn)()) {
        std::cout << "[RUN] " << name << " ... ";
        if (fn()) {
            std::cout << "PASSED\n";
            testsPassed++;
        } else {
            std::cout << "FAILED\n";
            testsFailed++;
        }
    };

    run("Haversine Distance Calculations", testHaversine);
    run("Bounding Box Spatial Filtering", testBbox);
    run("Centroid Calculation for Geometries", testCentroidComputation);
    run("NSW RFS HTML Description Parser", testRfsDescriptionParser);
    run("Circular Day-of-Year Seasonality", testCircularDoySeasonality);
    run("Similarity Engine Identity Property", testSimilarityEngineIdentity);
    run("Similarity Engine Monotonic Decay", testSimilarityMonotonicDecay);
    run("SQLite CacheStore & Settings", testCacheStore);
    run("Export Service (GeoJSON, CSV, Markdown)", testExportService);

    std::cout << "\nTest Results: " << testsPassed << " passed, " << testsFailed << " failed.\n";
    return (testsFailed == 0) ? 0 : 1;
}
