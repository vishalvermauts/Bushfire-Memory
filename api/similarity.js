// Vercel Serverless Function: 4-Factor Deterministic Similarity Matcher
function haversineDistanceKm(lat1, lon1, lat2, lon2) {
  const R = 6371.0;
  const dLat = (lat2 - lat1) * Math.PI / 180.0;
  const dLon = (lon2 - lon1) * Math.PI / 180.0;
  const a = Math.sin(dLat / 2) * Math.sin(dLat / 2) +
            Math.cos(lat1 * Math.PI / 180.0) * Math.cos(lat2 * Math.PI / 180.0) *
            Math.sin(dLon / 2) * Math.sin(dLon / 2);
  const c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
  return R * c;
}

export default async function handler(req, res) {
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'GET, OPTIONS');
  res.setHeader('Content-Type', 'application/json; charset=utf-8');

  if (req.method === 'OPTIONS') {
    return res.status(200).end();
  }

  const { event_id, limit = '5' } = req.query;
  const maxLimit = parseInt(limit, 10) || 5;

  const fires = [
    { id: "NPWS-HIST-2019-GOSPERS", fireName: "Gospers Mountain", fireSeason: "2019-20", fireType: "Wildfire", areaHa: 512000.0, bioregionOrCouncil: "Hawkesbury / Wollemi", dayOfYear: 299, centroidLng: 150.45, centroidLat: -33.15 },
    { id: "NPWS-HIST-2019-CURROWAN", fireName: "Currowan", fireSeason: "2019-20", fireType: "Wildfire", areaHa: 314000.0, bioregionOrCouncil: "Shoalhaven / South Coast", dayOfYear: 330, centroidLng: 150.18, centroidLat: -35.52 },
    { id: "NPWS-HIST-2019-DUNNS", fireName: "Dunns Road", fireSeason: "2019-20", fireType: "Wildfire", areaHa: 333000.0, bioregionOrCouncil: "Snowy Valleys / Riverina", dayOfYear: 362, centroidLng: 148.15, centroidLat: -35.65 },
    { id: "NPWS-HIST-2019-GREENWATTLE", fireName: "Green Wattle Creek", fireSeason: "2019-20", fireType: "Wildfire", areaHa: 278000.0, bioregionOrCouncil: "Wollondilly / Southern Highlands", dayOfYear: 331, centroidLng: 150.32, centroidLat: -34.18 },
    { id: "NPWS-HIST-2013-WARRUMBUNGLE", fireName: "Warrumbungle National Park", fireSeason: "2012-13", fireType: "Wildfire", areaHa: 56000.0, bioregionOrCouncil: "Warrumbungle / Coonabarabran", dayOfYear: 12, centroidLng: 149.02, centroidLat: -31.28 },
    { id: "NPWS-HIST-2003-CANBERRA", fireName: "Brindabella & McIntyre's Hut", fireSeason: "2002-03", fireType: "Wildfire", areaHa: 160000.0, bioregionOrCouncil: "ACT / Brindabella", dayOfYear: 8, centroidLng: 148.88, centroidLat: -35.32 },
    { id: "NPWS-HIST-2023-ROYAL-PB", fireName: "Royal NP Garie Ridge HR", fireSeason: "2023-24", fireType: "Prescribed Burn", areaHa: 450.0, bioregionOrCouncil: "Sutherland Shire", dayOfYear: 227, centroidLng: 151.08, centroidLat: -34.15 },
    { id: "NPWS-HIST-2022-BLUEMT-PB", fireName: "Sublime Point Leura HR", fireSeason: "2022-23", fireType: "Prescribed Burn", areaHa: 180.0, bioregionOrCouncil: "Blue Mountains", dayOfYear: 245, centroidLng: 150.34, centroidLat: -33.74 }
  ];

  const ref = fires.find(f => f.id === event_id) || fires[0];

  const matches = fires.map(target => {
    if (target.id === ref.id) {
      return {
        record: target,
        totalScorePct: 100.0,
        geoScore: 1.0,
        sizeScore: 1.0,
        seasonScore: 1.0,
        typeScore: 1.0,
        distanceKm: 0.0,
        explanation: "Identical fire baseline match (100.0% similarity)."
      };
    }

    const distKm = haversineDistanceKm(ref.centroidLat, ref.centroidLng, target.centroidLat, target.centroidLng);
    const sGeo = Math.max(0.0, 1.0 - distKm / 500.0);
    const sSize = Math.min(ref.areaHa, target.areaHa) / Math.max(ref.areaHa, target.areaHa);
    const diffDoy = Math.abs(ref.dayOfYear - target.dayOfYear);
    const sSeason = Math.max(0.0, 1.0 - Math.min(diffDoy, 365 - diffDoy) / 91.25);
    const sType = (ref.fireType === target.fireType) ? 1.0 : 0.2;

    const totalScorePct = (0.35 * sGeo + 0.25 * sSize + 0.20 * sSeason + 0.20 * sType) * 100.0;

    let narrative = [];
    if (sGeo > 0.7) narrative.push("Same regional cluster");
    if (sSize > 0.6) narrative.push("Comparable fire footprint");
    if (sSeason > 0.7) narrative.push("Matching seasonal window");
    if (sType === 1.0) narrative.push("Same wildfire classification");

    return {
      record: target,
      totalScorePct,
      geoScore: sGeo,
      sizeScore: sSize,
      seasonScore: sSeason,
      typeScore: sType,
      distanceKm: distKm,
      explanation: narrative.length > 0 ? narrative.join(" • ") : "Heuristic multi-factor match."
    };
  });

  matches.sort((a, b) => b.totalScorePct - a.totalScorePct);

  return res.status(200).json({
    referenceEvent: ref,
    matches: matches.slice(0, maxLimit)
  });
}
