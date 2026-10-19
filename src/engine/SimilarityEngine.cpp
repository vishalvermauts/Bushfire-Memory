#include "bushfire/SimilarityEngine.hpp"
#include "bushfire/GeoUtils.hpp"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace bushfire {

double SimilarityEngine::computeGeoScore(double distanceKm) {
    if (distanceKm < 0.0) distanceKm = 0.0;
    return std::clamp(std::exp(-distanceKm / 250.0), 0.0, 1.0);
}

double SimilarityEngine::computeSizeScore(double areaA, double areaB) {
    double safeA = std::max(0.0, areaA);
    double safeB = std::max(0.0, areaB);
    double logDiff = std::abs(std::log(safeA + 1.0) - std::log(safeB + 1.0));
    double score = 1.0 - (logDiff / 6.0);
    return std::clamp(score, 0.0, 1.0);
}

double SimilarityEngine::computeSeasonScore(int doyA, int doyB) {
    int diff = std::abs(doyA - doyB);
    int circularDiff = std::min(diff, 365 - diff);
    double score = 1.0 - (static_cast<double>(circularDiff) / 182.5);
    return std::clamp(score, 0.0, 1.0);
}

double SimilarityEngine::computeTypeScore(const std::string& typeA, const std::string& typeB,
                                         const std::string& bioA, const std::string& bioB) {
    if (typeA.empty() || typeB.empty()) return 0.0;

    bool sameType = (typeA == typeB);
    if (!sameType) return 0.0;

    bool sameBio = (!bioA.empty() && !bioB.empty() && bioA == bioB);
    return sameBio ? 1.0 : 0.5;
}

SimilarityMatch SimilarityEngine::evaluate(const HistoricalFireRecord& reference, const HistoricalFireRecord& candidate) {
    SimilarityMatch match;
    match.record = candidate;

    match.distanceKm = GeoUtils::haversineDistanceKm(
        reference.centroidLat, reference.centroidLng,
        candidate.centroidLat, candidate.centroidLng
    );

    match.geoScore = computeGeoScore(match.distanceKm);
    match.sizeScore = computeSizeScore(reference.areaHa, candidate.areaHa);
    match.seasonScore = computeSeasonScore(reference.dayOfYear, candidate.dayOfYear);
    match.typeScore = computeTypeScore(reference.fireType, candidate.fireType,
                                       reference.bioregionOrCouncil, candidate.bioregionOrCouncil);

    double total = (WEIGHT_GEO * match.geoScore) +
                   (WEIGHT_SIZE * match.sizeScore) +
                   (WEIGHT_SEASON * match.seasonScore) +
                   (WEIGHT_TYPE * match.typeScore);

    match.totalScorePct = std::clamp(total * 100.0, 0.0, 100.0);

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1);
    ss << "Match: " << match.totalScorePct << "% ("
       << "Geo: " << (match.geoScore * 100.0) << "% @ " << match.distanceKm << " km, "
       << "Scale: " << (match.sizeScore * 100.0) << "% (" << static_cast<long>(candidate.areaHa) << " ha), "
       << "Season: " << (match.seasonScore * 100.0) << "%, "
       << "Type: " << (match.typeScore * 100.0) << "%). "
       << "DISCLAIMER: Statistical attribute similarity reflects historical archive comparisons only and does not predict future fire behaviour, spread, or risk.";

    match.explanation = ss.str();
    return match;
}

std::vector<SimilarityMatch> SimilarityEngine::findTopSimilar(
    const HistoricalFireRecord& reference,
    const std::vector<HistoricalFireRecord>& candidates,
    size_t limit) {

    std::vector<SimilarityMatch> matches;
    matches.reserve(candidates.size());

    for (const auto& cand : candidates) {
        // Exclude the exact same event by ID
        if (cand.id == reference.id) continue;
        matches.push_back(evaluate(reference, cand));
    }

    std::sort(matches.begin(), matches.end(), [](const SimilarityMatch& a, const SimilarityMatch& b) {
        return a.totalScorePct > b.totalScorePct;
    });

    if (matches.size() > limit) {
        matches.resize(limit);
    }

    return matches;
}

} // namespace bushfire
