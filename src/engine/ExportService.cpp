#include "bushfire/ExportService.hpp"
#include "bushfire/GeoUtils.hpp"
#include <sstream>
#include <iomanip>

namespace bushfire {

static std::string escapeCsv(const std::string& field) {
    bool needsQuotes = (field.find(',') != std::string::npos ||
                        field.find('"') != std::string::npos ||
                        field.find('\n') != std::string::npos ||
                        field.find('\r') != std::string::npos);
    if (!needsQuotes) return field;

    std::string escaped = "\"";
    for (char c : field) {
        if (c == '"') escaped += "\"\"";
        else escaped += c;
    }
    escaped += "\"";
    return escaped;
}

std::string ExportService::exportGeoJson(
    const std::vector<UnifiedIncident>& incidents,
    const std::vector<HistoricalFireRecord>& historicalFires,
    const std::vector<HotspotDetection>& hotspots) {

    nlohmann::json root = {
        {"type", "FeatureCollection"},
        {"crs", {
            {"type", "name"},
            {"properties", {{"name", "urn:ogc:def:crs:OGC:1.3:CRS84"}}}
        }},
        {"features", nlohmann::json::array()}
    };

    // Add incidents
    for (const auto& inc : incidents) {
        nlohmann::json feat = {
            {"type", "Feature"},
            {"id", inc.id},
            {"geometry", inc.geometryJson},
            {"properties", {
                {"featureClass", "Incident"},
                {"originalId", inc.originalId},
                {"title", inc.title},
                {"alertLevel", inc.alertLevel},
                {"status", inc.status},
                {"category", inc.category},
                {"councilArea", inc.councilArea},
                {"responsibleAgency", inc.responsibleAgency},
                {"sizeHa", inc.sizeHa.has_value() ? nlohmann::json(*inc.sizeHa) : nlohmann::json(nullptr)},
                {"isFire", inc.isFire},
                {"updatedAtIso", inc.updatedAtIso.has_value() ? nlohmann::json(*inc.updatedAtIso) : nlohmann::json(nullptr)},
                {"sourceProvider", inc.provenance.providerName},
                {"sourceLicence", inc.provenance.licence},
                {"freshness", freshnessToString(inc.provenance.freshness)},
                {"ingestionTimestamp", inc.provenance.fetchedAtIso}
            }}
        };
        root["features"].push_back(feat);
    }

    // Add historical fires
    for (const auto& hist : historicalFires) {
        nlohmann::json feat = {
            {"type", "Feature"},
            {"id", hist.id},
            {"geometry", hist.geometryJson},
            {"properties", {
                {"featureClass", "HistoricalFire"},
                {"fireName", hist.fireName},
                {"fireSeason", hist.fireSeason},
                {"fireType", hist.fireType},
                {"areaHa", hist.areaHa},
                {"bioregionOrCouncil", hist.bioregionOrCouncil},
                {"ignitionDateIso", hist.ignitionDateIso.has_value() ? nlohmann::json(*hist.ignitionDateIso) : nlohmann::json(nullptr)},
                {"extinguishedDateIso", hist.extinguishedDateIso.has_value() ? nlohmann::json(*hist.extinguishedDateIso) : nlohmann::json(nullptr)},
                {"sourceProvider", hist.provenance.providerName},
                {"sourceLicence", hist.provenance.licence},
                {"freshness", freshnessToString(hist.provenance.freshness)}
            }}
        };
        root["features"].push_back(feat);
    }

    // Add hotspots
    for (const auto& h : hotspots) {
        nlohmann::json feat = {
            {"type", "Feature"},
            {"id", h.id},
            {"geometry", {
                {"type", "Point"},
                {"coordinates", {h.longitude, h.latitude}}
            }},
            {"properties", {
                {"featureClass", "SatelliteThermalAnomaly"},
                {"satellite", h.satellite},
                {"instrument", h.instrument},
                {"confidence", h.confidence},
                {"frpMw", h.frpMw.has_value() ? nlohmann::json(*h.frpMw) : nlohmann::json(nullptr)},
                {"brightnessK", h.brightnessK.has_value() ? nlohmann::json(*h.brightnessK) : nlohmann::json(nullptr)},
                {"dayNight", h.dayNight},
                {"acquiredAtIso", h.acquiredAtIso},
                {"sourceProvider", h.provenance.providerName},
                {"sourceLicence", h.provenance.licence}
            }}
        };
        root["features"].push_back(feat);
    }

    return root.dump(2);
}

std::string ExportService::exportIncidentsCsv(const std::vector<UnifiedIncident>& incidents) {
    std::ostringstream ss;
    ss << "id,original_id,title,alert_level,status,category,council_area,agency,size_ha,is_fire,lng,lat,updated_utc,updated_aest,freshness,provider,licence\n";

    for (const auto& inc : incidents) {
        std::string updIso = inc.updatedAtIso.value_or("");
        std::string updAest = GeoUtils::formatAestIso(updIso);

        ss << escapeCsv(inc.id) << ","
           << escapeCsv(inc.originalId) << ","
           << escapeCsv(inc.title) << ","
           << escapeCsv(inc.alertLevel) << ","
           << escapeCsv(inc.status) << ","
           << escapeCsv(inc.category) << ","
           << escapeCsv(inc.councilArea) << ","
           << escapeCsv(inc.responsibleAgency) << ","
           << (inc.sizeHa.has_value() ? std::to_string(*inc.sizeHa) : "") << ","
           << (inc.isFire ? "true" : "false") << ","
           << inc.centroidLng << ","
           << inc.centroidLat << ","
           << escapeCsv(updIso) << ","
           << escapeCsv(updAest) << ","
           << freshnessToString(inc.provenance.freshness) << ","
           << escapeCsv(inc.provenance.providerName) << ","
           << escapeCsv(inc.provenance.licence) << "\n";
    }

    return ss.str();
}

std::string ExportService::exportHistoryCsv(const std::vector<HistoricalFireRecord>& historicalFires) {
    std::ostringstream ss;
    ss << "id,fire_name,season,fire_type,area_ha,bioregion,ignition_date,extinguished_date,centroid_lng,centroid_lat,provider,licence\n";

    for (const auto& h : historicalFires) {
        ss << escapeCsv(h.id) << ","
           << escapeCsv(h.fireName) << ","
           << escapeCsv(h.fireSeason) << ","
           << escapeCsv(h.fireType) << ","
           << h.areaHa << ","
           << escapeCsv(h.bioregionOrCouncil) << ","
           << escapeCsv(h.ignitionDateIso.value_or("")) << ","
           << escapeCsv(h.extinguishedDateIso.value_or("")) << ","
           << h.centroidLng << ","
           << h.centroidLat << ","
           << escapeCsv(h.provenance.providerName) << ","
           << escapeCsv(h.provenance.licence) << "\n";
    }

    return ss.str();
}

std::string ExportService::exportHotspotsCsv(const std::vector<HotspotDetection>& hotspots) {
    std::ostringstream ss;
    ss << "id,latitude,longitude,acquired_at_utc,satellite,instrument,confidence,frp_mw,brightness_k,day_night,provider,licence\n";

    for (const auto& h : hotspots) {
        ss << escapeCsv(h.id) << ","
           << h.latitude << ","
           << h.longitude << ","
           << escapeCsv(h.acquiredAtIso) << ","
           << escapeCsv(h.satellite) << ","
           << escapeCsv(h.instrument) << ","
           << escapeCsv(h.confidence) << ","
           << (h.frpMw.has_value() ? std::to_string(*h.frpMw) : "") << ","
           << (h.brightnessK.has_value() ? std::to_string(*h.brightnessK) : "") << ","
           << escapeCsv(h.dayNight) << ","
           << escapeCsv(h.provenance.providerName) << ","
           << escapeCsv(h.provenance.licence) << "\n";
    }

    return ss.str();
}

std::string ExportService::exportMarkdownReport(
    const std::vector<UnifiedIncident>& incidents,
    const std::vector<HistoricalFireRecord>& historicalFires,
    const std::vector<HotspotDetection>& hotspots,
    const std::vector<DataSourceStatus>& providerStatuses) {

    std::ostringstream ss;
    std::string nowUtc = GeoUtils::getCurrentUtcIso();
    std::string nowAest = GeoUtils::formatAestIso(nowUtc);

    ss << "# Bushfire Memory — Geospatial Intelligence & Historical Research Briefing\n\n";

    // Non-Emergency Safety Banner
    ss << "> [!IMPORTANT]\n"
       << "> **CRITICAL PUBLIC SAFETY NOTICE: RESEARCH & HISTORICAL INFORMATION ONLY**\n"
       << "> Bushfire Memory is an observational research platform, **not** an emergency warning service, evacuation router, or emergency dispatch tool.\n"
       << "> In life-threatening emergencies, immediately call **000** or consult official state emergency authorities:\n"
       << "> - NSW: [NSW Rural Fire Service](https://www.rfs.nsw.gov.au/)\n"
       << "> - VIC: [VicEmergency](https://emergency.vic.gov.au/)\n"
       << "> - QLD: [Queensland Fire & Emergency Services](https://www.fire.qld.gov.au/)\n"
       << "> - WA: [Emergency WA](https://www.emergency.wa.gov.au/)\n"
       << "> - SA: [Country Fire Service](https://www.cfs.sa.gov.au/)\n\n";

    ss << "### Briefing Generation Metadata\n";
    ss << "- **Report Generated (UTC)**: `" << nowUtc << "`\n";
    ss << "- **Report Generated (AEST)**: `" << nowAest << "`\n";
    ss << "- **Platform Version**: Bushfire Memory v3.0 (C++20 Native Engine)\n\n";

    ss << "## 1. Executive Summary & Inventory\n\n";
    ss << "| Category | Total Records | Key Spatial Domain |\n";
    ss << "| :--- | :--- | :--- |\n";
    ss << "| Active / Reported Incidents | " << incidents.size() << " | New South Wales (NSW RFS) |\n";
    ss << "| Satellite Thermal Anomalies | " << hotspots.size() << " | Australian Continent (DEA / FIRMS) |\n";
    ss << "| Historical Vector Fire Perimeters | " << historicalFires.size() << " | NSW Black Summer & Major Fire Seasons |\n\n";

    // Current Incidents Table
    if (!incidents.empty()) {
        ss << "## 2. Reported Incidents (NSW RFS)\n\n";
        ss << "| ID | Title | Alert Level | Status | Size (ha) | Council Area | Ingestion Mode |\n";
        ss << "| :--- | :--- | :--- | :--- | :--- | :--- | :--- |\n";
        for (const auto& inc : incidents) {
            ss << "| `" << inc.id << "` | " << inc.title << " | **" << inc.alertLevel << "** | "
               << inc.status << " | " << (inc.sizeHa.has_value() ? std::to_string(static_cast<long>(*inc.sizeHa)) : "-")
               << " | " << inc.councilArea << " | `" << freshnessToString(inc.provenance.freshness) << "` |\n";
        }
        ss << "\n";
    }

    // Historical Records Table
    if (!historicalFires.empty()) {
        ss << "## 3. Indexed Historical Fire Perimeters (NPWS)\n\n";
        ss << "| ID | Fire Name | Season | Type | Area (ha) | Bioregion / Council |\n";
        ss << "| :--- | :--- | :--- | :--- | :--- | :--- |\n";
        for (const auto& h : historicalFires) {
            ss << "| `" << h.id << "` | **" << h.fireName << "** | " << h.fireSeason << " | "
               << h.fireType << " | " << static_cast<long>(h.areaHa) << " ha | " << h.bioregionOrCouncil << " |\n";
        }
        ss << "\n";
    }

    // Remote Sensing Methodology & Caveats
    ss << "## 4. Scientific Remote Sensing & Data Quality Caveats\n\n";
    ss << "1. **Satellite Thermal Detections**: Hotspots represent mid-infrared thermal anomalies detected by VIIRS (375m) and MODIS (1km) sensors. They do not distinguish between active bushfires, industrial gas flares, agricultural stubble burning, or heated bare rock surfaces.\n";
    ss << "2. **Severity Mapping (FESM vs. GEEBAM)**: FESM is a calibrated machine-learning model validated with post-fire field measurements. GEEBAM is a rapid, uncalibrated interim dNBR threshold product that lacks ground-truthing.\n";
    ss << "3. **Spectral Indices (NDVI / dNBR)**: Phenological drought curing, topographical cast shadows, and agricultural harvesting can mimic burn scar spectral signatures. Positive delta indices require cross-verification with official perimeter layers.\n\n";

    // Citations & Data Licences
    ss << "## 5. Official Data Sources & Attribution Statements\n\n";
    for (const auto& p : providerStatuses) {
        ss << "- **" << p.name << "** (`" << p.providerId << "`): Licence: `" << p.licence << "`. Endpoint: [" << p.endpointUrl << "](" << p.endpointUrl << "). Status: `" << freshnessToString(p.status) << "` (" << p.recordCount << " records).\n";
    }
    ss << "\n---\n*End of Briefing. Produced by Bushfire Memory.*";

    return ss.str();
}

} // namespace bushfire
