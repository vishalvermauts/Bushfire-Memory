#pragma once

#include <string>
#include <vector>
#include <array>
#include <optional>
#include <nlohmann/json.hpp>

namespace bushfire {

enum class FreshnessStatus {
    Live,
    CachedFresh,
    StaleCache,
    FallbackSnapshot,
    Error
};

inline std::string freshnessToString(FreshnessStatus status) {
    switch (status) {
        case FreshnessStatus::Live: return "live";
        case FreshnessStatus::CachedFresh: return "cached-fresh";
        case FreshnessStatus::StaleCache: return "stale-cache";
        case FreshnessStatus::FallbackSnapshot: return "fallback-snapshot";
        case FreshnessStatus::Error: return "error";
    }
    return "unknown";
}

inline FreshnessStatus stringToFreshness(const std::string& str) {
    if (str == "live") return FreshnessStatus::Live;
    if (str == "cached-fresh") return FreshnessStatus::CachedFresh;
    if (str == "stale-cache") return FreshnessStatus::StaleCache;
    if (str == "fallback-snapshot") return FreshnessStatus::FallbackSnapshot;
    return FreshnessStatus::Error;
}

struct SourceProvenance {
    std::string providerId;
    std::string providerName;
    std::string datasetTitle;
    std::string sourceUrl;
    std::string documentationUrl;
    std::string licence;
    std::optional<std::string> sourcePublishedAtIso;
    std::string fetchedAtIso;
    FreshnessStatus freshness{FreshnessStatus::FallbackSnapshot};
    std::string caveatNote;
};

struct DataSourceStatus {
    std::string providerId;
    std::string name;
    std::string endpointUrl;
    std::string docsUrl;
    std::string licence;
    FreshnessStatus status{FreshnessStatus::FallbackSnapshot};
    std::optional<std::string> lastSuccessAtIso;
    std::optional<std::string> sourceUpdatedAtIso;
    std::optional<long> latencyMs;
    std::size_t recordCount{0};
    std::optional<std::string> errorMessage;
    bool requiresKey{false};
    bool keyConfigured{false};
};

struct UnifiedIncident {
    std::string id;
    std::string originalId;
    std::string title;
    std::string alertLevel{"Not Applicable"}; // Emergency Warning, Watch and Act, Advice, Not Applicable
    std::string status{"Under control"};
    std::string category{"Bush Fire"};
    std::string councilArea;
    std::string responsibleAgency{"NSW Rural Fire Service"};
    std::optional<double> sizeHa;
    bool isFire{true};
    double centroidLng{0.0};
    double centroidLat{0.0};
    nlohmann::json geometryJson;
    std::optional<std::string> updatedAtIso;
    std::optional<std::string> updatedAtRaw;
    SourceProvenance provenance;
};

struct HotspotDetection {
    std::string id;
    double latitude{0.0};
    double longitude{0.0};
    std::string acquiredAtIso;
    std::string satellite;     // e.g. 'VIIRS SNPP', 'VIIRS NOAA-20', 'MODIS'
    std::string instrument;    // e.g. 'VIIRS', 'MODIS'
    std::string confidence;    // e.g. 'low', 'nominal', 'high'
    std::optional<double> frpMw;
    std::optional<double> brightnessK;
    std::string dayNight{"Unknown"}; // 'D', 'N', 'Unknown'
    SourceProvenance provenance;
};

struct HistoricalFireRecord {
    std::string id;
    std::string fireName;
    std::string fireSeason;    // e.g. '2019-20'
    std::optional<std::string> ignitionDateIso;
    std::optional<std::string> extinguishedDateIso;
    std::string fireType{"Wildfire"}; // 'Wildfire', 'Prescribed Burn', 'Other'
    double areaHa{0.0};
    std::string bioregionOrCouncil;
    int dayOfYear{1};
    double centroidLng{0.0};
    double centroidLat{0.0};
    std::array<double, 4> bbox{0.0, 0.0, 0.0, 0.0}; // [minLng, minLat, maxLng, maxLat]
    nlohmann::json geometryJson;
    SourceProvenance provenance;
};

struct SimilarityMatch {
    HistoricalFireRecord record;
    double totalScorePct{0.0};
    double geoScore{0.0};
    double sizeScore{0.0};
    double seasonScore{0.0};
    double typeScore{0.0};
    double distanceKm{0.0};
    std::string explanation;
};

struct SatelliteSceneItem {
    std::string id;
    std::string collection;
    std::string platform;      // e.g. 'Sentinel-2A', 'Sentinel-2B'
    std::string acquiredAtIso;
    std::optional<double> cloudCoverPct;
    std::array<double, 4> bbox{0.0, 0.0, 0.0, 0.0};
    nlohmann::json geometryJson;
    std::optional<std::string> previewUrl;
    std::string wmsLayerName;
    std::string stacSelfUrl;
    double gsdMeters{10.0};
    SourceProvenance provenance;
};

// JSON Serializers
inline void to_json(nlohmann::json& j, const SourceProvenance& p) {
    j = nlohmann::json{
        {"providerId", p.providerId},
        {"providerName", p.providerName},
        {"datasetTitle", p.datasetTitle},
        {"sourceUrl", p.sourceUrl},
        {"documentationUrl", p.documentationUrl},
        {"licence", p.licence},
        {"sourcePublishedAtIso", p.sourcePublishedAtIso.has_value() ? nlohmann::json(*p.sourcePublishedAtIso) : nlohmann::json(nullptr)},
        {"fetchedAtIso", p.fetchedAtIso},
        {"freshness", freshnessToString(p.freshness)},
        {"caveatNote", p.caveatNote}
    };
}

inline void from_json(const nlohmann::json& j, SourceProvenance& p) {
    p.providerId = j.value("providerId", "");
    p.providerName = j.value("providerName", "");
    p.datasetTitle = j.value("datasetTitle", "");
    p.sourceUrl = j.value("sourceUrl", "");
    p.documentationUrl = j.value("documentationUrl", "");
    p.licence = j.value("licence", "");
    if (j.contains("sourcePublishedAtIso") && !j["sourcePublishedAtIso"].is_null()) {
        p.sourcePublishedAtIso = j["sourcePublishedAtIso"].get<std::string>();
    }
    p.fetchedAtIso = j.value("fetchedAtIso", "");
    p.freshness = stringToFreshness(j.value("freshness", "fallback-snapshot"));
    p.caveatNote = j.value("caveatNote", "");
}

inline void to_json(nlohmann::json& j, const DataSourceStatus& s) {
    j = nlohmann::json{
        {"providerId", s.providerId},
        {"name", s.name},
        {"endpointUrl", s.endpointUrl},
        {"docsUrl", s.docsUrl},
        {"licence", s.licence},
        {"status", freshnessToString(s.status)},
        {"lastSuccessAtIso", s.lastSuccessAtIso.has_value() ? nlohmann::json(*s.lastSuccessAtIso) : nlohmann::json(nullptr)},
        {"sourceUpdatedAtIso", s.sourceUpdatedAtIso.has_value() ? nlohmann::json(*s.sourceUpdatedAtIso) : nlohmann::json(nullptr)},
        {"latencyMs", s.latencyMs.has_value() ? nlohmann::json(*s.latencyMs) : nlohmann::json(nullptr)},
        {"recordCount", s.recordCount},
        {"errorMessage", s.errorMessage.has_value() ? nlohmann::json(*s.errorMessage) : nlohmann::json(nullptr)},
        {"requiresKey", s.requiresKey},
        {"keyConfigured", s.keyConfigured}
    };
}

inline void to_json(nlohmann::json& j, const UnifiedIncident& inc) {
    j = nlohmann::json{
        {"id", inc.id},
        {"originalId", inc.originalId},
        {"title", inc.title},
        {"alertLevel", inc.alertLevel},
        {"status", inc.status},
        {"category", inc.category},
        {"councilArea", inc.councilArea},
        {"responsibleAgency", inc.responsibleAgency},
        {"sizeHa", inc.sizeHa.has_value() ? nlohmann::json(*inc.sizeHa) : nlohmann::json(nullptr)},
        {"isFire", inc.isFire},
        {"centroid", {inc.centroidLng, inc.centroidLat}},
        {"geometry", inc.geometryJson},
        {"updatedAtIso", inc.updatedAtIso.has_value() ? nlohmann::json(*inc.updatedAtIso) : nlohmann::json(nullptr)},
        {"updatedAtRaw", inc.updatedAtRaw.has_value() ? nlohmann::json(*inc.updatedAtRaw) : nlohmann::json(nullptr)},
        {"provenance", inc.provenance}
    };
}

inline void to_json(nlohmann::json& j, const HotspotDetection& h) {
    j = nlohmann::json{
        {"id", h.id},
        {"latitude", h.latitude},
        {"longitude", h.longitude},
        {"acquiredAtIso", h.acquiredAtIso},
        {"satellite", h.satellite},
        {"instrument", h.instrument},
        {"confidence", h.confidence},
        {"frpMw", h.frpMw.has_value() ? nlohmann::json(*h.frpMw) : nlohmann::json(nullptr)},
        {"brightnessK", h.brightnessK.has_value() ? nlohmann::json(*h.brightnessK) : nlohmann::json(nullptr)},
        {"dayNight", h.dayNight},
        {"provenance", h.provenance}
    };
}

inline void to_json(nlohmann::json& j, const HistoricalFireRecord& r) {
    j = nlohmann::json{
        {"id", r.id},
        {"fireName", r.fireName},
        {"fireSeason", r.fireSeason},
        {"ignitionDateIso", r.ignitionDateIso.has_value() ? nlohmann::json(*r.ignitionDateIso) : nlohmann::json(nullptr)},
        {"extinguishedDateIso", r.extinguishedDateIso.has_value() ? nlohmann::json(*r.extinguishedDateIso) : nlohmann::json(nullptr)},
        {"fireType", r.fireType},
        {"areaHa", r.areaHa},
        {"bioregionOrCouncil", r.bioregionOrCouncil},
        {"dayOfYear", r.dayOfYear},
        {"centroid", {r.centroidLng, r.centroidLat}},
        {"bbox", r.bbox},
        {"geometry", r.geometryJson},
        {"provenance", r.provenance}
    };
}

inline void to_json(nlohmann::json& j, const SimilarityMatch& m) {
    j = nlohmann::json{
        {"record", m.record},
        {"totalScorePct", m.totalScorePct},
        {"geoScore", m.geoScore},
        {"sizeScore", m.sizeScore},
        {"seasonScore", m.seasonScore},
        {"typeScore", m.typeScore},
        {"distanceKm", m.distanceKm},
        {"explanation", m.explanation}
    };
}

inline void to_json(nlohmann::json& j, const SatelliteSceneItem& s) {
    j = nlohmann::json{
        {"id", s.id},
        {"collection", s.collection},
        {"platform", s.platform},
        {"acquiredAtIso", s.acquiredAtIso},
        {"cloudCoverPct", s.cloudCoverPct.has_value() ? nlohmann::json(*s.cloudCoverPct) : nlohmann::json(nullptr)},
        {"bbox", s.bbox},
        {"geometry", s.geometryJson},
        {"previewUrl", s.previewUrl.has_value() ? nlohmann::json(*s.previewUrl) : nlohmann::json(nullptr)},
        {"wmsLayerName", s.wmsLayerName},
        {"stacSelfUrl", s.stacSelfUrl},
        {"gsdMeters", s.gsdMeters},
        {"provenance", s.provenance}
    };
}

} // namespace bushfire
