#pragma once

#include "bushfire/providers/IDataProviderAdapter.hpp"
#include "bushfire/HttpClient.hpp"
#include "bushfire/CacheStore.hpp"
#include <vector>
#include <memory>
#include <mutex>

namespace bushfire {

class RfsIncidentAdapter : public IDataProviderAdapter {
public:
    RfsIncidentAdapter(std::shared_ptr<HttpClient> http, std::shared_ptr<CacheStore> cache);
    ~RfsIncidentAdapter() override = default;

    std::string getProviderId() const override { return "nsw-rfs-incidents"; }
    std::string getProviderName() const override { return "NSW Rural Fire Service"; }
    DataSourceStatus getStatus() override;
    void refresh(bool force = false) override;

    // Retrieve parsed incidents with optional filters
    std::vector<UnifiedIncident> getIncidents(
        const std::optional<std::array<double, 4>>& bbox = std::nullopt,
        const std::string& alertLevel = "",
        bool fireOnly = false,
        const std::string& searchQuery = ""
    );

    // Parse raw GeoJSON payload string into UnifiedIncident list
    static std::vector<UnifiedIncident> parseGeoJson(const std::string& jsonStr, FreshnessStatus freshness, long latencyMs);

private:
    std::shared_ptr<HttpClient> http_;
    std::shared_ptr<CacheStore> cache_;
    std::vector<UnifiedIncident> incidents_;
    DataSourceStatus status_;
    std::mutex mutex_;
    long lastFetchEpoch_{0};
};

} // namespace bushfire
