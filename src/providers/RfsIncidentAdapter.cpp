#include "bushfire/providers/RfsIncidentAdapter.hpp"
#include "bushfire/GeoUtils.hpp"
#include "bushfire/VerifiedSnapshots.hpp"
#include <chrono>
#include <algorithm>
#include <iostream>

namespace bushfire {

static const std::string RFS_URL = "https://www.rfs.nsw.gov.au/feeds/majorIncidents.json";
static const int CACHE_TTL_SEC = 300; // 5 minutes

RfsIncidentAdapter::RfsIncidentAdapter(std::shared_ptr<HttpClient> http, std::shared_ptr<CacheStore> cache)
    : http_(std::move(http)), cache_(std::move(cache)) {
    status_.providerId = "nsw-rfs-incidents";
    status_.name = "NSW Rural Fire Service";
    status_.endpointUrl = RFS_URL;
    status_.docsUrl = "https://data.nsw.gov.au/data/dataset/nsw-rural-fire-service-current-incidents-feed";
    status_.licence = "CC BY 4.0";
    status_.status = FreshnessStatus::FallbackSnapshot;
    status_.requiresKey = false;
    status_.keyConfigured = false;
    incidents_ = VerifiedSnapshots::getFallbackIncidents();
    status_.recordCount = incidents_.size();
}

DataSourceStatus RfsIncidentAdapter::getStatus() {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
}

void RfsIncidentAdapter::refresh(bool force) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::system_clock::now();
    long nowEpoch = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    // Respectful rate limiting: avoid polling faster than 300s unless forced
    if (!force && lastFetchEpoch_ > 0 && (nowEpoch - lastFetchEpoch_) < CACHE_TTL_SEC) {
        return;
    }

    lastFetchEpoch_ = nowEpoch;

    // Check cached ETag
    std::string etag, lastModified;
    auto cached = cache_->get("rfs:incidents");
    if (cached) {
        etag = cached->etag;
        lastModified = cached->lastModified;
    }

    // Tier 1: Live HTTP Fetch
    HttpResponse resp = http_->get(RFS_URL, 10, etag, lastModified);

    if (resp.isSuccess() && !resp.body.empty()) {
        auto parsed = parseGeoJson(resp.body, FreshnessStatus::Live, resp.latencyMs);
        if (!parsed.empty()) {
            incidents_ = std::move(parsed);
            status_.status = FreshnessStatus::Live;
            status_.latencyMs = resp.latencyMs;
            status_.lastSuccessAtIso = GeoUtils::getCurrentUtcIso();
            status_.recordCount = incidents_.size();
            status_.errorMessage = std::nullopt;

            cache_->set("rfs:incidents", resp.body, CACHE_TTL_SEC, resp.etag, resp.lastModified);
            return;
        }
    } else if (resp.isNotModified() && cached) {
        // Cache is still fresh on upstream
        auto parsed = parseGeoJson(cached->payload, FreshnessStatus::CachedFresh, resp.latencyMs);
        if (!parsed.empty()) {
            incidents_ = std::move(parsed);
            status_.status = FreshnessStatus::CachedFresh;
            status_.latencyMs = resp.latencyMs;
            status_.lastSuccessAtIso = GeoUtils::getCurrentUtcIso();
            status_.recordCount = incidents_.size();
            return;
        }
    }

    // Tier 2: SQLite Stale Cache Fallback
    if (cached && !cached->payload.empty()) {
        auto parsed = parseGeoJson(cached->payload, cached->freshness, 0);
        if (!parsed.empty()) {
            incidents_ = std::move(parsed);
            status_.status = cached->freshness;
            status_.recordCount = incidents_.size();
            status_.errorMessage = "Using local SQLite cache due to upstream fetch failure: " + resp.errorMessage;
            return;
        }
    }

    // Tier 3: Bundled Verified Historical Snapshot Fallback
    incidents_ = VerifiedSnapshots::getFallbackIncidents();
    status_.status = FreshnessStatus::FallbackSnapshot;
    status_.recordCount = incidents_.size();
    status_.errorMessage = "Upstream unreachable and cache empty; loaded verified historical fallback snapshot.";
}

std::vector<UnifiedIncident> RfsIncidentAdapter::parseGeoJson(const std::string& jsonStr, FreshnessStatus freshness, long latencyMs) {
    std::vector<UnifiedIncident> results;

    auto root = nlohmann::json::parse(jsonStr, nullptr, false);
    if (root.is_discarded() || !root.is_object() || !root.contains("features") || !root["features"].is_array()) {
        return results;
    }

    SourceProvenance prov{
        "nsw-rfs-incidents",
        "NSW Rural Fire Service",
        "NSW RFS Current Incidents Feed",
        RFS_URL,
        "https://data.nsw.gov.au",
        "CC BY 4.0",
        std::nullopt,
        GeoUtils::getCurrentUtcIso(),
        freshness,
        "Official reported NSW bushfire incidents. Geometry from RFS feed."
    };

    int index = 1;
    for (const auto& feat : root["features"]) {
        if (!feat.is_object() || !feat.contains("properties") || !feat.contains("geometry")) continue;

        const auto& props = feat["properties"];
        const auto& geom = feat["geometry"];

        UnifiedIncident inc;
        inc.originalId = props.value("guid", "RFS-" + std::to_string(index));
        inc.id = "RFS-" + std::to_string(index++);
        inc.title = props.value("title", "Bush Fire Incident");

        // Parse HTML description into structured fields
        std::string rawDesc = props.value("description", "");
        auto parsedDesc = GeoUtils::parseRfsDescription(rawDesc);

        inc.alertLevel = parsedDesc.alertLevel.empty() ? props.value("category", "Advice") : parsedDesc.alertLevel;
        inc.status = parsedDesc.status;
        inc.category = parsedDesc.type;
        inc.councilArea = parsedDesc.councilArea;
        inc.responsibleAgency = parsedDesc.responsibleAgency;
        inc.sizeHa = parsedDesc.sizeHa;
        inc.isFire = parsedDesc.isFire;
        inc.updatedAtRaw = parsedDesc.updatedRaw;
        inc.geometryJson = geom;

        auto centroid = GeoUtils::computeCentroid(geom);
        inc.centroidLng = centroid.first;
        inc.centroidLat = centroid.second;

        inc.provenance = prov;
        results.push_back(inc);
    }

    return results;
}

std::vector<UnifiedIncident> RfsIncidentAdapter::getIncidents(
    const std::optional<std::array<double, 4>>& bbox,
    const std::string& alertLevel,
    bool fireOnly,
    const std::string& searchQuery) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    if (incidents_.empty()) {
        refresh(false);
    }

    std::vector<UnifiedIncident> filtered;
    filtered.reserve(incidents_.size());

    std::string searchLower = searchQuery;
    std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

    for (const auto& inc : incidents_) {
        if (fireOnly && !inc.isFire) continue;

        if (!alertLevel.empty() && inc.alertLevel != alertLevel) continue;

        if (bbox.has_value() && !GeoUtils::pointInBbox(inc.centroidLng, inc.centroidLat, *bbox)) {
            continue;
        }

        if (!searchLower.empty()) {
            std::string t = inc.title;
            std::string c = inc.councilArea;
            std::transform(t.begin(), t.end(), t.begin(), ::tolower);
            std::transform(c.begin(), c.end(), c.begin(), ::tolower);
            if (t.find(searchLower) == std::string::npos && c.find(searchLower) == std::string::npos) {
                continue;
            }
        }

        filtered.push_back(inc);
    }

    return filtered;
}

} // namespace bushfire
