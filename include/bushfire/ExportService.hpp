#pragma once

#include "bushfire/Types.hpp"
#include <string>
#include <vector>

namespace bushfire {

class ExportService {
public:
    // Generate RFC 7946 GeoJSON string for a mixed collection of incidents and historical fires
    static std::string exportGeoJson(
        const std::vector<UnifiedIncident>& incidents,
        const std::vector<HistoricalFireRecord>& historicalFires,
        const std::vector<HotspotDetection>& hotspots
    );

    // Generate RFC 4180 CSV string for incidents
    static std::string exportIncidentsCsv(const std::vector<UnifiedIncident>& incidents);

    // Generate RFC 4180 CSV string for historical fires
    static std::string exportHistoryCsv(const std::vector<HistoricalFireRecord>& historicalFires);

    // Generate RFC 4180 CSV string for hotspots
    static std::string exportHotspotsCsv(const std::vector<HotspotDetection>& hotspots);

    // Generate comprehensive Markdown research briefing with provenance and citations
    static std::string exportMarkdownReport(
        const std::vector<UnifiedIncident>& incidents,
        const std::vector<HistoricalFireRecord>& historicalFires,
        const std::vector<HotspotDetection>& hotspots,
        const std::vector<DataSourceStatus>& providerStatuses
    );
};

} // namespace bushfire
