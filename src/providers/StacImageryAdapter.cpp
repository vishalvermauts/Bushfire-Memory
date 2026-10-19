#include "bushfire/providers/StacImageryAdapter.hpp"
#include "bushfire/GeoUtils.hpp"
#include "bushfire/VerifiedSnapshots.hpp"
#include <iostream>
#include <sstream>

namespace bushfire {

static const std::string DEA_STAC_SEARCH_URL = "https://explorer.dea.ga.gov.au/stac/search";

StacImageryAdapter::StacImageryAdapter(std::shared_ptr<HttpClient> http, std::shared_ptr<CacheStore> cache)
    : http_(std::move(http)), cache_(std::move(cache)) {
    status_.providerId = "dea-stac-ows";
    status_.name = "DEA & Copernicus STAC";
    status_.endpointUrl = DEA_STAC_SEARCH_URL;
    status_.docsUrl = "https://explorer.dea.ga.gov.au/about";
    status_.licence = "CC BY 4.0";
    status_.status = FreshnessStatus::FallbackSnapshot;
    status_.requiresKey = false;
    status_.keyConfigured = false;

    cachedScenes_ = VerifiedSnapshots::getFallbackScenes();
    status_.recordCount = cachedScenes_.size();
}

DataSourceStatus StacImageryAdapter::getStatus() {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
}

void StacImageryAdapter::refresh(bool /*force*/) {
    std::lock_guard<std::mutex> lock(mutex_);
    // Test STAC availability
    HttpResponse resp = http_->get("https://explorer.dea.ga.gov.au/stac", 8);
    if (resp.isSuccess()) {
        status_.status = FreshnessStatus::Live;
        status_.latencyMs = resp.latencyMs;
        status_.lastSuccessAtIso = GeoUtils::getCurrentUtcIso();
        status_.errorMessage = std::nullopt;
    } else {
        status_.status = FreshnessStatus::FallbackSnapshot;
        status_.errorMessage = "DEA STAC root ping failed: " + resp.errorMessage;
    }
}

std::vector<SatelliteSceneItem> StacImageryAdapter::searchScenes(
    const std::array<double, 4>& bbox,
    const std::string& startIso,
    const std::string& endIso,
    double maxCloudPct,
    const std::string& collection) {

    std::lock_guard<std::mutex> lock(mutex_);

    // Construct STAC Search JSON payload
    nlohmann::json queryJson = {
        {"bbox", {bbox[0], bbox[1], bbox[2], bbox[3]}},
        {"limit", 15}
    };

    if (!startIso.empty() && !endIso.empty()) {
        queryJson["datetime"] = startIso + "/" + endIso;
    }
    if (!collection.empty()) {
        queryJson["collections"] = {collection};
    }

    std::string cacheKey = "stac:" + collection + ":" + std::to_string(bbox[0]) + ":" + std::to_string(bbox[1]);
    auto cached = cache_->get(cacheKey);

    HttpResponse resp = http_->post(DEA_STAC_SEARCH_URL, queryJson.dump(), 12);
    if (resp.isSuccess() && !resp.body.empty()) {
        auto root = nlohmann::json::parse(resp.body, nullptr, false);
        if (!root.is_discarded() && root.contains("features") && root["features"].is_array()) {
            std::vector<SatelliteSceneItem> liveScenes;
            SourceProvenance prov{
                "dea-stac-ows",
                "Geoscience Australia",
                "Digital Earth Australia STAC / Sentinel-2 ARD",
                DEA_STAC_SEARCH_URL,
                "https://knowledge.dea.ga.gov.au/",
                "CC BY 4.0",
                std::nullopt,
                GeoUtils::getCurrentUtcIso(),
                FreshnessStatus::Live,
                "Analysis Ready Data Sentinel-2 MSI Surface Reflectance."
            };

            for (const auto& f : root["features"]) {
                if (!f.contains("properties") || !f.contains("id")) continue;
                const auto& props = f["properties"];

                double cloud = props.value("eo:cloud_cover", 0.0);
                if (cloud > maxCloudPct) continue;

                SatelliteSceneItem s;
                s.id = f["id"].get<std::string>();
                s.collection = collection;
                s.platform = props.value("platform", "Sentinel-2");
                s.acquiredAtIso = props.value("datetime", GeoUtils::getCurrentUtcIso());
                s.cloudCoverPct = cloud;
                s.geometryJson = f.value("geometry", nlohmann::json::object());
                s.bbox = GeoUtils::computeBbox(s.geometryJson);
                s.wmsLayerName = "s2_ard_granule_nbar_t";
                s.stacSelfUrl = "https://explorer.dea.ga.gov.au/stac/collections/" + collection + "/items/" + s.id;
                s.gsdMeters = 10.0;
                s.provenance = prov;

                liveScenes.push_back(s);
            }

            if (!liveScenes.empty()) {
                cache_->set(cacheKey, resp.body, 3600); // 1 hour cache
                return liveScenes;
            }
        }
    }

    // Fallback: return verified snapshot scenes matching criteria
    std::vector<SatelliteSceneItem> results;
    for (const auto& s : cachedScenes_) {
        if (s.cloudCoverPct.has_value() && *s.cloudCoverPct <= maxCloudPct) {
            results.push_back(s);
        }
    }
    return results;
}

std::vector<SpectralModeInfo> StacImageryAdapter::getSpectralModes() const {
    std::vector<SpectralModeInfo> modes;

    // 1. True Colour RGB
    {
        SpectralModeInfo m;
        m.id = "true-colour";
        m.name = "True Colour (Natural RGB)";
        m.description = "Simulates human visual perception using visible light bands.";
        m.bandsUsed = "B04 (Red: 665 nm), B03 (Green: 560 nm), B02 (Blue: 490 nm)";
        m.formula = "R = B04, G = B03, B = B02";
        m.spatialResolutionMeters = 10.0;
        m.interpretationGuide = "Healthy vegetation appears vibrant green. Fresh burn scars appear dark grey, charcoal, or brown. Smoke appears light blue-grey.";
        m.scientificCaveats = "Cannot penetrate thick smoke or cloud. Cannot reliably distinguish dark shadow or moist dark soil from light burn scars.";
        modes.push_back(m);
    }

    // 2. False Colour SWIR (Burn Scar Emphasis)
    {
        SpectralModeInfo m;
        m.id = "false-colour-swir";
        m.name = "False Colour SWIR (Burn Scar Composite)";
        m.description = "Combines Short-Wave Infrared with Near-Infrared to penetrate atmospheric smoke haze and highlight charred organic matter.";
        m.bandsUsed = "B12 (SWIR-2: 2190 nm), B8A (Narrow NIR: 865 nm), B04 (Red: 665 nm)";
        m.formula = "R = B12 (SWIR2), G = B8A (NIR), B = B04 (Red)";
        m.spatialResolutionMeters = 20.0;
        m.interpretationGuide = "Active fire hot spots glow bright orange/red. Recent burn scars appear intense reddish-brown or rust. Unburnt forest appears bright green. Cleared land appears pale.";
        m.scientificCaveats = "SWIR bands have 20m pixel resolution (resampled to 10m). Bare dry red soil or rocky ridges can mimic burn scar reflectance.";
        modes.push_back(m);
    }

    // 3. NDVI (Normalized Difference Vegetation Index)
    {
        SpectralModeInfo m;
        m.id = "ndvi";
        m.name = "NDVI (Normalized Difference Vegetation Index)";
        m.description = "Measures live green vegetation density and chlorophyll absorption.";
        m.bandsUsed = "B08 (NIR: 842 nm), B04 (Red: 665 nm)";
        m.formula = "NDVI = (NIR - Red) / (NIR + Red)";
        m.spatialResolutionMeters = 10.0;
        m.interpretationGuide = "Values range from -1.0 to +1.0. Dense healthy forest: 0.6 to 0.85. Grassland: 0.2 to 0.5. Burnt canopy / bare ground: 0.0 to 0.15. Water / cloud: < 0.0.";
        m.scientificCaveats = "Severe drought or seasonal curing significantly reduces NDVI even in unburnt vegetation. Not a fire-specific index.";
        modes.push_back(m);
    }

    // 4. NBR & Delta NBR (Normalized Burn Ratio)
    {
        SpectralModeInfo m;
        m.id = "dnbr";
        m.name = "NBR & dNBR (Normalized Burn Ratio Delta)";
        m.description = "Optimal index for mapping fire severity and vegetation consumption.";
        m.bandsUsed = "B08 (NIR: 842 nm), B12 (SWIR-2: 2190 nm)";
        m.formula = "NBR = (NIR - SWIR2) / (NIR + SWIR2),  dNBR = NBR_pre - NBR_post";
        m.spatialResolutionMeters = 20.0;
        m.interpretationGuide = "dNBR < 0.1: Unburnt. 0.1 - 0.27: Low severity. 0.27 - 0.44: Moderate severity. 0.44 - 0.66: High severity. > 0.66: Extreme canopy consumption.";
        m.scientificCaveats = "Requires cloud-free and shadow-free pre-fire and post-fire imagery. Agricultural harvesting, logging, or flood recession can also cause positive dNBR without wildfire occurrence.";
        modes.push_back(m);
    }

    return modes;
}

std::string StacImageryAdapter::buildDeaWmsUrl(const std::string& layerName, const std::string& timeIso, const std::string& styles) const {
    std::ostringstream ss;
    ss << "https://ows.dea.ga.gov.au/?SERVICE=WMS&VERSION=1.3.0&REQUEST=GetMap"
       << "&LAYERS=" << layerName
       << "&CRS=EPSG:3857&BBOX={bbox-epsg-3857}&WIDTH=256&HEIGHT=256&FORMAT=image/png&TRANSPARENT=TRUE";
    if (!timeIso.empty()) {
        ss << "&TIME=" << timeIso;
    }
    if (!styles.empty()) {
        ss << "&STYLES=" << styles;
    }
    return ss.str();
}

} // namespace bushfire
