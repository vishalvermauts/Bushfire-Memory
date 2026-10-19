#include "bushfire/GeoUtils.hpp"
#include <cmath>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <regex>

namespace bushfire {

static constexpr double DEG_TO_RAD = M_PI / 180.0;
static constexpr double RAD_TO_DEG = 180.0 / M_PI;

double GeoUtils::haversineDistanceKm(double lat1, double lng1, double lat2, double lng2) {
    double dLat = (lat2 - lat1) * DEG_TO_RAD;
    double dLng = (lng2 - lng1) * DEG_TO_RAD;

    double rLat1 = lat1 * DEG_TO_RAD;
    double rLat2 = lat2 * DEG_TO_RAD;

    double a = std::sin(dLat / 2.0) * std::sin(dLat / 2.0) +
               std::cos(rLat1) * std::cos(rLat2) *
               std::sin(dLng / 2.0) * std::sin(dLng / 2.0);

    a = std::clamp(a, 0.0, 1.0);
    double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));

    return EARTH_RADIUS_KM * c;
}

bool GeoUtils::pointInBbox(double lng, double lat, const std::array<double, 4>& bbox) {
    return (lng >= bbox[0] && lng <= bbox[2] && lat >= bbox[1] && lat <= bbox[3]);
}

bool GeoUtils::bboxIntersects(const std::array<double, 4>& a, const std::array<double, 4>& b) {
    return !(a[2] < b[0] || a[0] > b[2] || a[3] < b[1] || a[1] > b[3]);
}

bool GeoUtils::isValidCoordinate(double lng, double lat) {
    return !std::isnan(lng) && !std::isnan(lat) &&
           lng >= -180.0 && lng <= 180.0 &&
           lat >= -90.0 && lat <= 90.0;
}

bool GeoUtils::isInAustralianDomain(double lng, double lat) {
    // Australian monitoring bounds: 110°E to 155°E, 45°S to 10°S
    return (lng >= 110.0 && lng <= 155.0 && lat >= -45.0 && lat <= -10.0);
}

std::pair<double, double> GeoUtils::computeCentroid(const nlohmann::json& geometry) {
    if (!geometry.is_object() || !geometry.contains("type")) {
        return {0.0, 0.0};
    }

    std::string type = geometry["type"].get<std::string>();

    if (type == "Point" && geometry.contains("coordinates") && geometry["coordinates"].is_array()) {
        auto coords = geometry["coordinates"];
        if (coords.size() >= 2) {
            return {coords[0].get<double>(), coords[1].get<double>()};
        }
    } else if (type == "Polygon" && geometry.contains("coordinates") && geometry["coordinates"].is_array()) {
        const auto& rings = geometry["coordinates"];
        if (!rings.empty() && rings[0].is_array() && !rings[0].empty()) {
            double sumLng = 0.0;
            double sumLat = 0.0;
            size_t count = 0;
            for (const auto& pt : rings[0]) {
                if (pt.is_array() && pt.size() >= 2) {
                    sumLng += pt[0].get<double>();
                    sumLat += pt[1].get<double>();
                    count++;
                }
            }
            if (count > 0) {
                return {sumLng / static_cast<double>(count), sumLat / static_cast<double>(count)};
            }
        }
    } else if (type == "MultiPolygon" && geometry.contains("coordinates") && geometry["coordinates"].is_array()) {
        double sumLng = 0.0;
        double sumLat = 0.0;
        size_t count = 0;
        for (const auto& poly : geometry["coordinates"]) {
            if (poly.is_array() && !poly.empty() && poly[0].is_array()) {
                for (const auto& pt : poly[0]) {
                    if (pt.is_array() && pt.size() >= 2) {
                        sumLng += pt[0].get<double>();
                        sumLat += pt[1].get<double>();
                        count++;
                    }
                }
            }
        }
        if (count > 0) {
            return {sumLng / static_cast<double>(count), sumLat / static_cast<double>(count)};
        }
    } else if (type == "GeometryCollection" && geometry.contains("geometries") && geometry["geometries"].is_array()) {
        // Prefer the centroid of the first Point or Polygon
        for (const auto& g : geometry["geometries"]) {
            auto pt = computeCentroid(g);
            if (pt.first != 0.0 || pt.second != 0.0) {
                return pt;
            }
        }
    }

    return {0.0, 0.0};
}

