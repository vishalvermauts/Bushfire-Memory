#include "bushfire/HttpClient.hpp"
#include "bushfire/CacheStore.hpp"
#include "bushfire/providers/RfsIncidentAdapter.hpp"
#include "bushfire/providers/HotspotAdapter.hpp"
#include "bushfire/providers/HistoricalFireAdapter.hpp"
#include "bushfire/providers/StacImageryAdapter.hpp"
#include "bushfire/SimilarityEngine.hpp"
#include "bushfire/ExportService.hpp"
#include "bushfire/GeoUtils.hpp"
#include "bushfire/VerifiedSnapshots.hpp"
#include <iostream>
#include <fstream>
#include <iomanip>

int runSelfCheck() {
    std::cout << "[SELF-CHECK] Running Bushfire Memory Core Validation Suite...\n";
    int passed = 0;
    int failed = 0;

    auto assertTest = [&](const std::string& name, bool condition) {
        if (condition) {
            std::cout << "  [PASS] " << name << "\n";
            passed++;
        } else {
            std::cout << "  [FAIL] " << name << "\n";
            failed++;
        }
    };

    // 1. Haversine distance sanity check
    // Sydney (-33.8688, 151.2093) to Melbourne (-37.8136, 144.9631) is ~713 km
    double distSydMel = bushfire::GeoUtils::haversineDistanceKm(-33.8688, 151.2093, -37.8136, 144.9631);
    assertTest("Haversine Distance (Sydney to Melbourne ~713km)", distSydMel > 700.0 && distSydMel < 730.0);

    // 2. Identity distance
    double distZero = bushfire::GeoUtils::haversineDistanceKm(-33.8688, 151.2093, -33.8688, 151.2093);
    assertTest("Haversine Distance (Self Identity == 0km)", distZero < 0.001);

    // 3. RFS HTML description parser
    std::string testDesc = "ALERT LEVEL: Watch and Act <br /> LOCATION: Blue Mountains <br /> COUNCIL AREA: Blue Mountains <br /> STATUS: Out of control <br /> TYPE: Bush Fire <br /> FIRE: Yes <br /> SIZE: 12500 ha <br /> RESPONSIBLE AGENCY: Rural Fire Service <br /> UPDATED: 15 Jan 2020 14:00";
    auto parsed = bushfire::GeoUtils::parseRfsDescription(testDesc);
    assertTest("RFS HTML Parser: Alert Level", parsed.alertLevel == "Watch and Act");
    assertTest("RFS HTML Parser: Location", parsed.location == "Blue Mountains");
    assertTest("RFS HTML Parser: Size in Hectares", parsed.sizeHa.has_value() && *parsed.sizeHa == 12500.0);
    assertTest("RFS HTML Parser: Is Fire Flag", parsed.isFire == true);

    // 4. Similarity engine identity assertion
    auto histFires = bushfire::VerifiedSnapshots::getHistoricalFires();
    assertTest("Verified Snapshots: Historical records populated", !histFires.empty());

    if (!histFires.empty()) {
        const auto& gospers = histFires[0];
        auto match = bushfire::SimilarityEngine::evaluate(gospers, gospers);
        assertTest("Similarity Engine: Self-Identity Score == 100%", std::abs(match.totalScorePct - 100.0) < 0.01);
        assertTest("Similarity Engine: Geo Score Component == 1.0", std::abs(match.geoScore - 1.0) < 0.01);
        assertTest("Similarity Engine: Size Score Component == 1.0", std::abs(match.sizeScore - 1.0) < 0.01);
        assertTest("Similarity Engine: Season Score Component == 1.0", std::abs(match.seasonScore - 1.0) < 0.01);
        assertTest("Similarity Engine: Type Score Component == 1.0", std::abs(match.typeScore - 1.0) < 0.01);
    }

    // 5. Monotonic distance penalty
    double sGeoClose = bushfire::SimilarityEngine::computeGeoScore(10.0);
    double sGeoFar = bushfire::SimilarityEngine::computeGeoScore(500.0);
    assertTest("Similarity Engine: Monotonic distance decay", sGeoClose > sGeoFar);

    // 6. FIRMS CSV parsing
    std::string rawCsv = bushfire::VerifiedSnapshots::getRawFirmsCsvFixture();
    auto hotspots = bushfire::HotspotAdapter::parseFirmsCsv(rawCsv, bushfire::FreshnessStatus::FallbackSnapshot, 10);
    assertTest("Hotspot CSV Parser: Parsed rows", hotspots.size() == 2);
    if (!hotspots.empty()) {
        assertTest("Hotspot CSV Parser: FRP Extraction", hotspots[0].frpMw.has_value() && *hotspots[0].frpMw == 48.5);
        assertTest("Hotspot CSV Parser: Confidence", hotspots[0].confidence == "high");
    }

    // 7. GeoJSON Export validation
    auto incidents = bushfire::VerifiedSnapshots::getFallbackIncidents();
    std::string geojson = bushfire::ExportService::exportGeoJson(incidents, histFires, hotspots);
    auto gjRoot = nlohmann::json::parse(geojson, nullptr, false);
    assertTest("GeoJSON Export: Valid FeatureCollection", !gjRoot.is_discarded() && gjRoot["type"] == "FeatureCollection");

    std::cout << "\n[SELF-CHECK COMPLETE] " << passed << " passed, " << failed << " failed.\n";
    return (failed == 0) ? 0 : 1;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Bushfire Memory CLI (v3.0 C++20)\n"
                  << "Usage: bushfire_memory_cli [command]\n"
                  << "Commands:\n"
                  << "  --self-check               Run automated mathematical and parser verification\n"
                  << "  --status                   Display data feed status and cache health\n"
                  << "  --incidents                List active reported incidents\n"
                  << "  --history                  List indexed historical fire perimeters\n"
                  << "  --similar <event_id>       Find historically similar fire events\n"
                  << "  --export-report <file.md>  Generate Markdown research briefing\n"
                  << "  --export-geojson <file.json> Generate GeoJSON layer\n";
        return 0;
    }

    std::string cmd = argv[1];

    if (cmd == "--self-check") {
        return runSelfCheck();
    }

    bushfire::HttpClient::globalInit();
    auto http = std::make_shared<bushfire::HttpClient>();
    auto cache = std::make_shared<bushfire::CacheStore>("bushfire_cache.sqlite3");

    auto rfs = std::make_shared<bushfire::RfsIncidentAdapter>(http, cache);
    auto hotspots = std::make_shared<bushfire::HotspotAdapter>(http, cache);
    auto history = std::make_shared<bushfire::HistoricalFireAdapter>(http, cache);
    auto stac = std::make_shared<bushfire::StacImageryAdapter>(http, cache);

    if (cmd == "--status") {
        rfs->refresh(false);
        hotspots->refresh(false);
        history->refresh(false);
        stac->refresh(false);

        std::cout << "Provider Status Report:\n";
        for (const auto& p : {rfs->getStatus(), hotspots->getStatus(), history->getStatus(), stac->getStatus()}) {
            std::cout << "  - [" << bushfire::freshnessToString(p.status) << "] " 
                      << std::left << std::setw(28) << p.name 
                      << " Records: " << std::setw(5) << p.recordCount 
                      << " Latency: " << (p.latencyMs.has_value() ? std::to_string(*p.latencyMs) + "ms" : "N/A") << "\n";
        }
    } else if (cmd == "--incidents") {
        auto list = rfs->getIncidents();
        std::cout << "Reported Incidents (" << list.size() << "):\n";
        for (const auto& inc : list) {
            std::cout << "  - [" << inc.alertLevel << "] " << inc.title << " (" << inc.councilArea << ") - "
                      << inc.status << " - Size: " << (inc.sizeHa.has_value() ? std::to_string(static_cast<long>(*inc.sizeHa)) + " ha" : "Unknown") << "\n";
        }
    } else if (cmd == "--history") {
        auto list = history->getRecords();
        std::cout << "Historical Fire Perimeters (" << list.size() << "):\n";
        for (const auto& h : list) {
            std::cout << "  - [" << h.fireSeason << "] " << h.fireName << " (" << h.fireType << ") - "
                      << static_cast<long>(h.areaHa) << " ha - Bioregion: " << h.bioregionOrCouncil << "\n";
        }
    } else if (cmd == "--similar" && argc >= 3) {
        std::string id = argv[2];
        auto ref = history->getRecordById(id);
        if (!ref) {
            std::cerr << "Event ID not found in history: " << id << "\n";
            return 1;
        }

        std::cout << "Top Similar Events for: " << ref->fireName << " (" << ref->fireSeason << ", " << static_cast<long>(ref->areaHa) << " ha)\n";
        auto cands = history->getRecords();
        auto matches = bushfire::SimilarityEngine::findTopSimilar(*ref, cands, 5);
        for (size_t i = 0; i < matches.size(); ++i) {
            const auto& m = matches[i];
            std::cout << "  #" << (i + 1) << " " << std::left << std::setw(28) << m.record.fireName
                      << " [" << m.record.fireSeason << "] Match: " << std::fixed << std::setprecision(1) << m.totalScorePct << "%\n"
                      << "     " << m.explanation << "\n";
        }
    } else if (cmd == "--export-report" && argc >= 3) {
        std::string outPath = argv[2];
        auto incidents = rfs->getIncidents();
        auto historyFires = history->getRecords();
        auto hspots = hotspots->getHotspots();
        std::vector<bushfire::DataSourceStatus> statuses = {
            rfs->getStatus(), hotspots->getStatus(), history->getStatus(), stac->getStatus()
        };
        std::string md = bushfire::ExportService::exportMarkdownReport(incidents, historyFires, hspots, statuses);
        std::ofstream f(outPath);
        f << md;
        std::cout << "Exported Markdown report to: " << outPath << " (" << md.length() << " bytes)\n";
    } else if (cmd == "--export-geojson" && argc >= 3) {
        std::string outPath = argv[2];
        auto incidents = rfs->getIncidents();
        auto historyFires = history->getRecords();
        auto hspots = hotspots->getHotspots();
        std::string gj = bushfire::ExportService::exportGeoJson(incidents, historyFires, hspots);
        std::ofstream f(outPath);
        f << gj;
        std::cout << "Exported GeoJSON layer to: " << outPath << " (" << gj.length() << " bytes)\n";
    }

    bushfire::HttpClient::globalCleanup();
    return 0;
}
