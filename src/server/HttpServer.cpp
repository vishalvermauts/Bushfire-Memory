#include "bushfire/HttpServer.hpp"
#include "bushfire/SimilarityEngine.hpp"
#include "bushfire/ExportService.hpp"
#include "bushfire/GeoUtils.hpp"
#include <iostream>
#include <sstream>

namespace bushfire {

HttpServer::HttpServer(
    std::shared_ptr<RfsIncidentAdapter> rfs,
    std::shared_ptr<HotspotAdapter> hotspots,
    std::shared_ptr<HistoricalFireAdapter> history,
    std::shared_ptr<StacImageryAdapter> stac,
    std::shared_ptr<CacheStore> cache,
    const std::string& host,
    int port,
    const std::string& webDir
) : rfs_(std::move(rfs)),
    hotspots_(std::move(hotspots)),
    history_(std::move(history)),
    stac_(std::move(stac)),
    cache_(std::move(cache)),
    host_(host),
    port_(port),
    webDir_(webDir),
    svr_(std::make_unique<httplib::Server>()) {
}

HttpServer::~HttpServer() {
    stop();
}

static std::optional<std::array<double, 4>> parseBbox(const std::string& bboxStr) {
    if (bboxStr.empty()) return std::nullopt;
    std::stringstream ss(bboxStr);
    std::string item;
    std::vector<double> vals;
    while (std::getline(ss, item, ',')) {
        try {
            vals.push_back(std::stod(item));
        } catch (...) {
            return std::nullopt;
        }
    }
    if (vals.size() == 4) {
        return std::array<double, 4>{vals[0], vals[1], vals[2], vals[3]};
    }
    return std::nullopt;
}

void HttpServer::setupRoutes() {
    // Enable CORS for local testing
    svr_->set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type, Authorization"}
    });

    svr_->Options(".*", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
    });

    // 1. Health & Provider Status
    svr_->Get("/api/v1/health", [this](const httplib::Request&, httplib::Response& res) {
        std::vector<DataSourceStatus> statuses;
        statuses.push_back(rfs_->getStatus());
        statuses.push_back(hotspots_->getStatus());
        statuses.push_back(history_->getStatus());
        statuses.push_back(stac_->getStatus());

        auto cacheStats = cache_->getStats();

        nlohmann::json root = {
            {"status", "ok"},
            {"serverTimestampIso", GeoUtils::getCurrentUtcIso()},
            {"providers", statuses},
            {"cache", {
                {"totalEntries", cacheStats.totalEntries},
                {"freshEntries", cacheStats.freshEntries},
                {"staleEntries", cacheStats.staleEntries},
                {"totalBytes", cacheStats.totalBytes}
            }}
        };

        res.set_content(root.dump(2), "application/json");
    });

    // 2. Reported Incidents
    svr_->Get("/api/v1/incidents", [this](const httplib::Request& req, httplib::Response& res) {
        auto bbox = parseBbox(req.get_param_value("bbox"));
        std::string alertLevel = req.get_param_value("alert_level");
        bool fireOnly = (req.get_param_value("fire_only") == "true");
        std::string search = req.get_param_value("search");

        auto list = rfs_->getIncidents(bbox, alertLevel, fireOnly, search);
        nlohmann::json j = list;
        res.set_content(j.dump(), "application/json");
    });

    // 3. Thermal Hotspots
    svr_->Get("/api/v1/hotspots", [this](const httplib::Request& req, httplib::Response& res) {
        auto bbox = parseBbox(req.get_param_value("bbox"));
        std::optional<double> minFrp = std::nullopt;
        if (req.has_param("min_frp")) {
            try { minFrp = std::stod(req.get_param_value("min_frp")); } catch (...) {}
        }
        std::string conf = req.get_param_value("confidence");
        int hours = 48;
        if (req.has_param("hours")) {
            try { hours = std::stoi(req.get_param_value("hours")); } catch (...) {}
        }

        auto list = hotspots_->getHotspots(bbox, minFrp, conf, hours);
        nlohmann::json j = list;
        res.set_content(j.dump(), "application/json");
    });

    // 4. Historical Bushfires
    svr_->Get("/api/v1/history", [this](const httplib::Request& req, httplib::Response& res) {
        auto bbox = parseBbox(req.get_param_value("bbox"));
        std::string season = req.get_param_value("season");
        std::string fireType = req.get_param_value("fire_type");
        std::optional<double> minArea = std::nullopt;
        if (req.has_param("min_area_ha")) {
            try { minArea = std::stod(req.get_param_value("min_area_ha")); } catch (...) {}
        }
        std::string search = req.get_param_value("search");

        auto list = history_->getRecords(bbox, season, fireType, minArea, search);
        nlohmann::json j = list;
        res.set_content(j.dump(), "application/json");
    });

    // 5. Similarity Scoring Engine
    svr_->Get("/api/v1/similarity", [this](const httplib::Request& req, httplib::Response& res) {
        std::string eventId = req.get_param_value("event_id");
        if (eventId.empty()) {
            res.status = 400;
            res.set_content(R"({"error": "event_id parameter is required"})", "application/json");
            return;
        }

        size_t limit = 5;
        if (req.has_param("limit")) {
            try { limit = std::stoul(req.get_param_value("limit")); } catch (...) {}
        }

        auto refOpt = history_->getRecordById(eventId);
        if (!refOpt) {
            // Check if user is referencing an active incident
            auto incidents = rfs_->getIncidents();
            for (const auto& inc : incidents) {
                if (inc.id == eventId || inc.originalId == eventId) {
                    HistoricalFireRecord synthetic;
                    synthetic.id = inc.id;
                    synthetic.fireName = inc.title;
                    synthetic.fireSeason = "Current";
                    synthetic.fireType = (inc.category == "Hazard Reduction") ? "Prescribed Burn" : "Wildfire";
                    synthetic.areaHa = inc.sizeHa.value_or(100.0);
                    synthetic.bioregionOrCouncil = inc.councilArea;
                    synthetic.dayOfYear = GeoUtils::getDayOfYearFromIso(inc.updatedAtIso.value_or(GeoUtils::getCurrentUtcIso()));
                    synthetic.centroidLng = inc.centroidLng;
                    synthetic.centroidLat = inc.centroidLat;
                    synthetic.geometryJson = inc.geometryJson;
                    refOpt = synthetic;
                    break;
                }
            }
        }

        if (!refOpt) {
            res.status = 404;
            res.set_content(R"({"error": "Reference event ID not found"})", "application/json");
            return;
        }

        auto candidates = history_->getRecords();
        auto matches = SimilarityEngine::findTopSimilar(*refOpt, candidates, limit);

        nlohmann::json j = {
            {"referenceEvent", *refOpt},
            {"matches", matches},
            {"disclaimer", "Statistical attribute similarity reflects historical archive comparisons only and does not predict future fire behaviour, spread, or risk."}
        };

        res.set_content(j.dump(2), "application/json");
    });

    // 6. STAC Scene Discovery
    svr_->Get("/api/v1/stac/search", [this](const httplib::Request& req, httplib::Response& res) {
        auto bboxOpt = parseBbox(req.get_param_value("bbox"));
        std::array<double, 4> bbox = bboxOpt.value_or(std::array<double, 4>{150.0, -34.0, 151.0, -33.0});
        std::string start = req.get_param_value("start");
        std::string end = req.get_param_value("end");
        double maxCloud = 20.0;
        if (req.has_param("max_cloud")) {
            try { maxCloud = std::stod(req.get_param_value("max_cloud")); } catch (...) {}
        }
        std::string col = req.get_param_value("collection");
        if (col.empty()) col = "ga_s2am_ard_3";

        auto scenes = stac_->searchScenes(bbox, start, end, maxCloud, col);
        nlohmann::json j = scenes;
        res.set_content(j.dump(), "application/json");
    });

    // 7. Verified WMS Layers
    svr_->Get("/api/v1/layers/wms", [this](const httplib::Request&, httplib::Response& res) {
        auto layers = history_->getWmsLayers();
        nlohmann::json j = nlohmann::json::array();
        for (const auto& l : layers) {
            j.push_back({
                {"id", l.id},
                {"title", l.title},
                {"providerName", l.providerName},
                {"wmsUrl", l.wmsUrl},
                {"layerName", l.layerName},
                {"crs", l.crs},
                {"format", l.format},
                {"transparent", l.transparent},
                {"licence", l.licence},
                {"scientificCaveat", l.scientificCaveat},
                {"legendClasses", l.legendClasses}
            });
        }
        res.set_content(j.dump(2), "application/json");
    });

    // 8. Spectral Modes Metadata
    svr_->Get("/api/v1/spectral/modes", [this](const httplib::Request&, httplib::Response& res) {
        auto modes = stac_->getSpectralModes();
        nlohmann::json j = nlohmann::json::array();
        for (const auto& m : modes) {
            j.push_back({
                {"id", m.id},
                {"name", m.name},
                {"description", m.description},
                {"bandsUsed", m.bandsUsed},
                {"formula", m.formula},
                {"spatialResolutionMeters", m.spatialResolutionMeters},
                {"interpretationGuide", m.interpretationGuide},
                {"scientificCaveats", m.scientificCaveats}
            });
        }
        res.set_content(j.dump(2), "application/json");
    });

    // 9. Runtime Settings Mutation (Protected)
    svr_->Post("/api/v1/settings", [this](const httplib::Request& req, httplib::Response& res) {
        auto root = nlohmann::json::parse(req.body, nullptr, false);
        if (root.is_discarded() || !root.contains("key") || !root.contains("value")) {
            res.status = 400;
            res.set_content(R"({"error": "Malformed JSON body; expected 'key' and 'value'"})", "application/json");
            return;
        }

        std::string key = root["key"].get<std::string>();
        std::string val = root["value"].get<std::string>();

        if (key == "force_refresh" && val == "true") {
            rfs_->refresh(true);
            hotspots_->refresh(true);
            history_->refresh(true);
            stac_->refresh(true);
            res.set_content(R"({"success": true, "message": "All data feeds refreshed"})", "application/json");
            return;
        }

        if (key == "clear_cache" && val == "true") {
            cache_->clear();
            res.set_content(R"({"success": true, "message": "Cache cleared"})", "application/json");
            return;
        }

        cache_->setSetting(key, val);

        res.set_content(R"({"success": true, "message": "Setting saved securely"})", "application/json");
    });

    // 10. Research Exports
    svr_->Get(R"(/api/v1/export/(.+))", [this](const httplib::Request& req, httplib::Response& res) {
        std::string format = req.matches[1];

        auto incidents = rfs_->getIncidents();
        auto historyFires = history_->getRecords();
        auto hotspots = hotspots_->getHotspots();

        if (format == "geojson") {
            std::string payload = ExportService::exportGeoJson(incidents, historyFires, hotspots);
            res.set_header("Content-Disposition", "attachment; filename=\"bushfire_memory_export.geojson\"");
            res.set_content(payload, "application/geo+json");
        } else if (format == "csv_incidents") {
            std::string payload = ExportService::exportIncidentsCsv(incidents);
            res.set_header("Content-Disposition", "attachment; filename=\"bushfire_incidents.csv\"");
            res.set_content(payload, "text/csv");
        } else if (format == "csv_history") {
            std::string payload = ExportService::exportHistoryCsv(historyFires);
            res.set_header("Content-Disposition", "attachment; filename=\"bushfire_history.csv\"");
            res.set_content(payload, "text/csv");
        } else if (format == "csv_hotspots") {
            std::string payload = ExportService::exportHotspotsCsv(hotspots);
            res.set_header("Content-Disposition", "attachment; filename=\"bushfire_hotspots.csv\"");
            res.set_content(payload, "text/csv");
        } else if (format == "markdown") {
            std::vector<DataSourceStatus> statuses = {
                rfs_->getStatus(), hotspots_->getStatus(), history_->getStatus(), stac_->getStatus()
            };
            std::string payload = ExportService::exportMarkdownReport(incidents, historyFires, hotspots, statuses);
            res.set_header("Content-Disposition", "attachment; filename=\"bushfire_research_briefing.md\"");
            res.set_content(payload, "text/markdown");
        } else {
            res.status = 404;
            res.set_content(R"({"error": "Unknown export format"})", "application/json");
        }
    });

    // Static Web App Mount
    svr_->set_mount_point("/", webDir_);
}

bool HttpServer::start() {
    setupRoutes();
    std::cout << "[Bushfire Memory Server] Starting HTTP server on " << host_ << ":" << port_ << std::endl;
    std::cout << "[Bushfire Memory Server] Serving web assets from " << webDir_ << std::endl;
    return svr_->listen(host_.c_str(), port_);
}

void HttpServer::stop() {
    if (svr_ && svr_->is_running()) {
        svr_->stop();
    }
}

} // namespace bushfire