std::array<double, 4> GeoUtils::computeBbox(const nlohmann::json& geometry) {
    double minLng = 180.0, minLat = 90.0, maxLng = -180.0, maxLat = -90.0;
    bool found = false;

    auto updateBounds = [&](double lng, double lat) {
        if (isValidCoordinate(lng, lat)) {
            minLng = std::min(minLng, lng);
            minLat = std::min(minLat, lat);
            maxLng = std::max(maxLng, lng);
            maxLat = std::max(maxLat, lat);
            found = true;
        }
    };

    if (!geometry.is_object() || !geometry.contains("type")) {
        return {0.0, 0.0, 0.0, 0.0};
    }

    std::string type = geometry["type"].get<std::string>();

    if (type == "Point" && geometry.contains("coordinates")) {
        auto c = geometry["coordinates"];
        if (c.size() >= 2) {
            updateBounds(c[0].get<double>(), c[1].get<double>());
        }
    } else if (type == "Polygon" && geometry.contains("coordinates")) {
        for (const auto& ring : geometry["coordinates"]) {
            for (const auto& pt : ring) {
                if (pt.size() >= 2) updateBounds(pt[0].get<double>(), pt[1].get<double>());
            }
        }
    } else if (type == "MultiPolygon" && geometry.contains("coordinates")) {
        for (const auto& poly : geometry["coordinates"]) {
            for (const auto& ring : poly) {
                for (const auto& pt : ring) {
                    if (pt.size() >= 2) updateBounds(pt[0].get<double>(), pt[1].get<double>());
                }
            }
        }
    } else if (type == "GeometryCollection" && geometry.contains("geometries")) {
        for (const auto& g : geometry["geometries"]) {
            auto b = computeBbox(g);
            if (b[0] != 0.0 || b[1] != 0.0 || b[2] != 0.0 || b[3] != 0.0) {
                updateBounds(b[0], b[1]);
                updateBounds(b[2], b[3]);
            }
        }
    }

    if (!found) {
        return {0.0, 0.0, 0.0, 0.0};
    }
    return {minLng, minLat, maxLng, maxLat};
}

ParsedRfsDescription GeoUtils::parseRfsDescription(const std::string& raw) {
    ParsedRfsDescription result;
    if (raw.empty()) return result;

    // Split on <br />, <br/>, <br>, or newlines
    std::regex brRegex(R"(<br\s*/?>|\n)", std::regex_constants::icase);
    std::sregex_token_iterator it(raw.begin(), raw.end(), brRegex, -1);
    std::sregex_token_iterator end;

    std::unordered_map<std::string, std::string> kv;

    for (; it != end; ++it) {
        std::string token = it->str();
        auto colonPos = token.find(':');
        if (colonPos != std::string::npos) {
            std::string key = token.substr(0, colonPos);
            std::string val = token.substr(colonPos + 1);

            // Trim key & val
            auto trim = [](std::string& s) {
                s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
                    return !std::isspace(ch);
                }));
                s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
                    return !std::isspace(ch);
                }).base(), s.end());
            };

            trim(key);
            trim(val);

            // Uppercase key for case-insensitive matching
            std::string upperKey = key;
            std::transform(upperKey.begin(), upperKey.end(), upperKey.begin(), ::toupper);
            kv[upperKey] = val;
        }
    }

    if (kv.find("ALERT LEVEL") != kv.end()) result.alertLevel = kv["ALERT LEVEL"];
    if (kv.find("LOCATION") != kv.end()) result.location = kv["LOCATION"];
    if (kv.find("COUNCIL AREA") != kv.end()) result.councilArea = kv["COUNCIL AREA"];
    if (kv.find("STATUS") != kv.end()) result.status = kv["STATUS"];
    if (kv.find("TYPE") != kv.end()) result.type = kv["TYPE"];
    if (kv.find("RESPONSIBLE AGENCY") != kv.end()) result.responsibleAgency = kv["RESPONSIBLE AGENCY"];
    if (kv.find("UPDATED") != kv.end()) result.updatedRaw = kv["UPDATED"];

    if (kv.find("FIRE") != kv.end()) {
        std::string f = kv["FIRE"];
        std::transform(f.begin(), f.end(), f.begin(), ::tolower);
        result.isFire = (f == "yes" || f == "true" || f == "y");
    }

    if (kv.find("SIZE") != kv.end()) {
        std::string sz = kv["SIZE"];
        // Extract numeric part (e.g. "512000 ha" -> 512000.0)
        std::regex numRegex(R"(([0-9]+(?:\.[0-9]+)?))");
        std::smatch sm;
        if (std::regex_search(sz, sm, numRegex)) {
            try {
                result.sizeHa = std::stod(sm[1].str());
            } catch (...) {
                result.sizeHa = std::nullopt;
            }
        }
    }

    return result;
}

