#pragma once

#include "bushfire/providers/IDataProviderAdapter.hpp"
#include "bushfire/HttpClient.hpp"
#include "bushfire/CacheStore.hpp"
#include <vector>
#include <memory>
#include <mutex>

namespace bushfire {

class HotspotAdapter : public IDataProviderAdapter {
public:
    HotspotAdapter(std::shared_ptr<HttpClient> http, std::shared_ptr<CacheStore> cache);
    ~HotspotAdapter() override = default;

    std::string getProviderId() const override { return "dea-hotspots"; }
    std::string getProviderName() const override { return "DEA & NASA Thermal Hotspots"; }
    DataSourceStatus getStatus() override;
    void refresh(bool force = false) override;

    // Retrieve parsed hotspots with optional filters
    std::vector<HotspotDetection> getHotspots(
        const std::optional<std::array<double, 4>>& bbox = std::nullopt,
        std::optional<double> minFrp = std::nullopt,
        const std::string& confidence = "",
        int maxHours = 48
    );

    // CSV parser for NASA FIRMS stream
    static std::vector<HotspotDetection> parseFirmsCsv(const std::string& csvStr, FreshnessStatus freshness, long latencyMs);

    // GeoJSON parser for DEA Hotspots WFS
    static std::vector<HotspotDetection> parseDeaHotspotsJson(const std::string& jsonStr, FreshnessStatus freshness, long latencyMs);

private:
    std::shared_ptr<HttpClient> http_;
    std::shared_ptr<CacheStore> cache_;
    std::vector<HotspotDetection> hotspots_;
    DataSourceStatus status_;
    std::mutex mutex_;
    long lastFetchEpoch_{0};
};

} // namespace bushfire
