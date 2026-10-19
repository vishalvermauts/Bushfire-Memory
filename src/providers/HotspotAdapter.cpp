#include "bushfire/providers/HotspotAdapter.hpp"
#include "bushfire/GeoUtils.hpp"
#include "bushfire/VerifiedSnapshots.hpp"
#include <chrono>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace bushfire {

static const std::string DEA_WFS_URL = "https://hotspots.dea.ga.gov.au/geoserver/wfs?service=WFS&version=1.1.0&request=GetFeature&typeName=public:hotspots&maxFeatures=200&outputFormat=application/json";
static const int CACHE_TTL_SEC = 300; // 5 minutes

HotspotAdapter::HotspotAdapter(std::shared_ptr<HttpClient> http, std::shared_ptr<CacheStore> cache)
    : http_(std::move(http)), cache_(std::move(cache)) {
    status_.providerId = "dea-hotspots";
    status_.name = "DEA Hotspots & NASA FIRMS";
    status_.endpointUrl = DEA_WFS_URL;
    status_.docsUrl = "https://knowledge.dea.ga.gov.au/data/product/dea-hotspots/";
    status_.licence = "CC BY 4.0 / NASA Open Data";
    status_.status = FreshnessStatus::FallbackSnapshot;
    status_.requiresKey = false;
    status_.keyConfigured = false;
}

DataSourceStatus HotspotAdapter::getStatus() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto keyOpt = cache_->getSetting("NASA_FIRMS_MAP_KEY");
    status_.keyConfigured = (keyOpt.has_value() && !keyOpt->empty());
    return status_;
}

void HotspotAdapter::refresh(bool force) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::system_clock::now();
    long nowEpoch = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    if (!force && lastFetchEpoch_ > 0 && (nowEpoch - lastFetchEpoch_) < CACHE_TTL_SEC) {
        return;
    }

    lastFetchEpoch_ = nowEpoch;

    // Check if user has an optional NASA FIRMS key
    auto firmsKeyOpt = cache_->getSetting("NASA_FIRMS_MAP_KEY");
    bool hasFirmsKey = (firmsKeyOpt.has_value() && !firmsKeyOpt->empty());
    status_.keyConfigured = hasFirmsKey;

    std::string cachedKey = "hotspots:feed";
    auto cached = cache_->get(cachedKey);

    if (hasFirmsKey) {
        // Query NASA FIRMS CSV for Australia (1 day)
        std::string firmsUrl = "https://firms.modaps.eosdis.nasa.gov/api/area/csv/" + 
                               *firmsKeyOpt + "/VIIRS_SNPP_NRT/112,-44,154,-10/1";
        HttpResponse resp = http_->get(firmsUrl, 12);
        if (resp.isSuccess() && !resp.body.empty() && resp.body.find("latitude") != std::string::npos) {
            auto parsed = parseFirmsCsv(resp.body, FreshnessStatus::Live, resp.latencyMs);
            if (!parsed.empty()) {
                hotspots_ = std::move(parsed);
                status_.status = FreshnessStatus::Live;
                status_.latencyMs = resp.latencyMs;
                status_.lastSuccessAtIso = GeoUtils::getCurrentUtcIso();
                status_.recordCount = hotspots_.size();
                status_.errorMessage = std::nullopt;
                cache_->set(cachedKey, resp.body, CACHE_TTL_SEC);
                return;
            }
        }
    }

    // Default: Query DEA Hotspots WFS
    HttpResponse resp = http_->get(DEA_WFS_URL, 5);
    if (resp.isSuccess() && !resp.body.empty()) {
        auto parsed = parseDeaHotspotsJson(resp.body, FreshnessStatus::Live, resp.latencyMs);
        if (!parsed.empty()) {
            hotspots_ = std::move(parsed);
            status_.status = FreshnessStatus::Live;
            status_.latencyMs = resp.latencyMs;
            status_.lastSuccessAtIso = GeoUtils::getCurrentUtcIso();
            status_.recordCount = hotspots_.size();
            status_.errorMessage = std::nullopt;
            cache_->set(cachedKey, resp.body, CACHE_TTL_SEC);
            return;
        }
    }

    // Tier 2: Local Cache
    if (cached && !cached->payload.empty()) {
        std::vector<HotspotDetection> parsed;
        if (cached->payload.find("FeatureCollection") != std::string::npos) {
            parsed = parseDeaHotspotsJson(cached->payload, cached->freshness, 0);
        } else {
            parsed = parseFirmsCsv(cached->payload, cached->freshness, 0);
        }

        if (!parsed.empty()) {
            hotspots_ = std::move(parsed);
            status_.status = cached->freshness;
            status_.recordCount = hotspots_.size();
            status_.errorMessage = "Using local SQLite cache due to upstream network failure.";
            return;
        }
    }

    // Tier 3: Verified Historical Fallback
    hotspots_ = VerifiedSnapshots::getFallbackHotspots();
    status_.status = FreshnessStatus::FallbackSnapshot;
    status_.recordCount = hotspots_.size();
    status_.errorMessage = "Upstream unreachable and cache empty; loaded verified historical fallback snapshot.";
}

