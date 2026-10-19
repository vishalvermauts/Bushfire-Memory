#pragma once

#include <string>
#include <array>
#include <optional>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace bushfire {

struct ParsedRfsDescription {
    std::string alertLevel{"Advice"};
    std::string location;
    std::string councilArea;
    std::string status{"Under control"};
    std::string type{"Bush Fire"};
    bool isFire{true};
    std::optional<double> sizeHa;
    std::string responsibleAgency{"Rural Fire Service"};
    std::string updatedRaw;
};

class GeoUtils {
public:
    // Earth radius in kilometers (WGS 84 mean)
    static constexpr double EARTH_RADIUS_KM = 6371.0088;

    // Haversine distance in kilometers between two [lat, lng] coordinates
    static double haversineDistanceKm(double lat1, double lng1, double lat2, double lng2);

    // Check if point [lng, lat] is within bounding box [minLng, minLat, maxLng, maxLat]
    static bool pointInBbox(double lng, double lat, const std::array<double, 4>& bbox);

    // Check if two bounding boxes intersect
    static bool bboxIntersects(const std::array<double, 4>& a, const std::array<double, 4>& b);

    // Validate coordinate ranges
    static bool isValidCoordinate(double lng, double lat);

    // Check if coordinate is in Australian terrestrial/marine monitoring domain
    static bool isInAustralianDomain(double lng, double lat);

    // Compute centroid of a GeoJSON geometry (Point, Polygon, MultiPolygon, GeometryCollection)
    static std::pair<double, double> computeCentroid(const nlohmann::json& geometry);

    // Compute bounding box [minLng, minLat, maxLng, maxLat] of a GeoJSON geometry
    static std::array<double, 4> computeBbox(const nlohmann::json& geometry);

    // Parse NSW RFS HTML description string into structured key-value pairs
    static ParsedRfsDescription parseRfsDescription(const std::string& rawHtmlDescription);

    // Dual-timezone & ISO timestamp utilities
    static std::string getCurrentUtcIso();
    static std::string formatAestIso(const std::string& utcIso);
    static std::string getRelativeAge(const std::string& isoTimestamp);

    // Day of year calculation (1 to 365/366) with leap year support
    static bool isLeapYear(int year);
    static int getDayOfYear(int year, int month, int day);
    static int getDayOfYearFromIso(const std::string& isoTimestamp);
};

} // namespace bushfire