std::string GeoUtils::getCurrentUtcIso() {
    auto now = std::chrono::system_clock::now();
    auto timeT = std::chrono::system_clock::to_time_t(now);
    std::tm tmUtc{};
#if defined(_WIN32)
    gmtime_s(&tmUtc, &timeT);
#else
    gmtime_r(&timeT, &tmUtc);
#endif

    std::ostringstream ss;
    ss << std::put_time(&tmUtc, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

std::string GeoUtils::formatAestIso(const std::string& utcIso) {
    if (utcIso.length() < 19) return utcIso;

    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    char sep;
    std::istringstream ss(utcIso);
    ss >> year >> sep >> month >> sep >> day >> sep >> hour >> sep >> minute >> sep >> second;

    std::tm tmUtc{};
    tmUtc.tm_year = year - 1900;
    tmUtc.tm_mon = month - 1;
    tmUtc.tm_mday = day;
    tmUtc.tm_hour = hour;
    tmUtc.tm_min = minute;
    tmUtc.tm_sec = second;
    tmUtc.tm_isdst = 0;

    // AEST is UTC+10 (or AEDT +11 during Daylight Saving between Oct and April)
    // Approximate AEST standard +10 hours
    time_t t = 0;
#if defined(_WIN32)
    t = _mkgmtime(&tmUtc);
#else
    t = timegm(&tmUtc);
#endif

    if (t == -1) return utcIso;

    // Add 10 hours for AEST
    time_t tAest = t + (10 * 3600);
    std::tm tmAest{};
#if defined(_WIN32)
    gmtime_s(&tmAest, &tAest);
#else
    gmtime_r(&tAest, &tmAest);
#endif

    std::ostringstream out;
    out << std::put_time(&tmAest, "%Y-%m-%dT%H:%M:%S") << "+10:00 (AEST)";
    return out.str();
}

std::string GeoUtils::getRelativeAge(const std::string& iso) {
    if (iso.empty()) return "Unknown";

    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    char sep;
    std::istringstream ss(iso);
    if (!(ss >> year >> sep >> month >> sep >> day)) return "Unknown";

    // If historical (more than 1 year in the past), return Month Year
    auto now = std::chrono::system_clock::now();
    auto timeT = std::chrono::system_clock::to_time_t(now);
    std::tm nowTm{};
#if defined(_WIN32)
    gmtime_s(&nowTm, &timeT);
#else
    gmtime_r(&nowTm, &tmUtc);
#endif

    if (nowTm.tm_year + 1900 > year) {
        static const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        std::string m = (month >= 1 && month <= 12) ? months[month - 1] : "";
        return "Historical: " + m + " " + std::to_string(year);
    }

    if (ss >> sep >> hour >> sep >> minute >> sep >> second) {
        std::tm tmIso{};
        tmIso.tm_year = year - 1900;
        tmIso.tm_mon = month - 1;
        tmIso.tm_mday = day;
        tmIso.tm_hour = hour;
        tmIso.tm_min = minute;
        tmIso.tm_sec = second;
#if defined(_WIN32)
        time_t t = _mkgmtime(&tmIso);
#else
        time_t t = timegm(&tmIso);
#endif
        if (t > 0 && timeT >= t) {
            long diffSec = static_cast<long>(timeT - t);
            if (diffSec < 60) return std::to_string(diffSec) + "s ago";
            if (diffSec < 3600) return std::to_string(diffSec / 60) + "m ago";
            if (diffSec < 86400) return std::to_string(diffSec / 3600) + "h ago";
            return std::to_string(diffSec / 86400) + "d ago";
        }
    }

    return "Recent";
}

bool GeoUtils::isLeapYear(int year) {
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

int GeoUtils::getDayOfYear(int year, int month, int day) {
    static const int daysBeforeMonth[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    if (month < 1 || month > 12 || day < 1 || day > 31) return 1;

    int doy = daysBeforeMonth[month - 1] + day;
    if (month > 2 && isLeapYear(year)) {
        doy += 1;
    }
    return doy;
}

int GeoUtils::getDayOfYearFromIso(const std::string& isoTimestamp) {
    if (isoTimestamp.length() < 10) return 1;
    try {
        int y = std::stoi(isoTimestamp.substr(0, 4));
        int m = std::stoi(isoTimestamp.substr(5, 2));
        int d = std::stoi(isoTimestamp.substr(8, 2));
        return getDayOfYear(y, m, d);
    } catch (...) {
        return 1;
    }
}

} // namespace bushfire
