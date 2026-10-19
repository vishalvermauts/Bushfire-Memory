#pragma once

#include "bushfire/providers/IDataProviderAdapter.hpp"
#include "bushfire/HttpClient.hpp"
#include "bushfire/CacheStore.hpp"
#include <vector>
#include <memory>
#include <mutex>

namespace bushfire {

struct SpectralModeInfo {
    std::string id;
    std::string name;
    std::string description;
    std::string bandsUsed;
    std::string formula;
    double spatialResolutionMeters{10.0};
    std::string interpretationGuide;
    std::string scientificCaveats;
};

class StacImageryAdapter : public IDataProviderAdapter {
public:
    StacImageryAdapter(std::shared_ptr<HttpClient> http, std::shared_ptr<CacheStore> cache);
    ~StacImageryAdapter() override = default;

    std::string getProviderId() const override { return "dea-stac-ows"; }
    std::string getProviderName() const override { return "DEA & Copernicus STAC"; }
    DataSourceStatus getStatus() override;
    void refresh(bool force = false) override;

    // Search STAC catalogue for scenes
    std::vector<SatelliteSceneItem> searchScenes(
        const std::array<double, 4>& bbox,
        const std::string& startIso,
        const std::string& endIso,
        double maxCloudPct = 20.0,
        const std::string& collection = "ga_s2am_ard_3"
    );

    // Returns verified spectral indices documentation and metadata
    std::vector<SpectralModeInfo> getSpectralModes() const;

    // Build DEA OWS WMS URL for a specific time and layer
    std::string buildDeaWmsUrl(const std::string& layerName, const std::string& timeIso, const std::string& styles = "") const;

private:
    std::shared_ptr<HttpClient> http_;
    std::shared_ptr<CacheStore> cache_;
    std::vector<SatelliteSceneItem> cachedScenes_;
    DataSourceStatus status_;
    std::mutex mutex_;
};

} // namespace bushfire