std::vector<HotspotDetection> HotspotAdapter::parseFirmsCsv(const std::string& csvStr, FreshnessStatus freshness, long latencyMs) {
    std::vector<HotspotDetection> results;
    std::istringstream stream(csvStr);
    std::string line;

    SourceProvenance prov{
        "nasa-firms",
        "NASA EOSDIS FIRMS",
        "NASA FIRMS Active Fire VIIRS/MODIS Detections",
        "https://firms.modaps.eosdis.nasa.gov",
        "https://firms.modaps.eosdis.nasa.gov/api/data_availability/",
        "NASA Open Data",
        std::nullopt,
        GeoUtils::getCurrentUtcIso(),
        freshness,
        "Satellite thermal anomaly detection. Does not confirm ground bushfire activity without field validation."
    };

    if (!std::getline(stream, line)) return results;

    // Header index mapping
    std::vector<std::string> headers;
    std::istringstream hStream(line);
    std::string col;
    while (std::getline(hStream, col, ',')) {
        col.erase(std::remove_if(col.begin(), col.end(), ::isspace), col.end());
        headers.push_back(col);
    }

    int idxLat = -1, idxLng = -1, idxDate = -1, idxTime = -1, idxFrp = -1;
    int idxConf = -1, idxSat = -1, idxInst = -1, idxBright = -1, idxDn = -1;

    for (size_t i = 0; i < headers.size(); ++i) {
        if (headers[i] == "latitude") idxLat = i;
        else if (headers[i] == "longitude") idxLng = i;
        else if (headers[i] == "acq_date") idxDate = i;
        else if (headers[i] == "acq_time") idxTime = i;
        else if (headers[i] == "frp") idxFrp = i;
        else if (headers[i] == "confidence") idxConf = i;
        else if (headers[i] == "satellite") idxSat = i;
        else if (headers[i] == "instrument") idxInst = i;
        else if (headers[i] == "bright_ti4" || headers[i] == "brightness") idxBright = i;
        else if (headers[i] == "daynight") idxDn = i;
    }

    if (idxLat == -1 || idxLng == -1) return results;

    int counter = 1;
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        std::istringstream lStream(line);
        std::vector<std::string> values;
        std::string val;
        while (std::getline(lStream, val, ',')) {
            values.push_back(val);
        }

        if (values.size() <= static_cast<size_t>(std::max(idxLat, idxLng))) continue;

        try {
            HotspotDetection h;
            h.id = "FIRMS-" + std::to_string(counter++);
            h.latitude = std::stod(values[idxLat]);
            h.longitude = std::stod(values[idxLng]);

            if (idxDate != -1 && static_cast<size_t>(idxDate) < values.size()) {
                std::string d = values[idxDate];
                std::string t = (idxTime != -1 && static_cast<size_t>(idxTime) < values.size()) ? values[idxTime] : "0000";
                if (t.length() == 4) {
                    h.acquiredAtIso = d + "T" + t.substr(0, 2) + ":" + t.substr(2, 2) + ":00Z";
                } else {
                    h.acquiredAtIso = d + "T00:00:00Z";
                }
            } else {
                h.acquiredAtIso = GeoUtils::getCurrentUtcIso();
            }

            if (idxSat != -1 && static_cast<size_t>(idxSat) < values.size()) h.satellite = values[idxSat];
            if (idxInst != -1 && static_cast<size_t>(idxInst) < values.size()) h.instrument = values[idxInst];
            if (idxConf != -1 && static_cast<size_t>(idxConf) < values.size()) h.confidence = values[idxConf];
            if (idxDn != -1 && static_cast<size_t>(idxDn) < values.size()) h.dayNight = values[idxDn];

            if (idxFrp != -1 && static_cast<size_t>(idxFrp) < values.size() && !values[idxFrp].empty()) {
                h.frpMw = std::stod(values[idxFrp]);
            }
            if (idxBright != -1 && static_cast<size_t>(idxBright) < values.size() && !values[idxBright].empty()) {
                h.brightnessK = std::stod(values[idxBright]);
            }

            h.provenance = prov;
            results.push_back(h);
        } catch (...) {
            // Skip malformed row
        }
    }

    return results;
}

