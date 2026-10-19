#pragma once

#include "bushfire/Types.hpp"
#include <vector>
#include <string>

namespace bushfire {

class SimilarityEngine {
public:
    static constexpr double WEIGHT_GEO = 0.35;
    static constexpr double WEIGHT_SIZE = 0.30;
    static constexpr double WEIGHT_SEASON = 0.20;
    static constexpr double WEIGHT_TYPE = 0.15;

    // Compute detailed similarity match between reference record and candidate
    static SimilarityMatch evaluate(const HistoricalFireRecord& reference, const HistoricalFireRecord& candidate);

    // Find top-N most similar events from candidate list
    static std::vector<SimilarityMatch> findTopSimilar(
        const HistoricalFireRecord& reference,
        const std::vector<HistoricalFireRecord>& candidates,
        size_t limit = 5
    );

    // Individual component scorers
    static double computeGeoScore(double distanceKm);
    static double computeSizeScore(double areaA, double areaB);
    static double computeSeasonScore(int doyA, int doyB);
    static double computeTypeScore(const std::string& typeA, const std::string& typeB,
                                   const std::string& bioA, const std::string& bioB);
};

} // namespace bushfire
