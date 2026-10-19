#pragma once

#include "bushfire/Types.hpp"
#include <vector>

namespace bushfire {

class VerifiedSnapshots {
public:
    // Returns authentic historical fire perimeters (Black Summer 2019-20, Warrumbungle 2013, Canberra 2003, and Prescribed Burns)
    static std::vector<HistoricalFireRecord> getHistoricalFires();

    // Returns verified NSW RFS incident snapshots for offline/fallback mode
    static std::vector<UnifiedIncident> getFallbackIncidents();

    // Returns verified DEA / NASA FIRMS hotspot detections for offline/fallback mode
    static std::vector<HotspotDetection> getFallbackHotspots();

    // Returns verified DEA STAC Sentinel-2 scene records
    static std::vector<SatelliteSceneItem> getFallbackScenes();

    // Raw GeoJSON payload for NSW RFS test fixture
    static std::string getRawRfsGeoJsonFixture();

    // Raw CSV payload for NASA FIRMS test fixture
    static std::string getRawFirmsCsvFixture();
};

} // namespace bushfire