std::vector<HotspotDetection> HotspotAdapter::parseDeaHotspotsJson(const std::string& jsonStr, FreshnessStatus freshness, long latencyMs) {
    std::vector<HotspotDetection> results;
    auto root = nlohmann::json::parse(jsonStr, nullptr, false);
    if (root.is_discarded() || !root.is_object() || !root.contains("features") || !root["features"].is_array()) {
        return results;
    }

    SourceProvenance prov{
        "dea-hotspots",
        "Geoscience Australia",
        "Digital Earth Australia Hotspots",
        "https://hotspots.dea.ga.gov.au/geoserver/wfs",
        "https://knowledge.dea.ga.gov.au/data/product/dea-hotspots/",
        "CC BY 4.0",
        std::nullopt,
        GeoUtils::getCurrentUtcIso(),
        freshness,
        "Satellite thermal anomaly detection. Does not confirm ground bushfire activity without field validation."
    };

    int counter = 1;
    for (const auto& feat : root["features"]) {
        if (!feat.is_object() || !feat.contains("properties") || !feat.contains("geometry")) continue;
        const auto& geom = feat["geometry"];
        const auto& props = feat["properties"];

        if (!geom.contains("coordinates") || !geom["coordinates"].is_array() || geom["coordinates"].size() < 2) continue;

        HotspotDetection h;
        h.id = "DEA-" + std::to_string(counter++);
        h.longitude = geom["coordinates"][0].get<double>();
        h.latitude = geom["coordinates"][1].get<double>();

        h.acquiredAtIso = props.value("datetime", props.value("date", GeoUtils::getCurrentUtcIso()));
        h.satellite = props.value("satellite", "VIIRS/MODIS");
        h.instrument = props.value("sensor", "Thermal Infrared");
        h.confidence = props.value("confidence", "nominal");
        h.dayNight = props.value("daynight", "D");

        if (props.contains("frp") && !props["frp"].is_null()) {
            h.frpMw = props["frp"].get<double>();
        }
        if (props.contains("temp_kelvin") && !props["temp_kelvin"].is_null()) {
            h.brightnessK = props["temp_kelvin"].get<double>();
        }

        h.provenance = prov;
        results.push_back(h);
    }

    return results;
}

std::vector<HotspotDetection> HotspotAdapter::getHotspots(
    const std::optional<std::array<double, 4>>& bbox,
    std::optional<double> minFrp,
    const std::string& confidence,
    int maxHours) {

    std::lock_guard<std::mutex> lock(mutex_);
    if (hotspots_.empty()) {
        refresh(false);
    }

    std::vector<HotspotDetection> filtered;
    filtered.reserve(hotspots_.size());

    for (const auto& h : hotspots_) {
        if (bbox.has_value() && !GeoUtils::pointInBbox(h.longitude, h.latitude, *bbox)) {
            continue;
        }

        if (minFrp.has_value() && (!h.frpMw.has_value() || *h.frpMw < *minFrp)) {
            continue;
        }

        if (!confidence.empty() && h.confidence != confidence) {
            continue;
        }

        filtered.push_back(h);
    }

    return filtered;
}

} // namespace bushfire
