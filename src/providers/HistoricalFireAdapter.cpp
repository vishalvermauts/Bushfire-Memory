#include "bushfire/providers/HistoricalFireAdapter.hpp"
#include "bushfire/GeoUtils.hpp"
#include "bushfire/VerifiedSnapshots.hpp"
#include <algorithm>

namespace bushfire {

HistoricalFireAdapter::HistoricalFireAdapter(std::shared_ptr<HttpClient> http, std::shared_ptr<CacheStore> cache)
    : http_(std::move(http)), cache_(std::move(cache)) {
    status_.providerId = "nsw-npws-history";
    status_.name = "NSW NPWS Fire History";
    status_.endpointUrl = "https://data.nsw.gov.au/data/dataset/fire-history-wildfires-and-prescribed-burns-1e8b6";
    status_.docsUrl = "https://datasets.seed.nsw.gov.au";
    status_.licence = "CC BY 4.0";
    status_.status = FreshnessStatus::FallbackSnapshot;
    status_.requiresKey = false;
    status_.keyConfigured = false;

    // Seed authentic historical perimeters
    records_ = VerifiedSnapshots::getHistoricalFires();
    status_.recordCount = records_.size();
    status_.lastSuccessAtIso = GeoUtils::getCurrentUtcIso();
}

DataSourceStatus HistoricalFireAdapter::getStatus() {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
}

void HistoricalFireAdapter::refresh(bool /*force*/) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (records_.empty()) {
        records_ = VerifiedSnapshots::getHistoricalFires();
    }
    status_.recordCount = records_.size();
}

std::vector<HistoricalFireRecord> HistoricalFireAdapter::getRecords(
    const std::optional<std::array<double, 4>>& bbox,
    const std::string& season,
    const std::string& fireType,
    std::optional<double> minAreaHa,
    const std::string& searchQuery) {

    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<HistoricalFireRecord> results;
    results.reserve(records_.size());

    std::string searchLower = searchQuery;
    std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

    for (const auto& rec : records_) {
        if (!season.empty() && rec.fireSeason != season) continue;
        if (!fireType.empty() && rec.fireType != fireType) continue;
        if (minAreaHa.has_value() && rec.areaHa < *minAreaHa) continue;

        if (bbox.has_value() && !GeoUtils::bboxIntersects(rec.bbox, *bbox)) {
            continue;
        }

        if (!searchLower.empty()) {
            std::string n = rec.fireName;
            std::string b = rec.bioregionOrCouncil;
            std::transform(n.begin(), n.end(), n.begin(), ::tolower);
            std::transform(b.begin(), b.end(), b.begin(), ::tolower);
            if (n.find(searchLower) == std::string::npos && b.find(searchLower) == std::string::npos) {
                continue;
            }
        }

        results.push_back(rec);
    }

    return results;
}

std::optional<HistoricalFireRecord> HistoricalFireAdapter::getRecordById(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& rec : records_) {
        if (rec.id == id) {
            return rec;
        }
    }
    return std::nullopt;
}

std::vector<WmsLayerDescriptor> HistoricalFireAdapter::getWmsLayers() const {
    std::vector<WmsLayerDescriptor> layers;

    // 1. FESM (Fire Extent and Severity Mapping)
    {
        WmsLayerDescriptor l;
        l.id = "nsw-fesm-wms";
        l.title = "NSW Fire Extent and Severity Mapping (FESM)";
        l.providerName = "NSW Department of Planning and Environment (SEED)";
        l.wmsUrl = "https://datasets.seed.nsw.gov.au/geoserver/wms";
        l.layerName = "fesm_v1";
        l.crs = "EPSG:3857";
        l.format = "image/png";
        l.transparent = true;
        l.licence = "CC BY 4.0";
        l.scientificCaveat = "Calibrated machine-learning classification combining Sentinel-2 multi-spectral imagery with post-fire field validation. Distinguishes unburnt, low, moderate, high, and extreme canopy/understorey burn severity.";
        l.legendClasses = nlohmann::json::array({
            {{"class", "Unburnt"}, {"color", "#2ca02c"}, {"description", "No canopy or ground scorch detected"}},
            {{"class", "Low Severity"}, {"color", "#ffff00"}, {"description", "Understorey burnt, canopy unburnt/intact"}},
            {{"class", "Moderate Severity"}, {"color", "#ff7f0e"}, {"description", "Partial canopy scorch, understorey consumed"}},
            {{"class", "High Severity"}, {"color", "#d62728"}, {"description", "Complete canopy scorch, severe foliage loss"}},
            {{"class", "Extreme Severity"}, {"color", "#7b1fa2"}, {"description", "Complete canopy consumption / crown fire"}}
        });
        layers.push_back(l);
    }

    // 2. GEEBAM (Google Earth Engine Burnt Area Map)
    {
        WmsLayerDescriptor l;
        l.id = "nsw-geebam-wms";
        l.title = "Google Earth Engine Burnt Area Map (GEEBAM)";
        l.providerName = "NSW DPIE / Google Earth Engine";
        l.wmsUrl = "https://datasets.seed.nsw.gov.au/geoserver/wms";
        l.layerName = "geebam_v2";
        l.crs = "EPSG:3857";
        l.format = "image/png";
        l.transparent = true;
        l.licence = "CC BY 4.0";
        l.scientificCaveat = "Rapid, uncalibrated Sentinel-2 dNBR threshold product created during the 2019-20 emergency for urgent operational scoping. WARNING: Lacks ground-truthing and field calibration compared to FESM.";
        l.legendClasses = nlohmann::json::array({
            {{"class", "Low / Unburnt"}, {"color", "#a6d96a"}, {"description", "Minimal spectral change"}},
            {{"class", "Moderate Scorch"}, {"color", "#fdae61"}, {"description", "Moderate vegetation index delta"}},
            {{"class", "Severe Burn"}, {"color", "#d7191c"}, {"description", "Strong spectral delta consistent with burning"}}
        });
        layers.push_back(l);
    }

    // 3. DEA Burnt Area Vectors (Geoscience Australia)
    {
        WmsLayerDescriptor l;
        l.id = "dea-bav-wms";
        l.title = "DEA Burnt Area Characteristics (Sentinel-2)";
        l.providerName = "Geoscience Australia";
        l.wmsUrl = "https://ows.dea.ga.gov.au/";
        l.layerName = "s2_bav";
        l.crs = "EPSG:3857";
        l.format = "image/png";
        l.transparent = true;
        l.licence = "CC BY 4.0";
        l.scientificCaveat = "Near-real-time Sentinel-2 burnt area boundaries derived from spectral index analysis across the Australian continent.";
        l.legendClasses = nlohmann::json::array({
            {{"class", "Detected Burnt Area"}, {"color", "#e6550d"}, {"description", "Multi-spectral Sentinel-2 NRT detection"}}
        });
        layers.push_back(l);
    }

    return layers;
}

} // namespace bushfire
