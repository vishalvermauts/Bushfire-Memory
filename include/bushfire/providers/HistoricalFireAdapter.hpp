#pragma once

#include "bushfire/providers/IDataProviderAdapter.hpp"
#include "bushfire/HttpClient.hpp"
#include "bushfire/CacheStore.hpp"
#include <vector>
#include <memory>
#include <mutex>

namespace bushfire {

struct WmsLayerDescriptor {
    std::string id;
    std::string title;
    std::string providerName;
    std::string wmsUrl;
    std::string layerName;
    std::string crs{"EPSG:3857"};
    std::string format{"image/png"};
    bool transparent{true};
    std::string scientificCaveat;
    std::string licence;
    nlohmann::json legendClasses;
};

class HistoricalFireAdapter : public IDataProviderAdapter {
public:
    HistoricalFireAdapter(std::shared_ptr<HttpClient> http, std::shared_ptr<CacheStore> cache);
    ~HistoricalFireAdapter() override = default;

    std::string getProviderId() const override { return "nsw-npws-history"; }
    std::string getProviderName() const override { return "NSW NPWS Fire History & Severity"; }
    DataSourceStatus getStatus() override;
    void refresh(bool force = false) override;

    // Search historical fire records
    std::vector<HistoricalFireRecord> getRecords(
        const std::optional<std::array<double, 4>>& bbox = std::nullopt,
        const std::string& season = "",
        const std::string& fireType = "",
        std::optional<double> minAreaHa = std::nullopt,
        const std::string& searchQuery = ""
    );

    // Look up single record by ID
    std::optional<HistoricalFireRecord> getRecordById(const std::string& id);

    // Get verified WMS layer descriptors for FESM, GEEBAM, etc.
    std::vector<WmsLayerDescriptor> getWmsLayers() const;

private:
    std::shared_ptr<HttpClient> http_;
    std::shared_ptr<CacheStore> cache_;
    std::vector<HistoricalFireRecord> records_;
    DataSourceStatus status_;
    std::mutex mutex_;
};

} // namespace bushfire
